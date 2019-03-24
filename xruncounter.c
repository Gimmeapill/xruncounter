#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <math.h>

#include <jack/jack.h>

/*   gcc -Wall xruncounter.c -lm `pkg-config --cflags --libs jack` -o xruncounter */

jack_client_t *client;
jack_port_t *in_port;
jack_port_t *out_port;

static int xruns = 0;
static int grow = 1;
static int first_x_run = 0;
static float dsp_load = 0;
static int run = 1;


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
   }
   fprintf (stderr, "Xrun %i at DSP load %f\n",xruns , jack_cpu_load(client));
   if ((int)jack_cpu_load(client)>95) run = 0;
   return 0;
}

int
jack_srate_callback(jack_nframes_t samplerate, void* arg)
{
    fprintf (stderr, "Samplerate %i \n", samplerate);
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
    double d = 0;
    for (int j ; j < grow; ++j) {
        d = tan(atan(tan(atan(tan(atan(tan(atan(tan(atan(123456789.123456789))))))))));
    }
    grow +=100;
    d = 0;
    return (int)d;
}

void
signal_handler (int sig)
{
   jack_client_close (client);
   fprintf (stderr, " signal %i received, exiting ...\n", sig);
   exit (0);
}

int
main (int argc, char *argv[])

{

   if ((client = jack_client_open ("xruncounter", JackNullOption, NULL)) == 0) {
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
   
   if (!jack_is_realtime(client)) {
       fprintf (stderr, "jack isn't running with realtime priority\n");
   } else {
      fprintf (stderr, "jack running with realtime priority\n");
   }
   
   while (run) {
      usleep (100000);
      fprintf (stderr, "DSP load %f  \r", jack_cpu_load(client));
   }
   
   fprintf(stderr, "in complete %i Xruns in %i circles\nfirst Xrun happen at DSP load %f circle %i\n", xruns, grow/100, dsp_load, first_x_run);

   jack_client_close (client);
   exit (0);
}
