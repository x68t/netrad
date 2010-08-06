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

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "header.h"

void hhttpp_header_free(struct hhttpp_header *h)
{
    if (!h)
        return;

    if (h->key)
        free(h->key);
    if (h->value)
        free(h->value);
    free(h);
}

struct hhttpp_header *hhttpp_header_alloc(const char *key, const char *value)
{
    struct hhttpp_header *h;

    if ((h = calloc(sizeof(*h), 1)) == NULL ||
        (h->key = strdup(key)) == NULL ||
        (h->value = strdup(value)) == NULL)
    {
        hhttpp_header_free(h);
        return NULL;
    }

    return h;
}

struct hhttpp_header *hhttpp_header_find(struct hhttpp_header_list *list, const char *key)
{
    struct hhttpp_header *h;

    if (!list)
        return NULL;

    STAILQ_FOREACH(h, list, next) {
        if (strcasecmp(key, h->key) == 0)
            return h;
    }

    return NULL;
}

int hhttpp_header_add(struct hhttpp_header_list *list, const char *key, const char *value, int overwrite)
{
    char *v;
    struct hhttpp_header *h;

    if (!list || !key || !value)
        return -1;

    if ((h = hhttpp_header_find(list, key)) != NULL &&
        overwrite == 0)
    {
        return 0;
    }

    if (h) {
        if ((v = strdup(value)) == NULL)
            return -1;
        free(h->value);
        h->value = v;
    } else {
        if ((h = hhttpp_header_alloc(key, value)) == NULL)
            return -1;
        STAILQ_INSERT_TAIL(list, h, next);
    }

    return 0;
}

int hhttpp_header_del(struct hhttpp_header_list *list, const char *key)
{
    struct hhttpp_header *h;

    if (!list || !key)
        return -1;

    if ((h = hhttpp_header_find(list, key)) == NULL)
        return -1;
    STAILQ_REMOVE(list, h, hhttpp_header, next);

    return 0;
}

void hhttpp_header_list_free(struct hhttpp_header_list *list)
{
    struct hhttpp_header *h;

    if (!list)
        return;

    while ((h = STAILQ_FIRST(list)) != NULL) {
        STAILQ_REMOVE_HEAD(list, next);
        hhttpp_header_free(h);
    }

    free(list);
}

struct hhttpp_header_list *hhttpp_header_list_alloc()
{
    struct hhttpp_header_list *list;

    if ((list = malloc(sizeof(*list))) == NULL)
        return NULL;

    STAILQ_INIT(list);

    return list;
}

struct hhttpp_header *hhttpp_header_first(const struct hhttpp_header_list *list)
{
    if (!list)
        return NULL;
    return STAILQ_FIRST(list);
}

struct hhttpp_header *hhttpp_header_next(const struct hhttpp_header *header)
{
    if (!header)
        return NULL;
    return STAILQ_NEXT(header, next);
}

void hhttpp_header_list_each(const struct hhttpp_header_list *list,
                             void (*f)(const struct hhttpp_header *, void *),
                             void *ctx)
{
    struct hhttpp_header *h;

    if (!list)
        return;

    for (h = STAILQ_FIRST(list); h; h = STAILQ_NEXT(h, next))
        (*f)(h, ctx);
}
