/*
 * Copyright (c) 2019-2021 Apple Inc. All rights reserved.
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

#ifndef PAS_RANGE_H
#define PAS_RANGE_H

#include "pas_utils.h"

PAS_BEGIN_EXTERN_C;

struct pas_range;
typedef struct pas_range pas_range;

struct pas_range {
    uintptr_t begin;
    uintptr_t end;
};

static inline pas_range pas_range_create(uintptr_t begin, uintptr_t end)
{
    pas_range result;
    PAS_ASSERT(end >= begin);
    result.begin = begin;
    result.end = end;
    return result;
}

static inline pas_range pas_range_create_empty(void)
{
    return pas_range_create(0, 0);
}

static inline pas_range pas_range_create_forgiving(uintptr_t begin, uintptr_t end)
{
    if (end < begin)
        return pas_range_create_empty();
    return pas_range_create(begin, end);
}

static inline bool pas_range_is_empty(pas_range range)
{
    return range.begin == range.end;
}

static inline size_t pas_range_size(pas_range range)
{
    PAS_ASSERT(range.end >= range.begin);
    return range.end - range.begin;
}

static inline bool pas_range_contains(pas_range left, uintptr_t right)
{
    return right >= left.begin && right < left.end;
}

static inline bool pas_range_subsumes(pas_range left, pas_range right)
{
    if (pas_range_is_empty(right))
        return true;
    return right.begin >= left.begin && right.end <= left.end;
}

static inline bool pas_range_overlaps(pas_range left, pas_range right)
{
    return pas_ranges_overlap(left.begin, left.end,
                              right.begin, right.end);
}

static inline pas_range pas_range_create_intersection(pas_range left, pas_range right)
{
    if (!pas_range_overlaps(left, right))
        return pas_range_create_empty();
    
    return pas_range_create(PAS_MAX(left.begin, right.begin),
                            PAS_MIN(left.end, right.end));
}

static inline int pas_range_compare(pas_range left, pas_range right)
{
    if (pas_range_overlaps(left, right))
        return 0;
    
    if (left.begin < right.begin)
        return -1;
    return 1;
}

PAS_END_EXTERN_C;

#endif /* PAS_RANGE_H */

