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

#ifndef PAS_GET_HEAP_H
#define PAS_GET_HEAP_H

#include "pas_bitfit_global_directory.h"
#include "pas_heap.h"
#include "pas_heap_config.h"
#include "pas_large_map.h"
#include "pas_segregated_page_inlines.h"
#include "pas_segregated_size_directory.h"

PAS_BEGIN_EXTERN_C;

static PAS_ALWAYS_INLINE pas_heap*
pas_get_heap_known_segregated(uintptr_t begin,
                              pas_segregated_page_config page_config)
{
    return pas_heap_for_segregated_heap(
        pas_segregated_page_get_directory_for_address_and_page_config(
            begin, page_config)->heap);
}

static PAS_ALWAYS_INLINE pas_heap* pas_get_heap(void* ptr,
                                                pas_heap_config config)
{
    uintptr_t begin;
    
    begin = (uintptr_t)ptr;
    
    switch (config.fast_megapage_kind_func(begin)) {
    case pas_small_segregated_fast_megapage_kind:
        return pas_heap_for_segregated_heap(
            pas_segregated_page_get_directory_for_address_and_page_config(
                begin, config.small_segregated_config)->heap);
    case pas_small_bitfit_fast_megapage_kind:
        return pas_heap_for_segregated_heap(
            pas_compact_bitfit_global_directory_ptr_load_non_null(
                &pas_compact_atomic_bitfit_view_ptr_load_non_null(
                    &pas_bitfit_page_for_address_and_page_config(
                        begin, config.small_bitfit_config)->owner)->global_directory)->heap);
    case pas_not_a_fast_megapage_kind: {
        pas_page_base* page_base;
        pas_large_map_entry entry;
        pas_heap* result;

        page_base = config.page_header_func(begin);
        if (page_base) {
            switch (pas_page_base_get_kind(page_base)) {
            case pas_small_segregated_page_kind:
                PAS_ASSERT(!config.small_segregated_is_in_megapage);
                return pas_heap_for_segregated_heap(
                    pas_segregated_page_get_directory_for_address_in_page(
                        pas_page_base_get_segregated(page_base),
                        begin, config.small_segregated_config)->heap);
            case pas_medium_segregated_page_kind:
                return pas_heap_for_segregated_heap(
                    pas_segregated_page_get_directory_for_address_in_page(
                        pas_page_base_get_segregated(page_base),
                        begin, config.medium_segregated_config)->heap);
            case pas_small_bitfit_page_kind:
            case pas_medium_bitfit_page_kind:
            case pas_marge_bitfit_page_kind:
                return pas_heap_for_segregated_heap(
                    pas_compact_bitfit_global_directory_ptr_load_non_null(
                        &pas_compact_atomic_bitfit_view_ptr_load_non_null(
                            &pas_page_base_get_bitfit(page_base)->owner)->global_directory)->heap);
            }
            PAS_ASSERT(!"Bad page kind");
            return NULL;
        }
        
        pas_heap_lock_lock();
        
        entry = pas_large_map_find(begin);
        
        PAS_ASSERT(!pas_large_map_entry_is_empty(entry));
        PAS_ASSERT(entry.begin == begin);
        PAS_ASSERT(entry.end > begin);
        
        result = pas_heap_for_large_heap(entry.heap);
        
        pas_heap_lock_unlock();
        
        return result;
    } }
    
    PAS_ASSERT(!"Should not be reached");
    return NULL;
}

PAS_END_EXTERN_C;

#endif /* PAS_GET_HEAP */

