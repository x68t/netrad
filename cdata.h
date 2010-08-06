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

#ifndef _CDATA_H_
#define _CDATA_H_

#include <sys/types.h>

struct cdata {
    int ident;
    void *handler;
    void *ctx;
    int used;
};

struct cdata_head {
    size_t nalloc;
    struct cdata **cdata;
};

typedef int (*cdata_matcher_t)(const struct cdata *cd1, const struct cdata *cd2);

void cdata_head_free(struct cdata_head *head);
int cdata_head_alloc(struct cdata_head *head, size_t nalloc);

int cdata_match_by_ident(const struct cdata *cd1, const struct cdata *cd2);
int cdata_match_by_all(const struct cdata *cd1, const struct cdata *cd2);
struct cdata *cdata_find(const struct cdata_head *head, const struct cdata *cd, cdata_matcher_t matcher);

int cdata_is_used(const struct cdata *cd);

struct cdata *cdata_add(struct cdata_head *head, int ident, void *handler, void *ctx);
void cdata_del(struct cdata *cd);

#endif // _CDATA_H_
