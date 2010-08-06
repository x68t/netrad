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
#include <unistd.h>
#include "event.h"
#include "audio.h"
#include "http.h"
#include "logger.h"
#include "libhhttpp/hhttpp.h"

int cmd_tune(int fd, void *ctx)
{
    int fd_http;
    struct hhttpp *req;
    char *url;

    if ((url = (char *)ctx) == NULL)
        return -1;

    if ((req = hhttpp_alloc()) == NULL)
        return -1;

    hhttpp_request_header_add(req, "ICY-Metadata", "1", 1);
    hhttpp_request_header_add(req, "User-Agent", "Winamp", 1);
    hhttpp_request_header_add(req, "Accept", "*/*", 1);
    if (hhttpp_url_set(req, url) < 0 ||
        hhttpp_connect(req) == hhttpp_current_status_error ||
        (fd_http = hhttpp_fd_get(req)) < 0)
    {
        hhttpp_free(req);
        return -1;
    }
    logger(LOG_INFO, "cmd_tune: fd=%d: `%s'", fd, url);

    return event_fdwrite_add(fd_http, http_request, req);
}

int cmd_stop(int fd, void *ctx)
{
    logger(LOG_INFO, "cmd_stop: fd=%d", fd);

    return audio_stop(NULL, NULL);
}

int cmd_close(int fd, void *ctx)
{
    logger(LOG_INFO, "cmd_close: fd=%d", fd);

    event_fd_del(fd);
    close(fd);

    return 0;
}

int cmd_status(int fd, void *ctx)
{
    logger(LOG_INFO, "cmd_status: fd=%d", fd);

    return audio_status(fd);
}
