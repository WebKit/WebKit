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

#ifndef PAS_LARGE_VIRTUAL_RANGE_MIN_HEAP_H
#define PAS_LARGE_VIRTUAL_RANGE_MIN_HEAP_H

#include "pas_min_heap.h"
#include "pas_utils.h"
#include "pas_large_virtual_range.h"

PAS_BEGIN_EXTERN_C;

static inline int pas_large_virtual_range_compare_begin(pas_large_virtual_range* left,
                                                        pas_large_virtual_range* right)
{
    if (left->begin < right->begin)
        return -1;
    if (left->begin == right->begin)
        return 0;
    return 1;
}

static inline size_t pas_large_virtual_range_get_index(pas_large_virtual_range* range)
{
    PAS_ASSERT(!"Should not be reached");
    PAS_UNUSED_PARAM(range);
    return 0;
}

static inline void pas_large_virtual_range_set_index(pas_large_virtual_range* range, size_t index)
{
    PAS_UNUSED_PARAM(range);
    PAS_UNUSED_PARAM(index);
}

PAS_CREATE_MIN_HEAP(
    pas_large_virtual_range_min_heap,
    pas_large_virtual_range,
    32,
    .compare = pas_large_virtual_range_compare_begin,
    .get_index = pas_large_virtual_range_get_index,
    .set_index = pas_large_virtual_range_set_index);

PAS_END_EXTERN_C;

#endif /* PAS_LARGE_VIRTUAL_RANGE_MIN_HEAP_H */

