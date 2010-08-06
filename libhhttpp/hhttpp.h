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

#ifndef _HHTTPP_H_
#define _HHTTPP_H_

#include <sys/types.h>

struct hhttpp;

enum hhttpp_current_status {
    hhttpp_current_status_error = -1,
    hhttpp_current_status_none = 0,
    hhttpp_current_status_connecting,
    hhttpp_current_status_header_reading,
    hhttpp_current_status_body_reading,
    hhttpp_current_status_done
};


void hhttpp_free(struct hhttpp *hh);
struct hhttpp *hhttpp_alloc();
int hhttpp_url_set(struct hhttpp *hh, const char *url);

int hhttpp_request_header_add(struct hhttpp *hh, const char *key, const char *value, int overwrite);
int hhttpp_request_header_del(struct hhttpp *hh, const char *key);

enum hhttpp_current_status hhttpp_connect(struct hhttpp *hh);
enum hhttpp_current_status hhttpp_request(struct hhttpp *hh);
enum hhttpp_current_status hhttpp_process(struct hhttpp *hh);
enum hhttpp_current_status hhttpp_close(struct hhttpp *hh);

int hhttpp_status_code_get(const struct hhttpp *hh);
int hhttpp_fd_get(const struct hhttpp *hh);
enum hhttpp_current_status hhttpp_current_status_get(const struct hhttpp *hh);
ssize_t hhttpp_content_length_get(struct hhttpp *hh);
const char *hhttpp_content_type_get(struct hhttpp *hh);
char *hhttpp_body_get(const struct hhttpp *hh);
ssize_t hhttpp_body_length_get(const struct hhttpp *hh);
const char *hhttpp_response_header_get(const struct hhttpp *hh, const char *key);
void hhttpp_response_headers_free(char **headers);
ssize_t hhttpp_response_headers_get(const struct hhttpp *hh, char ***headers);

#endif // _HHTTPP_H_
