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

#ifndef PAS_HEAP_CONFIG_UTILS_INLINES_H
#define PAS_HEAP_CONFIG_UTILS_INLINES_H

#include "pas_bitfit_page_config_utils_inlines.h"
#include "pas_heap_config_inlines.h"
#include "pas_heap_config_utils.h"
#include "pas_large_heap_physical_page_sharing_cache.h"
#include "pas_medium_megapage_cache.h"
#include "pas_segregated_page_config_utils_inlines.h"

PAS_BEGIN_EXTERN_C;

PAS_API pas_aligned_allocation_result
pas_heap_config_utils_allocate_aligned(
    size_t size,
    pas_alignment alignment,
    pas_large_heap* large_heap,
    pas_heap_config* config,
    bool should_zero);

PAS_API void* pas_heap_config_utils_prepare_to_enumerate(pas_enumerator* enumerator,
                                                         pas_heap_config* config);

typedef struct {
    bool allocate_page_should_zero;
} pas_basic_heap_config_definitions_arguments;

#define PAS_BASIC_HEAP_CONFIG_DEFINITIONS(name, upcase_name, ...) \
    pas_basic_heap_page_caches name ## _page_caches = \
        PAS_BASIC_HEAP_PAGE_CACHES_INITIALIZER(PAS_SMALL_SHARED_PAGE_LOG_SHIFT, \
                                               PAS_MEDIUM_SHARED_PAGE_LOG_SHIFT); \
    \
    pas_basic_heap_runtime_config name ## _intrinsic_primitive_runtime_config = { \
        .base = { \
            .lookup_kind = pas_segregated_heap_lookup_primitive, \
            .sharing_mode = pas_share_pages, \
            .statically_allocated = true, \
            .is_part_of_heap = true, \
            .directory_size_bound_for_partial_views = \
                PAS_INTRINSIC_PRIMITIVE_BOUND_FOR_PARTIAL_VIEWS, \
            .directory_size_bound_for_baseline_allocators = \
                PAS_INTRINSIC_PRIMITIVE_BOUND_FOR_BASELINE_ALLOCATORS, \
            .max_segregated_object_size = \
                PAS_INTRINSIC_PRIMITIVE_MAX_SEGREGATED_OBJECT_SIZE, \
            .max_bitfit_object_size = \
                PAS_INTRINSIC_PRIMITIVE_MAX_BITFIT_OBJECT_SIZE \
        }, \
        .page_caches = &name ## _page_caches \
    }; \
    \
    pas_basic_heap_runtime_config name ## _primitive_runtime_config = { \
        .base = { \
            .lookup_kind = pas_segregated_heap_lookup_primitive, \
            .sharing_mode = pas_share_pages, \
            .statically_allocated = false, \
            .is_part_of_heap = true, \
            .directory_size_bound_for_partial_views = \
                PAS_PRIMITIVE_BOUND_FOR_PARTIAL_VIEWS, \
            .directory_size_bound_for_baseline_allocators = \
                PAS_PRIMITIVE_BOUND_FOR_BASELINE_ALLOCATORS, \
            .max_segregated_object_size = \
                PAS_PRIMITIVE_MAX_SEGREGATED_OBJECT_SIZE, \
            .max_bitfit_object_size = \
                PAS_PRIMITIVE_MAX_BITFIT_OBJECT_SIZE \
        }, \
        .page_caches = &name ## _page_caches \
    }; \
    \
    pas_basic_heap_runtime_config name ## _typed_runtime_config = { \
        .base = { \
            .lookup_kind = pas_segregated_heap_lookup_type_based, \
            .sharing_mode = pas_share_pages, \
            .statically_allocated = false, \
            .is_part_of_heap = true, \
            .directory_size_bound_for_partial_views = \
                PAS_TYPED_BOUND_FOR_PARTIAL_VIEWS, \
            .directory_size_bound_for_baseline_allocators = \
                PAS_TYPED_BOUND_FOR_BASELINE_ALLOCATORS, \
            .max_segregated_object_size = \
                PAS_TYPED_MAX_SEGREGATED_OBJECT_SIZE, \
            .max_bitfit_object_size = \
                PAS_TYPED_MAX_BITFIT_OBJECT_SIZE \
        }, \
        .page_caches = &name ## _page_caches \
    }; \
    \
    pas_basic_heap_runtime_config name ## _objc_runtime_config = { \
        .base = { \
            .lookup_kind = pas_segregated_heap_lookup_primitive, \
            .sharing_mode = pas_share_pages, \
            .statically_allocated = false, \
            .is_part_of_heap = true, \
            .directory_size_bound_for_partial_views = \
                PAS_OBJC_BOUND_FOR_PARTIAL_VIEWS, \
            .directory_size_bound_for_baseline_allocators = \
                PAS_OBJC_BOUND_FOR_BASELINE_ALLOCATORS, \
            .max_segregated_object_size = \
                PAS_OBJC_MAX_SEGREGATED_OBJECT_SIZE, \
            .max_bitfit_object_size = \
                PAS_OBJC_MAX_BITFIT_OBJECT_SIZE \
        }, \
        .page_caches = &name ## _page_caches \
    }; \
    \
    void* name ## _heap_config_allocate_small_segregated_page(pas_segregated_heap* heap) \
    { \
        pas_basic_heap_config_definitions_arguments arguments = {__VA_ARGS__}; \
        \
        pas_segregated_page_config page_config; \
        pas_basic_heap_page_caches* page_caches; \
        void* allocation; \
        \
        page_config = upcase_name ## _HEAP_CONFIG.small_segregated_config; \
        \
        PAS_ASSERT(page_config.base.is_enabled); \
        \
        page_caches = ((pas_basic_heap_runtime_config*)heap->runtime_config)->page_caches; \
        \
        allocation = pas_fast_megapage_cache_try_allocate( \
            &page_caches->small_segregated_megapage_cache, \
            &name ## _megapage_table, \
            page_config.base.page_config_ptr, \
            pas_small_segregated_fast_megapage_kind, \
            arguments.allocate_page_should_zero); \
        \
        return allocation; \
    } \
    \
    void* name ## _heap_config_allocate_small_bitfit_page(pas_segregated_heap* heap) \
    { \
        pas_basic_heap_config_definitions_arguments arguments = {__VA_ARGS__}; \
        \
        pas_bitfit_page_config page_config; \
        pas_basic_heap_page_caches* page_caches; \
        void* allocation; \
        \
        page_config = upcase_name ## _HEAP_CONFIG.small_bitfit_config; \
        \
        PAS_ASSERT(page_config.base.is_enabled); \
        \
        page_caches = ((pas_basic_heap_runtime_config*)heap->runtime_config)->page_caches; \
        \
        allocation = pas_fast_megapage_cache_try_allocate( \
            &page_caches->small_bitfit_megapage_cache, \
            &name ## _megapage_table, \
            page_config.base.page_config_ptr, \
            pas_small_bitfit_fast_megapage_kind, \
            arguments.allocate_page_should_zero); \
        \
        return allocation; \
    } \
    \
    void* name ## _heap_config_allocate_medium_segregated_page(pas_segregated_heap* heap) \
    { \
        pas_basic_heap_config_definitions_arguments arguments = {__VA_ARGS__}; \
        \
        pas_segregated_page_config page_config; \
        pas_basic_heap_page_caches* page_caches; \
        void* allocation; \
        \
        page_config = upcase_name ## _HEAP_CONFIG.medium_segregated_config; \
        \
        PAS_ASSERT(page_config.base.is_enabled); \
        \
        page_caches = ((pas_basic_heap_runtime_config*)heap->runtime_config)->page_caches; \
        \
        allocation = pas_medium_megapage_cache_try_allocate( \
            &page_caches->medium_megapage_cache, \
            page_config.base.page_config_ptr, \
            arguments.allocate_page_should_zero); \
        \
        return allocation; \
    } \
    \
    void* name ## _heap_config_allocate_medium_bitfit_page(pas_segregated_heap* heap) \
    { \
        pas_basic_heap_config_definitions_arguments arguments = {__VA_ARGS__}; \
        \
        pas_bitfit_page_config page_config; \
        pas_basic_heap_page_caches* page_caches; \
        void* allocation; \
        \
        page_config = upcase_name ## _HEAP_CONFIG.medium_bitfit_config; \
        \
        PAS_ASSERT(page_config.base.is_enabled); \
        \
        page_caches = ((pas_basic_heap_runtime_config*)heap->runtime_config)->page_caches; \
        \
        allocation = pas_medium_megapage_cache_try_allocate( \
            &page_caches->medium_megapage_cache, \
            page_config.base.page_config_ptr, \
            arguments.allocate_page_should_zero); \
        \
        return allocation; \
    } \
    \
    void* name ## _heap_config_allocate_marge_bitfit_page(pas_segregated_heap* heap) \
    { \
        pas_basic_heap_config_definitions_arguments arguments = {__VA_ARGS__}; \
        \
        pas_bitfit_page_config page_config; \
        pas_basic_heap_page_caches* page_caches; \
        void* allocation; \
        \
        page_config = upcase_name ## _HEAP_CONFIG.marge_bitfit_config; \
        \
        PAS_ASSERT(page_config.base.is_enabled); \
        \
        page_caches = ((pas_basic_heap_runtime_config*)heap->runtime_config)->page_caches; \
        \
        allocation = pas_medium_megapage_cache_try_allocate( \
            &page_caches->medium_megapage_cache, \
            page_config.base.page_config_ptr, \
            arguments.allocate_page_should_zero); \
        \
        return allocation; \
    } \
    \
    pas_aligned_allocation_result name ## _aligned_allocator( \
        size_t size, \
        pas_alignment alignment, \
        pas_large_heap* large_heap, \
        pas_heap_config* config) \
    { \
        return pas_heap_config_utils_allocate_aligned( \
            size, alignment, large_heap, config, \
            ((pas_basic_heap_config_definitions_arguments){__VA_ARGS__}).allocate_page_should_zero); \
    } \
    \
    pas_fast_megapage_table name ## _megapage_table = PAS_FAST_MEGAPAGE_TABLE_INITIALIZER; \
    pas_page_header_table name ## _medium_page_header_table = PAS_PAGE_HEADER_TABLE_INITIALIZER( \
        upcase_name ##_HEAP_CONFIG.medium_segregated_config.base.page_size); \
    pas_page_header_table name ## _marge_page_header_table = PAS_PAGE_HEADER_TABLE_INITIALIZER( \
        upcase_name ##_HEAP_CONFIG.marge_bitfit_config.base.page_size); \
    \
    pas_basic_heap_config_root_data name ## _root_data = { \
        .medium_page_header_table = &name ## _medium_page_header_table, \
        .marge_page_header_table = &name ## _marge_page_header_table \
    }; \
    \
    void* name ## _prepare_to_enumerate(pas_enumerator* enumerator) \
    { \
        return pas_heap_config_utils_prepare_to_enumerate(enumerator, &name ## _heap_config); \
    } \
    \
    PAS_BASIC_SEGREGATED_PAGE_CONFIG_DEFINITIONS( \
        name ## _small_segregated, \
        .page_config = upcase_name ## _HEAP_CONFIG.small_segregated_config, \
        .header_table = NULL); \
    \
    PAS_BASIC_SEGREGATED_PAGE_CONFIG_DEFINITIONS( \
        name ## _medium_segregated, \
        .page_config = upcase_name ## _HEAP_CONFIG.medium_segregated_config, \
        .header_table = &name ## _medium_page_header_table); \
    \
    PAS_BASIC_BITFIT_PAGE_CONFIG_DEFINITIONS( \
        name ## _small_bitfit, \
        .page_config = upcase_name ## _HEAP_CONFIG.small_bitfit_config, \
        .header_table = NULL); \
    \
    PAS_BASIC_BITFIT_PAGE_CONFIG_DEFINITIONS( \
        name ## _medium_bitfit, \
        .page_config = upcase_name ## _HEAP_CONFIG.medium_bitfit_config, \
        .header_table = &name ## _medium_page_header_table); \
    \
    PAS_BASIC_BITFIT_PAGE_CONFIG_DEFINITIONS( \
        name ## _marge_bitfit, \
        .page_config = upcase_name ## _HEAP_CONFIG.marge_bitfit_config, \
        .header_table = &name ## _marge_page_header_table); \
    \
    PAS_HEAP_CONFIG_SPECIALIZATION_DEFINITIONS(name ## _heap_config, upcase_name ## _HEAP_CONFIG)

PAS_END_EXTERN_C;

#endif /* PAS_HEAP_CONFIG_UTILS_INLINES_H */

