/*
 * Copyright (c) 2018-2020 Apple Inc. All rights reserved.
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

#ifndef PAS_STRING_STREAM_H
#define PAS_STRING_STREAM_H

#include "pas_allocation_config.h"
#include "pas_stream.h"
#include "pas_utils.h"

PAS_BEGIN_EXTERN_C;

/* pas_stream is a low-level string print stream that uses printf-like APIs like FILE. */

struct pas_string_stream;
typedef struct pas_string_stream pas_string_stream;

struct pas_string_stream {
    pas_stream base;
    pas_allocation_config allocation_config;
    char* buffer;
    size_t next;
    size_t size;
    char inline_buffer[128];
};

PAS_API void pas_string_stream_construct(pas_string_stream* stream,
                                         const pas_allocation_config* allocation_config);
PAS_API void pas_string_stream_destruct(pas_string_stream* stream);
PAS_API void pas_string_stream_reset(pas_string_stream* stream);

PAS_API void pas_string_stream_vprintf(pas_string_stream* stream, const char* format, va_list) PAS_FORMAT_PRINTF(2, 0);

#define pas_string_stream_printf(stream, ...) \
    pas_stream_printf((pas_stream*)(stream), __VA_ARGS__)

/* Returns a string that is live so long as you don't destruct the stream. */
static inline const char* pas_string_stream_get_string(pas_string_stream* stream)
{
    return stream->buffer;
}

static inline size_t pas_string_stream_get_string_length(pas_string_stream* stream)
{
    return stream->next;
}

PAS_END_EXTERN_C;

#endif /* PAS_STRING_STREAM_H */

