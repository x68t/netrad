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
    int8_t ver;
    int8_t type;
    int16_t flags;
    int16_t notification_length;
    int16_t title_length;
    int16_t description_length;
    int16_t app_name_length;
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
            service = "23052";
            break;
          case SOCK_DGRAM:
            service = "9887";
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

static int regist(int sd)
{
    int n;
    struct growl_registration_packet packet;
    int16_t notification_length;
    uint8_t md5[16], ndef[1];
    struct iovec iov[20];
    MD5_CTX ctx;

    MD5_Init(&ctx);

    n = 0;
    memset(&packet, 0, sizeof(packet));
    packet.ver = GROWL_PROTOCOL_VERSION;
    packet.type = GROWL_TYPE_REGISTRATION;
    packet.app_name_length = htons(strlen(APP_NAME));
    packet.nall = 1;
    packet.ndef = 1;
    add_packet(&iov[n++], &packet, sizeof(packet), &ctx);
    add_packet(&iov[n++], APP_NAME, strlen(APP_NAME), &ctx);

    notification_length = htons(strlen(NOTIFICATION_NAME));
    add_packet(&iov[n++], &notification_length, sizeof(notification_length), &ctx);
    add_packet(&iov[n++], NOTIFICATION_NAME, strlen(NOTIFICATION_NAME), &ctx);

    ndef[0] = 0;
    add_packet(&iov[n++], ndef, sizeof(ndef), &ctx);

    MD5_Final(md5, &ctx);
    add_packet(&iov[n++], md5, sizeof(md5), NULL);

    return writev(sd, iov, n) < 0? -1: 0;
}

int growl_init(const char *node, const char *service, int socktype)
{
    if ((sd = connect_to(node, service, socktype)) < 0)
        return -1;
    if (regist(sd) < 0) {
        close(sd);
        sd = -1;
        return -1;
    }

    return 0;
}

int growl_notification(const char *title, const char *description)
{
    struct growl_notification_packet packet;
    int n;
    struct iovec iov[10];
    MD5_CTX ctx;
    unsigned char md5[16];

    if (sd < 0)
        return -1;

    n = 0;
    MD5_Init(&ctx);
    memset(&packet, 0, sizeof(packet));
    packet.ver = GROWL_PROTOCOL_VERSION;
    packet.type = GROWL_TYPE_NOTIFICATION;
    packet.flags = 2;
    packet.notification_length = htons(strlen(NOTIFICATION_NAME));
    packet.title_length = htons(strlen(title));
    packet.description_length = htons(strlen(description));
    packet.app_name_length = htons(strlen(APP_NAME));
    add_packet(&iov[n++], &packet, sizeof(packet), &ctx);

    add_packet(&iov[n++], NOTIFICATION_NAME, strlen(NOTIFICATION_NAME), &ctx);
    add_packet(&iov[n++], title, strlen(title), &ctx);
    add_packet(&iov[n++], description, strlen(description), &ctx);
    add_packet(&iov[n++], APP_NAME, strlen(APP_NAME), &ctx);
    MD5_Final(md5, &ctx);
    add_packet(&iov[n++], md5, sizeof(md5), NULL);

    return writev(sd, iov, n) < 0? -1: 0;
}
