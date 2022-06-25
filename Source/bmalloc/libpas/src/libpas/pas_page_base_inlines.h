/*
 * Copyright (c) 2020 Apple Inc. All rights reserved.
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

#ifndef PAS_PAGE_BASE_INLINES_H
#define PAS_PAGE_BASE_INLINES_H

#include "pas_log.h"
#include "pas_page_base.h"

PAS_BEGIN_EXTERN_C;

typedef struct {
    bool did_find_empty_granule;
} pas_page_base_free_granule_uses_in_range_data;

static PAS_ALWAYS_INLINE void pas_page_base_free_granule_uses_in_range_action(
    pas_page_granule_use_count* use_count_ptr,
    void* arg)
{
    static const bool verbose = false;
    
    pas_page_base_free_granule_uses_in_range_data* data;
    pas_page_granule_use_count use_count;

    data = (pas_page_base_free_granule_uses_in_range_data*)arg;

    if (verbose)
        pas_log("Decrementing use count at %p\n", use_count_ptr);
    
    use_count = *use_count_ptr;
    
    /* I'm assuming that we do have available cycles for asserts here. */
    PAS_ASSERT(use_count);
    PAS_ASSERT(use_count != PAS_PAGE_GRANULE_DECOMMITTED);
    
    use_count--;
    
    *use_count_ptr = use_count;
    
    if (!use_count)
        data->did_find_empty_granule = true;
}

/* Returns true if we found an empty granule. */
static PAS_ALWAYS_INLINE bool pas_page_base_free_granule_uses_in_range(
    pas_page_granule_use_count* use_count_ptr,
    uintptr_t begin_offset,
    uintptr_t end_offset,
    pas_page_base_config page_config)
{
    pas_page_base_free_granule_uses_in_range_data free_uses_in_range_data;
    free_uses_in_range_data.did_find_empty_granule = false;
    
    pas_page_granule_for_each_use_in_range(
        use_count_ptr,
        begin_offset,
        end_offset,
        page_config.page_size,
        page_config.granule_size,
        pas_page_base_free_granule_uses_in_range_action,
        &free_uses_in_range_data);
    
    return free_uses_in_range_data.did_find_empty_granule;
}

PAS_END_EXTERN_C;

#endif /* PAS_PAGE_BASE_INLINES_H */

