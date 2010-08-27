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

#include <signal.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <pwd.h>
#include <grp.h>
#include <stdio.h>
#include <unistd.h>
#include "event.h"
#include "audio.h"
#include "client.h"
#include "logger.h"

void usage()
{
    fprintf(stderr, "usage: netrad [-a [node][:service]][-d] [-u user] [-g group] [-c dir] [-l log-level] [-p pid-file]\n");
    exit(1);
}

int sigint(int signo, void *ctx)
{
    audio_stop(NULL, NULL);
    kill(0, signo);
    exit(0);

    /* NOTREACHED */
    return 0;
}

void sigusr(int signo, siginfo_t *info, void *uctx)
{
    int i;

    i = logger_get_level();
    if (signo == SIGUSR1)
        i++;
    else if (signo == SIGUSR2)
        i--;

    if (i < LOG_ERR)
        i = LOG_ERR;
    else if (LOG_DEBUG < i)
        i = LOG_DEBUG;

    logger_set_level(i);
}

int daemonize(const char *user, const char *group, const char *pid_file, const char *cwd)
{
    uid_t uid;
    gid_t gid;
    struct passwd *pwd, pwdbuf;
    struct group *grp, grpbuf;
    char *p;
    FILE *fp;
    char buf[2048];

    uid = 0;
    gid = 0;

    if (user) {
        if (getpwnam_r(user, &pwdbuf, buf, sizeof(buf), &pwd) == 0)
            uid = pwd->pw_uid;
        else {
            if (!isdigit(*user)) {
              baduser:
                logger(LOG_ERR, "getpwnam_r: %s: %m", user);
                return -1;
            }
            uid = strtol(user, &p, 10);
            if (*p)
                goto baduser;
        }
    }

    if (group) {
        if (getgrnam_r(group, &grpbuf, buf, sizeof(buf), &grp) == 0)
            gid = grp->gr_gid;
        else {
            if (!isdigit(*group)) {
              badgroup:
                logger(LOG_ERR, "getgrnam_r: %s: %m", group);
                return -1;
            }
            gid = strtol(group, &p, 10);
            if (*p)
                goto badgroup;
        }
    }

    if (daemon(0, 0) < 0) {
        logger(LOG_ERR, "daemon: %m");
        return -1;
    }

    if (pid_file) {
        if ((fp = fopen(pid_file, "w")) == NULL) {
            logger(LOG_ERR, "fopen: %s: %m", pid_file);
            return -1;
        }
        fprintf(fp, "%d\n", getpid());
        fclose(fp);
    }

    if (cwd) {
        if (chdir(cwd) < 0) {
            logger(LOG_ERR, "chdir: %s", cwd);
            return -1;
        }
    }

    if (gid > 0) {
        if (setgid(gid) < 0) {
            logger(LOG_ERR, "setgid: %d: %m", gid);
            return -1;
        }
    }

    if (uid > 0) {
        if (setuid(uid) < 0) {
            logger(LOG_ERR, "setuid: %d: %m", uid);
            return -1;
        }
    }

    return 0;
}

int main(int argc, char *argv[])
{
    int c;
    int debug;
    int client_init_done;
    char *node, *service;
    const char *user, *group, *pid_file, *cwd;

    logger_init("netrad", LOG_ERR);
    logger(LOG_ERR, "start");

    if (event_open() < 0)
        return 1;
    if (signal(SIGPIPE, SIG_IGN) == SIG_ERR)
        return 1;
    if (audio_init() < 0)
        return 1;

    debug = 0;
    client_init_done = 0;
    user = group = pid_file = cwd = NULL;
    while ((c = getopt(argc, argv, "a:du:g:l:p:c:D:")) != -1) {
        switch (c) {
          case 'a':
            node = optarg;
            if ((service = strchr(node, ':')) != NULL)
                *service++ = '\0';
            if (client_init(node, service) < 0)
                return 1;
            client_init_done = 1;
            break;
          case 'd':
            debug = 1;
            break;
          case 'u':
            user = optarg;
            break;
          case 'g':
            group = optarg;
            break;
          case 'l':
            if (logger_set_level_by_string(optarg))
                usage();
            break;
          case 'p':
            pid_file = optarg;
            break;
          case 'c':
            cwd = optarg;
            break;
          case 'D':
            audio_device_name = optarg;
            break;
          default:
            usage();
            /* NOTREACHED */
        }
    }

    if (event_sigaction_add(SIGHUP, NULL, NULL) < 0 ||
        event_sigaction_add(SIGINT, NULL, NULL) < 0 ||
        event_sigaction_add(SIGTERM, NULL, NULL) < 0 ||
        event_sigaction_add(SIGUSR1, sigusr, NULL) < 0 ||
        event_sigaction_add(SIGUSR2, sigusr, NULL) < 0)
    {
        logger(LOG_ERR, "event_sigaction_add: %m");
        return 1;
    }

    if (event_sigcleanup_add(SIGHUP, sigint, NULL) < 0 ||
        event_sigcleanup_add(SIGINT, sigint, NULL) < 0 ||
        event_sigcleanup_add(SIGTERM, sigint, NULL) < 0)
    {
        logger(LOG_ERR, "event_sigcleanup_add: %m");
        return 1;
    }

    if (!client_init_done) {
        if (client_init(NULL, NULL) < 0)
            return 1;
    }

    if (!debug) {
        if (daemonize(user, group, pid_file, cwd) < 0)
            return 1;
    }

    if (!debug && getuid() == 0) {
        logger(LOG_ERR, "dangerous!: uid = 0");
        return 1;
    }

    event_ploop();
    event_close();

    return 0;
}
