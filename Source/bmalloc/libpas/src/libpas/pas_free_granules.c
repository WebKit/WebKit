/*
 * Copyright (c) 2020-2021 Apple Inc. All rights reserved.
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

#include "pas_free_granules.h"

#include "pas_commit_span.h"
#include "pas_heap_config.h"
#include "pas_log.h"
#include "pas_page_base_config.h"

void pas_free_granules_compute_and_mark_decommitted(pas_free_granules* free_granules,
                                                    pas_page_granule_use_count* use_counts,
                                                    size_t num_granules)
{
    static const bool verbose = false;
    
    size_t granule_index;
    uintptr_t granule_begin;
    
    PAS_ASSERT(num_granules >= 2); /* If there is only one granule then we don't have use counts. */
    PAS_ASSERT(num_granules <= PAS_MAX_GRANULES);
    
    granule_begin = (uintptr_t)free_granules;
    PAS_PROFILE(ZERO_FREE_GRANULES, granule_begin);
    free_granules = (pas_free_granules*)granule_begin;

    pas_zero_memory(free_granules, sizeof(pas_free_granules));

    for (granule_index = num_granules; granule_index--;) {
        if (use_counts[granule_index]) {
            if (use_counts[granule_index] == PAS_PAGE_GRANULE_DECOMMITTED)
                free_granules->num_already_decommitted_granules++;
        
            continue;
        }

        if (verbose)
            pas_log("Freeing granule with index = %zu\n", granule_index);
        
        pas_bitvector_set(free_granules->free_granules, granule_index, true);
        free_granules->num_free_granules++;
        
        /* Right now, the page is not eligible - so it's OK for us to do this. */
        use_counts[granule_index] = PAS_PAGE_GRANULE_DECOMMITTED;
    }
}

void pas_free_granules_unmark_decommitted(pas_free_granules* free_granules,
                                          pas_page_granule_use_count* use_counts,
                                          size_t num_granules)
{
    size_t num_free_granules;
    size_t granule_index;

    PAS_ASSERT(num_granules >= 2); /* If there is only one granule then we don't have use counts. */
    PAS_ASSERT(num_granules <= PAS_MAX_GRANULES);

    num_free_granules = 0;
    
    for (granule_index = num_granules; granule_index--;) {
        if (!pas_bitvector_get(free_granules->free_granules, granule_index))
            continue;
        
        PAS_ASSERT(use_counts[granule_index] == PAS_PAGE_GRANULE_DECOMMITTED);
        use_counts[granule_index] = 0;
        num_free_granules++;
    }

    PAS_ASSERT(num_free_granules == free_granules->num_free_granules);
}

void pas_free_granules_decommit_after_locking_range(pas_free_granules* free_granules,
                                                    pas_page_base* page,
                                                    pas_deferred_decommit_log* log,
                                                    pas_lock* commit_lock,
                                                    const pas_page_base_config* page_config,
                                                    pas_lock_hold_mode heap_lock_hold_mode)
{
    size_t granule_index;
    size_t num_granules;
    pas_commit_span commit_span;

    num_granules = page_config->page_size / page_config->granule_size;
    
    PAS_ASSERT(num_granules >= 2); /* If there is only one granule then we don't have use counts. */
    PAS_ASSERT(num_granules <= PAS_MAX_GRANULES);

    pas_commit_span_construct(&commit_span, page_config->heap_config_ptr->mmap_capability);

    for (granule_index = 0; granule_index < num_granules; ++granule_index) {
        if (pas_free_granules_is_free(free_granules, granule_index)) {
            pas_commit_span_add_to_change(&commit_span, granule_index);
            continue;
        }

        pas_commit_span_add_unchanged_and_decommit(
            &commit_span, page, granule_index, log, commit_lock, page_config, heap_lock_hold_mode);
    }
    
    pas_commit_span_add_unchanged_and_decommit(
        &commit_span, page, num_granules, log, commit_lock, page_config, heap_lock_hold_mode);
}

#endif /* LIBPAS_ENABLED */
