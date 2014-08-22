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
#include "url.h"

#define has_prefix(s, prefix) !strncmp(s, prefix, strlen(prefix))

void hhttpp_url_free(struct hhttpp_url *url)
{
    if (!url)
        return;
    if (url->host)
        free(url->host);
    free(url);
}

struct hhttpp_url *hhttpp_url_parse(const char *url_string)
{
    char *p;
    const char *host;
    struct hhttpp_url *url;

    if (!url_string)
        return NULL;

    if (has_prefix(url_string, "http://"))
        host = url_string + 7;
    else
        host = url_string;
    if (!*host)
        return NULL;

    if ((url = calloc(sizeof(*url), 1)) == NULL)
        return NULL;

    if ((url->host = strdup(host)) == NULL)
        goto bad;
    if ((p = strchr(url->host, '/')) == NULL) {
        // "http://www.example.com"
        url->path = "";
    } else {
        *p = '\0';
        url->path = p + 1;      // without root's "/"
    }

    if ((p = strchr(url->host, ':')) == NULL)
        return url;

    // with port
    *p++ = '\0';
    if (*p) {
        if ((url->port = strtol(p, &p, 10)) < 0 || *p) {
            // bad port number
            goto bad;
        }
    } else {
        // http://www.example.com:/path
        goto bad;
    }

    return url;

  bad:
    hhttpp_url_free(url);

    return NULL;
}
