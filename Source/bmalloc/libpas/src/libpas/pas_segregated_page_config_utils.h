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

#define PAS_BASIC_SEGREGATED_PAGE_HEADER_SIZE_EXCLUSIVE(min_align_shift, page_size, granule_size) \
    PAS_SEGREGATED_PAGE_HEADER_SIZE( \
        PAS_BASIC_SEGREGATED_NUM_ALLOC_BITS((min_align_shift), (page_size)), \
        (page_size) / (granule_size))

#define PAS_BASIC_SEGREGATED_PAYLOAD_OFFSET_EXCLUSIVE(min_align_shift, page_size, granule_size) \
    PAS_BASIC_SEGREGATED_PAGE_HEADER_SIZE_EXCLUSIVE((min_align_shift), (page_size), (granule_size))

#define PAS_BASIC_SEGREGATED_PAYLOAD_SIZE_EXCLUSIVE(min_align_shift, page_size, granule_size) \
    ((page_size) - PAS_BASIC_SEGREGATED_PAYLOAD_OFFSET_EXCLUSIVE( \
         (min_align_shift), (page_size), (granule_size)))

#define PAS_BASIC_SEGREGATED_PAGE_HEADER_SIZE_SHARED(min_align_shift, page_size, granule_size) \
    PAS_SEGREGATED_PAGE_HEADER_SIZE( \
        PAS_BASIC_SEGREGATED_NUM_ALLOC_BITS((min_align_shift), (page_size)), \
        (page_size) / (granule_size))

#define PAS_BASIC_SEGREGATED_PAYLOAD_OFFSET_SHARED(min_align_shift, page_size, granule_size) \
    PAS_BASIC_SEGREGATED_PAGE_HEADER_SIZE_SHARED((min_align_shift), (page_size), (granule_size))

#define PAS_BASIC_SEGREGATED_PAYLOAD_SIZE_SHARED(min_align_shift, page_size, granule_size) \
    ((page_size) - PAS_BASIC_SEGREGATED_PAYLOAD_OFFSET_SHARED((min_align_shift), (page_size), (granule_size)))

#define PAS_BASIC_SEGREGATED_PAGE_CONFIG_FORWARD_DECLARATIONS(name) \
    PAS_SEGREGATED_PAGE_CONFIG_SPECIALIZATION_DECLARATIONS(name ## _page_config); \
    PAS_BASIC_PAGE_BASE_CONFIG_FORWARD_DECLARATIONS(name); \
    \
    PAS_API pas_segregated_shared_page_directory* \
    name ## _page_config_select_shared_page_directory( \
        pas_segregated_heap* heap, pas_segregated_size_directory* directory); \

typedef struct {
    pas_page_header_placement_mode header_placement_mode;
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
    struct pas_dummy

PAS_END_EXTERN_C;

#endif /* PAS_SEGREGATED_PAGE_CONFIG_UTILS_H */

