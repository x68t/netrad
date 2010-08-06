/*-
 * Copyright (c) 2008-2010
 *      Hiroyuki Yamashita <hiroya@magical-technology.com>.  All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <unistd.h>
#include <stdio.h>
#include <linux/soundcard.h>
#include <sys/ioctl.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <signal.h>
#include <getopt.h>
#include <errno.h>
#include <unistd.h>
#include "meta.h"
#include "logger.h"

#define STREAM_IN 0
#define META_OUT 1
#define PCM_BUFSIZ (1152*2*2*8)

int fd_audio;

int swrite(int fd, const char *s)
{
    size_t len;

    len = strlen(s);

    return write(fd, s, len) == len? 0: -1;
}

void usage()
{
    swrite(2, "usage: player.raw [-m metaint]\n");
    exit(1);
}

int audio_open(const char *dev)
{
    if (!dev)
        dev = "/dev/dsp";

    if ((fd_audio = open(dev, O_WRONLY)) < 0) {
        logger(LOG_ERR, "open: %s: %m", dev);
        return -1;
    }

    return 0;
}

int audio_init()
{
    const int rate = 44100;
    const int channels = 2;
    const int fmt = AFMT_S16_NE;

    if (ioctl(fd_audio, SNDCTL_DSP_SPEED, &rate) < 0 ||
        ioctl(fd_audio, SNDCTL_DSP_CHANNELS, &channels) < 0 ||
        ioctl(fd_audio, SNDCTL_DSP_SETFMT, &fmt) < 0)
    {
        logger(LOG_ERR, "ioctl: %m");
        return -1;
    }
    logger(LOG_DEBUG, "audio_init: %dHz, %dch", rate, channels);

    return 0;
}

void audio_close()
{
    if (fd_audio < 0)
        return;

    close(fd_audio);
    fd_audio = -1;
}

void sighup(int signo)
{
    //logger(LOG_DEBUG, "SIGHUP");
    audio_close();
    close(STREAM_IN);
    _exit(0);
}

void option(int argc, char *argv[])
{
    int c, i;
    char *p;

    while ((c = getopt(argc, argv, "m:l:")) != -1) {
        switch (c) {
          case 'm':
            i = strtol(optarg, &p, 10);
            if (*p)
                usage();
            //meta_set_metaint(i);
            break;
          case 'l':
            if (logger_set_level_by_string(optarg) < 0)
                usage();
            break;
          default:
            usage();
            /* NOTREACHED */
        }
    }
}

int main(int argc, char *argv[])
{
    int i;
    ssize_t n;
    static int16_t pcm[1152*2*8];

    fd_audio = -1;
    logger_init("player.raw", -1);
    logger(LOG_INFO, "start");

    if (signal(SIGHUP, sighup) == SIG_ERR) {
        logger(LOG_ERR, "signal: %m");
        return 1;
    }

    for (i = 3; i < 256; i++)
        close(i);

    if (audio_open(NULL) < 0)
        return 1;

    option(argc, argv);

#if 0
    if (meta_get_metaint() > 0) {
        if (meta_sync(STREAM_IN, META_OUT) < 0)
            return 1;
    }
#endif

    if (audio_init() < 0) {
        logger(LOG_ERR, "audio_init: %m");
        return 1;
    }

    for (;;) {
        if ((n = read(STREAM_IN, pcm, sizeof(pcm))) < 0) {
            logger(LOG_ERR, "read: %m");
            return 1;
        }
        if (write(fd_audio, pcm, n) != n) {
            logger(LOG_ERR, "write: %m");
            return 1;
        }
    }

    audio_close();
    close(STREAM_IN);
    close(fd_audio);

    return 0;
}
