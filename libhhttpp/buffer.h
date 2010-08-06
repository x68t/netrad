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

#ifndef _BUFFER_H_
#define _BUFFER_H_

#include <sys/types.h>

struct hhttpp;

struct hhttpp_buffer {
    size_t nalloc;
    size_t nread;
    void *data;
};

struct hhttpp_linebuffer {
    struct hhttpp_buffer *buf;
    char *nextline;
};

void hhttpp_buffer_free(struct hhttpp_buffer *buf);
int hhttpp_buffer_resize(struct hhttpp_buffer *buf, size_t nalloc);
struct hhttpp_buffer *hhttpp_buffer_alloc(size_t nalloc);
ssize_t hhttpp_buffer_read(int fd, struct hhttpp_buffer *buf);
void *hhttpp_buffer_begin(const struct hhttpp_buffer *buf);
void *hhttpp_buffer_end(const struct hhttpp_buffer *buf);
void *hhttpp_buffer_find(const struct hhttpp_buffer *buf, const char *s);
void hhttpp_buffer_flush(struct hhttpp_buffer *buf);
int hhttpp_buffer_move(struct hhttpp_buffer *buf, void *top);

struct hhttpp_linebuffer *hhttpp_linebuffer_alloc(struct hhttpp_buffer *buf);
void hhttpp_linebuffer_free(struct hhttpp_linebuffer *lbuf);
char *hhttpp_linebuffer_getline(struct hhttpp_linebuffer *lbuf);


#endif // _BUFFER_H_
