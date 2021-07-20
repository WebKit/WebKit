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

#ifndef PAS_SEGREGATED_PAGE_CONFIG_UTILS_INLINES_H
#define PAS_SEGREGATED_PAGE_CONFIG_UTILS_INLINES_H

#include "pas_config.h"
#include "pas_heap.h"
#include "pas_page_base_config_utils_inlines.h"
#include "pas_segregated_page_config_inlines.h"
#include "pas_segregated_page_config_utils.h"
#include "pas_fast_megapage_cache.h"

PAS_BEGIN_EXTERN_C;

typedef struct {
    pas_segregated_page_config page_config;
    pas_page_header_table* header_table;
} pas_basic_segregated_page_config_definitions_arguments;

#define PAS_BASIC_SEGREGATED_PAGE_CONFIG_DEFINITIONS(name, ...) \
    PAS_SEGREGATED_PAGE_CONFIG_SPECIALIZATION_DEFINITIONS( \
        name ## _page_config, \
        ((pas_basic_segregated_page_config_definitions_arguments){__VA_ARGS__}).page_config); \
    \
    PAS_BASIC_PAGE_BASE_CONFIG_DEFINITIONS( \
        name, \
        ((pas_basic_segregated_page_config_definitions_arguments){__VA_ARGS__}).page_config.base, \
        ((pas_basic_segregated_page_config_definitions_arguments){__VA_ARGS__}).header_table); \
    \
    pas_segregated_shared_page_directory* \
    name ## _page_config_select_shared_page_directory( \
        pas_segregated_heap* heap, pas_segregated_global_size_directory* size_directory) \
    { \
        PAS_UNUSED_PARAM(size_directory); \
        \
        pas_basic_segregated_page_config_definitions_arguments arguments = \
            (pas_basic_segregated_page_config_definitions_arguments){__VA_ARGS__}; \
        \
        pas_shared_page_directory_by_size* directory_by_size; \
        pas_segregated_shared_page_directory* directory; \
        \
        PAS_ASSERT(arguments.page_config.base.is_enabled); \
        \
        directory_by_size = pas_basic_heap_page_caches_get_shared_page_directories( \
            ((pas_basic_heap_runtime_config*)heap->runtime_config)->page_caches, \
            arguments.page_config.variant); \
        \
        directory = pas_shared_page_directory_by_size_get( \
            directory_by_size, size_directory->object_size, \
            (pas_segregated_page_config*)arguments.page_config.base.page_config_ptr); \
        \
        return directory; \
    } \
    \
    struct pas_dummy

PAS_END_EXTERN_C;

#endif /* PAS_SEGREGATED_PAGE_CONFIG_UTILS_INLINES_H */

