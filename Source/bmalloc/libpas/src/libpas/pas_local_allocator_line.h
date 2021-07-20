/*
 * Copyright (c) 2021 Apple Inc. All rights reserved.
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

#ifndef PAS_LOCAL_ALLOCATOR_LINE_H
#define PAS_LOCAL_ALLOCATOR_LINE_H

#include "pas_segregated_page_config.h"

PAS_BEGIN_EXTERN_C;

struct pas_local_allocator_line;
typedef struct pas_local_allocator_line pas_local_allocator_line;

struct pas_local_allocator_line {
    uint16_t encoded_remaining;
    uint16_t encoded_payload_end_delta;
};

static PAS_ALWAYS_INLINE bool pas_local_allocator_line_fits_without_shift(
    pas_segregated_page_config page_config)
{
    size_t max_page_offset;
    size_t shifted;

    max_page_offset = page_config.base.page_size - 1;

    if ((uint16_t)max_page_offset == max_page_offset)
        return true;

    shifted = max_page_offset >> page_config.base.min_align_shift;

    PAS_ASSERT((uint16_t)shifted == shifted);
    return false;
}

static PAS_ALWAYS_INLINE size_t pas_local_allocator_line_decode(uint16_t value,
                                                                pas_segregated_page_config page_config)
{
    if (pas_local_allocator_line_fits_without_shift(page_config))
        return value;
    return (size_t)value << page_config.base.min_align_shift;
}

static PAS_ALWAYS_INLINE uint16_t pas_local_allocator_line_encode(size_t value,
                                                                  pas_segregated_page_config page_config)
{
    uint16_t encoded;
    
    if (pas_local_allocator_line_fits_without_shift(page_config))
        encoded = (uint16_t)value;
    else
        encoded = (uint16_t)(value >> page_config.base.min_align_shift);

    PAS_TESTING_ASSERT(pas_local_allocator_line_decode(encoded, page_config) == value);

    return encoded;
}

static PAS_ALWAYS_INLINE size_t pas_local_allocator_line_remaining(
    pas_local_allocator_line line,
    pas_segregated_page_config page_config)
{
    return pas_local_allocator_line_decode(line.encoded_remaining, page_config);
}

static PAS_ALWAYS_INLINE void pas_local_allocator_line_set_remaining(
    pas_local_allocator_line* line,
    size_t remaining,
    pas_segregated_page_config page_config)
{
    line->encoded_remaining = pas_local_allocator_line_encode(remaining, page_config);
}

static PAS_ALWAYS_INLINE size_t pas_local_allocator_line_payload_end_delta(
    pas_local_allocator_line line,
    pas_segregated_page_config page_config)
{
    return pas_local_allocator_line_decode(line.encoded_payload_end_delta, page_config);
}

static PAS_ALWAYS_INLINE void pas_local_allocator_line_set_payload_end_delta(
    pas_local_allocator_line* line,
    size_t payload_end_delta,
    pas_segregated_page_config page_config)
{
    line->encoded_payload_end_delta = pas_local_allocator_line_encode(payload_end_delta, page_config);
}

PAS_END_EXTERN_C;

#endif /* PAS_LOCAL_ALLOCATOR_LINE_H */

