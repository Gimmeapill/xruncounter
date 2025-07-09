#define _GNU_SOURCE
#include <sys/sysinfo.h>

#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#include <jack/jack.h>

/*   gcc -Wall xruncounter.c -lm `pkg-config --cflags --libs jack` -o xruncounter */

#define MAX_CPUS  72 // Intel Xeon Phi 7290 
#define BUF_MAX 1024

jack_client_t   *client[MAX_CPUS];
jack_port_t     *in_port[MAX_CPUS];
jack_port_t     *out_port[MAX_CPUS];
jack_time_t      start[MAX_CPUS];
jack_time_t      stop[MAX_CPUS];
jack_time_t      frame_time[MAX_CPUS];
jack_time_t      frame_time1[MAX_CPUS];

int   run = 1;

int   xruns[MAX_CPUS];
int   grow[MAX_CPUS];
int   grow_it = 100;
int   first_x_run = 0;
float first_xrun_ms = 0;
float xrt = 0;
float dsp_load = 0;

double elapsedTime[MAX_CPUS];
double round_trip[MAX_CPUS];

char nperiods[10];
char rtprio[10];

char terminal_clearline [10];
char terminal_moveup [10];
char terminal_movedown [10];

unsigned long long int cpu_stats[4][MAX_CPUS]; // ticks/ticks_old/idle/idle_old
unsigned long long int ticks[10];

int cpus;
int CPUS;
// int CPU[MAX_CPUS]; disabled, as print is to slow to monitor threads switching CPU num.
FILE *fpstat;
int read_stat = 0;

int
read_ticks (FILE *fpsat, unsigned long long int *ticks)
{
    int retval;
    char buffer[BUF_MAX];
    
    if (!fgets (buffer, BUF_MAX, fpsat)) {
        perror ("Error"); 
    }
    retval = sscanf (buffer, "c%*s %16llu %16llu %16llu %16llu %16llu %16llu %16llu %16llu %Lu %16llu",
        &ticks[0], &ticks[1], &ticks[2],  &ticks[3],  &ticks[4],  &ticks[5], 
        &ticks[6], &ticks[7], &ticks[8], &ticks[9]); 
    if (retval == 0) {
        return -1;
    }
    if (retval < 4) {
        fprintf (stderr, "Error reading /proc/stat cpu field\n");
        return 0;
    }
    return 1;
}

void
cpu_info(FILE *fpsat, double *percent_usage)
{
    int i;
    unsigned long long int cpu_ticks;
    unsigned long long int cpu_idle;

    fseek (fpstat, 0, SEEK_SET);
    fflush (fpstat);

    for (int count = 0; count < CPUS+1; count++) {
        cpu_stats[1][count] = cpu_stats[0][count];
        cpu_stats[3][count] = cpu_stats[2][count];

        if (!read_ticks (fpstat, ticks)) break; 
        for (i=0, cpu_stats[0][count] = 0; i<8; i++) { // guest and guest_nice been included in user and nice already
          cpu_stats[0][count] += ticks[i]; 
        }
        cpu_stats[2][count] = ticks[3]; // idle time

        cpu_ticks = cpu_stats[0][count] - cpu_stats[1][count];
        cpu_idle = cpu_stats[2][count] - cpu_stats[3][count];

        percent_usage[count] = ((cpu_ticks - cpu_idle) / (double) cpu_ticks) * 100;
    }
}

int
monitor_stat(FILE *fpsat)
{
    int i;
    fpstat = fopen ("/proc/stat", "r");
    if (fpstat == NULL) {
        return 0;
    }
    for (int count = 0; count < CPUS+1; count++) {
        if (!read_ticks (fpstat, ticks)) break; 
        for (i=0, cpu_stats[0][count] = 0; i<8; i++) {
            cpu_stats[0][count] += ticks[i];
        }
        cpu_stats[2][count] = ticks[3]; /* idle ticks index */
    }
    return 1;
}

void
sys_info()
{
    FILE *fp;
    char infostr[BUF_MAX];
    char logstr[BUF_MAX];
    char log2str[BUF_MAX];

    printf("\n******************** SYSTEM CHECK *********************\n\n");

    fp = popen("grep -rnw -H '/proc/asound/card'*'/pcm'*'p/sub'*'/status' -e 'RUNNING' | awk -F '/' '{print $4}'|awk '{print substr($0,length,1)}' | uniq", "r");
    if (fp != NULL) {
        while (fgets(infostr, sizeof(infostr)-1, fp) != NULL) {
            strcpy(logstr, "cat /proc/asound/cards | sed -n '/^ ");
            strcat(logstr, strtok(infostr, "\n"));
            strcat(logstr, "/p' ");
            strcat(logstr, " | sed 's/.*:/Sound Playback:/' ");
            fp = popen(logstr, "r");
            if (fgets(infostr, sizeof(infostr)-1, fp) != NULL) {
                printf("    %s", infostr);
            }
        }
    }
    fp = NULL;

    fp = popen("grep -rnw -H '/proc/asound/card'*'/pcm'*'c/sub'*'/status' -e 'RUNNING' | awk -F '/' '{print $4}'|awk '{print substr($0,length,1)}' | uniq", "r");
    if (fp != NULL) {
        while (fgets(infostr, sizeof(infostr)-1, fp) != NULL) {
            strcpy(logstr, "cat /proc/asound/cards | sed -n '/^ ");
            strcat(logstr, strtok(infostr, "\n"));
            strcat(logstr, "/p' ");
            strcat(logstr, " | sed 's/.*:/Sound Capture:/' ");
            fp = popen(logstr, "r");
            if (fgets(infostr, sizeof(infostr)-1, fp) != NULL) {
                printf("     %s", infostr);
            }
        }
    }
    fp = NULL;

    fp = popen("lspci | grep VGA | sed 's/.*:/Graphic Card:/'", "r");
    if (fp != NULL) {
        if (fgets(infostr, sizeof(infostr)-1, fp) != NULL) {
            printf("      %s", infostr);
        }
    }
    fp = NULL;

    fp = popen("hostnamectl | sed -e '/Operating System/b' -e '/Kernel/b' -e '/Architecture/b' -e d", "r");
    if (fp == NULL) {
        printf("Failed to fetch system informations\n" );
    } else {
        while (fgets(infostr, sizeof(infostr)-1, fp) != NULL) {
            printf("%s", infostr);
        }
    }
    fp = NULL;

    fp = popen("cat /proc/cpuinfo | grep 'model name'| uniq | sed 's/[[:space:]]/ /g' | sed 's/model name /CPU/'", "r");
    if (fp != NULL) {
        if (fgets(infostr, sizeof(infostr)-1, fp) != NULL) {
            printf("               %s", infostr);
        }
    }
    fp = NULL;

    fp = popen("pidof jackd ", "r");
    if (fp == NULL) {
        fprintf(stderr,"jack isn't running?\n" );
    } else {
        if (fgets(infostr, sizeof(infostr)-1, fp) != NULL) {
            strcpy(logstr, "ps -p ");
            strcat(logstr, strtok(infostr, "\n"));
            strcat(logstr, " -o command=");
            strcpy(log2str, logstr);
            fp = NULL;
            fp = popen(logstr, "r");
            if (fgets(infostr, sizeof(infostr)-1, fp) != NULL) {
                printf("\n***************** jackd start parameter ****************\n\n");
                printf("    %s", infostr);
            }
            strcat(logstr, "| grep -oP '(?<=-n).*' | cut  -d' ' -f1");
            fp = NULL;
            fp = popen(logstr, "r");
            if (fgets(infostr, sizeof(infostr)-1, fp) != NULL) {
                strcpy(nperiods, strtok(infostr, "\n"));
            }
            strcat(log2str, "| grep -oP '(?<=-P).*' | cut  -d' ' -f1");
            fp = NULL;
            fp = popen(log2str, "r");
            if (fgets(infostr, sizeof(infostr)-1, fp) != NULL) {
                strcpy(rtprio, strtok(infostr, "\n"));
            }
        } else {
            fp = NULL;
            fp = popen("pidof jackdbus ", "r");
            if (fp == NULL) {
                fprintf(stderr,"jackdbus isn't running?\n" );
            } else {
                if (fgets(infostr, sizeof(infostr)-1, fp) != NULL) {
                    printf("\n****************** jackd dbus running ******************\n\n");
                    strcpy(logstr, getenv("HOME"));
                    strcat(logstr, "/.config/jack/conf.xml");
                    fp = NULL;
                    fp = fopen(logstr, "r");
                    if (fp == NULL) {
                        fprintf(stderr,".config/jack/conf.xml not found\n" );
                    } else {
                        while (fgets(infostr, sizeof(infostr), fp)) {
                            if(strstr(infostr, "nperiods") != NULL) {
                                const char *PATTERN1 = "<option name=\"nperiods\">";
                                const char *PATTERN2 = "</option>";
                                char *target = NULL;
                                char *start, *end;

                                if ( (start = strstr( infostr, PATTERN1 )) ) {
                                    start += strlen( PATTERN1 );
                                    if ( (end = strstr( start, PATTERN2 )) ) {
                                        target = ( char * )malloc( end - start + 1 );
                                        memcpy( target, start, end - start );
                                        target[end - start] = '\0';
                                    }
                                }
                                if ( target ) {
                                    strcpy(nperiods,target );
                                    free( target );
                                }
                            } else if(strstr(infostr, "realtime-priority") != NULL) {
                                const char *PATTERN1 = "<option name=\"realtime-priority\">";
                                const char *PATTERN2 = "</option>";
                                char *target = NULL;
                                char *start, *end;

                                if ( (start = strstr( infostr, PATTERN1 )) ) {
                                    start += strlen( PATTERN1 );
                                    if ( (end = strstr( start, PATTERN2 )) ) {
                                        target = ( char * )malloc( end - start + 1 );
                                        memcpy( target, start, end - start );
                                        target[end - start] = '\0';
                                    }
                                }
                                if ( target ) {
                                    strcpy(rtprio,target );
                                    free( target );
                                }
                            }
                        }
                    fclose(fp);
                    }
                }
            }
        }
    }
    fp = NULL;

    fp = popen("pactl list short modules 2>/dev/null | grep jack-", "r");
    printf("\n********************** Pulseaudio **********************\n\n");
    if (fp == NULL) {
        printf("    pulse isn't installed?\n" );
    } else {
        if (fgets(infostr, sizeof(infostr)-1, fp) != NULL) {
            printf("    pulse is active\n" );
        } else {
            printf("    pulse is not active\n" );
        }
    }
    pclose(fp);

}

void
jack_shutdown (void *arg)
{
    if (read_stat) {
        fclose(fpstat);
        for (int i = 1; i < CPUS+2; i++) {
            fprintf(stderr,"%s", terminal_clearline);
            fprintf(stderr,"%s", terminal_movedown);
        }
        for (int i = 1; i < CPUS+1; i++) {
            fprintf(stderr,"%s", terminal_moveup);
        }
    }
    exit (1);
}

int
jack_xrun_callback(void *arg)
{
    int v = (int) (intptr_t) arg;
    /* count xruns */
    xruns[v] += 1;
    if (xruns[v] == 1) {
        first_x_run = grow[v]/grow_it;
        dsp_load = jack_cpu_load(client[v]);
        first_xrun_ms = elapsedTime[v];
        xrt = round_trip[v];
    }
    fprintf (stderr, "Xrun %i at DSP load %.2f%s use %.2fms from %.2fms jack cycle time\n",
       xruns[v] ,jack_cpu_load(client[v]), "%", elapsedTime[v],round_trip[v]);
    if ((int)jack_cpu_load(client[v])>95) run = 0;
    return 0;
}

int
jack_srate_callback(jack_nframes_t samplerate, void* arg)
{
    fprintf (stderr, "Samplerate is %iHz \n", samplerate);
    return 0;
}

int
jack_buffersize_callback(jack_nframes_t nframes, void* arg)
{
    fprintf (stderr, "Buffersize is %i \n", nframes);
    return 0;
}

int
jack_process(jack_nframes_t nframes, void *arg)
{
    int v = (int) (intptr_t) arg;
    start[v] = jack_get_time();

    frame_time[v] = jack_frames_to_time (client[v], jack_last_frame_time(client[v]));
    round_trip[v] = (double)(frame_time[v] - frame_time1[v])/1000.0;
    frame_time1[v] = frame_time[v];

    double d = 0;
    for (int j ; j < grow[v]; ++j) {
        d = tan(atan(tan(atan(tan(atan(tan(atan(tan(atan(123456789.123456789))))))))));
    }
    grow[v] +=grow_it;
    d = 0;

    stop[v] = jack_get_time();
    elapsedTime[v] = (double)(stop[v]-start[v])/1000.0;
    // CPU[v] = sched_getcpu(); // monitor contex switches here!!
    return (int)d;
}

void
signal_handler (int sig)
{
    for (int i=0; i<cpus; i++) {
        jack_client_close (client[i]);
    }
    if (read_stat) {
        fclose(fpstat);
        for (int i = 1; i < CPUS+2; i++) {
            fprintf(stderr,"%s", terminal_clearline);
            fprintf(stderr,"%s", terminal_movedown);
        }
        for (int i = 1; i < CPUS+1; i++) {
            fprintf(stderr,"%s", terminal_moveup);
        }
    }
    fprintf (stderr, "\n signal %i received, exiting ...\n", sig);
    exit (0);
}

int
main (int argc, char *argv[])
{
    sys_info();

    double percent_usage[MAX_CPUS];

    sprintf(terminal_clearline, "%c[2K", 0x1B);
    sprintf(terminal_moveup, "%c[1A", 0x1B);
    sprintf(terminal_movedown, "%c[1B", 0x1B);

    cpus = 1;
    CPUS = get_nprocs();

    size_t optind;
    for (optind = 1; optind < argc && argv[optind][0] == '-'; optind++) {
        switch (argv[optind][1]) {
        case 's': grow_it = 10;  cpus = CPUS; break;
        case 'm': grow_it = 100; cpus = CPUS; break;
        default: grow_it = 100; cpus = 1; break;
        }   
    }   

    signal (SIGQUIT, signal_handler);
    signal (SIGTERM, signal_handler);
    signal (SIGHUP, signal_handler);
    signal (SIGINT, signal_handler);

    read_stat = monitor_stat(fpstat);

    for (int i=0; i<MAX_CPUS; i++) {
        grow[i] = grow_it;
    }

    printf("\n********************** Test %i Core *********************\n\n", cpus);

    for (int i=0; i<cpus; i++) {
        if ((client[i] = jack_client_open ("xruncounter", JackNoStartServer, NULL)) == 0) {
           fprintf (stderr, "jack server not running?\n");
           return 1;
        }

        in_port[i] = jack_port_register(
            client[i], "in_0", JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0);
        out_port[i] = jack_port_register(
            client[i], "out_0", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);

        jack_set_process_callback(client[i], jack_process, (void *) (intptr_t) i);
    }

    jack_set_xrun_callback(client[0], jack_xrun_callback, (void *) (intptr_t) 0);
    jack_set_sample_rate_callback(client[0], jack_srate_callback, (void *) (intptr_t) 0);
    jack_set_buffer_size_callback(client[0], jack_buffersize_callback, (void *) (intptr_t) 0);
    jack_on_shutdown (client[0], jack_shutdown, (void *) (intptr_t) 0);

    for (int i=0; i<cpus; i++) {
        if (jack_activate (client[i])) {
           fprintf (stderr, "cannot activate client");
           return 1;
        }
    }

    if (strlen(nperiods)) {
        fprintf (stderr, "Buffer/Periods  %s\n", nperiods);
    }

    if (jack_is_realtime(client[0])) {
        if(strlen(rtprio)) {
            fprintf (stderr, "jack running with realtime priority %s\n", rtprio);
        } else {
            fprintf (stderr, "jack running with realtime priority\n");
        }
    } else {
        fprintf (stderr, "jack isn't running with realtime priority\n"); 
    }

    while (run) {
        usleep (200000);

        if (read_stat) {
            cpu_info(fpstat, percent_usage);

            for (int i=1; i<CPUS+1; i++) {
                if (i == 0) {
                    fprintf (stderr,"Total = %3.2lf%%                                       \n", percent_usage[i]);
                } else {
                    fprintf (stderr,"CPU %i = %3.2lf%%                                       \n", i, percent_usage[i]);
                }
            }
        }
        fprintf (stderr, "DSP load %.2f%s use %.2fms from %.2fms jack cycle time \r",
         jack_cpu_load(client[0]),"%", elapsedTime[0], round_trip[0]);

        if (read_stat) {
            for (int i = 1; i < CPUS+1; i++) {
                fprintf(stderr,"%s", terminal_moveup);
            }
        }
    }
    if (read_stat) {
        for (int i = 1; i < CPUS+2; i++) {
            fprintf(stderr,"%s", terminal_clearline);
            fprintf(stderr,"%s", terminal_movedown);
        }
        for (int i = 1; i < CPUS+1; i++) {
            fprintf(stderr,"%s", terminal_moveup);
        }
    }
    fprintf(stderr, "in complete %i Xruns in %i cycles                                  \n", xruns[0], grow[0]/grow_it);
    fprintf(stderr, "first Xrun happen at DSP load %.2f%s in cycle %i\n", dsp_load, "%", first_x_run);
    fprintf(stderr, "process takes %.2fms from total %.2fms jack cycle time\n",first_xrun_ms, xrt);

    for (int i=0; i<cpus; i++) {
        jack_client_close (client[i]);
    }
    if (read_stat) fclose(fpstat);
    exit (0);
}
