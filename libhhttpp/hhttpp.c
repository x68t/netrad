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

#define _GNU_SOURCE
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <ctype.h>
#include <netdb.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include "hhttpp.h"
#include "hhttpp-int.h"
#include "url.h"
#include "buffer.h"
#include "header.h"

#include <err.h>

#define DEFAULT_USER_AGENT "Mozilla/4.0 (compatible; libhhttpp/0.1)"

void hhttpp_free(struct hhttpp *hh)
{
    if (!hh)
        return;

    if (hh->fd != -1)
        close(hh->fd);
    if (hh->content_type)
        free(hh->content_type);
    if (hh->url)
        hhttpp_url_free(hh->url);
    if (hh->request_header)
        hhttpp_header_list_free(hh->request_header);
    if (hh->response_header)
        hhttpp_header_list_free(hh->response_header);
    if (hh->response_buffer)
        hhttpp_buffer_free(hh->response_buffer);
    free(hh);
}

struct hhttpp *hhttpp_alloc()
{
    struct hhttpp *hh;

    if ((hh = calloc(sizeof(*hh), 1)) == NULL)
        return NULL;
    hh->fd = -1;
    hh->content_length = -1;
    if ((hh->request_header = hhttpp_header_list_alloc()) == NULL ||
        (hh->response_header = hhttpp_header_list_alloc()) == NULL ||
        (hh->response_buffer = hhttpp_buffer_alloc(0)) == NULL)
    {
        hhttpp_free(hh);
        return NULL;
    }

    return hh;
}

int hhttpp_url_set(struct hhttpp *hh, const char *url_string)
{
    struct hhttpp_url *url;

    if ((url = hhttpp_url_parse(url_string)) == NULL)
        return -1;
    hh->url = url;

    return 0;
}

int hhttpp_request_header_add(struct hhttpp *hh, const char *key, const char *value, int overwrite)
{
    struct hhttpp_header_list *list;

    if (!hh || !(list = hh->request_header))
        return -1;

    return hhttpp_header_add(list, key, value, overwrite);
}

int hhttpp_request_header_del(struct hhttpp *hh, const char *key)
{
    struct hhttpp_header_list *list;

    if (!hh || !(list = hh->request_header))
        return -1;

    return hhttpp_header_del(list, key);
}

enum hhttpp_current_status hhttpp_connect(struct hhttpp *hh)
{
    int sd, flags;
    struct addrinfo hints, *res0, *res;
    char port[32];

    hh->current_status = hhttpp_current_status_error;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = PF_UNSPEC;
    hints.ai_socktype= SOCK_STREAM;
    if (hh->url->port)
        snprintf(port, sizeof(port), "%d", hh->url->port);
    else
        strcpy(port, "http");
    if (getaddrinfo(hh->url->host, port, &hints, &res0))
        return hh->current_status;

    sd = -1;
    for (res = res0; res; res = res->ai_next) {
        if ((sd = socket(res->ai_family, res->ai_socktype, res->ai_protocol)) < 0)
            continue;
        if ((flags = fcntl(sd, F_GETFL)) < 0 ||
            fcntl(sd, F_SETFL, flags|O_NONBLOCK) < 0)
        {
            goto bad;
        }

        if (connect(sd, res->ai_addr, res->ai_addrlen) < 0) {
            if (errno != EINPROGRESS) {
              bad:
                close(sd);
                sd = -1;
                continue;
            }
            hh->current_status = hhttpp_current_status_connecting;
        }
        hh->fd = sd;
        break;
    }
    freeaddrinfo(res0);

    return hh->current_status;
}

static int add_default_headers(struct hhttpp *hh)
{
    int r;
    char *host;
    struct hhttpp_header_list *list;
    struct hhttpp_url *url;

    list = hh->request_header;
    if (hhttpp_header_find(list, "Host") == NULL) {
        url = hh->url;
        if (url->port) {
            if (asprintf(&host, "%s:%d", url->host, url->port) < 0)
                return -1;
        } else {
            if (asprintf(&host, "%s", url->host) < 0)
                return -1;
        }
        r = hhttpp_header_add(list, "Host", host, 0);
        free(host);
        if (r < 0)
            return -1;
    }

    if (hhttpp_header_add(list, "User-Agent", DEFAULT_USER_AGENT, 0) < 0)
        return -1;

    return 0;
}

static int write_header(int sd, char *key, char *value)
{
    struct iovec iov[4];

    iov[0].iov_base = key;
    iov[0].iov_len = strlen(key);
    iov[1].iov_base = ": ";
    iov[1].iov_len = 2;
    iov[2].iov_base = value;
    iov[2].iov_len = strlen(value);
    iov[3].iov_base = "\r\n";
    iov[3].iov_len = 2;

    return writev(sd, iov, 4);
}

enum hhttpp_current_status hhttpp_request(struct hhttpp *hh)
{
    int sd, flags;
    char *path;
    struct iovec iov[3];
    socklen_t sslen;
    struct sockaddr_storage ss;
    struct hhttpp_header_list *list;
    struct hhttpp_header *h;

    sd = hh->fd;
    sslen =sizeof(ss);
    if (getpeername(sd, (struct sockaddr *)&ss, &sslen) < 0 ||
        (flags = fcntl(sd, F_GETFL)) < 0 ||
        fcntl(sd, F_SETFL, flags & ~O_NONBLOCK) < 0)
    {
        return hh->current_status = hhttpp_current_status_error;
    }

    path = hh->url->path;
    iov[0].iov_base = "GET /";
    iov[0].iov_len = 5;
    iov[1].iov_base = path;
    iov[1].iov_len = strlen(path);
    iov[2].iov_base = " HTTP/1.0\r\n";
    iov[2].iov_len = 11;
    if (writev(sd, iov, 3) < 0)
        return hh->current_status = hhttpp_current_status_error;

    // Host, User-Agent
    if (add_default_headers(hh) < 0)
        return hh->current_status = hhttpp_current_status_error;

    // request header
    list = hh->request_header;
    for (h = hhttpp_header_first(list); h; h = hhttpp_header_next(h)) {
        if (write_header(sd, h->key, h->value) < 0)
            return hh->current_status = hhttpp_current_status_error;
    }


    if (write(sd, "\r\n", 2) != 2)
        return hh->current_status = hhttpp_current_status_error;
        
    return hh->current_status = hhttpp_current_status_header_reading;
}

static int get_kv(char *line, char **k, char **v)
{
    char *p;

    *k = *v = NULL;
    if ((p = strchr(line, ':')) == NULL)
        return -1;
    *p++ = '\0';
    while (*p && isascii(*p) && isspace(*p))
        p++;
    *k = line;
    *v = p;

    return 0;
}

static int parse_header(struct hhttpp *hh)
{
    int r;
    struct hhttpp_linebuffer *lbuf;
    char *p, *k, *v;

    if ((lbuf = hhttpp_linebuffer_alloc(hh->response_buffer)) == NULL)
        return -1;

    r = -1;

    // status code
    if ((p = hhttpp_linebuffer_getline(lbuf)) == NULL ||
        (p = strchr(p, ' ')) == NULL)
    {
        goto out;
    }
    p++;
    hh->status_code = strtol(p, &p, 10);
    if (!isascii(*p) || !isspace(*p))
        goto out;
        
    // headers
    while ((p = hhttpp_linebuffer_getline(lbuf)) != NULL) {
        if (*p == '\0')
            break;
        if (get_kv(p, &k, &v) < 0) {
            err(1, "get_kv");
            goto out;
        }
        if (hhttpp_header_add(hh->response_header, k, v, 1) < 0) {
            err(1, "hhttpp_header_add");
            goto out;
        }
    }

    r = 0;

  out:
    hhttpp_linebuffer_free(lbuf);

    return r;
}

static enum hhttpp_current_status do_header(struct hhttpp *hh)
{
    int sd;
    ssize_t n;
    void *body;
    struct hhttpp_buffer *buf;

    sd = hh->fd;
    buf = hh->response_buffer;
    if ((n = hhttpp_buffer_read(sd, buf)) < 0)
        return hh->current_status = hhttpp_current_status_error;
    else if (n == 0) {
        if (parse_header(hh) < 0)
            return hh->current_status = hhttpp_current_status_error;
        hhttpp_buffer_flush(buf);
        return hh->current_status = hhttpp_current_status_done;
    }

    if ((body = hhttpp_buffer_find(buf, "\r\n\r\n")) == NULL)
        return hh->current_status;
    body += 4;

    // header done
    if (parse_header(hh) < 0)
        return hh->current_status = hhttpp_current_status_error;
    if (hhttpp_buffer_move(buf, body) < 0)
        return hh->current_status = hhttpp_current_status_error;

    return hh->current_status = hhttpp_current_status_body_reading;
}

static enum hhttpp_current_status do_body(struct hhttpp *hh)
{
    int sd;
    struct hhttpp_buffer *buf;

    sd = hh->fd;
    buf = hh->response_buffer;
    switch (hhttpp_buffer_read(sd, buf)) {
      case -1:
        return hh->current_status = hhttpp_current_status_error;
      case 0:
        return hh->current_status = hhttpp_current_status_done;
    }

    return hh->current_status;
}

enum hhttpp_current_status hhttpp_process(struct hhttpp *hh)
{
    if (!hh)
        return hhttpp_current_status_error;

    switch (hh->current_status) {
      case hhttpp_current_status_header_reading:
        return do_header(hh);
      case hhttpp_current_status_body_reading:
        return do_body(hh);
      case hhttpp_current_status_done:
        return hhttpp_current_status_done;
      default:
        break;
    }

    return hhttpp_current_status_error;
}

enum hhttpp_current_status hhttpp_close(struct hhttpp *hh)
{
    if (!hh || hh->fd < 0)
        return hhttpp_current_status_error;

    close(hh->fd);
    hh->fd = -1;

    return hh->current_status = hhttpp_current_status_done;
}

int hhttpp_status_code_get(const struct hhttpp *hh)
{
    return hh->status_code;
}

int hhttpp_fd_get(const struct hhttpp *hh)
{
    if (!hh)
        return -1;

    return hh->fd;
}

enum hhttpp_current_status hhttpp_current_status_get(const struct hhttpp *hh)
{
    if (!hh)
        return -1;

    return hh->current_status;
}

ssize_t hhttpp_content_length_get(struct hhttpp *hh)
{
    char *p;
    struct hhttpp_header_list *list;
    struct hhttpp_header *h;

    if (!hh)
        return -1;

    if (hh->content_length != -1)
        return hh->content_length;

    list = hh->response_header;
    if ((h = hhttpp_header_find(list, "Content-Length")) == NULL)
        return -1;

    hh->content_length = strtol(h->value, &p, 10);
    if (*p)
        return hh->content_length = -1;

    return hh->content_length;
}

const char *hhttpp_content_type_get(struct hhttpp *hh)
{
    char *p;
    struct hhttpp_header_list *list;
    struct hhttpp_header *h;

    if (!hh)
        return NULL;

    if (hh->content_type)
        return hh->content_type;

    list = hh->response_header;
    if ((h = hhttpp_header_find(list, "Content-Type")) == NULL)
        return NULL;
    if ((hh->content_type = strdup(h->value)) != NULL) {
        if ((p = strchr(hh->content_type, ';')) != NULL)
            *p = '\0';
    }

    return hh->content_type;
}

char *hhttpp_body_get(const struct hhttpp *hh)
{
    if (!hh)
        return NULL;

    switch (hh->current_status) {
      case hhttpp_current_status_none:
      case hhttpp_current_status_error:
      case hhttpp_current_status_connecting:
      case hhttpp_current_status_header_reading:
        return NULL;
      default:
        break;
    }

    return hhttpp_buffer_begin(hh->response_buffer);
}

ssize_t hhttpp_body_length_get(const struct hhttpp *hh)
{
    if (!hh)
        return -1;

    switch (hh->current_status) {
      case hhttpp_current_status_none:
      case hhttpp_current_status_error:
      case hhttpp_current_status_connecting:
      case hhttpp_current_status_header_reading:
        return -1;
      default:
        break;
    }

    return hh->response_buffer->nread;
}

const char *hhttpp_response_header_get(const struct hhttpp *hh, const char *key)
{
    struct hhttpp_header_list *list;
    struct hhttpp_header *h;

    if (!hh || !key)
        return NULL;

    list = hh->response_header;
    if ((h = hhttpp_header_find(list, key)) == NULL)
        return NULL;

    return h->value;
}

void hhttpp_response_headers_free(char **headers)
{
    size_t i;

    if (!headers)
        return;

    for (i = 0; headers[i]; i++)
        free(headers[i]);

    free(headers);
}

ssize_t hhttpp_response_headers_get(const struct hhttpp *hh, char ***headers)
{
    size_t n, i, len;
    char **k;
    struct hhttpp_header_list *list;
    struct hhttpp_header *h;

    if (!hh || !headers)
        return -1;

    *headers = NULL;
    list = hh->response_header;
    n = 0;
    for (h = hhttpp_header_first(list); h; h = hhttpp_header_next(h))
        n++;

    if ((k = calloc(sizeof(*k), n + 1)) == NULL)  // NULL terminate
        return -1;

    for (h = hhttpp_header_first(list), i = 0; h; h = hhttpp_header_next(h), i++) {
        len = strlen(h->key) + 2 + strlen(h->value) + 1;        // 2: ": ", 1: "\0"
        if ((k[i] = malloc(len)) == NULL)
            goto bad;
        strcpy(k[i], h->key);
        strcat(k[i], ": ");
        strcat(k[i], h->value);
    }

    *headers = k;

    return n;

  bad:
    hhttpp_response_headers_free(k);

    return -1;
}
