/*
 * Copyright (c) 2019-2020 Apple Inc. All rights reserved.
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

#ifndef PAS_HEAP_SUMMARY_H
#define PAS_HEAP_SUMMARY_H

#include "pas_utils.h"

PAS_BEGIN_EXTERN_C;

struct pas_heap_summary;
struct pas_stream;
typedef struct pas_heap_summary pas_heap_summary;
typedef struct pas_stream pas_stream;

struct pas_heap_summary {
    size_t free;
    size_t allocated;
    size_t meta;
    size_t committed;
    size_t decommitted;
    size_t free_ineligible_for_decommit;
    size_t free_eligible_for_decommit;
    size_t free_decommitted;
    size_t meta_ineligible_for_decommit;
    size_t meta_eligible_for_decommit;
    size_t cached;
};

static inline pas_heap_summary pas_heap_summary_create_empty(void)
{
    pas_heap_summary result;
    result.free = 0;
    result.allocated = 0;
    result.meta = 0;
    result.committed = 0;
    result.decommitted = 0;
    result.free_ineligible_for_decommit = 0;
    result.free_eligible_for_decommit = 0;
    result.free_decommitted = 0;
    result.meta_ineligible_for_decommit = 0;
    result.meta_eligible_for_decommit = 0;
    result.cached = 0;
    return result;
}

static inline bool pas_heap_summary_is_empty(pas_heap_summary summary)
{
    return !summary.free
        && !summary.allocated
        && !summary.meta
        && !summary.committed
        && !summary.decommitted
        && !summary.free_ineligible_for_decommit
        && !summary.free_eligible_for_decommit
        && !summary.free_decommitted
        && !summary.meta_ineligible_for_decommit
        && !summary.meta_eligible_for_decommit
        && !summary.cached;
}

static inline pas_heap_summary pas_heap_summary_add(pas_heap_summary left,
                                                    pas_heap_summary right)
{
    pas_heap_summary result;
    result.free = left.free + right.free;
    result.allocated = left.allocated + right.allocated;
    result.meta = left.meta + right.meta;
    result.committed = left.committed + right.committed;
    result.decommitted = left.decommitted + right.decommitted;
    result.free_ineligible_for_decommit =
        left.free_ineligible_for_decommit + right.free_ineligible_for_decommit;
    result.free_eligible_for_decommit =
        left.free_eligible_for_decommit + right.free_eligible_for_decommit;
    result.free_decommitted = left.free_decommitted + right.free_decommitted;
    result.meta_ineligible_for_decommit =
        left.meta_ineligible_for_decommit + right.meta_ineligible_for_decommit;
    result.meta_eligible_for_decommit =
        left.meta_eligible_for_decommit + right.meta_eligible_for_decommit;
    result.cached = left.cached + right.cached;
    return result;
}

PAS_API void pas_heap_summary_validate(pas_heap_summary* summary);

static inline size_t pas_heap_summary_committed_objects(pas_heap_summary summary)
{
    return summary.allocated
        + summary.free_ineligible_for_decommit
        + summary.free_eligible_for_decommit;
}

static inline size_t pas_heap_summary_total(pas_heap_summary summary)
{
    return summary.committed + summary.decommitted;
}

static inline size_t pas_heap_summary_fragmentation(pas_heap_summary summary)
{
    return summary.free_ineligible_for_decommit + summary.meta_ineligible_for_decommit;
}

PAS_API void pas_heap_summary_dump(pas_heap_summary summary, pas_stream* stream);

PAS_END_EXTERN_C;

#endif /* PAS_HEAP_SUMMARY_H */

