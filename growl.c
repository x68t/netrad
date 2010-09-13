/*-
 * Copyright (c) 2010
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
#include <netinet/in.h>
#include <netdb.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <err.h>
#include <sys/uio.h>
#include <openssl/md5.h>
#include "growl.h"

#define GROWL_SERVICE_TCP "23052"
#define GROWL_SERVICE_UDP "9887"

#define GROWL_PROTOCOL_VERSION 1
#define GROWL_PROTOCOL_VERSION_AES128 2

#define GROWL_TYPE_REGISTRATION 0
#define GROWL_TYPE_NOTIFICATION 1
#define GROWL_TYPE_REGISTRATION_SHA256 2
#define GROWL_TYPE_NOTIFICATION_SHA256 3
#define GROWL_TYPE_REGISTRATION_NOAUTH 4
#define GROWL_TYPE_NOTIFICATION_NOAUTH 5

#define APP_NAME "netrad"
#define NOTIFICATION_NAME "metadata received"

static int sd = -1;

struct growl_registration_packet {
    uint8_t ver;
    uint8_t type;
    uint16_t app_name_length;
    uint8_t nall;
    uint8_t ndef;
};

struct growl_notification_packet {
    uint8_t ver;
    uint8_t type;
    uint16_t flags;
    uint16_t notification_length;
    uint16_t title_length;
    uint16_t description_length;
    uint16_t app_name_length;
};

static int connect_to(const char *node, const char *service, int socktype)
{
    int fd;
    struct addrinfo hints, *res0, *res;

    if (!node)
        node = "localhost";

    if (!service) {
        switch (socktype) {
          case SOCK_STREAM:
            service = GROWL_SERVICE_TCP;
            break;
          case SOCK_DGRAM:
            service = GROWL_SERVICE_UDP;
            break;
          default:
            return -1;
        }
    }

    if (!*node || !*service)
        return -1;
    
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = PF_UNSPEC;
    hints.ai_socktype = socktype;
    if (getaddrinfo(node, service, &hints, &res0))
        return -1;
    
    fd = -1;
    for (res = res0; res; res = res->ai_next) {
        if ((fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol)) < 0)
            continue;
        if (connect(fd, res->ai_addr, res->ai_addrlen) < 0) {
            close(fd);
            fd = -1;
            continue;
        }
        break;
    }
    freeaddrinfo(res0);
    
    return fd;
}

static void add_packet(struct iovec *iov, const void *p, size_t size, MD5_CTX *ctx)
{
    iov->iov_base = (void *)p;
    iov->iov_len = size;
    if (ctx)
        MD5_Update(ctx, p, size);
}

static int registration(int sd)
{
    int n, i;
    size_t length;
    struct growl_registration_packet packet;
    MD5_CTX ctx;
    struct iovec iov[20];
    struct {
        const char *name;
        uint16_t length;
    } notifications[] = {
        { GROWL_NOTIFICATION_TUNE_IN, 0 },
        { GROWL_NOTIFICATION_METADATA_RECEIVED, 0 },
        { GROWL_NOTIFICATION_DISCONNECTED, 0 },
    };
    uint8_t ndef[sizeof(notifications)/sizeof(notifications[0])];
    unsigned char md5[MD5_DIGEST_LENGTH];

    packet.ver = GROWL_PROTOCOL_VERSION;
    packet.type = GROWL_TYPE_REGISTRATION;
    packet.app_name_length = htons(strlen(APP_NAME));
    packet.nall = sizeof(notifications)/sizeof(notifications[0]);
    packet.ndef = sizeof(notifications)/sizeof(notifications[0]);
    for (i = 0; i < sizeof(ndef)/sizeof(ndef[0]); i++)
        ndef[i] = i;

    n = 0;
    MD5_Init(&ctx);
    add_packet(&iov[n++], &packet, sizeof(packet), &ctx);
    add_packet(&iov[n++], APP_NAME, strlen(APP_NAME), &ctx);
    for (i = 0; i < sizeof(notifications)/sizeof(notifications[0]); i++) {
        length = strlen(notifications[i].name);
        notifications[i].length = htons(length);
        add_packet(&iov[n++], &notifications[i].length, sizeof(notifications[i].length), &ctx);
        add_packet(&iov[n++], notifications[i].name, length, &ctx);
    }
    add_packet(&iov[n++], ndef, sizeof(ndef), &ctx);
    MD5_Final(md5, &ctx);
    add_packet(&iov[n++], md5, sizeof(md5), NULL);

    return writev(sd, iov, n) < 0? -1: 0;
}

int growl_init(const char *node, const char *service, int socktype)
{
    if ((sd = connect_to(node, service, socktype)) < 0)
        return -1;
    if (registration(sd) < 0) {
        close(sd);
        sd = -1;
        return -1;
    }

    return 0;
}

int growl_notification(const char *notification_name, const char *title, const char *description, int priority, int sticky)
{
    int n;
    int16_t flags;
    struct growl_notification_packet packet;
    MD5_CTX ctx;
    struct iovec iov[10];
    uint8_t md5[MD5_DIGEST_LENGTH];

    if (sd < 0)
        return -1;

    flags = (priority & 7) * 2;
    if (priority < 0)
        flags |= 8;
    if (sticky)
        flags |= 0x100;

    packet.ver = GROWL_PROTOCOL_VERSION;
    packet.type = GROWL_TYPE_NOTIFICATION;
    packet.flags = htons(flags);
    packet.notification_length = htons(strlen(notification_name));
    packet.title_length = htons(strlen(title));
    packet.description_length = htons(strlen(description));
    packet.app_name_length = htons(strlen(APP_NAME));

    n = 0;
    MD5_Init(&ctx);
    add_packet(&iov[n++], &packet, sizeof(packet), &ctx);
    add_packet(&iov[n++], notification_name, strlen(notification_name), &ctx);
    add_packet(&iov[n++], title, strlen(title), &ctx);
    add_packet(&iov[n++], description, strlen(description), &ctx);
    add_packet(&iov[n++], APP_NAME, strlen(APP_NAME), &ctx);
    MD5_Final(md5, &ctx);
    add_packet(&iov[n++], md5, sizeof(md5), NULL);

    return writev(sd, iov, n) < 0? -1: 0;
}
