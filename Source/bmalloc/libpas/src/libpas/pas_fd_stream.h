/*
 * Copyright (c) 2019 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#ifndef PAS_FD_STREAM_H
#define PAS_FD_STREAM_H

#include "pas_stream.h"

PAS_BEGIN_EXTERN_C;

struct pas_fd_stream;
typedef struct pas_fd_stream pas_fd_stream;

struct pas_fd_stream {
    pas_stream base;
    int fd;
};

PAS_API extern pas_stream_functions pas_fd_stream_functions;

#define PAS_FD_STREAM_INITIALIZER(passed_fd) { \
        .base = PAS_STREAM_INITIALIZER(&pas_fd_stream_functions), \
        .fd = passed_fd \
    }

PAS_API extern pas_fd_stream pas_log_stream;

PAS_API void pas_fd_stream_construct(pas_fd_stream* stream, int fd);

PAS_API void pas_fd_stream_vprintf(pas_fd_stream* stream, const char* format, va_list arg_list);

#define pas_fd_stream_printf(stream, ...) \
    pas_stream_printf((pas_stream*)(stream), __VA_ARGS__)

PAS_END_EXTERN_C;

#endif /* PAS_FD_STREAM_H */

