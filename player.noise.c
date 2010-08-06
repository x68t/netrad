/*-
 * Copyright (c) 2008
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

#include <sys/types.h>
#include <linux/soundcard.h>
#include <sys/ioctl.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <err.h>

int xwrite(int fd, const void *p, size_t size)
{
    return write(fd, p, size) == size? 0: -1;
}

int audio_open(const char *dev, int rate, int channels)
{
    int fd;
    const int fmt = AFMT_S16_NE;

    if (!dev)
        dev = "/dev/dsp";

    if ((fd = open(dev, O_WRONLY)) < 0)
        return -1;

    if (ioctl(fd, SNDCTL_DSP_SPEED, &rate) < 0 ||
        ioctl(fd, SNDCTL_DSP_CHANNELS, &channels) < 0 ||
        ioctl(fd, SNDCTL_DSP_SETFMT, &fmt) < 0)
    {
        close(fd);
        return -1;
    }

    return fd;
}

#define SIGN(x) ((x) < 0? -1: 1)

void noise(int16_t *pcm, size_t pcmsize)
{
    int i;

    srandom(time(NULL));
    for (i = 0; i < pcmsize/sizeof(*pcm); i++)
        pcm[i] = random();
}

void amp(double mult, int16_t *pcm, size_t pcmsize)
{
    int i;

    for (i = 0; i < pcmsize/sizeof(*pcm); i++)
        pcm[i] *= mult;
}

int main()
{
    int fd, i;
    int16_t pcm[1152*2*8];

    noise(pcm, sizeof(pcm));
    amp(0.3, pcm, sizeof(pcm));

    if ((fd = audio_open(NULL, 44100, 2)) < 0)
        err(1, "audio_open");

    for (i = 0; i < 4; i++)
        xwrite(fd, pcm, sizeof(pcm));
    for (i = 0; i < 16; i++) {
        amp(0.95, pcm, sizeof(pcm));
        xwrite(fd, pcm, sizeof(pcm));
    }

    close(fd);

    return 0;
}
