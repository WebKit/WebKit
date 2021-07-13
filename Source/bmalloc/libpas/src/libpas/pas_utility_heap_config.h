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

#ifndef PAS_UTILITY_HEAP_CONFIG_H
#define PAS_UTILITY_HEAP_CONFIG_H

#include "pas_heap_config_utils.h"
#include "pas_segregated_page.h"
#include "pas_segregated_page_config_utils.h"

PAS_BEGIN_EXTERN_C;

#define PAS_UTILITY_NUM_ALLOC_BITS \
    PAS_BASIC_SEGREGATED_NUM_ALLOC_BITS(PAS_INTERNAL_MIN_ALIGN_SHIFT, \
                                        PAS_SMALL_PAGE_DEFAULT_SIZE)

#define PAS_UTILITY_HEAP_HEADER_SIZE \
    PAS_BASIC_SEGREGATED_PAGE_HEADER_SIZE(PAS_INTERNAL_MIN_ALIGN_SHIFT, \
                                          PAS_SMALL_PAGE_DEFAULT_SIZE, \
                                          PAS_SMALL_PAGE_DEFAULT_SIZE)

#define PAS_UTILITY_HEAP_PAYLOAD_OFFSET \
    PAS_BASIC_SEGREGATED_PAYLOAD_OFFSET(PAS_INTERNAL_MIN_ALIGN_SHIFT, \
                                        PAS_SMALL_PAGE_DEFAULT_SIZE, \
                                        PAS_SMALL_PAGE_DEFAULT_SIZE)

PAS_API extern pas_segregated_shared_page_directory pas_utility_heap_shared_page_directory;

static inline pas_page_base* pas_utility_heap_page_header_for_boundary(void* allocation)
{
    return (pas_page_base*)allocation;
}

static inline void* pas_utility_heap_boundary_for_page_header(pas_page_base* page)
{
    return page;
}

PAS_API void* pas_utility_heap_allocate_page(pas_segregated_heap* heap);

static inline pas_segregated_shared_page_directory*
pas_utility_heap_shared_page_directory_selector(pas_segregated_heap* heap,
                                                pas_segregated_global_size_directory* directory)
{
    PAS_UNUSED_PARAM(heap);
    PAS_UNUSED_PARAM(directory);
    return &pas_utility_heap_shared_page_directory;
}

static inline pas_page_base* pas_utility_heap_create_page_header(
    void* boundary, pas_lock_hold_mode heap_lock_hold_mode)
{
    PAS_UNUSED_PARAM(heap_lock_hold_mode);
    return (pas_page_base*)boundary;
}

static inline void pas_utility_heap_destroy_page_header(
    pas_page_base* page_base, pas_lock_hold_mode heap_lock_hold_mode)
{
    PAS_UNUSED_PARAM(page_base);
    PAS_UNUSED_PARAM(heap_lock_hold_mode);
}

PAS_API bool pas_utility_heap_config_for_each_shared_page_directory(
    pas_segregated_heap* heap,
    bool (*callback)(pas_segregated_shared_page_directory* directory,
                     void* arg),
    void* arg);

#define PAS_UTILITY_HEAP_CONFIG ((pas_heap_config){ \
        .config_ptr = &pas_utility_heap_config, \
        .kind = pas_heap_config_kind_pas_utility, \
        .activate_callback = NULL, \
        .get_type_size = NULL, \
        .get_type_alignment = NULL, \
        .large_alignment = PAS_INTERNAL_MIN_ALIGN, \
        .small_segregated_config = { \
            .base = { \
                .is_enabled = true, \
                .heap_config_ptr = &pas_utility_heap_config, \
                .page_config_ptr = &pas_utility_heap_config.small_segregated_config.base, \
                .page_kind = pas_small_segregated_page_kind, \
                .min_align_shift = PAS_INTERNAL_MIN_ALIGN_SHIFT, \
                .page_size = PAS_SMALL_PAGE_DEFAULT_SIZE, \
                .granule_size = PAS_SMALL_PAGE_DEFAULT_SIZE, \
                .max_object_size = PAS_UTILITY_LOOKUP_SIZE_UPPER_BOUND, \
                .page_header_size = PAS_UTILITY_HEAP_HEADER_SIZE, \
                .page_header_for_boundary = pas_utility_heap_page_header_for_boundary, \
                .boundary_for_page_header = pas_utility_heap_boundary_for_page_header, \
                .page_header_for_boundary_remote = NULL, \
                .page_object_payload_offset = PAS_UTILITY_HEAP_PAYLOAD_OFFSET, \
                .page_object_payload_size = \
                    PAS_SMALL_PAGE_DEFAULT_SIZE - PAS_UTILITY_HEAP_PAYLOAD_OFFSET, \
                .page_allocator = pas_utility_heap_allocate_page, \
                .create_page_header = pas_utility_heap_create_page_header, \
                .destroy_page_header = pas_utility_heap_destroy_page_header, \
            }, \
            .variant = pas_small_segregated_page_config_variant, \
            .kind = pas_segregated_page_config_kind_pas_utility_small, \
            .wasteage_handicap = 1., \
            .sharing_shift = PAS_SMALL_SHARING_SHIFT, \
            .num_alloc_bits = PAS_UTILITY_NUM_ALLOC_BITS, \
            .dealloc_func = NULL, \
            .use_reversed_current_word = PAS_ARM64, \
            .check_deallocation = false, \
            .enable_empty_word_eligibility_optimization = false, \
            .shared_page_directory_selector = pas_utility_heap_shared_page_directory_selector, \
            PAS_SEGREGATED_PAGE_CONFIG_SPECIALIZATIONS(pas_utility_heap_page_config) \
        }, \
        .medium_segregated_config = { \
            .base = { \
                .is_enabled = false \
            } \
        }, \
        .small_bitfit_config = { \
            .base = { \
                .is_enabled = false \
            } \
        }, \
        .medium_bitfit_config = { \
            .base = { \
                .is_enabled = false \
            } \
        }, \
        .marge_bitfit_config = { \
            .base = { \
                .is_enabled = false \
            } \
        }, \
        .small_lookup_size_upper_bound = PAS_UTILITY_LOOKUP_SIZE_UPPER_BOUND, \
        .fast_megapage_kind_func = NULL, \
        .small_segregated_is_in_megapage = false, \
        .small_bitfit_is_in_megapage = false, \
        .page_header_func = NULL, \
        .aligned_allocator = NULL, \
        .aligned_allocator_talks_to_sharing_pool = false, \
        .deallocator = NULL, \
        .root_data = NULL, \
        .prepare_to_enumerate = NULL, \
        .for_each_shared_page_directory = pas_utility_heap_config_for_each_shared_page_directory, \
        .for_each_shared_page_directory_remote = NULL, \
        PAS_HEAP_CONFIG_SPECIALIZATIONS(pas_utility_heap_config) \
    })

PAS_API extern pas_heap_config pas_utility_heap_config;

PAS_SEGREGATED_PAGE_CONFIG_SPECIALIZATION_DECLARATIONS(pas_utility_heap_page_config);
PAS_HEAP_CONFIG_SPECIALIZATION_DECLARATIONS(pas_utility_heap_config);

static PAS_ALWAYS_INLINE bool pas_heap_config_is_utility(pas_heap_config* config)
{
    return config == &pas_utility_heap_config;
}

static PAS_ALWAYS_INLINE pas_lock_hold_mode pas_heap_config_heap_lock_hold_mode(pas_heap_config* config)
{
    return pas_heap_config_is_utility(config)
        ? pas_lock_is_held
        : pas_lock_is_not_held;
}

PAS_END_EXTERN_C;

#endif /* PAS_UTILITY_HEAP_CONFIG_H */

