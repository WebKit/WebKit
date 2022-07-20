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

#include "pas_page_base.h"

#include "pas_bitfit_page.h"
#include "pas_segregated_page.h"

size_t pas_page_base_header_size(const pas_page_base_config* config,
                                 pas_page_kind page_kind)
{
    switch (config->page_config_kind) {
    case pas_page_config_kind_segregated:
        PAS_ASSERT(pas_page_kind_get_config_kind(page_kind) == pas_page_config_kind_segregated);
        return pas_segregated_page_header_size(
            *pas_page_base_config_get_segregated(config),
            pas_page_kind_get_segregated_role(page_kind));
    case pas_page_config_kind_bitfit:
        PAS_ASSERT(pas_page_kind_get_config_kind(page_kind) == pas_page_config_kind_bitfit);
        return pas_bitfit_page_header_size(*pas_page_base_config_get_bitfit(config));
    }
    PAS_ASSERT(!"Should not be reached");
    return 0;
}

const pas_page_base_config* pas_page_base_get_config(pas_page_base* page)
{
    switch (pas_page_base_get_config_kind(page)) {
    case pas_page_config_kind_segregated:
        return &pas_segregated_page_get_config(pas_page_base_get_segregated(page))->base;
    case pas_page_config_kind_bitfit:
        return &pas_bitfit_page_get_config(pas_page_base_get_bitfit(page))->base;
    }
    PAS_ASSERT(!"Should not be reached");
    return NULL;
}

pas_page_granule_use_count*
pas_page_base_get_granule_use_counts(pas_page_base* page)
{
    switch (pas_page_base_get_config_kind(page)) {
    case pas_page_config_kind_segregated: {
        pas_segregated_page* segregated_page;
        segregated_page = pas_page_base_get_segregated(page);
        return pas_segregated_page_get_granule_use_counts(
            segregated_page, *pas_segregated_page_get_config(segregated_page));
    }
    case pas_page_config_kind_bitfit: {
        pas_bitfit_page* bitfit_page;
        bitfit_page = pas_page_base_get_bitfit(page);
        return pas_bitfit_page_get_granule_use_counts(
            bitfit_page, *pas_bitfit_page_get_config(bitfit_page));
    } }
    PAS_ASSERT(!"Should not be reached");
    return NULL;
}

void pas_page_base_compute_committed_when_owned(pas_page_base* page,
                                                pas_heap_summary* summary)
{
    pas_page_granule_use_count* use_counts;
    uintptr_t num_granules;
    uintptr_t granule_index;
    const pas_page_base_config* config_ptr;
    pas_page_base_config config;

    config_ptr = pas_page_base_get_config(page);
    config = *config_ptr;
    
    if (config.page_size == config.granule_size) {
        summary->committed += config.page_size;
        return;
    }
    
    use_counts = pas_page_base_get_granule_use_counts(page);
    num_granules = config.page_size / config.granule_size;
    
    for (granule_index = num_granules; granule_index--;) {
        if (use_counts[granule_index] == PAS_PAGE_GRANULE_DECOMMITTED)
            summary->decommitted += config.granule_size;
        else
            summary->committed += config.granule_size;
    }
}

bool pas_page_base_is_empty(pas_page_base* page)
{
    switch (pas_page_base_get_config_kind(page)) {
    case pas_page_config_kind_segregated:
        return !pas_page_base_get_segregated(page)->emptiness.num_non_empty_words;
    case pas_page_config_kind_bitfit:
        return !pas_page_base_get_bitfit(page)->num_live_bits;
    }
    PAS_ASSERT(!"Should not be reached");
    return false;
}

void pas_page_base_add_free_range(pas_page_base* page,
                                  pas_heap_summary* result,
                                  pas_range range,
                                  pas_free_range_kind kind)
{
    const pas_page_base_config* page_config_ptr;
    pas_page_base_config page_config;
    size_t* ineligible_for_decommit;
    size_t* eligible_for_decommit;
    size_t* decommitted;
    size_t dummy;
    bool empty;
    pas_page_granule_use_count* use_counts;
    uintptr_t first_granule_index;
    uintptr_t last_granule_index;
    uintptr_t granule_index;

    if (pas_range_is_empty(range))
        return;

    PAS_ASSERT(range.end > range.begin);

    page_config_ptr = pas_page_base_get_config(page);
    page_config = *page_config_ptr;

    PAS_ASSERT(range.end <= page_config.page_size);

    empty = pas_page_base_is_empty(page);

    dummy = 0; /* Tell the compiler to chill out and relax. */

    switch (kind) {
    case pas_free_object_range:
        result->free += pas_range_size(range);
        
        ineligible_for_decommit = &result->free_ineligible_for_decommit;
        eligible_for_decommit = &result->free_eligible_for_decommit;
        decommitted = &result->free_decommitted;
        break;
    case pas_free_meta_range:
        result->meta += pas_range_size(range);
        
        ineligible_for_decommit = &result->meta_ineligible_for_decommit;
        eligible_for_decommit = &result->meta_eligible_for_decommit;
        decommitted = &dummy;
        break;
    }
    
    if (page_config.page_size == page_config.granule_size) {
        if (empty)
            (*eligible_for_decommit) += pas_range_size(range);
        else
            (*ineligible_for_decommit) += pas_range_size(range);
        return;
    }
    
    use_counts = pas_page_base_get_granule_use_counts(page);

    first_granule_index = range.begin / page_config.granule_size;
    last_granule_index = (range.end - 1) / page_config.granule_size;

    for (granule_index = first_granule_index;
         granule_index <= last_granule_index;
         ++granule_index) {
        pas_range granule_range;
        size_t overlap_size;
        
        granule_range = pas_range_create(granule_index * page_config.granule_size,
                                         (granule_index + 1) * page_config.granule_size);

        PAS_ASSERT(pas_range_overlaps(range, granule_range));

        overlap_size = pas_range_size(pas_range_create_intersection(range,
                                                                    granule_range));

        switch (use_counts[granule_index]) {
        case 0:
            *eligible_for_decommit += overlap_size;
            break;
        case PAS_PAGE_GRANULE_DECOMMITTED:
            *decommitted += overlap_size;
            break;
        default:
            *ineligible_for_decommit += overlap_size;
            break;
        }
    }
}

#endif /* LIBPAS_ENABLED */
