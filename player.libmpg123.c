/*
  mpglib: test program for libmpg123, in the style of the legacy mpglib test program

  copyright 2007 by the mpg123 project - free software under the terms of the LGPL 2.1
  see COPYING and AUTHORS files in distribution or http://mpg123.org
  initially written by Thomas Orgis
*/

#include <unistd.h>
#include <stdio.h>
#include <linux/soundcard.h>
#include <sys/ioctl.h>
#include <string.h>
#include <stdio.h>
#include <poll.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <getopt.h>
#include <errno.h>
#include <err.h>
#include "mpg123.h"
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
    swrite(2, "usage: player.libmpg123 [-m metaint]\n");
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

int audio_init(int rate, int channels, int encoding)
{
    const int fmt = AFMT_S16_NE;

    if (encoding != MPG123_ENC_SIGNED_16) {
        logger(LOG_ERR, "audio_init: invalid encoding: %d", encoding);
        return -1;
    }

    if (ioctl(fd_audio, SNDCTL_DSP_SPEED, &rate) < 0 ||
        ioctl(fd_audio, SNDCTL_DSP_CHANNELS, &channels) < 0 ||
        ioctl(fd_audio, SNDCTL_DSP_SETFMT, &fmt) < 0)
    {
        logger(LOG_ERR, "ioctl: %m");
        return -1;
    }
    logger(LOG_INFO, "audio_init: %dHz, %dch", rate, channels);

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
            meta_set_metaint(i);
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

static ssize_t safe_read(int fd, void *buf, size_t bufsize)
{
    ssize_t n, r;
    char *p;

    p = buf;
    r = bufsize;
    while (r > 0) {
        if ((n = read(fd, p, r)) < 0)
            return -1;
        else if (n == 0)
            break;
        p += n;
        r -= n;
    }

    return bufsize - r;
}

void *stream_reader(void *ctx)
{
    int fd, *fds;
    int has_metadata;
    size_t stream_size;
    char *stream;

    fds = ctx;
    fd = fds[1];
    has_metadata = 0;
    if ((stream_size = meta_get_metaint()))
        has_metadata = 1;
    else
        stream_size = 2048;
    if (!(stream = malloc(stream_size))) {
        logger(LOG_ERR, "malloc: %m");
        exit(1);
    }

    for (;;) {
        if (safe_read(STREAM_IN, stream, stream_size) != stream_size) {
            logger(LOG_ERR, "read: %m");
            exit(0);
        }
        if (write(fd, stream, stream_size) != stream_size) {
            logger(LOG_ERR, "write: %m");
            exit(1);
        }
        if (!has_metadata)
            continue;
        if (meta_read(STREAM_IN, META_OUT) < 0) {
            logger(LOG_ERR, "meta_read: %m");
            exit(1);
        }
    }

    return NULL;
}

int player(int fd)
{
    int r, ch;
    long flags;
    size_t pcm_size, len;
    char *meta;
    struct mpg123_frameinfo frameinfo;
    mpg123_handle *mh;
    static unsigned char pcm[PCM_BUFSIZ];

    r = -1;
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

    if (mpg123_open_fd(mh, fd) != MPG123_OK) {
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
            logger(LOG_ERR, "audio_init: %ldHz, %dch", frameinfo.rate, ch);
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
                if (write(META_OUT, meta, len) != len) {
                    logger(LOG_ERR, "write: meta: %m");
                    return 1;
                }
            }
            if (write(fd_audio, pcm, pcm_size) != pcm_size) {
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
    int fds[2];
    pthread_t th;

    fd_audio = -1;
    logger_init("player.libmpg123", -1);
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

    if (meta_get_metaint() > 0) {
        if (meta_sync(STREAM_IN, META_OUT) < 0) {
            logger(LOG_ERR, "meta_sync");
            return 1;
        }
    }

    if (pipe(fds) < 0) {
        logger(LOG_ERR, "pipe: %m");
        return 1;
    }

    if (pthread_create(&th, NULL, stream_reader, fds)) {
        logger(LOG_ERR, "pthread_create: %m");
        return 1;
    }

    r = player(fds[0]);

    audio_close();

    return r < 0? 1: 0;
}
