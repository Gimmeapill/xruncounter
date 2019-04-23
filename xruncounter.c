#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#include <jack/jack.h>

/*   gcc -Wall xruncounter.c -lm `pkg-config --cflags --libs jack` -o xruncounter */

jack_client_t   *client;
jack_port_t     *in_port;
jack_port_t     *out_port;
jack_time_t      start;
jack_time_t      stop;
jack_time_t      frame_time;
jack_time_t      frame_time1;

static int   xruns = 0;
static int   grow = 100;
static int   first_x_run = 0;
static float first_xrun_ms = 0;
static float xrt = 0;
static float dsp_load = 0;
static int   run = 1;

double elapsedTime = 0.0;
double round_trip = 0.0;

char nperiods[10];

void
sys_info()
{
    FILE *fp;
    char infostr[200];
    char logstr[200];

    fp = popen("hostnamectl | sed -e '/Chassis/b' -e '/Operating System/b' -e '/Kernel/b' -e '/Architecture/b' -e d", "r");
    if (fp == NULL) {
        printf("Failed to fetch system informations\n" );
    } else {
        printf("\n************************ SYSTEM ************************\n\n");
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
                strcpy(nperiods, infostr);
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
                                break;
                            }
                        }
                    fclose(fp);
                    }
                }
            }
        }
    }
    fp = NULL;

    fp = popen("pactl info | grep jack", "r");
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

    printf("\n************************* Test *************************\n\n");
}

void
jack_shutdown (void *arg)
{
    exit (1);
}

int
jack_xrun_callback(void *arg)
{
    /* count xruns */
    xruns += 1;
    if (xruns == 1) {
        first_x_run = grow/100;
        dsp_load = jack_cpu_load(client);
        first_xrun_ms = elapsedTime;
        xrt = round_trip;
    }
    fprintf (stderr, "Xrun %i at DSP load %.2f%s use %.2fms from %.2fms jack cycle time\n",
      xruns ,jack_cpu_load(client), "%", elapsedTime,round_trip);
    if ((int)jack_cpu_load(client)>95) run = 0;
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
    start = jack_get_time();

    frame_time = jack_frames_to_time (client, jack_last_frame_time(client));
    round_trip = (double)(frame_time - frame_time1)/1000.0;
    frame_time1 = frame_time;

    double d = 0;
    for (int j ; j < grow; ++j) {
        d = tan(atan(tan(atan(tan(atan(tan(atan(tan(atan(123456789.123456789))))))))));
    }
    grow +=100;
    d = 0;

    stop = jack_get_time();
    elapsedTime = (double)(stop-start)/1000.0;
    
    return (int)d;
}

void
signal_handler (int sig)
{
    jack_client_close (client);
    fprintf (stderr, "\n signal %i received, exiting ...\n", sig);
    exit (0);
}

int
main (int argc, char *argv[])
{
    sys_info();

    if ((client = jack_client_open ("xruncounter", JackNoStartServer, NULL)) == 0) {
       fprintf (stderr, "jack server not running?\n");
       return 1;
    }

    in_port = jack_port_register(
        client, "in_0", JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0);
    out_port = jack_port_register(
        client, "out_0", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);

    signal (SIGQUIT, signal_handler);
    signal (SIGTERM, signal_handler);
    signal (SIGHUP, signal_handler);
    signal (SIGINT, signal_handler);

    jack_set_xrun_callback(client, jack_xrun_callback, 0);
    jack_set_sample_rate_callback(client, jack_srate_callback, 0);
    jack_set_buffer_size_callback(client, jack_buffersize_callback, 0);
    jack_set_process_callback(client, jack_process, 0);
    jack_on_shutdown (client, jack_shutdown, 0);

    if (jack_activate (client)) {
       fprintf (stderr, "cannot activate client");
       return 1;
    }

    if (strlen(nperiods)) {
        fprintf (stderr, "Buffer/Periods  %s\n", nperiods);
    }

    if (!jack_is_realtime(client)) {
       fprintf (stderr, "jack isn't running with realtime priority\n");
    } else {
       fprintf (stderr, "jack running with realtime priority\n"); 
    }

    while (run) {
       usleep (100000);
       fprintf (stderr, "DSP load %.2f%s use %.2fms from %.2fms jack cycle time \r",
         jack_cpu_load(client),"%", elapsedTime, round_trip);
    }

    fprintf(stderr, "in complete %i Xruns in %i cycles                      \n", xruns, grow/100);
    fprintf(stderr, "first Xrun happen at DSP load %.2f%s in cycle %i\n", dsp_load, "%", first_x_run);
    fprintf(stderr, "process takes %.2fms from total %.2fms jack cycle time\n",first_xrun_ms, xrt);

    jack_client_close (client);
    exit (0);
}
