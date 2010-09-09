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

#define _GNU_SOURCE
#include <sys/types.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "event.h"
#include "http.h"
#include "audio.h"
#include "cmd.h"
#include "logger.h"
#include "libhhttpp/hhttpp.h"
#include <err.h>

#ifndef MP3_PLAYER
#define MP3_PLAYER "./player.libmpg123"
#endif

#ifndef OGG_PLAYER
#define OGG_PLAYER "./player.libogg"
#endif

#ifndef NOISE_PLAYER
#define NOISE_PLAYER "./player.noise"
#endif

int http_request(int fd, void *ctx)
{
    struct hhttpp *req;

    event_fd_del(fd);

    req = ctx;
    if (hhttpp_request(req) == hhttpp_current_status_error) {
        hhttpp_free(req);
        return -1;
    }

    return event_fdread_add(fd, http_receive, req);
}

static int type_audio_mpeg(int fd, struct hhttpp *req)
{
    event_fd_del(fd);

    return audio_start(fd, req, MP3_PLAYER);
}

static int type_application_ogg(int fd, struct hhttpp *req)
{
    event_fd_del(fd);

    return audio_start(fd, req, OGG_PLAYER);
}

static int type_audio_x_raw(int fd, struct hhttpp *req)
{
    event_fd_del(fd);

    return audio_start(fd, req, RAW_PLAYER);
}

static char *get_body(struct hhttpp *req)
{
    ssize_t len;
    char *body, *p;

    if (hhttpp_current_status_get(req) != hhttpp_current_status_done)
        return NULL;

    if ((len = hhttpp_body_length_get(req)) < 0)
        return NULL;
    if ((body = hhttpp_body_get(req)) == NULL)
        return NULL;
    if ((p = malloc(len + 1)) == NULL)
        return NULL;
    memcpy(p, body, len);
    p[len] = '\0';

    return p;
}

static int type_audio_mpegurl(int fd, struct hhttpp *req)
{
    int r;
    char *body, *p, *q;

    if ((body = get_body(req)) == NULL)
        return 0;

    r = -1;
    p = body;
    if (strncmp(p, "http://", 7)) {
        if ((p = strstr(p, "\nhttp://")) == NULL)
            goto out;
        p++;
    }

    if ((q = strchr(p, '\r')) != NULL)
        *q = '\0';
    if ((q = strchr(p, '\n')) != NULL)
        *q = '\0';

    r = cmd_tune(fd, p);

  out:
    free(body);
    event_fd_del(fd);
    hhttpp_free(req);

    return r;
}

static int type_audio_x_scpls(int fd, struct hhttpp *req)
{
    int r;
    char *body, *b, *p;

    if ((body = get_body(req)) == NULL)
        return 0;

    r = -1;
    if ((b = strstr(body, "File1=")) == NULL)
        goto done;
    b += 6;
    if ((p = strchr(b, '\r')) != NULL)
        *p = '\0';
    if ((p = strchr(b, '\n')) != NULL)
        *p = '\0';

    r = cmd_tune(fd, b);

  done:
    free(body);
    event_fd_del(fd);
    hhttpp_free(req);

    return r;
}

static int type_application_xspf(int fd, struct hhttpp *req)
{
    int r;
    char *body, *b, *p;

    if ((body = get_body(req)) == NULL)
        return 0;

    r = -1;
    if ((b = strstr(body, "<location>")) == NULL)
        goto done;
    b += 10;
    if ((p = strstr(b, "</location>")) == NULL)
        goto done;
    *p = '\0';

    r = cmd_tune(fd, b);

  done:
    free(body);
    event_fd_del(fd);
    hhttpp_free(req);

    return r;
}

static int type_video_x_ms_asx(int fd, struct hhttpp *req)
{
    int r, c;
    char *body, *b, *p;

    if (!(body = get_body(req)))
        return 0;

    r = -1;
    if (!(b = strcasestr(body, "<ref")))
        goto done;
    if (!(p = strstr(b, "/>")))
        goto done;
    *p = '\0';
    p = b;
    if (!(b = strchr(p, '"')) &&
        !(b = strchr(p, '\'')))
    {
        goto done;
    }
    c = *b++;
    if (!(p = strchr(b, c)))
        goto done;
    *p = '\0';

    r = cmd_tune(fd, b);

  done:
    free(body);
    event_fd_del(fd);
    hhttpp_free(req);

    return r;
}

#if 0
static void diag(struct hhttpp *req)
{
    int i;
    char *body;
    char **headers;

    logger(LOG_ERR, "diag: start");

    logger(LOG_ERR, "Status-Code: %d", hhttpp_status_code_get(req));
    if (hhttpp_response_headers_get(req, &headers) < 0)
        logger(LOG_ERR, "ghttp_get_header_names");
    else {
        for (i = 0; headers[i]; i++)
            logger(LOG_ERR, "%s", headers[i]);
        hhttpp_response_headers_free(headers);
    }

    if ((body = get_body(req)) == NULL)
        logger(LOG_ERR, "body: NULL");
    else {
        logger(LOG_ERR, "body: `%s'", body);
        free(body);
    }

    logger(LOG_ERR, "diag: end");
}
#endif

int http_receive(int fd, void *ctx)
{
    int i;
    char *p;
    const char *content_type, *location;
    struct hhttpp *req;
    static const struct {
        const char *name;
        int (*fn)(int, struct hhttpp *);
    } types[] = {
        { "audio/mpeg", type_audio_mpeg },
        { "application/ogg", type_application_ogg },
        { "audio/x-raw", type_audio_x_raw },
        { "audio/mpegurl", type_audio_mpegurl },
        { "audio/x-mpegurl", type_audio_mpegurl },
        { "audio/x-scpls", type_audio_x_scpls },
        { "application/xspf+xml", type_application_xspf },
        { "video/x-ms-asx", type_video_x_ms_asx },
        { "video/x-ms-asf", type_video_x_ms_asx },
        { NULL, NULL }
    };

    req = (struct hhttpp *)ctx;

    if (hhttpp_process(req) == hhttpp_current_status_error) {
        logger(LOG_WARNING, "hhttpp_process");
        goto bad;
    }

    if (hhttpp_current_status_get(req) != hhttpp_current_status_body_reading &&
        hhttpp_current_status_get(req) != hhttpp_current_status_done)
    {
        return 0;
    }

    /* header done */
    if ((content_type = hhttpp_response_header_get(req, "Content-Type")) == NULL)
        goto bad;
    if ((p = strchr(content_type, ';')))
        *p = '\0';
    logger(LOG_INFO, "Content-Type: %s", content_type);
    for (i = 0; types[i].name; i++) {
        if (strcmp(content_type, types[i].name))
            continue;
        return (*types[i].fn)(fd, req);
    }

    /* 30x moved */
    switch (hhttpp_status_code_get(req)) {
      case 301:
      case 302:
        if ((location = hhttpp_response_header_get(req, "Location")) != NULL) {
            logger(LOG_INFO, "Location: `%s'", location);
            i = cmd_tune(fd, (void *)location);
            event_fd_del(fd);
            hhttpp_free(req);
            return i;
        } else
            logger(LOG_WARNING, "Location: NULL");
        break;
      default:
        break;
    }

    event_fd_del(fd);
    return audio_start(fd, req, NOISE_PLAYER);
    //diag(req);

  bad:
    event_fd_del(fd);
    hhttpp_free(req);

    return -1;
}
