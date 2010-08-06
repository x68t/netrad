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
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/uio.h>
#include <signal.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include "audio.h"
#include "event.h"
#include "logger.h"
#include "libhhttpp/hhttpp.h"

static pid_t pid_player = -1;
static int fd_player = -1;  // metadata from player process
static char **headers = NULL;
static char *icy_meta = NULL;

struct audio_ctx {
    int fd;
    struct hhttpp *req;
    const char *player;
};

static void sigchld(int signo, siginfo_t *info, void *ctx)
{
    int e, s;
    pid_t pid, r;

    if ((pid = pid_player) < 0 || info->si_pid != pid)
        return;

    e = errno;
    r = waitpid(pid, &s, WNOHANG);
    errno = e;
    if (r != pid)
        return;

    if (WIFEXITED(s) || WIFSIGNALED(s))
        pid_player = -1;
}

int audio_init()
{
    pid_player = -1;
    fd_player = -1;
    headers = NULL;
    icy_meta = NULL;

    if (event_sigaction_add(SIGCHLD, sigchld, NULL) < 0)
        return -1;

    return 0;
}

static int get_headers(struct hhttpp *req)
{
    hhttpp_response_headers_free(headers);

    if (hhttpp_response_headers_get(req, &headers) < 0)
        return -1;

    return 0;
}

static int is_running()
{
    if (pid_player > 0)
        return 1;
    return 0;
}

static int stop(int signo, void *ctx)
{
    if (is_running())
        return -1;

    event_sigcleanup_del(signo, stop, ctx);

    hhttpp_response_headers_free(headers);
    headers = NULL;
    if (icy_meta) {
        free(icy_meta);
        icy_meta = NULL;
    }

    return 0;
}

int audio_stop(event_handler_t cleanup, void *ctx)
{
    pid_t pid;

    if (!is_running())
        return 0;

    if (cleanup)
        event_sigcleanup_add(SIGCHLD, cleanup, ctx);
    else
        event_sigcleanup_add(SIGCHLD, stop, NULL);

    pid = pid_player;
    logger(LOG_INFO, "audio_stop: kill(%d)", pid);

    return kill(pid, SIGHUP);
}

static int metadata_receive(int fd, void *ctx)
{
    ssize_t n;
    char buf[4096];

    if (icy_meta) {
        free(icy_meta);
        icy_meta = NULL;
    }

    if ((n = read(fd, buf , sizeof(buf)-1)) <= 0) {
        event_fd_del(fd);
        close(fd);

        return fd_player = -1;
    }
    buf[n] = '\0';

    if (*buf == '\0')
        return -1;

    if ((icy_meta = strdup(buf)) == NULL)
        return -1;
    logger(LOG_INFO, "metadata_receive: `%s'", icy_meta);

    return 0;
}

static int start(int signo, void *ctx)
{
    int r;
    pid_t pid;
    int fds[2];
    struct audio_ctx *ac;
    const char *metaint;
    const char *argv[6];

    event_sigcleanup_del(signo, start, ctx);
    r = -1;
    ac = ctx;
    if (is_running())
        goto out;

    memset(argv, 0, sizeof(argv));
    argv[0] = ac->player;
    argv[1] = "-l";
    if ((argv[2] = logger_get_level_by_string()) == NULL)
        goto out;
    if ((metaint = hhttpp_response_header_get(ac->req, "icy-metaint")) != NULL) {
        argv[3] = "-m";
        argv[4] = metaint;
    }

    get_headers(ac->req);
    if (pipe(fds) < 0)
        goto out;

    switch (pid = fork()) {
      case -1:
        goto out;
      case 0:
        dup2(ac->fd, 0);
        if (ac->fd != 0)
            close(ac->fd);
        close(fds[0]);
        dup2(fds[1], 1);
        if (fds[1] != 1)
            close(fds[1]);
        execv(argv[0], (char *const *)argv);
        logger(LOG_ERR, "%s: exec: %m", argv[0]);
        _exit(0);
        /* NOTREACHED */
        break;
      default:
        pid_player = pid;
        fd_player = fds[0];
        event_fdread_add(fd_player, metadata_receive, NULL);
        close(fds[1]);
        logger(LOG_INFO, "pid_player: %d", pid);
        break;
    }

    r = 0;

  out:
    hhttpp_free(ac->req);
    free(ac);

    return r;
}

int audio_start(int fd, struct hhttpp *req, const char *player)
{
    struct audio_ctx *ac;

    if ((ac = malloc(sizeof(*ac))) == NULL)
        return -1;

    ac->fd = fd;
    ac->req = req;
    ac->player = player;

    if (is_running())
        return audio_stop(start, ac);

    return start(-1, ac);
}

int audio_status(int fd)
{
    int i;
    struct iovec iov[3];

    if (!is_running())
        return -1;

    iov[1].iov_base = "\n";
    iov[1].iov_len = 1;
    for (i = 0; headers[i]; i++) {
        iov[0].iov_base = headers[i];
        iov[0].iov_len = strlen(headers[i]);
        if (writev(fd, iov, 2) < 0)
            return -1;
    }

    if (icy_meta) {
        iov[0].iov_base = "icy-meta: ";
        iov[0].iov_len = 10;
        iov[1].iov_base = icy_meta;
        iov[1].iov_len = strlen(icy_meta);
        iov[2].iov_base = "\n";
        iov[2].iov_len = 1;
        if (writev(fd, iov, 3) < 0)
            return -1;
    }

    return 0;
}
