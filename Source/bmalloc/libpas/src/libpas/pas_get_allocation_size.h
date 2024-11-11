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

#ifndef PAS_GET_ALLOCATION_SIZE_H
#define PAS_GET_ALLOCATION_SIZE_H

#include "pas_get_page_base_and_kind_for_small_other_in_fast_megapage.h"
#include "pas_heap_config.h"
#include "pas_heap_lock.h"
#include "pas_large_map.h"
#include "pas_segregated_page_inlines.h"
#include "pas_segregated_size_directory.h"

PAS_BEGIN_EXTERN_C;

/* Returns zero if we don't own this object. */
static PAS_ALWAYS_INLINE size_t pas_get_allocation_size(void* ptr,
                                                        pas_heap_config config)
{
    uintptr_t begin;
    
    begin = (uintptr_t)ptr;

    switch (config.fast_megapage_kind_func(begin)) {
    case pas_small_exclusive_segregated_fast_megapage_kind:
        return pas_segregated_page_get_object_size_for_address_and_page_config(
            begin, config.small_segregated_config, pas_segregated_page_exclusive_role);
    case pas_small_other_fast_megapage_kind: {
        pas_page_base_and_kind page_and_kind;
        page_and_kind = pas_get_page_base_and_kind_for_small_other_in_fast_megapage(begin, config);
        switch (page_and_kind.page_kind) {
        case pas_small_shared_segregated_page_kind:
            return pas_segregated_page_get_object_size_for_address_in_page(
                pas_page_base_get_segregated(page_and_kind.page_base),
                begin,
                config.small_segregated_config,
                pas_segregated_page_shared_role);
        case pas_small_bitfit_page_kind:
            return config.small_bitfit_config.specialized_page_get_allocation_size_with_page(
                pas_page_base_get_bitfit(page_and_kind.page_base),
                begin);
        default:
            PAS_ASSERT(!"Should not be reached");
            return 0;
        }
    }
    case pas_not_a_fast_megapage_kind: {
        pas_page_base* page_base;
        pas_large_map_entry entry;
        size_t result;

        page_base = config.page_header_func(begin);
        if (page_base) {
            switch (pas_page_base_get_kind(page_base)) {
            case pas_small_shared_segregated_page_kind:
                PAS_ASSERT(!config.small_segregated_is_in_megapage);
                return pas_segregated_page_get_object_size_for_address_in_page(
                    pas_page_base_get_segregated(page_base),
                    begin,
                    config.small_segregated_config,
                    pas_segregated_page_shared_role);
            case pas_small_exclusive_segregated_page_kind:
                PAS_ASSERT(!config.small_segregated_is_in_megapage);
                return pas_segregated_page_get_object_size_for_address_in_page(
                    pas_page_base_get_segregated(page_base),
                    begin,
                    config.small_segregated_config,
                    pas_segregated_page_exclusive_role);
            case pas_small_bitfit_page_kind:
                PAS_ASSERT(!config.small_bitfit_is_in_megapage);
                return config.small_bitfit_config.specialized_page_get_allocation_size_with_page(
                    pas_page_base_get_bitfit(page_base),
                    begin);
            case pas_medium_shared_segregated_page_kind:
                return pas_segregated_page_get_object_size_for_address_in_page(
                    pas_page_base_get_segregated(page_base),
                    begin,
                    config.medium_segregated_config,
                    pas_segregated_page_shared_role);
            case pas_medium_exclusive_segregated_page_kind:
                return pas_segregated_page_get_object_size_for_address_in_page(
                    pas_page_base_get_segregated(page_base),
                    begin,
                    config.medium_segregated_config,
                    pas_segregated_page_exclusive_role);
            case pas_medium_bitfit_page_kind:
                return config.medium_bitfit_config.specialized_page_get_allocation_size_with_page(
                    pas_page_base_get_bitfit(page_base),
                    begin);
            case pas_marge_bitfit_page_kind:
                return config.marge_bitfit_config.specialized_page_get_allocation_size_with_page(
                    pas_page_base_get_bitfit(page_base),
                    begin);
            }
            PAS_ASSERT(!"Bad page kind");
            return 0;
        }
        
        pas_heap_lock_lock();
        
        entry = pas_large_map_find(begin);
        
        if (!pas_large_map_entry_is_empty(entry)) {
            PAS_PROFILE(LARGE_MAP_FOUND_ENTRY, &config, entry.begin, entry.end);
            PAS_ASSERT(entry.begin == begin);
            PAS_ASSERT(entry.end > begin);
            
            result = entry.end - begin;
        } else
            result = 0;
        
        pas_heap_lock_unlock();
        
        return result;
    } }
    
    PAS_ASSERT(!"Should not be reached");
    return 0;
}

PAS_END_EXTERN_C;

#endif /* PAS_GET_ALLOCATION_SIZE */

