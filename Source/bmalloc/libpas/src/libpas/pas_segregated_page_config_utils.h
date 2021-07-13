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

#ifndef PAS_SEGREGATED_PAGE_CONFIG_UTILS_H
#define PAS_SEGREGATED_PAGE_CONFIG_UTILS_H

#include "pas_config.h"
#include "pas_page_base_config_utils.h"
#include "pas_segregated_page.h"
#include "pas_segregated_page_config.h"
#include "pas_segregated_page_inlines.h"
#include "pas_segregated_shared_page_directory.h"
#include "pas_thread_local_cache.h"

PAS_BEGIN_EXTERN_C;

#define PAS_BASIC_SEGREGATED_NUM_ALLOC_BITS(min_align_shift, page_size) \
    ((page_size) >> (min_align_shift))

#define PAS_BASIC_SEGREGATED_PAGE_HEADER_SIZE(min_align_shift, page_size, granule_size) \
    PAS_SEGREGATED_PAGE_HEADER_SIZE( \
        PAS_BASIC_SEGREGATED_NUM_ALLOC_BITS((min_align_shift), (page_size)), \
        (page_size) / (granule_size))

#define PAS_BASIC_SEGREGATED_PAYLOAD_OFFSET(min_align_shift, page_size, granule_size) \
    PAS_BASIC_SEGREGATED_PAGE_HEADER_SIZE((min_align_shift), (page_size), (granule_size))

#define PAS_BASIC_SEGREGATED_PAYLOAD_SIZE(min_align_shift, page_size, granule_size) \
    ((page_size) - PAS_BASIC_SEGREGATED_PAYLOAD_OFFSET((min_align_shift), (page_size), (granule_size)))

static PAS_ALWAYS_INLINE void
pas_segregated_page_config_verify_dealloc(uintptr_t begin,
                                          pas_segregated_page_config config)
{
    if (PAS_UNLIKELY(!pas_segregated_page_is_allocated(begin, config)))
        pas_deallocation_did_fail("Page bit not set", begin);
}

#define PAS_BASIC_SEGREGATED_PAGE_CONFIG_FORWARD_DECLARATIONS(name) \
    PAS_SEGREGATED_PAGE_CONFIG_SPECIALIZATION_DECLARATIONS(name ## _page_config); \
    PAS_BASIC_PAGE_BASE_CONFIG_FORWARD_DECLARATIONS(name); \
    \
    PAS_API pas_segregated_shared_page_directory* \
    name ## _page_config_select_shared_page_directory( \
        pas_segregated_heap* heap, pas_segregated_global_size_directory* directory); \
    \
    static PAS_ALWAYS_INLINE void \
    name ## _dealloc_func(pas_thread_local_cache* cache, uintptr_t begin)

typedef struct {
    pas_page_header_placement_mode header_placement_mode;
    bool size_aware_logging;
    bool verify_before_logging;
    pas_page_header_table* header_table; /* Even if we have multiple tables, this will have one,
                                            since we use this when we know which page config we
                                            are dealing with. */
} pas_basic_segregated_page_config_declarations_arguments;

#define PAS_BASIC_SEGREGATED_PAGE_CONFIG_DECLARATIONS(name, config_value, ...) \
    PAS_BASIC_PAGE_BASE_CONFIG_DECLARATIONS( \
        name, (config_value).base, \
        .header_placement_mode = \
            ((pas_basic_segregated_page_config_declarations_arguments){__VA_ARGS__}) \
            .header_placement_mode, \
        .header_table = \
            ((pas_basic_segregated_page_config_declarations_arguments){__VA_ARGS__}) \
            .header_table); \
    \
    static PAS_ALWAYS_INLINE void \
    name ## _dealloc_func(pas_thread_local_cache* cache, uintptr_t begin) \
    { \
        pas_segregated_page_config config = (config_value); \
        pas_basic_segregated_page_config_declarations_arguments arguments = \
            ((pas_basic_segregated_page_config_declarations_arguments){__VA_ARGS__}); \
        \
        PAS_ASSERT(config.base.is_enabled); \
        \
        if (arguments.verify_before_logging) { \
            if (PAS_UNLIKELY(!pas_segregated_page_is_allocated(begin, config))) \
                pas_deallocation_did_fail("Page bit not set", begin); \
        } \
        \
        if (arguments.size_aware_logging) { \
            pas_thread_local_cache_append_deallocation_with_size( \
                cache, \
                begin, \
                pas_segregated_page_get_object_size_for_address_and_page_config(begin, config), \
                config.kind); \
            return; \
        } \
        \
        pas_thread_local_cache_append_deallocation(cache, begin, config.kind); \
    } \
    \
    struct pas_dummy

PAS_END_EXTERN_C;

#endif /* PAS_SEGREGATED_PAGE_CONFIG_UTILS_H */

