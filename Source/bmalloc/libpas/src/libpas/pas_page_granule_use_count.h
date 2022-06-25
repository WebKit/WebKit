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

#ifndef PAS_PAGE_GRANULE_USE_COUNT_H
#define PAS_PAGE_GRANULE_USE_COUNT_H

#include "pas_utils.h"

PAS_BEGIN_EXTERN_C;

typedef uint8_t pas_page_granule_use_count;
#define PAS_PAGE_GRANULE_DECOMMITTED 255u /* Has to be the max value. */

static PAS_ALWAYS_INLINE void pas_page_granule_get_indices(
    uintptr_t begin,
    uintptr_t end,
    uintptr_t page_size,
    uintptr_t granule_size,
    uintptr_t* index_of_first_granule,
    uintptr_t* index_of_last_granule)
{
    *index_of_first_granule = begin / granule_size;
    *index_of_last_granule = (end - 1) / granule_size;

    PAS_ASSERT(*index_of_last_granule < page_size / granule_size);
}

static PAS_ALWAYS_INLINE void pas_page_granule_for_each_use_in_range(
    pas_page_granule_use_count* use_counts,
    uintptr_t begin,
    uintptr_t end,
    uintptr_t page_size,
    uintptr_t granule_size,
    void (*action)(pas_page_granule_use_count* use_count, void* arg),
    void* arg)
{
    uintptr_t index_of_first_granule;
    uintptr_t index_of_last_granule;
    uintptr_t granule_index;

    if (begin == end)
        return;

    pas_page_granule_get_indices(
        begin, end, page_size, granule_size, &index_of_first_granule, &index_of_last_granule);

    for (granule_index = index_of_first_granule;
         granule_index <= index_of_last_granule;
         ++granule_index)
        action(use_counts + granule_index, arg);
}

static PAS_ALWAYS_INLINE void pas_page_granule_use_count_increment(
    pas_page_granule_use_count* use_count_ptr,
    void* arg)
{
    pas_page_granule_use_count use_count;

    PAS_UNUSED_PARAM(arg);

    use_count = *use_count_ptr;

    PAS_ASSERT(use_count != PAS_PAGE_GRANULE_DECOMMITTED);

    use_count++;

    PAS_ASSERT(use_count != PAS_PAGE_GRANULE_DECOMMITTED);

    *use_count_ptr = use_count;
}

static PAS_ALWAYS_INLINE void pas_page_granule_increment_uses_for_range(
    pas_page_granule_use_count* use_counts,
    uintptr_t begin,
    uintptr_t end,
    uintptr_t page_size,
    uintptr_t granule_size)
{
    pas_page_granule_for_each_use_in_range(
        use_counts, begin, end, page_size, granule_size,
        pas_page_granule_use_count_increment, NULL);
}

PAS_END_EXTERN_C;

#endif /* PAS_PAGE_GRANULE_USE_COUNT_H */

