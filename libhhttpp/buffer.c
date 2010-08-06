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
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "buffer.h"
#include "hhttpp.h"

void hhttpp_buffer_free(struct hhttpp_buffer *buf)
{
    if (!buf)
        return;
    if (buf->data)
        free(buf->data);
    free(buf);
}

int hhttpp_buffer_resize(struct hhttpp_buffer *buf, size_t nalloc)
{
    void *p;

    if (!buf)
        return -1;

    if ((p = realloc(buf->data, nalloc)) == NULL)
        return -1;
    buf->data = p;
    buf->nalloc = nalloc;

    return 0;
}

struct hhttpp_buffer *hhttpp_buffer_alloc(size_t nalloc)
{
    struct hhttpp_buffer *buf;

    if (nalloc == 0)
        nalloc = 4096;

    if ((buf = calloc(sizeof(*buf), 1)) == NULL)
        return NULL;
    if (hhttpp_buffer_resize(buf, nalloc) < 0) {
        hhttpp_buffer_free(buf);
        return NULL;
    }

    return buf;
}

void *hhttpp_buffer_begin(const struct hhttpp_buffer *buf)
{
    if (!buf)
        return NULL;

    return buf->data;
}

void *hhttpp_buffer_end(const struct hhttpp_buffer *buf)
{
    if (!buf)
        return NULL;

    return buf->data + buf->nread;
}

ssize_t hhttpp_buffer_read(int fd, struct hhttpp_buffer *buf)
{
    ssize_t n, r;
    void *p;

    if (!buf)
        return -1;

    p = hhttpp_buffer_end(buf);
    if ((r = buf->nalloc - buf->nread) < 2048) {
        if (hhttpp_buffer_resize(buf, buf->nalloc+1024) < 0)
            return -1;
        p = hhttpp_buffer_end(buf);
        r = buf->nalloc - buf->nread;
    }

    if ((n = read(fd, p, r)) < 0)
        return -1;

    buf->nread += n;

    return n;
}

void *hhttpp_buffer_find(const struct hhttpp_buffer *buf, const char *s)
{
    size_t slen;
    char *begin, *end, *p;

    if (!buf || !s || !*s)
        return NULL;

    slen = strlen(s);
    begin = hhttpp_buffer_begin(buf);
    end = hhttpp_buffer_end(buf) - slen;

    for (p = begin; p < end; p++) {
        if (*p != *s)
            continue;
        if (strncmp(p, s, slen) == 0)
            return p;
    }

    return NULL;
}

void hhttpp_buffer_flush(struct hhttpp_buffer *buf)
{
    if (!buf)
        return;
    buf->nread = 0;
}

int hhttpp_buffer_move(struct hhttpp_buffer *buf, void *top)
{
    ssize_t size;
    void *begin, *end;

    if (!buf || !top)
        return -1;

    begin = hhttpp_buffer_begin(buf);
    end = hhttpp_buffer_end(buf);
    if (top < begin || end < top)
        return -1;
    if (end == top) {
        hhttpp_buffer_flush(buf);
        return 0;
    }

    size = end - top;
    memmove(begin, top, size);
    buf->nread = size;

    return 0;
}

struct hhttpp_linebuffer *hhttpp_linebuffer_alloc(struct hhttpp_buffer *buf)
{
    struct hhttpp_linebuffer *lbuf;

    if (!buf)
        return NULL;

    if ((lbuf = calloc(sizeof(*lbuf), 1)) == NULL)
        return NULL;
    lbuf->buf = buf;
    lbuf->nextline = hhttpp_buffer_begin(buf);

    return lbuf;
}

void hhttpp_linebuffer_free(struct hhttpp_linebuffer *lbuf)
{
    if (!lbuf)
        return;
    free(lbuf);
}

char *hhttpp_linebuffer_getline(struct hhttpp_linebuffer *lbuf)
{
    char *line, *end, *p;
    struct hhttpp_buffer *buf;

    buf = lbuf->buf;
    end = hhttpp_buffer_end(buf);
    line = lbuf->nextline;
    if ((p = memchr(line, '\r', end - line)) == NULL)
        return NULL;
    *p = '\0';
    lbuf->nextline = p + 2;

    return line;
}
