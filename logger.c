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

#include <string.h>
#include <syslog.h>
#include <stdarg.h>
#include <stdio.h>
#include "logger.h"

static int loglevel = LOG_ERR;

int logger_get_level()
{
    return loglevel;
}

const char *logger_get_level_by_string()
{
    static const char *s[] = {
        "LOG_EMERG",
        "LOG_ALERT",
        "LOG_CRIT",
        "LOG_ERR",
        "LOG_WARNING",
        "LOG_NOTICE",
        "LOG_INFO",
        "LOG_DEBUG",
    };

    if (loglevel < 0 || loglevel >= sizeof(s)/sizeof(*s)) {
        logger(LOG_ERR, "logger_get_level_by_string: unknown loglevel: %d", loglevel);
        return NULL;
    }
        
    return s[loglevel];
}

void logger_set_level(int level)
{
    if (level < LOG_EMERG)
        loglevel = LOG_EMERG;
    else if (level > LOG_DEBUG)
        loglevel = LOG_DEBUG;
    else
        loglevel = level;
    //logger(LOG_DEBUG, "logger_set_level: %d", level);
}

int logger_set_level_by_string(const char *level)
{
    int i;
    struct {
        const char *name;
        int level;
    } levels[] = {
        { "LOG_EMERG", LOG_EMERG },
        { "LOG_EMERGENCY", LOG_EMERG },
        { "EMERG", LOG_EMERG },
        { "EMERGENCY", LOG_EMERG },
        { "LOG_ALERT", LOG_ALERT },
        { "ALERT", LOG_ALERT },
        { "LOG_ERROR", LOG_ERR },
        { "LOG_ERR", LOG_ERR },
        { "ERROR", LOG_ERR },
        { "ERR", LOG_ERR },
        { "LOG_WARNING", LOG_WARNING },
        { "LOG_WARN", LOG_WARNING },
        { "WARNING", LOG_WARNING },
        { "WARN", LOG_WARNING },
        { "LOG_NOTICE", LOG_NOTICE },
        { "NOTICE", LOG_NOTICE },
        { "LOG_INFO", LOG_INFO },
        { "INFO", LOG_INFO },
        { "LOG_DEBUG", LOG_DEBUG },
        { "DEBUG", LOG_DEBUG },
        { NULL, -1 },
    };

    for (i = 0; levels[i].name; i++) {
        if (strcasecmp(level, levels[i].name))
            continue;
        logger_set_level(levels[i].level);
        return 0;
    }
    logger(LOG_WARNING, "logger_set_level_string: unknown level `%s'", level);

    return -1;
}

void logger_init(const char *ident, int level)
{
    if (ident == NULL)
        return;

    openlog(ident, LOG_PID, LOG_DAEMON);
    logger_set_level(level);
}

void logger(int pri, const char *fmt, ...)
{
    va_list ap;

    if (pri > loglevel)
        return;

    va_start(ap, fmt);
    vsyslog(pri, fmt, ap);
    va_end(ap);
}
