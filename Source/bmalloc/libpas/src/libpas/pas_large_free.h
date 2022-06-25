/*
 * Copyright (c) 2018-2019 Apple Inc. All rights reserved.
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

#ifndef PAS_LARGE_FREE_H
#define PAS_LARGE_FREE_H

#include "pas_alignment.h"
#include "pas_allocation_result.h"
#include "pas_large_free_heap_config.h"
#include "pas_utils.h"

PAS_BEGIN_EXTERN_C;

struct pas_large_allocation_result;
struct pas_large_free;
struct pas_large_split;
typedef struct pas_large_allocation_result pas_large_allocation_result;
typedef struct pas_large_free pas_large_free;
typedef struct pas_large_split pas_large_split;

struct pas_large_free {
    uintptr_t begin : sizeof(void*) == 8 ? 48 : 32;
    uintptr_t end : sizeof(void*) == 8 ? 48 : 32;

    /* This is the offset of `begin` inside the type of this heap. This arises because we sometimes
       have to allocate a type of size T with the overall allocation size aligned to granule size G.
       The granule is usually minalign, but could be the requested alignent. So given an array
       length L, the actual allocation size is:
       
       S = roundUpToMultipleOf(L * T, G)
       
       So, if we allocate 1 * T out of a free array of 2 * T, we create a smidgen of free memory on
       the right that straddles type boundaries. This tells us by how much. */
    uintptr_t offset_in_type : sizeof(void*) == 8 ? 48 : 32;
    pas_zero_mode zero_mode : 8;
};

struct pas_large_split {
    pas_large_free left;
    pas_large_free right;
};

struct pas_large_allocation_result {
    pas_large_free left_free;
    pas_large_free allocation;
    pas_large_free right_free;
};

static inline pas_large_free pas_large_free_create_empty(void)
{
    pas_large_free result;
    result.begin = 0;
    result.end = 0;
    result.offset_in_type = 0;
    result.zero_mode = pas_zero_mode_may_have_non_zero;
    return result;
}

static inline bool pas_large_free_is_empty(pas_large_free free)
{
    return free.begin == free.end;
}

static inline size_t pas_large_free_size(pas_large_free free)
{
    return free.end - free.begin;
}

static inline uintptr_t
pas_large_free_offset_in_type_at_end(pas_large_free free,
                                     pas_large_free_heap_config* config)
{
    PAS_ASSERT(free.offset_in_type < config->type_size);
    return (free.end - free.begin + free.offset_in_type) % config->type_size;
}

PAS_END_EXTERN_C;

#endif /* PAS_LARGE_FREE_H */

