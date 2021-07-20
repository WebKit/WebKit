/*
 * Copyright (c) 2018-2021 Apple Inc. All rights reserved.
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

#ifndef PAS_STREAM_H
#define PAS_STREAM_H

#include "pas_utils.h"
#include <stdarg.h>

PAS_BEGIN_EXTERN_C;

/* pas_stream is a low-level string print stream that uses printf-like APIs like FILE. */

struct pas_stream;
struct pas_stream_functions;
typedef struct pas_stream pas_stream;
typedef struct pas_stream_functions pas_stream_functions;

struct pas_stream {
    const pas_stream_functions* functions;
};

struct pas_stream_functions {
    void (*vprintf)(pas_stream* stream, const char* format, va_list arg_list);
};

#define PAS_STREAM_INITIALIZER(passed_functions) { .functions = (passed_functions) }

PAS_API void pas_stream_vprintf(pas_stream* stream, const char* format, va_list);
PAS_API void pas_stream_printf(pas_stream* stream, const char* format, ...);

static inline void pas_stream_print_comma(pas_stream* stream, bool* comma, const char* string)
{
    if (!*comma)
        *comma = true;
    else
        pas_stream_printf(stream, "%s", string);
}

PAS_END_EXTERN_C;

#endif /* PAS_STREAM_H */

