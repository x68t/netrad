/********************************************************************
 *                                                                  *
 * THIS FILE IS PART OF THE OggVorbis SOFTWARE CODEC SOURCE CODE.   *
 * USE, DISTRIBUTION AND REPRODUCTION OF THIS LIBRARY SOURCE IS     *
 * GOVERNED BY A BSD-STYLE SOURCE LICENSE INCLUDED WITH THIS SOURCE *
 * IN 'COPYING'. PLEASE READ THESE TERMS BEFORE DISTRIBUTING.       *
 *                                                                  *
 * THE OggVorbis SOURCE CODE IS (C) COPYRIGHT 1994-2007             *
 * by the Xiph.Org Foundation http://www.xiph.org/                  *
 *                                                                  *
 ********************************************************************/

#include <fcntl.h>
#include <linux/soundcard.h>
#include <sys/ioctl.h>
#include <stdlib.h>
#include <signal.h>
#include <stdio.h>
#include <err.h>
#include <unistd.h>
#include <vorbis/vorbisfile.h>

#define S16_LE 0, 2, 1

int fd_audio;

int xwrite(int fd, const void *p, size_t size)
{
    return write(fd, p, size) == size? 0: -1;
}

int audio_open(const char *dev)
{
    if (!dev)
        dev = "/dev/dsp";

    if ((fd_audio = open(dev, O_WRONLY)) < 0)
        return -1;

    return 0;
}

int audio_init(int rate, int channels)
{
    const int fmt = AFMT_S16_NE;

    if (ioctl(fd_audio, SNDCTL_DSP_SPEED, &rate) < 0 ||
        ioctl(fd_audio, SNDCTL_DSP_CHANNELS, &channels) < 0 ||
        ioctl(fd_audio, SNDCTL_DSP_SETFMT, &fmt) < 0)
    {
        return -1;
    }

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
    audio_close();
    close(0);

    exit(0);
}

int main()
{
    int i;
    long n;
    int current_section;
    OggVorbis_File vf;
    vorbis_info *vi;
    char pcm[4096];

    if (signal(SIGHUP, sighup) == SIG_ERR)
        err(1, "signal");

    for (i = 3; i < 256; i++)
        close(i);

    if (ov_open_callbacks(stdin, &vf, NULL, 0, OV_CALLBACKS_NOCLOSE) < 0)
        errx(1, "ov_open_callbacks");
    
    if ((vi = ov_info(&vf, -1)) == NULL)
        errx(1, "ov_info");

    audio_open(NULL);
    audio_init(vi->rate, vi->channels);

    for (;;) {
        if ((n = ov_read(&vf, pcm, sizeof(pcm), S16_LE, &current_section)) > 0)
            xwrite(fd_audio, pcm, n);
        else if (n != OV_HOLE)
            break;
    }

    audio_close();
    close(0);
    if (n < 0)
        errx(1, "ov_read(%ld)", n);
    ov_clear(&vf);

    return 0;
}
