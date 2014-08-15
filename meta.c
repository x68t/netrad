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

#include <sys/types.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include "meta.h"
#include "logger.h"

#define META_SYNC_KEYWORD "StreamTitle="
#define META_SYNC_KEYWORD_LEN 12

static int metaint = 0;

int meta_set_metaint(int mi)
{
    int m;

    if (mi < 0)
        return -1;
    m = metaint;
    metaint = mi;

    return m;
}

int meta_get_metaint()
{
    return metaint;
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

static ssize_t meta_len(size_t mp, const char *buf, size_t bufsize)
{
    if (mp < bufsize)
        return (unsigned char)buf[mp] * 16;

    return -1;
}

static ssize_t meta_next(size_t mp, const char *buf, size_t bufsize)
{
    ssize_t len;

    if ((len = meta_len(mp, buf, bufsize)) < 0)
        return -1;

    return mp + 1 + len + metaint;
}

// find "StreamTitle="
static ssize_t meta_find(const char *buf, size_t bufsize)
{
    int i;

    for (i = 1; i < bufsize; i++) {
        if (buf[i] != META_SYNC_KEYWORD[0] || buf[i+1] != META_SYNC_KEYWORD[1])
            continue;
        if (strncmp(&buf[i], META_SYNC_KEYWORD, META_SYNC_KEYWORD_LEN) == 0) {
            if (buf[i-1] != '\0')
                return i - 1;
            // pseudo-sync
        }
    }
    logger(LOG_WARNING, "meta_find: cannot find `%s'", META_SYNC_KEYWORD);

    return -1;
}

// trace meta-chain
static ssize_t meta_find2(const char *buf, size_t bufsize)
{
    size_t i;
    ssize_t n, nn;

    for (i = 0; i < bufsize; i++) {
        if (buf[i] != 0)
            continue;
        if ((n = meta_next(i, buf, bufsize)) < 0)
            return -1;
        if (buf[n] != 0)
            continue;
        logger(LOG_INFO, "meta_find2: sync1: mp=%ld, metaint=%d", i, metaint);   // pseudo-sync probability: about 10%
        if ((nn = meta_next(n, buf, bufsize)) < 0)
            return -1;
        if (buf[nn] != 0)
            continue;
        logger(LOG_INFO, "meta_find2: sync2: mp=%ld, metaint=%d", i, metaint);
        return i; // pseudo-sync probability: less than 0.1%
    }

    return -1;
}

static const char *meta_body(size_t mp, const char *buf, size_t bufsize)
{
    ssize_t len;

    if ((len = meta_len(mp, buf, bufsize)) < 0)
        return NULL;
    if ((mp + 1 + len) < bufsize)
        return buf + mp + 1;
    logger(LOG_WARNING, "meta_body: incomplete");

    return NULL;
}

static int meta_write(int fd, size_t mp, const char *buf, size_t bufsize)
{
    ssize_t len;
    const char *body;

    if ((len = meta_len(mp, buf, bufsize)) < 0 ||
        (body = meta_body(mp, buf, bufsize)) == NULL)
    {
        return -1;
    }

    if (len == 0)
        return 0;

    return write(fd, body, len) == len? 0: -1;
}

int meta_read(int fdi, int fdo)
{
    unsigned char c;
    int len;
    char buf[4096];

    if (read(fdi, &c, 1) != 1)
        return -1;
    if ((len = c * 16) == 0)
        return 0;
    if (safe_read(fdi, buf, len) != len)
        return -1;
    if (write(fdo, buf, len) != len)
        return -1;

    return 0;
}

int meta_sync(int fdi, int fdo)
{
    int retval;
    ssize_t mp, smp;
    size_t bufsize, r, metafreq, nread;
    char *buf;

    if (metaint == 0)
        return 0;

    metafreq = metaint + 1 + 0xff * 16;
    bufsize = metafreq * 4;
    if ((buf = calloc(bufsize + META_SYNC_KEYWORD_LEN + 1, 1)) == NULL)
        return -1;

    retval = -1;
    nread = metafreq;
    if (safe_read(fdi, buf, nread) != nread)
        goto out;

    if ((mp = meta_find(buf, nread)) < 0) {
        if (safe_read(fdi, buf+nread, bufsize-nread) != bufsize-nread)
            goto out;
        nread = bufsize;
        if ((mp = meta_find(buf, nread)) < 0) {
            if ((mp = meta_find2(buf, nread)) < 0)
                goto out;
        }
    }

    if (mp != metaint)
        logger(LOG_WARNING, "meta_find: mp=%ld, metaint=%d", mp, metaint);

    if (meta_write(fdo, mp, buf, nread) < 0)
        goto out;

    // find last meta
    smp = mp;
    while ((mp = meta_next(mp, buf, nread)) < nread) {
        if (mp < 0)
            break;
        if (meta_write(fdo, mp, buf, nread) < 0)
            goto out;
        smp = mp;
    }
    mp = smp;

    // skip to next meta
    /*
     * buf
     *  |<---------- nread --------->|
     *  +-----------+---+--------+----..||.....+...+........+....
     *  |    ...    |len|metadata|   stream    |len|metadata| ...
     *  +-----------+---+--------+----..||.....+...+........+....
     *              ^mp          |<- metaint ->|
     *  |<-- mp+1+meta_len(mp) ->|
     *                           |<->|<-- r -->|
     *                              \
     *                              nread - (mp + 1 + meta_len(mp))
     */
    r = metaint - (nread - (mp + 1 + meta_len(mp, buf, nread)));
    if (safe_read(fdi, buf, r) != r)
        goto out;

    if (meta_read(fdi, fdo) < 0)
        goto out;

    retval = 0;

  out:
    free(buf);

    return retval;
}
