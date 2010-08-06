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
#include <stdio.h>
#include "cdata.h"

void cdata_head_free(struct cdata_head *head)
{
    int i;
    struct cdata *cd;

    if (head->cdata) {
        for (i = 0; i < head->nalloc; i++) {
            if ((cd = head->cdata[i]) == NULL)
                break;
            free(cd);
        }
        free(head->cdata);
    }
    head->nalloc = 0;
    head->cdata = NULL;
}

static int cdata_head_realloc(struct cdata_head *head, size_t nalloc)
{
    size_t i;
    struct cdata **cd;

    if (head->nalloc >= nalloc)
        return -1;

    if ((cd = realloc(head->cdata, sizeof(*cd) * nalloc)) == NULL)
        return -1;

    for (i = head->nalloc; i < nalloc; i++)
        cd[i] = NULL;

    head->nalloc = nalloc;
    head->cdata = cd;

    return 0;
}

int cdata_head_alloc(struct cdata_head *head, size_t nalloc)
{
    if (nalloc == 0)
        nalloc = 8;

    memset(head, 0, sizeof(*head));
    if (cdata_head_realloc(head, nalloc) < 0)
        return -1;

    return 0;
}

int cdata_match_by_ident(const struct cdata *cd1, const struct cdata *cd2)
{
    if (!cd1 || !cd2)
        return 1;

    return cd1->ident - cd2->ident;
}

int cdata_match_by_all(const struct cdata *cd1, const struct cdata *cd2)
{
    int i;
    ssize_t s;

    if (!cd1 || !cd2)
        return 1;

    if ((i = cd1->ident - cd2->ident) != 0)
        return i;
    if ((s = cd1->handler - cd2->handler) != 0)
        return s;
    if ((s = cd1->ctx - cd2->ctx) != 0)
        return s;

    return 0;
}

struct cdata *cdata_find(const struct cdata_head *head, const struct cdata *rcd,
                         int (*matcher)(const struct cdata *cd1, const struct cdata *cd2))
{
    size_t i;
    struct cdata *cd;

    for (i = 0; i < head->nalloc; i++) {
        if ((cd = head->cdata[i]) == NULL)
            break;
        if ((*matcher)(cd, rcd) == 0)
            return cd;
    }

    return NULL;
}

static size_t growsize(size_t nalloc)
{
    if (nalloc > 4096)
        abort();

    if (nalloc < 128)
        return nalloc * 2;
    return nalloc + 128;
}

static struct cdata *get_unused(struct cdata_head *head)
{
    size_t i, n;
    struct cdata *cd;

    for (i = 0; i < head->nalloc; i++) {
        if ((cd = head->cdata[i]) == NULL)
            return head->cdata[i] = calloc(sizeof(struct cdata), 1);
        if (!cd->used)
            return cd;
    }

    n = head->nalloc;
    if (cdata_head_realloc(head, growsize(n)) < 0)
        return NULL;

    return head->cdata[n] = calloc(sizeof(struct cdata), 1);
}

int cdata_is_used(const struct cdata *cd)
{
    if (!cd)
        return 0;

    return cd->used;
}

struct cdata *cdata_add(struct cdata_head *head, int ident, void *handler, void *ctx)
{
    struct cdata *cd;

    if ((cd = get_unused(head)) == NULL)
        return NULL;

    cd->ident = ident;
    cd->handler = handler;
    cd->ctx = ctx;
    cd->used = 1;

    return cd;
}

void cdata_del(struct cdata *cd)
{
    if (!cd)
        return;

    cd->used = 0;
}
