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

#ifndef _HEADER_H_
#define _HEADER_H_

#include <sys/queue.h>

struct hhttpp_header {
    STAILQ_ENTRY(hhttpp_header) next;
    char *key;
    char *value;
};

STAILQ_HEAD(hhttpp_header_list, hhttpp_header);

void hhttpp_header_free(struct hhttpp_header *h);
struct hhttpp_header *hhttpp_header_alloc(const char *key, const char *value);
struct hhttpp_header *hhttpp_header_find(struct hhttpp_header_list *list, const char *key);

int hhttpp_header_add(struct hhttpp_header_list *list, const char *key, const char *value, int overwrite);
int hhttpp_header_del(struct hhttpp_header_list *list, const char *key);

void hhttpp_header_list_free(struct hhttpp_header_list *list);
struct hhttpp_header_list *hhttpp_header_list_alloc();

struct hhttpp_header *hhttpp_header_first(const struct hhttpp_header_list *list);
struct hhttpp_header *hhttpp_header_next(const struct hhttpp_header *header);
void hhttpp_header_list_each(const struct hhttpp_header_list *list, void (*f)(const struct hhttpp_header *, void *), void *ctx);


#endif // _HEADER_H_
