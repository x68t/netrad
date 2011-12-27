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
#include <sys/ioctl.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <signal.h>
#include <getopt.h>
#include <errno.h>
#include <unistd.h>
#include <ao/ao.h>
#include "meta.h"
#include "logger.h"

#define STREAM_IN 0
#define META_OUT 1
#define PCM_BUFSIZ (1152*2*2*8)

const char *device_name;
ao_device *device;

int swrite(int fd, const char *s)
{
    size_t len;

    len = strlen(s);

    return write(fd, s, len) == len? 0: -1;
}

void usage()
{
    swrite(2, "usage: player.raw [-l log-level] [-m metaint] [-D device-name]\n");
    exit(1);
}

int audio_open(const char *dev)
{
    return 0;
}

int audio_init()
{
    ao_sample_format format;

    memset(&format, 0, sizeof(format));
    format.bits = 16;
    format.channels = 2;
    format.rate = 44100;
    format.byte_format = AO_FMT_LITTLE;
    if (!(device = ao_open_live(ao_default_driver_id(), &format, NULL))) {
        logger(LOG_ERR, "ao_open_live: %dHz, %dch", format.rate, format.channels);
        return -1;
    }
    logger(LOG_DEBUG, "audio_init: %dHz, %dch", format.rate, format.channels);

    return 0;
}

void audio_close()
{
    if (!device)
        return;
    ao_close(device);
    device = NULL;
}

void sighup(int signo)
{
    //logger(LOG_DEBUG, "SIGHUP");
    audio_close();
    close(STREAM_IN);
    _exit(0);
}

void options(int argc, char *argv[])
{
    int c, i;
    char *p;

    while ((c = getopt(argc, argv, "m:l:D:")) != -1) {
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
          case 'D':
            device_name = optarg;
            break;
          default:
            usage();
            /* NOTREACHED */
        }
    }
}

int player(int fd_stream)
{
    ssize_t n;
    static int16_t pcm[1152*2*8];

    for (;;) {
        if ((n = read(fd_stream, pcm, sizeof(pcm))) < 0) {
            logger(LOG_ERR, "read: %m");
            return -1;
        }
        if (ao_play(device, pcm, n) != 1) {
            logger(LOG_ERR, "ao_play: %m");
            return -1;
        }
    }

    return 0;
}

int main(int argc, char *argv[])
{
    int i, r;

    logger_init("player.raw", -1);
    logger(LOG_INFO, "start");

    for (i = 3; i < 256; i++)
        close(i);

    if (signal(SIGHUP, sighup) == SIG_ERR) {
        logger(LOG_ERR, "signal: %m");
        return 1;
    }

    options(argc, argv);

    ao_initialize();
    if (audio_open(device_name) < 0) {
        logger(LOG_ERR, "audio_init: %m");
        return 1;
    }

    if (audio_init() < 0) {
        logger(LOG_ERR, "audio_init: %m");
        return 1;
    }

    r = player(STREAM_IN);

    audio_close();
    ao_shutdown();
    close(STREAM_IN);

    return 0;
}
