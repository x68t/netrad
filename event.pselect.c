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
#include <sys/select.h>
#include <sys/signal.h>
#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include "event.h"
#include "cdata.h"
#include "logger.h"

static int maxfd;
static fd_set ofdr, ofdw;
static struct cdata_head fd_head;
static struct cdata_head sigaction_head;
static struct cdata_head sigcleanup_head;
static sigset_t sig_omask, sig_mask;
static sigset_t sig_received;

void event_close()
{
    cdata_head_free(&fd_head);
    cdata_head_free(&sigaction_head);
    cdata_head_free(&sigcleanup_head);
    sigprocmask(SIG_SETMASK, &sig_omask, NULL);
}

int event_open()
{
    maxfd = -1;
    FD_ZERO(&ofdr);
    ofdw = ofdr;

    if (cdata_head_alloc(&fd_head, 0) < 0)
        goto bad;
    if (cdata_head_alloc(&sigaction_head, 0) < 0)
        goto bad;
    if (cdata_head_alloc(&sigcleanup_head, 0) < 0)
        goto bad;

    if (sigprocmask(SIG_SETMASK, NULL, &sig_omask) < 0)
        goto bad;

    return 0;

  bad:
    event_close();

    return -1;
}

static int add(struct cdata_head *head, int ident, void *handler, void *ctx,
               int (*hook)(struct cdata *))
{
    struct cdata *cd;

    if ((cd = cdata_add(head, ident, handler, ctx)) == NULL)
        return -1;
    if (hook)
        return (*hook)(cd);

    return 0;
}

static int del(struct cdata_head *head, int ident, void *handler, void *ctx,
               int (*hook)(struct cdata *),
               cdata_matcher_t matcher)
{
    int r;
    struct cdata *cd;
    const struct cdata rcd = {
        .ident = ident,
        .handler = handler,
        .ctx = ctx
    };

    if (ident < 0)
        return -1;

    if ((cd = cdata_find(head, &rcd, matcher)) == NULL)
        return -1;

    r = 0;
    if (hook)
        r = (*hook)(cd);
    cdata_del(cd);

    return r;
}

static int fdread_add_hook(struct cdata *cd)
{
    if (maxfd < cd->ident)
        maxfd = cd->ident;
    FD_SET(cd->ident, &ofdr);

    return 0;
}

int event_fdread_add(int fd, event_handler_t handler, void *ctx)
{
    return add(&fd_head, fd, handler, ctx, fdread_add_hook);
}

static int fdwrite_add_hook(struct cdata *cd)
{
    if (maxfd < cd->ident)
        maxfd = cd->ident;
    FD_SET(cd->ident, &ofdw);

    return 0;
}

int event_fdwrite_add(int fd, event_handler_t handler, void *ctx)
{
    return add(&fd_head, fd, handler, ctx, fdwrite_add_hook);
}

static int fd_del_hook(struct cdata *cd)
{
    int i, m;

    i = cd->ident;
    FD_CLR(i, &ofdr);
    FD_CLR(i, &ofdw);

    if (i != maxfd)
        return 0;

    m = -1;
    for (i = maxfd - 1; i > -1; i--) {
        if (FD_ISSET(i, &ofdr) || FD_ISSET(i, &ofdw)) {
            m = i;
            break;
        }
    }
    maxfd = m;

    return 0;
}

int event_fd_del(int fd)
{
    return del(&fd_head, fd, NULL, NULL, fd_del_hook, cdata_match_by_ident);
}

static void sigwrapper(int signo, siginfo_t *info, void *uctx)
{
    ssize_t i;
    const struct cdata *cd;
    event_sigaction_handler_t action;

    sigaddset(&sig_received, signo);
    for (i = 0; i < sigaction_head.nuse; i++) {
        cd = &sigaction_head.cdata[i];
        if (!cdata_is_used(cd))
            continue;
        if (!sigismember(&sig_mask, signo))
            continue;
        if ((action = cd->handler) != NULL)
            (*action)(signo, info, uctx);
    }
}

int event_sigaction_add(int signo, event_sigaction_handler_t action, void *cookie)
{
    struct sigaction act = {
        .sa_flags = SA_SIGINFO,
        .sa_sigaction = sigwrapper
    };

    if (cdata_add(&sigaction_head, signo, action, cookie) == NULL)
        return -1;

    if (sigismember(&sig_mask, signo))
        return 0;

    sigaddset(&sig_mask, signo);
    sigemptyset(&act.sa_mask);

    return sigaction(signo, &act, NULL);
}

int event_sigaction_del(int signo, event_sigaction_handler_t action, void *cookie)
{
    return del(&sigaction_head, signo, action, cookie, NULL, cdata_match_by_all);
}

int event_sigcleanup_add(int signo, event_handler_t handler, void *ctx)
{
    return add(&sigcleanup_head, signo, handler, ctx, NULL);
}

int event_sigcleanup_del(int signo, event_handler_t handler, void *ctx)
{
    return del(&sigcleanup_head, signo, handler, ctx, NULL, cdata_match_by_all);
}

static void run_sigcleanups()
{
    ssize_t i;
    event_handler_t handler;
    struct cdata *cd;

    for (i = 0; i < sigcleanup_head.nuse; i++) {
        cd = &sigcleanup_head.cdata[i];
        if (!cdata_is_used(cd))
            continue;
        if (!sigismember(&sig_received, cd->ident))
            continue;
        if ((handler = cd->handler) != NULL)
            (*handler)(cd->ident, cd->ctx);
    }
}

static void run_handler(const fd_set *fds, int fd)
{
    struct cdata *cd;
    event_handler_t handler;
    const struct cdata rcd = {
        .ident = fd,
    };

    if (!FD_ISSET(fd, fds))
        return;
    if ((cd = cdata_find(&fd_head, &rcd, cdata_match_by_ident)) == NULL)
        return;

    handler = cd->handler;
    (*handler)(fd, cd->ctx);
}

void event_ploop()
{
    int n, i;
    fd_set fdr, fdw;

    if (maxfd == -1)
        return;

    for (;;) {
        sigemptyset(&sig_received);
        fdr = ofdr;
        fdw = ofdw;
        n = pselect(maxfd + 1, &fdr, &fdw, NULL, NULL, &sig_omask);
        run_sigcleanups();
        switch (n) {
          case -1:
            if (errno == EINTR)
                break;
            /* FALLTHRU */
          case 0:
            return;
          default:
            for (i = 0; i <= maxfd; i++) {
                run_handler(&fdr, i);
                run_handler(&fdw, i);
            }
        }
    }
}
