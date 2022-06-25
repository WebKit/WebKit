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

#ifndef PAS_ALIGNMENT_H
#define PAS_ALIGNMENT_H

#include "pas_utils.h"

PAS_BEGIN_EXTERN_C;

struct pas_alignment;
struct pas_stream;
typedef struct pas_alignment pas_alignment;
typedef struct pas_stream pas_stream;

struct pas_alignment {
    size_t alignment;
    uintptr_t alignment_begin;
};

static inline pas_alignment pas_alignment_create(size_t alignment, uintptr_t alignment_begin)
{
    PAS_ASSERT(pas_is_power_of_2(alignment));
    PAS_ASSERT(alignment_begin < alignment);
    
    pas_alignment result;
    result.alignment = alignment;
    result.alignment_begin = alignment_begin;
    return result;
}

static inline pas_alignment pas_alignment_create_traditional(size_t alignment)
{
    return pas_alignment_create(alignment, 0);
}

static inline pas_alignment pas_alignment_create_trivial(void)
{
    return pas_alignment_create(1, 0);
}

static inline void pas_alignment_validate(pas_alignment alignment)
{
    PAS_ASSERT(pas_is_power_of_2(alignment.alignment));
    PAS_ASSERT(alignment.alignment_begin < alignment.alignment);
}

static inline bool pas_alignment_is_ptr_aligned(pas_alignment alignment,
                                                uintptr_t ptr)
{
    return pas_is_aligned(ptr - alignment.alignment_begin, alignment.alignment);
}

static inline pas_alignment pas_alignment_round_up(pas_alignment alignment,
                                                   uintptr_t possibly_bigger_alignment)
{
    pas_alignment result;
    pas_alignment_validate(alignment);
    PAS_ASSERT(pas_is_power_of_2(possibly_bigger_alignment));
    
    /* This creates a new alignment that is either:
       
       - Exactly the same as the old one, or
       
       - Has a bigger alignment but the same offset. That's trivially correct, since that is
         the exact first-fit solution to the question of how to allocate with the input alignment
         constraint from an allocator that has the bigger alignment capability. */
    result = pas_alignment_create(
        PAS_MAX(alignment.alignment, possibly_bigger_alignment),
        alignment.alignment_begin);
    
    pas_alignment_validate(result);
    return result;
}

static inline bool pas_alignment_is_equal(pas_alignment left, pas_alignment right)
{
    return left.alignment == right.alignment
        && left.alignment_begin == right.alignment_begin;
}

PAS_API void pas_alignment_dump(pas_alignment alignment, pas_stream* stream);

PAS_END_EXTERN_C;

#endif /* PAS_ALIGNMENT_H */
