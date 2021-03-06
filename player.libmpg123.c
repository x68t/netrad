/*
  mpglib: test program for libmpg123, in the style of the legacy mpglib test program

  copyright 2007 by the mpg123 project - free software under the terms of the LGPL 2.1
  see COPYING and AUTHORS files in distribution or http://mpg123.org
  initially written by Thomas Orgis
*/

#include <unistd.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <string.h>
#include <stdio.h>
#include <poll.h>
#include <fcntl.h>
#include <signal.h>
#include <getopt.h>
#include <errno.h>
#include <err.h>
#include <ao/ao.h>
#include "mpg123.h"
#include "meta.h"
#include "logger.h"

#define STREAM_IN 0
#define META_OUT 1
#define PCM_BUFSIZ (1152*2*2*8)

const char *device_name;
ao_device *device;
int fs, ch;
int (*audio_write)(ao_device *device, char *p, uint_32 size);

int swrite(int fd, const char *s)
{
    size_t len;

    len = strlen(s);

    return write(fd, s, len) == len? 0: -1;
}

void usage()
{
    swrite(2, "usage: player.libmpg123 [-l log-level] [-m metaint] [-D device-name]\n");
    exit(1);
}

int audio_open(const char *dev)
{
    return 0;
}

int audio_init(int rate, int channels, int encoding)
{
    ao_sample_format format;

    if (encoding != MPG123_ENC_SIGNED_16) {
        logger(LOG_ERR, "audio_init: invalid encoding: %d", encoding);
        return -1;
    }

    memset(&format, 0, sizeof(format));
    format.bits = 16;
    format.channels = channels;
    format.rate = rate;
    format.byte_format = AO_FMT_LITTLE;
    if (!(device = ao_open_live(ao_default_driver_id(), &format, NULL))) {
        logger(LOG_ERR, "ao_open_live: %dHz, %dch", rate, channels);
        return -1;
    }

    logger(LOG_INFO, "audio_init: %dHz, %dch", rate, channels);
    fs = rate;
    ch = channels;

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
            if (i > 0)
                meta_set_metaint(i);
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

void amplitude(const void *p, size_t size, float gain)
{
    int i;
    size_t n;
    int16_t *q;

    q = (int16_t *)p;
    n = size / sizeof(*q);
    for (i = 0; i < n; i++)
        q[i] *= gain;
}

int mute_write(ao_device *device, char *p, uint_32 size)
{
    float gain;
    static size_t nbyte = 0;

#define DURATION 1.0
    nbyte += size;
    gain = nbyte / (DURATION * fs * ch * sizeof(int16_t));
    if (gain < 1)
        amplitude(p, size, gain);
    else
        audio_write = ao_play;

    return ao_play(device, p, size);
}

int player(int fd_stream, int fd_meta)
{
    int r, ch;
    long flags;
    size_t pcm_size, len;
    char *meta;
    struct mpg123_frameinfo frameinfo;
    mpg123_handle *mh;
    static unsigned char pcm[PCM_BUFSIZ];

    r = -1;
    audio_write = mute_write;
    mpg123_init();
    if ((mh = mpg123_new(NULL, &r)) == NULL) {
        logger(LOG_ERR, "mpg123_new: %s", mpg123_plain_strerror(r));
        goto done;
    }

    if (mpg123_getparam(mh, MPG123_FLAGS, &flags, NULL) != MPG123_OK) {
        logger(LOG_ERR, "mpg123_getparam: MPG123_FLAGS: %s", mpg123_strerror(mh));
        goto done;
    }
    flags |= MPG123_QUIET;
    if (mpg123_param(mh, MPG123_FLAGS, flags, 0) != MPG123_OK) {
        logger(LOG_ERR, "mpg123_param: MPG123_FLAGS: %s", mpg123_strerror(mh));
        goto done;
    }

    if (meta_get_metaint() > 0) {
        if (mpg123_param(mh, MPG123_ICY_INTERVAL, meta_get_metaint(), 0) != MPG123_OK) {
            logger(LOG_ERR, "mpg123_param: MPG123_ICY_INTERVAL: %s", mpg123_strerror(mh));
            return 1;
        }
        if (meta_sync(fd_stream, fd_meta) < 0) {
            logger(LOG_ERR, "meta_sync");
            return 1;
        }
    }

    if (mpg123_open_fd(mh, fd_stream) != MPG123_OK) {
        logger(LOG_ERR, "mpg123_open_fd: %s", mpg123_strerror(mh));
        goto done;
    }

    for (;;) {
        if ((r = mpg123_read(mh, pcm, PCM_BUFSIZ, &pcm_size)) == MPG123_DONE) {
            r = 0;
            logger(LOG_INFO, "EOF");
            goto done;
        }
        if (r != MPG123_OK)
            continue;
        if (mpg123_info(mh, &frameinfo) != MPG123_OK)
            continue;
        ch = frameinfo.mode == MPG123_M_MONO? 1: 2;
        if (audio_init((int)frameinfo.rate, ch, MPG123_ENC_SIGNED_16) < 0) {
            logger(LOG_ERR, "audio_init: %ldHz, %dch: %m", frameinfo.rate, ch);
            goto done;
        }
        break;
    }

    for (;;) {
        switch (r = mpg123_read(mh, pcm, sizeof(pcm), &pcm_size)) {
          case MPG123_NEW_FORMAT:
            break;
          case MPG123_OK:
            if (mpg123_meta_check(mh) & MPG123_NEW_ICY &&
                mpg123_icy(mh, &meta) == MPG123_OK)
            {
                len = strlen(meta);
                if (write(fd_meta, meta, len) != len) {
                    logger(LOG_ERR, "write: meta: %m");
                    return 1;
                }
            }
            if (audio_write(device, (char *)pcm, pcm_size) != 1) {
                logger(LOG_ERR, "write: PCM: %m");
                return 1;
            }
            break;
          case MPG123_DONE:
            r = 0;
            logger(LOG_DEBUG, "EOF");
            goto done;
          default:
            if (mpg123_errcode(mh) == MPG123_OUT_OF_SYNC)
                continue;
            logger(LOG_ERR, "mpg123_read: %s", mpg123_strerror(mh));
            return 1;
        }
    }

  done:
    if (mh)
        mpg123_delete(mh);
    mpg123_exit();

    return r;
}

int main(int argc, char *argv[])
{
    int i, r;

    logger_init("player.libmpg123", -1);
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

    r = player(STREAM_IN, META_OUT);
    audio_close();
    ao_shutdown();

    return r < 0? 1: 0;
}
