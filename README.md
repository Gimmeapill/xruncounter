# xruncounter
Small linux tool written in C by Hermann Meyer (aka https://github.com/brummer10) to measure jack xruns and evaluate the overall performance of a system for realtime audio.
Published originally on https://www.linuxmusicians.com
See https://www.linuxmusicians.com/viewtopic.php?f=27&t=19268&start=15 for usage details.

### Build

- just build with the command:

```
gcc -Wall xruncounter.c -lm `pkg-config --cflags --libs jack` -o xruncounter
```

## Features:

- collect System informations about:
    - active Sound Card,
    - Graphic Card,
    - Operating System,
    - Kernel,
    - Architecture,
    - CPU,
    - jackd/jackdbus,
    - pulseaudio,

- Monitor DSP and CPU load

## Usage

- to run a simple test run: ./xruncounter
- to run a test for multiple Core CPU, run: ./xruncounter -m
- to run a Stress Test (multiple Core test with slowly growing DSP load)
    - run: ./xruncounter -s 
    
    Be careful with that one, please note the next option before things turn critical !
- to quit a test before it's over, press `ctrl + c`

