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

#include "pas_config.h"

#if LIBPAS_ENABLED

#include "pas_string_stream.h"

#include "pas_bootstrap_free_heap.h"
#include "pas_snprintf.h"

static PAS_FORMAT_PRINTF(2, 0) void string_stream_vprintf(pas_stream* stream, const char* format, va_list arg_list)
{
    pas_string_stream_vprintf((pas_string_stream*)stream, format, arg_list);
}

static const pas_stream_functions string_stream_functions = {
    .vprintf = string_stream_vprintf
};

void pas_string_stream_construct(pas_string_stream* stream,
                                 const pas_allocation_config* allocation_config)
{
    pas_zero_memory(stream, sizeof(pas_string_stream));
    stream->allocation_config = *allocation_config;
    stream->base.functions = &string_stream_functions;
    stream->buffer = stream->inline_buffer;
    stream->size = sizeof(stream->inline_buffer);
}

void pas_string_stream_destruct(pas_string_stream* stream)
{
    if (stream->buffer != stream->inline_buffer) {
        stream->allocation_config.deallocate(
            stream->buffer, stream->size, pas_object_allocation, stream->allocation_config.arg);
    }
}

void pas_string_stream_reset(pas_string_stream* stream)
{
    pas_allocation_config allocation_config;
    allocation_config = stream->allocation_config;
    pas_string_stream_destruct(stream);
    pas_string_stream_construct(stream, &allocation_config);
}

void pas_string_stream_vprintf(pas_string_stream* stream, const char* format, va_list arg_list)
{
    va_list first_pass_arg_list;
    int number_of_bytes_not_including_terminator_that_would_have_been_written;
    int number_of_bytes_that_would_have_been_written;
    int number_of_bytes_not_including_terminator_that_were_written;
    int number_of_bytes_that_were_written;
    char* new_buffer;
    size_t new_size;
    
    PAS_ASSERT(stream->next < stream->size);
    PAS_ASSERT(!stream->buffer[stream->next]);
    
    va_copy(first_pass_arg_list, arg_list);
    
    number_of_bytes_not_including_terminator_that_would_have_been_written =
        pas_vsnprintf(stream->buffer + stream->next, stream->size - stream->next,
                      format, first_pass_arg_list);

    PAS_ASSERT(number_of_bytes_not_including_terminator_that_would_have_been_written >= 0);
    
    va_end(first_pass_arg_list);
    
    number_of_bytes_that_would_have_been_written =
        number_of_bytes_not_including_terminator_that_would_have_been_written + 1;

    PAS_ASSERT(number_of_bytes_that_would_have_been_written >= 0);
    
    if (stream->next + (size_t)number_of_bytes_that_would_have_been_written <= stream->size) {
        stream->next += (size_t)number_of_bytes_not_including_terminator_that_would_have_been_written;
        return;
    }
    
    new_size = (stream->next + (size_t)number_of_bytes_that_would_have_been_written) << 1;
    
    new_buffer = stream->allocation_config.allocate(
        new_size, "pas_stream/buffer", pas_object_allocation, stream->allocation_config.arg);
    memcpy(new_buffer, stream->buffer, stream->next + 1);
    if (stream->buffer != stream->inline_buffer) {
        stream->allocation_config.deallocate(
            stream->buffer, stream->size, pas_object_allocation, stream->allocation_config.arg);
    }
    stream->buffer = new_buffer;
    stream->size = new_size;
    
    number_of_bytes_not_including_terminator_that_were_written =
        pas_vsnprintf(stream->buffer + stream->next, stream->size - stream->next,
                      format, arg_list);

    PAS_ASSERT(number_of_bytes_not_including_terminator_that_were_written >= 0);
    
    number_of_bytes_that_were_written =
        number_of_bytes_not_including_terminator_that_were_written + 1;

    PAS_ASSERT(number_of_bytes_that_were_written >= 0);
    
    PAS_ASSERT(stream->next + (size_t)number_of_bytes_that_were_written <= stream->size);
    
    stream->next += (size_t)number_of_bytes_not_including_terminator_that_were_written;
    
    PAS_ASSERT(stream->next < stream->size);
    PAS_ASSERT(!stream->buffer[stream->next]);
}

#endif /* LIBPAS_ENABLED */
