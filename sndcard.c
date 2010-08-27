/*-
 * Copyright (c) 2007-2010
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
#include <alsa/asoundlib.h>
#include <string.h>

#define strequal !strcmp
#define strnequal !strncmp

static int snd_card_first()
{
    int i;

    i = -1;
    if (snd_card_next(&i) < 0)
        return -1;

    return i;
}

static int find_card(const char *device_name)
{
    int i, r;
    char *name;

    for (i = snd_card_first(); i != -1; snd_card_next(&i)) {
        if (snd_card_get_name(i, &name) < 0)
            continue;
        r = strequal(name, device_name);
        free(name);
        if (r)
            return i;
    }

    return -1;
}

static int sndcard_open(const char *device_name, int mode, const char *template)
{
    int card;
    char dev[32];

    if (strnequal(device_name, "/dev/", 5))
        return open(device_name, mode);

    if ((card = find_card(device_name)) < 0)
        return -1;
    snprintf(dev, sizeof(dev), template, card);
    if (!card) {
        // "/dev/dsp0" -> "/dev/dsp"
        dev[strlen(dev)-1] = '\0';
    }

    return open(dev, mode);
}

int sndcard_dsp_open(const char *dsp_name, int mode)
{
    return sndcard_open(dsp_name, mode, "/dev/dsp%d");
}

int sndcard_mixer_open(const char *mixer_name, int mode)
{
    return sndcard_open(mixer_name, mode, "/dev/mixer%d");
}
