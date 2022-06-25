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

#ifndef PAS_LARGE_VIRTUAL_RANGE_H
#define PAS_LARGE_VIRTUAL_RANGE_H

#include "pas_lock.h"
#include "pas_mmap_capability.h"
#include "pas_range.h"
#include "pas_utils.h"

PAS_BEGIN_EXTERN_C;

struct pas_large_virtual_range;
typedef struct pas_large_virtual_range pas_large_virtual_range;

struct pas_large_virtual_range {
    uintptr_t begin;
    uintptr_t end;
    pas_mmap_capability mmap_capability;
};

static inline pas_large_virtual_range pas_large_virtual_range_create(uintptr_t begin,
                                                                     uintptr_t end,
                                                                     pas_mmap_capability mmap_capability)
{
    pas_large_virtual_range result;
    result.begin = begin;
    result.end = end;
    result.mmap_capability = mmap_capability;
    return result;
}

static inline pas_large_virtual_range pas_large_virtual_range_create_empty(void)
{
    return pas_large_virtual_range_create(0, 0, pas_may_not_mmap);
}

static inline pas_range pas_large_virtual_range_get_range(pas_large_virtual_range range)
{
    return pas_range_create(range.begin, range.end);
}

static inline bool pas_large_virtual_range_is_empty(pas_large_virtual_range range)
{
    return pas_range_is_empty(pas_large_virtual_range_get_range(range));
}

static inline size_t pas_large_virtual_range_size(pas_large_virtual_range range)
{
    return pas_range_size(pas_large_virtual_range_get_range(range));
}

static inline bool pas_large_virtual_range_overlaps(pas_large_virtual_range left,
                                                    pas_large_virtual_range right)
{
    return pas_range_overlaps(pas_large_virtual_range_get_range(left),
                              pas_large_virtual_range_get_range(right));
}

PAS_END_EXTERN_C;

#endif /* PAS_LARGE_VIRTUAL_RANGE_H */

