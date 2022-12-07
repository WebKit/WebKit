/*
 * Copyright (c) 2018-2021 Apple Inc. All rights reserved.
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

#ifndef PAS_HEAP_CONFIG_UTILS_H
#define PAS_HEAP_CONFIG_UTILS_H

PAS_IGNORE_WARNINGS_BEGIN("cast-align")

#include "pas_basic_heap_config_root_data.h"
#include "pas_basic_heap_page_caches.h"
#include "pas_basic_heap_runtime_config.h"
#include "pas_bitfit_page_config_utils.h"
#include "pas_heap_config.h"
#include "pas_segregated_page_config_utils.h"
#include "pas_page_header_placement_mode.h"

PAS_BEGIN_EXTERN_C;

struct pas_large_heap_physical_page_sharing_cache;
typedef struct pas_large_heap_physical_page_sharing_cache pas_large_heap_physical_page_sharing_cache;

/* NOTE: always pass pas_heap_config by value to inline functions, using constants if possible.
   Pass pas_heap_config by pointer to out-of-line functions, using the provided globals if
   possible.  See thingy_heap_config.h and iso_heap_config.h for some valid
   configurations. */

PAS_API void pas_heap_config_utils_null_activate(void);

PAS_API bool pas_heap_config_utils_for_each_shared_page_directory(
    pas_segregated_heap* heap,
    bool (*callback)(pas_segregated_shared_page_directory* directory,
                     void* arg),
    void* arg);

PAS_API bool pas_heap_config_utils_for_each_shared_page_directory_remote(
    pas_enumerator* enumerator,
    pas_segregated_heap* heap,
    bool (*callback)(pas_enumerator* enumerator,
                     pas_segregated_shared_page_directory* directory,
                     void* arg),
    void* arg);

typedef struct {
    pas_heap_config_activate_callback activate;
    pas_heap_config_get_type_size get_type_size;
    pas_heap_config_get_type_alignment get_type_alignment;
    pas_heap_config_dump_type dump_type;
    bool check_deallocation;
    uint8_t small_segregated_min_align_shift;
    uint8_t small_segregated_sharing_shift;
    size_t small_segregated_page_size;
    double small_segregated_wasteage_handicap;
    pas_segregated_deallocation_logging_mode small_exclusive_segregated_logging_mode;
    pas_segregated_deallocation_logging_mode small_shared_segregated_logging_mode;
    bool small_exclusive_segregated_enable_empty_word_eligibility_optimization;
    bool small_shared_segregated_enable_empty_word_eligibility_optimization;
    bool small_segregated_use_reversed_current_word;
    bool enable_view_cache;
    bool use_small_bitfit;
    uint8_t small_bitfit_min_align_shift;
    size_t small_bitfit_page_size;
    size_t medium_page_size; /* segregated and bitfit must share the same page size. */
    size_t granule_size;
    bool use_medium_segregated;
    uint8_t medium_segregated_min_align_shift;
    uint8_t medium_segregated_sharing_shift;
    double medium_segregated_wasteage_handicap;
    pas_segregated_deallocation_logging_mode medium_exclusive_segregated_logging_mode;
    pas_segregated_deallocation_logging_mode medium_shared_segregated_logging_mode;
    bool use_medium_bitfit;
    uint8_t medium_bitfit_min_align_shift;
    bool use_marge_bitfit;
    uint8_t marge_bitfit_min_align_shift;
    size_t marge_bitfit_page_size;
    bool pgm_enabled;
} pas_basic_heap_config_arguments;

#define PAS_BASIC_HEAP_CONFIG_SEGREGATED_HEAP_FIELDS(name, ...) \
    .small_segregated_config = { \
        .base = { \
            .is_enabled = true, \
            .heap_config_ptr = &name ## _heap_config, \
            .page_config_ptr = &name ## _heap_config.small_segregated_config.base, \
            .page_config_kind = pas_page_config_kind_segregated, \
            .min_align_shift = \
                ((pas_basic_heap_config_arguments){__VA_ARGS__}).small_segregated_min_align_shift, \
            .page_size = \
                ((pas_basic_heap_config_arguments){__VA_ARGS__}).small_segregated_page_size, \
            .granule_size = \
                ((pas_basic_heap_config_arguments){__VA_ARGS__}).small_segregated_page_size, \
            .max_object_size = PAS_MAX_OBJECT_SIZE(PAS_BASIC_SEGREGATED_PAYLOAD_SIZE_EXCLUSIVE( \
                ((pas_basic_heap_config_arguments){__VA_ARGS__}).small_segregated_min_align_shift, \
                ((pas_basic_heap_config_arguments){__VA_ARGS__}).small_segregated_page_size, \
                ((pas_basic_heap_config_arguments){__VA_ARGS__}).small_segregated_page_size)), \
            .page_header_for_boundary = name ## _small_segregated_page_header_for_boundary, \
            .boundary_for_page_header = name ## _small_segregated_boundary_for_page_header, \
            .page_header_for_boundary_remote = name ## _small_segregated_page_header_for_boundary_remote, \
            .create_page_header = name ## _small_segregated_create_page_header, \
            .destroy_page_header = name ## _small_segregated_destroy_page_header \
        }, \
        .variant = pas_small_segregated_page_config_variant, \
        .kind = pas_segregated_page_config_kind_ ## name ## _small_segregated, \
        .wasteage_handicap = \
            ((pas_basic_heap_config_arguments){__VA_ARGS__}).small_segregated_wasteage_handicap, \
        .sharing_shift = \
            ((pas_basic_heap_config_arguments){__VA_ARGS__}).small_segregated_sharing_shift, \
        .num_alloc_bits = PAS_BASIC_SEGREGATED_NUM_ALLOC_BITS( \
            ((pas_basic_heap_config_arguments){__VA_ARGS__}).small_segregated_min_align_shift, \
            ((pas_basic_heap_config_arguments){__VA_ARGS__}).small_segregated_page_size), \
        .shared_payload_offset = PAS_BASIC_SEGREGATED_PAYLOAD_OFFSET_SHARED( \
            ((pas_basic_heap_config_arguments){__VA_ARGS__}).small_segregated_min_align_shift, \
            ((pas_basic_heap_config_arguments){__VA_ARGS__}).small_segregated_page_size, \
            ((pas_basic_heap_config_arguments){__VA_ARGS__}).small_segregated_page_size), \
        .exclusive_payload_offset = PAS_BASIC_SEGREGATED_PAYLOAD_OFFSET_EXCLUSIVE( \
            ((pas_basic_heap_config_arguments){__VA_ARGS__}).small_segregated_min_align_shift, \
            ((pas_basic_heap_config_arguments){__VA_ARGS__}).small_segregated_page_size, \
            ((pas_basic_heap_config_arguments){__VA_ARGS__}).small_segregated_page_size), \
        .shared_payload_size = PAS_BASIC_SEGREGATED_PAYLOAD_SIZE_SHARED( \
            ((pas_basic_heap_config_arguments){__VA_ARGS__}).small_segregated_min_align_shift, \
            ((pas_basic_heap_config_arguments){__VA_ARGS__}).small_segregated_page_size, \
            ((pas_basic_heap_config_arguments){__VA_ARGS__}).small_segregated_page_size), \
        .exclusive_payload_size = PAS_BASIC_SEGREGATED_PAYLOAD_SIZE_EXCLUSIVE( \
            ((pas_basic_heap_config_arguments){__VA_ARGS__}).small_segregated_min_align_shift, \
            ((pas_basic_heap_config_arguments){__VA_ARGS__}).small_segregated_page_size, \
            ((pas_basic_heap_config_arguments){__VA_ARGS__}).small_segregated_page_size), \
        .shared_logging_mode = \
            ((pas_basic_heap_config_arguments){__VA_ARGS__}).small_shared_segregated_logging_mode, \
        .exclusive_logging_mode = \
            ((pas_basic_heap_config_arguments){__VA_ARGS__}).small_exclusive_segregated_logging_mode, \
        .use_reversed_current_word = \
            ((pas_basic_heap_config_arguments){__VA_ARGS__}).small_segregated_use_reversed_current_word, \
        .check_deallocation = ((pas_basic_heap_config_arguments){__VA_ARGS__}).check_deallocation, \
        .enable_empty_word_eligibility_optimization_for_shared = \
            ((pas_basic_heap_config_arguments){__VA_ARGS__}).small_shared_segregated_enable_empty_word_eligibility_optimization, \
        .enable_empty_word_eligibility_optimization_for_exclusive = \
            ((pas_basic_heap_config_arguments){__VA_ARGS__}).small_exclusive_segregated_enable_empty_word_eligibility_optimization, \
        .enable_view_cache = \
            ((pas_basic_heap_config_arguments){__VA_ARGS__}).enable_view_cache, \
        .page_allocator = name ## _heap_config_allocate_small_segregated_page, \
        .shared_page_directory_selector = \
            name ## _small_segregated_page_config_select_shared_page_directory, \
        PAS_SEGREGATED_PAGE_CONFIG_SPECIALIZATIONS(name ## _small_segregated_page_config) \
    }, \
    .medium_segregated_config = { \
        .base = { \
            .is_enabled = \
                ((pas_basic_heap_config_arguments){__VA_ARGS__}).use_medium_segregated, \
            .heap_config_ptr = &name ## _heap_config, \
            .page_config_ptr = &name ## _heap_config.medium_segregated_config.base, \
            .page_config_kind = pas_page_config_kind_segregated, \
            .min_align_shift = \
                ((pas_basic_heap_config_arguments){__VA_ARGS__}).medium_segregated_min_align_shift, \
            .page_size = \
                ((pas_basic_heap_config_arguments){__VA_ARGS__}).medium_page_size, \
            .granule_size = \
                ((pas_basic_heap_config_arguments){__VA_ARGS__}).granule_size, \
            .max_object_size = PAS_MAX_OBJECT_SIZE( \
                ((pas_basic_heap_config_arguments){__VA_ARGS__}).medium_page_size), \
            .page_header_for_boundary = name ## _medium_segregated_page_header_for_boundary, \
            .boundary_for_page_header = name ## _medium_segregated_boundary_for_page_header, \
            .page_header_for_boundary_remote = \
                name ## _medium_segregated_page_header_for_boundary_remote, \
            .create_page_header = name ## _medium_segregated_create_page_header, \
            .destroy_page_header = name ## _medium_segregated_destroy_page_header \
        }, \
        .variant = pas_medium_segregated_page_config_variant, \
        .kind = pas_segregated_page_config_kind_ ## name ## _medium_segregated, \
        .wasteage_handicap = \
            ((pas_basic_heap_config_arguments){__VA_ARGS__}).medium_segregated_wasteage_handicap, \
        .sharing_shift = \
            ((pas_basic_heap_config_arguments){__VA_ARGS__}).medium_segregated_sharing_shift, \
        .num_alloc_bits = PAS_BASIC_SEGREGATED_NUM_ALLOC_BITS( \
            ((pas_basic_heap_config_arguments){__VA_ARGS__}).medium_segregated_min_align_shift, \
            ((pas_basic_heap_config_arguments){__VA_ARGS__}).medium_page_size), \
        .shared_payload_offset = 0, \
        .exclusive_payload_offset = 0, \
        .shared_payload_size = \
            ((pas_basic_heap_config_arguments){__VA_ARGS__}).medium_page_size, \
        .exclusive_payload_size = \
            ((pas_basic_heap_config_arguments){__VA_ARGS__}).medium_page_size, \
        .shared_logging_mode = \
            ((pas_basic_heap_config_arguments){__VA_ARGS__}).medium_shared_segregated_logging_mode, \
        .exclusive_logging_mode = \
            ((pas_basic_heap_config_arguments){__VA_ARGS__}).medium_exclusive_segregated_logging_mode, \
        .use_reversed_current_word = false, \
        .check_deallocation = ((pas_basic_heap_config_arguments){__VA_ARGS__}).check_deallocation, \
        .enable_empty_word_eligibility_optimization_for_shared = false, \
        .enable_empty_word_eligibility_optimization_for_exclusive = false, \
        .enable_view_cache = \
            ((pas_basic_heap_config_arguments){__VA_ARGS__}).enable_view_cache, \
        .page_allocator = name ## _heap_config_allocate_medium_segregated_page, \
        .shared_page_directory_selector = \
            name ## _medium_segregated_page_config_select_shared_page_directory, \
        PAS_SEGREGATED_PAGE_CONFIG_SPECIALIZATIONS(name ## _medium_segregated_page_config) \
    }, \
    .small_bitfit_config = { \
        .base = { \
            .is_enabled = \
                ((pas_basic_heap_config_arguments){__VA_ARGS__}).use_small_bitfit, \
            .heap_config_ptr = &name ## _heap_config, \
            .page_config_ptr = &name ## _heap_config.small_bitfit_config.base, \
            .page_config_kind = pas_page_config_kind_bitfit, \
            .min_align_shift = \
                ((pas_basic_heap_config_arguments){__VA_ARGS__}).small_bitfit_min_align_shift, \
            .page_size = \
                ((pas_basic_heap_config_arguments){__VA_ARGS__}).small_bitfit_page_size, \
            .granule_size = \
                ((pas_basic_heap_config_arguments){__VA_ARGS__}).small_bitfit_page_size, \
            .max_object_size = PAS_MAX_BITFIT_OBJECT_SIZE( \
                ((pas_basic_heap_config_arguments){__VA_ARGS__}).small_bitfit_page_size - \
                PAS_BITFIT_PAGE_HEADER_SIZE( \
                    ((pas_basic_heap_config_arguments){__VA_ARGS__}).small_bitfit_page_size, \
                    ((pas_basic_heap_config_arguments){__VA_ARGS__}).small_bitfit_page_size, \
                    ((pas_basic_heap_config_arguments){__VA_ARGS__}).small_bitfit_min_align_shift), \
                ((pas_basic_heap_config_arguments){__VA_ARGS__}).small_bitfit_min_align_shift), \
            .page_header_for_boundary = name ## _small_bitfit_page_header_for_boundary, \
            .boundary_for_page_header = name ## _small_bitfit_boundary_for_page_header, \
            .page_header_for_boundary_remote = name ## _small_bitfit_page_header_for_boundary_remote, \
            .create_page_header = name ## _small_bitfit_create_page_header, \
            .destroy_page_header = name ## _small_bitfit_destroy_page_header \
        }, \
        .variant = pas_small_bitfit_page_config_variant, \
        .kind = pas_bitfit_page_config_kind_ ## name ## _small_bitfit, \
        .page_object_payload_offset = PAS_BITFIT_PAGE_HEADER_SIZE( \
            ((pas_basic_heap_config_arguments){__VA_ARGS__}).small_bitfit_page_size, \
            ((pas_basic_heap_config_arguments){__VA_ARGS__}).small_bitfit_page_size, \
            ((pas_basic_heap_config_arguments){__VA_ARGS__}).small_bitfit_min_align_shift), \
        .page_object_payload_size = \
            ((pas_basic_heap_config_arguments){__VA_ARGS__}).small_bitfit_page_size - \
            PAS_BITFIT_PAGE_HEADER_SIZE( \
                ((pas_basic_heap_config_arguments){__VA_ARGS__}).small_bitfit_page_size, \
                ((pas_basic_heap_config_arguments){__VA_ARGS__}).small_bitfit_page_size, \
                ((pas_basic_heap_config_arguments){__VA_ARGS__}).small_bitfit_min_align_shift), \
        .page_allocator = name ## _heap_config_allocate_small_bitfit_page, \
        PAS_BITFIT_PAGE_CONFIG_SPECIALIZATIONS(name ## _small_bitfit_page_config) \
    }, \
    .medium_bitfit_config = { \
        .base = { \
            .is_enabled = \
                ((pas_basic_heap_config_arguments){__VA_ARGS__}).use_medium_bitfit, \
            .heap_config_ptr = &name ## _heap_config, \
            .page_config_ptr = &name ## _heap_config.medium_bitfit_config.base, \
            .page_config_kind = pas_page_config_kind_bitfit, \
            .min_align_shift = \
                ((pas_basic_heap_config_arguments){__VA_ARGS__}).medium_bitfit_min_align_shift, \
            .page_size = \
                ((pas_basic_heap_config_arguments){__VA_ARGS__}).medium_page_size, \
            .granule_size = \
                ((pas_basic_heap_config_arguments){__VA_ARGS__}).granule_size, \
            .max_object_size = PAS_MAX_BITFIT_OBJECT_SIZE( \
                ((pas_basic_heap_config_arguments){__VA_ARGS__}).medium_page_size, \
                ((pas_basic_heap_config_arguments){__VA_ARGS__}).medium_bitfit_min_align_shift), \
            .page_header_for_boundary = name ## _medium_bitfit_page_header_for_boundary, \
            .boundary_for_page_header = name ## _medium_bitfit_boundary_for_page_header, \
            .page_header_for_boundary_remote = name ## _medium_bitfit_page_header_for_boundary_remote, \
            .create_page_header = name ## _medium_bitfit_create_page_header, \
            .destroy_page_header = name ## _medium_bitfit_destroy_page_header \
        }, \
        .variant = pas_medium_bitfit_page_config_variant, \
        .kind = pas_bitfit_page_config_kind_ ## name ## _medium_bitfit, \
        .page_object_payload_offset = 0, \
        .page_object_payload_size = \
            ((pas_basic_heap_config_arguments){__VA_ARGS__}).medium_page_size, \
        .page_allocator = name ## _heap_config_allocate_medium_bitfit_page, \
        PAS_BITFIT_PAGE_CONFIG_SPECIALIZATIONS(name ## _medium_bitfit_page_config) \
    }, \
    .marge_bitfit_config = { \
        .base = { \
            .is_enabled = \
                ((pas_basic_heap_config_arguments){__VA_ARGS__}).use_marge_bitfit, \
            .heap_config_ptr = &name ## _heap_config, \
            .page_config_ptr = &name ## _heap_config.marge_bitfit_config.base, \
            .page_config_kind = pas_page_config_kind_bitfit, \
            .min_align_shift = \
                ((pas_basic_heap_config_arguments){__VA_ARGS__}).marge_bitfit_min_align_shift, \
            .page_size = \
                ((pas_basic_heap_config_arguments){__VA_ARGS__}).marge_bitfit_page_size, \
            .granule_size = \
                ((pas_basic_heap_config_arguments){__VA_ARGS__}).granule_size, \
            .max_object_size = PAS_MAX_BITFIT_OBJECT_SIZE( \
                ((pas_basic_heap_config_arguments){__VA_ARGS__}).marge_bitfit_page_size, \
                ((pas_basic_heap_config_arguments){__VA_ARGS__}).marge_bitfit_min_align_shift), \
            .page_header_for_boundary = name ## _marge_bitfit_page_header_for_boundary, \
            .boundary_for_page_header = name ## _marge_bitfit_boundary_for_page_header, \
            .page_header_for_boundary_remote = name ## _marge_bitfit_page_header_for_boundary_remote, \
            .create_page_header = name ## _marge_bitfit_create_page_header, \
            .destroy_page_header = name ## _marge_bitfit_destroy_page_header \
        }, \
        .variant = pas_marge_bitfit_page_config_variant, \
        .kind = pas_bitfit_page_config_kind_ ## name ## _marge_bitfit, \
        .page_object_payload_offset = 0, \
        .page_object_payload_size = \
            ((pas_basic_heap_config_arguments){__VA_ARGS__}).marge_bitfit_page_size, \
        .page_allocator = name ## _heap_config_allocate_marge_bitfit_page, \
        PAS_BITFIT_PAGE_CONFIG_SPECIALIZATIONS(name ## _marge_bitfit_page_config) \
    }, \
    .small_lookup_size_upper_bound = PAS_SMALL_LOOKUP_SIZE_UPPER_BOUND, \
    .fast_megapage_kind_func = name ## _heap_config_fast_megapage_kind, \
    .small_segregated_is_in_megapage = true, \
    .small_bitfit_is_in_megapage = true, \
    .page_header_func = name ## _heap_config_page_header, \

#define PAS_BASIC_HEAP_CONFIG(name, ...) ((pas_heap_config){ \
        .config_ptr = &name ## _heap_config, \
        .kind = pas_heap_config_kind_ ## name, \
        .activate_callback = ((pas_basic_heap_config_arguments){__VA_ARGS__}).activate, \
        .get_type_size = ((pas_basic_heap_config_arguments){__VA_ARGS__}).get_type_size, \
        .get_type_alignment = ((pas_basic_heap_config_arguments){__VA_ARGS__}).get_type_alignment, \
        .dump_type = ((pas_basic_heap_config_arguments){__VA_ARGS__}).dump_type, \
        .large_alignment = \
            (size_t)1 << ((pas_basic_heap_config_arguments){__VA_ARGS__}).small_segregated_min_align_shift, \
        PAS_BASIC_HEAP_CONFIG_SEGREGATED_HEAP_FIELDS(name, __VA_ARGS__) \
        .aligned_allocator = name ## _aligned_allocator, \
        .aligned_allocator_talks_to_sharing_pool = true, \
        .deallocator = NULL, \
        .mmap_capability = pas_may_mmap, \
        .root_data = &name ## _root_data, \
        .prepare_to_enumerate = name ## _prepare_to_enumerate, \
        .for_each_shared_page_directory = \
            pas_heap_config_utils_for_each_shared_page_directory, \
        .for_each_shared_page_directory_remote = \
            pas_heap_config_utils_for_each_shared_page_directory_remote, \
        .dump_shared_page_directory_arg = pas_shared_page_directory_by_size_dump_directory_arg, \
        PAS_HEAP_CONFIG_SPECIALIZATIONS(name ## _heap_config), \
        .pgm_enabled = false \
    })

#define PAS_BASIC_HEAP_CONFIG_SEGREGATED_HEAP_DECLARATIONS(name, upcase_name) \
    PAS_BASIC_SEGREGATED_PAGE_CONFIG_FORWARD_DECLARATIONS(name ## _small_segregated); \
    PAS_BASIC_SEGREGATED_PAGE_CONFIG_FORWARD_DECLARATIONS(name ## _medium_segregated); \
    PAS_BASIC_BITFIT_PAGE_CONFIG_FORWARD_DECLARATIONS(name ## _small_bitfit); \
    PAS_BASIC_BITFIT_PAGE_CONFIG_FORWARD_DECLARATIONS(name ## _medium_bitfit); \
    PAS_BASIC_BITFIT_PAGE_CONFIG_FORWARD_DECLARATIONS(name ## _marge_bitfit); \
    PAS_API extern pas_fast_megapage_table name ## _megapage_table; \
    PAS_API extern pas_page_header_table name ## _medium_page_header_table; \
    PAS_API extern pas_page_header_table name ## _marge_page_header_table; \
    PAS_API extern pas_basic_heap_page_caches name ## _page_caches; \
    PAS_API extern pas_basic_heap_runtime_config name ## _intrinsic_runtime_config; \
    PAS_API extern pas_basic_heap_runtime_config name ## _primitive_runtime_config; \
    PAS_API extern pas_basic_heap_runtime_config name ## _typed_runtime_config; \
    PAS_API extern pas_basic_heap_runtime_config name ## _flex_runtime_config; \
    \
    PAS_API extern pas_basic_heap_config_root_data name ## _root_data; \
    \
    PAS_API void* name ## _heap_config_allocate_small_segregated_page( \
        pas_segregated_heap* heap, pas_physical_memory_transaction* transaction, \
        pas_segregated_page_role role); \
    PAS_API void* name ## _heap_config_allocate_small_bitfit_page( \
        pas_segregated_heap* heap, pas_physical_memory_transaction* transaction); \
    PAS_API void* name ## _heap_config_allocate_medium_segregated_page( \
        pas_segregated_heap* heap, pas_physical_memory_transaction* transaction, \
        pas_segregated_page_role role); \
    PAS_API void* name ## _heap_config_allocate_medium_bitfit_page( \
        pas_segregated_heap* heap, pas_physical_memory_transaction* transaction); \
    PAS_API void* name ## _heap_config_allocate_marge_bitfit_page( \
        pas_segregated_heap* heap, pas_physical_memory_transaction* transaction); \
    \
    PAS_API void* name ## _prepare_to_enumerate(pas_enumerator* enumerator); \
    \
    static PAS_ALWAYS_INLINE pas_fast_megapage_kind \
    name ## _heap_config_fast_megapage_kind(uintptr_t begin) \
    { \
        return pas_fast_megapage_table_get(&name ## _megapage_table, begin); \
    } \
    static PAS_ALWAYS_INLINE pas_page_base* \
    name ## _heap_config_page_header(uintptr_t begin) \
    { \
        pas_heap_config config; \
        \
        config = (upcase_name ## _HEAP_CONFIG); \
        \
        if (config.medium_segregated_config.base.is_enabled \
            || config.medium_bitfit_config.base.is_enabled) { \
            pas_page_base* result; \
            \
            PAS_ASSERT( \
                !config.medium_segregated_config.base.is_enabled || \
                !config.medium_bitfit_config.base.is_enabled || \
                config.medium_segregated_config.base.page_size \
                == config.medium_bitfit_config.base.page_size); \
            \
            result = pas_page_header_table_get_for_address( \
                &name ## _medium_page_header_table, \
                config.medium_segregated_config.base.is_enabled \
                ? config.medium_segregated_config.base.page_size \
                : config.medium_bitfit_config.base.page_size, \
                (void*)begin); \
            if (result) \
                return result; \
        } \
        \
        if (config.marge_bitfit_config.base.is_enabled) { \
            return pas_page_header_table_get_for_address( \
                &name ## _marge_page_header_table, \
                config.marge_bitfit_config.base.page_size, \
                (void*)begin); \
        } \
        \
        return NULL; \
    } \
    \
    PAS_BASIC_SEGREGATED_PAGE_CONFIG_DECLARATIONS( \
        name ## _small_segregated, (upcase_name ## _HEAP_CONFIG).small_segregated_config, \
        pas_page_header_at_head_of_page, \
        NULL); \
    PAS_BASIC_SEGREGATED_PAGE_CONFIG_DECLARATIONS( \
        name ## _medium_segregated, (upcase_name ## _HEAP_CONFIG).medium_segregated_config, \
        pas_page_header_in_table, \
        &name ## _medium_page_header_table); \
    \
    PAS_BASIC_BITFIT_PAGE_CONFIG_DECLARATIONS( \
        name ## _small_bitfit, (upcase_name ## _HEAP_CONFIG).small_bitfit_config, \
        pas_page_header_at_head_of_page, \
        NULL); \
    PAS_BASIC_BITFIT_PAGE_CONFIG_DECLARATIONS( \
        name ## _medium_bitfit, (upcase_name ## _HEAP_CONFIG).medium_bitfit_config, \
        pas_page_header_in_table, \
        &name ## _medium_page_header_table); \
    PAS_BASIC_BITFIT_PAGE_CONFIG_DECLARATIONS( \
        name ## _marge_bitfit, (upcase_name ## _HEAP_CONFIG).marge_bitfit_config, \
        pas_page_header_in_table, \
        &name ## _marge_page_header_table); \
    \
    struct pas_dummy

#define PAS_BASIC_HEAP_CONFIG_DECLARATIONS(name, upcase_name) \
    PAS_API pas_aligned_allocation_result name ## _aligned_allocator( \
        size_t size, \
        pas_alignment alignment, \
        pas_large_heap* large_heap, \
        const pas_heap_config* config); \
    PAS_HEAP_CONFIG_SPECIALIZATION_DECLARATIONS(name ## _heap_config); \
    PAS_BASIC_HEAP_CONFIG_SEGREGATED_HEAP_DECLARATIONS(name, upcase_name)

PAS_END_EXTERN_C;

PAS_IGNORE_WARNINGS_END

#endif /* PAS_HEAP_CONFIG_UTILS_H */
