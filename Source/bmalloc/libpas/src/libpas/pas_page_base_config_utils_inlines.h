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

#ifndef PAS_PAGE_BASE_CONFIG_UTILS_INLINES_H
#define PAS_PAGE_BASE_CONFIG_UTILS_INLINES_H

#include "pas_basic_heap_config_enumerator_data.h"
#include "pas_enumerator.h"
#include "pas_heap_lock.h"
#include "pas_page_base_config_utils.h"

PAS_BEGIN_EXTERN_C;

typedef struct {
    pas_page_base_config page_config;
    pas_page_header_table* header_table;
} pas_basic_page_base_config_definitions_arguments;

#define PAS_BASIC_PAGE_BASE_CONFIG_DEFINITIONS(name, ...) \
    pas_page_base* \
    name ## _page_header_for_boundary_remote(pas_enumerator* enumerator, void* boundary) \
    { \
        pas_basic_page_base_config_definitions_arguments arguments = \
            ((pas_basic_page_base_config_definitions_arguments){__VA_ARGS__}); \
        \
        PAS_ASSERT(arguments.page_config.is_enabled); \
        \
        switch (name ## _header_placement_mode) { \
        case pas_page_header_at_head_of_page: { \
            PAS_PROFILE(boundary, PAGE_HEADER); \
            return (pas_page_base*)boundary; \
        } \
        \
        case pas_page_header_in_table: { \
            pas_basic_heap_config_enumerator_data* data; \
            pas_heap_config_kind kind; \
            \
            kind = arguments.page_config.heap_config_ptr->kind; \
            PAS_ASSERT((unsigned)kind < (unsigned)pas_heap_config_kind_num_kinds); \
            data = (pas_basic_heap_config_enumerator_data*)enumerator->heap_config_datas[kind]; \
            PAS_ASSERT(data); \
            \
            return (pas_page_base*)pas_ptr_hash_map_get(&data->page_header_table, boundary).value; \
        } } \
        \
        PAS_ASSERT(!"Should not be reached"); \
        return NULL; \
    } \
    \
    pas_page_base* name ## _create_page_header( \
        void* boundary, pas_page_kind kind, pas_lock_hold_mode heap_lock_hold_mode) \
    { \
        pas_basic_page_base_config_definitions_arguments arguments = \
            ((pas_basic_page_base_config_definitions_arguments){__VA_ARGS__}); \
        \
        PAS_ASSERT(arguments.page_config.is_enabled); \
        \
        switch (name ## _header_placement_mode) { \
        case pas_page_header_at_head_of_page: { \
            return (pas_page_base*)boundary; \
        } \
        \
        case pas_page_header_in_table: { \
            pas_page_base* result; \
            pas_heap_lock_lock_conditionally(heap_lock_hold_mode); \
            result = pas_page_header_table_add( \
                arguments.header_table, \
                arguments.page_config.page_size, \
                pas_page_base_header_size(arguments.page_config.page_config_ptr, kind), \
                boundary); \
            pas_heap_lock_unlock_conditionally(heap_lock_hold_mode); \
            return result; \
        } } \
        \
        PAS_ASSERT(!"Should not be reached"); \
        return NULL; \
    } \
    \
    void name ## _destroy_page_header( \
        pas_page_base* page, pas_lock_hold_mode heap_lock_hold_mode) \
    { \
        pas_basic_page_base_config_definitions_arguments arguments = \
            ((pas_basic_page_base_config_definitions_arguments){__VA_ARGS__}); \
        \
        PAS_ASSERT(arguments.page_config.is_enabled); \
        \
        switch (name ## _header_placement_mode) { \
        case pas_page_header_at_head_of_page: \
            return; \
        \
        case pas_page_header_in_table: \
            pas_heap_lock_lock_conditionally(heap_lock_hold_mode); \
            pas_page_header_table_remove(arguments.header_table, \
                                         arguments.page_config.page_size, \
                                         page); \
            pas_heap_lock_unlock_conditionally(heap_lock_hold_mode); \
            return; \
        } \
        \
        PAS_ASSERT(!"Should not be reached"); \
        return; \
    } \
    \
    struct pas_dummy

PAS_END_EXTERN_C;

#endif /* PAS_PAGE_BASE_CONFIG_UTILS_INLINES_H */

