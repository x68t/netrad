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

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include "event.h"
#include "cmd.h"
#include "client.h"
#include "logger.h"

#ifndef DEFAULT_NODE
#define DEFAULT_NODE "localhost"
#endif // DEFAULT_NODE

#ifndef DEFAULT_SERVICE
#define DEFAULT_SERVICE "9649"
#endif // DEFAULT_SERVICE

static int swrite(int fd, const char *s)
{
    size_t len;

    len = strlen(s);

    return write(fd, s, len) == len? 0: -1;
}

static int client_read(int fd, void *ctx)
{
    int i, r;
    ssize_t n;
    char *p, *arg;
    char buf[4096];
    const struct {
        const char *name;
        event_handler_t fn;
    } cmds[] = {
        { "TUNE", cmd_tune },
        { "STOP", cmd_stop },
        { "CLOSE", cmd_close },
        { "STATUS", cmd_status },
        { NULL, NULL },
    };

    if ((n = read(fd, buf, sizeof(buf)-1)) <= 0)
        return cmd_close(fd, NULL);
    buf[n] = '\0';

    if ((p = strchr(buf, '\r')) != NULL)
        *p = '\0';
    if ((p = strchr(buf, '\n')) != NULL)
        *p = '\0';

    if ((arg = strchr(buf, ' ')) != NULL)
        *arg++ = '\0';

    for (i = 0; cmds[i].name; i++) {
        if (strcasecmp(cmds[i].name, buf))
            continue;
        r = (cmds[i].fn)(fd, arg);
        if (r == 0)
            swrite(fd, "+OK\n");
        else
            swrite(fd, "-ERR\n");

        return r;
    }
    swrite(fd, "-ERR\n");
    logger(LOG_WARNING, "client_read: unknown command `%s'", buf);

    return -1;
}

static int client_accept(int sd, void *ctx)
{
    int fd, e;
    socklen_t sslen;
    struct sockaddr_storage ss;
    char host[64], port[32];

    sslen = sizeof(ss);
    if ((fd = accept(sd, (struct sockaddr *)&ss, &sslen)) < 0) {
        logger(LOG_DEBUG, "client_accept: %m");
        switch (errno) {
          case EWOULDBLOCK:
          case ECONNABORTED:
          case EPROTO:
          case EINTR:
            return 0;
        }
        return -1;
    }

    if (logger_get_level() >= LOG_INFO) {
        if ((e = getnameinfo((struct sockaddr *)&ss, sslen, host, sizeof(host), port, sizeof(port), NI_NUMERICHOST)))
            logger(LOG_WARNING, "getnameinfo: %s", gai_strerror(e));
        else
            logger(LOG_INFO, "client_accept: fd=%d: `%s:%s'", fd, host, port);
    }

    return event_fdread_add(fd, client_read, NULL);
}

int client_init(const char *node, const char *service)
{
    int sd, e, r;
    int flags;
    const int on = 1;
    struct addrinfo hints, *res0, *res;
    char host[64], port[64];

    if (!node || !*node)
        node = DEFAULT_NODE;
    if (!service || !*service)
        service = DEFAULT_SERVICE;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = PF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    if ((e = getaddrinfo(node, service, &hints, &res0)) != 0) {
        logger(LOG_ERR, "getaddrinfo: %s", gai_strerror(e));
        return -1;
    }

    r = -1;
    for (res = res0; res; res = res->ai_next) {
        if ((sd = socket(res->ai_family, res->ai_socktype, res->ai_protocol)) < 0)
            continue;
        setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
        if (bind(sd, res->ai_addr, res->ai_addrlen) < 0) {
            if (errno == EADDRINUSE) {
                if (setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) < 0)
                    goto bad;
                if (bind(sd, res->ai_addr, res->ai_addrlen) < 0) {
                    logger(LOG_WARNING, "bind: %m");
                    goto bad;
                }
            }
          bad:
            close(sd);
            continue;
        }

        if (listen(sd, SOMAXCONN) < 0 ||
            (flags = fcntl(sd, F_GETFL)) < 0 ||
            fcntl(sd, F_SETFL, flags|O_NONBLOCK) < 0)
        {
            goto bad;
        }

        if (event_fdread_add(sd, client_accept, NULL) < 0)
            goto bad;

        if ((e = getnameinfo(res->ai_addr, res->ai_addrlen, host, sizeof(host), port, sizeof(port), NI_NUMERICHOST|NI_NUMERICSERV)) == 0)
            logger(LOG_DEBUG, "listening on %s:%s", host, port);
        else
            logger(LOG_WARNING, "getnameinfo: %s", gai_strerror(e));

        r = 0;
    }
    freeaddrinfo(res0);

    return r;
}
