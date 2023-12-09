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

#ifndef PAS_PAGE_BASE_CONFIG_UTILS_H
#define PAS_PAGE_BASE_CONFIG_UTILS_H

#include "pas_config.h"
#include "pas_internal_config.h"
#include "pas_page_base_config.h"
#include "pas_page_header_placement_mode.h"
#include "pas_page_header_table.h"

PAS_BEGIN_EXTERN_C;

#define PAS_BASIC_PAGE_BASE_CONFIG_FORWARD_DECLARATIONS(name) \
    static PAS_ALWAYS_INLINE pas_page_base* \
    name ## _page_header_for_boundary(void* boundary); \
    static PAS_ALWAYS_INLINE void* \
    name ## _boundary_for_page_header(pas_page_base* page); \
    PAS_API pas_page_base* \
    name ## _page_header_for_boundary_remote(pas_enumerator* enumerator, void* boundary); \
    \
    PAS_API pas_page_base* name ## _create_page_header( \
        void* boundary, pas_page_kind kind, pas_lock_hold_mode heap_lock_hold_mode); \
    PAS_API void name ## _destroy_page_header( \
        pas_page_base* page, pas_lock_hold_mode heap_lock_hold_mode)

typedef struct {
    pas_page_header_placement_mode header_placement_mode;
    pas_page_header_table* header_table; /* Even if we have multiple tables, this will have one,
                                            since we use this when we know which page config we
                                            are dealing with. */
} pas_basic_page_base_config_declarations_arguments;

#define PAS_BASIC_PAGE_BASE_CONFIG_DECLARATIONS(name, config_value, header_placement_mode_value, header_table_value) \
    static const pas_page_header_placement_mode name ## _header_placement_mode = (header_placement_mode_value); \
    static PAS_ALWAYS_INLINE pas_page_base* \
    name ## _page_header_for_boundary(void* boundary) \
    { \
        pas_basic_page_base_config_declarations_arguments arguments = { .header_placement_mode = (header_placement_mode_value), .header_table = (header_table_value) }; \
        pas_page_base_config config; \
        \
        config = (config_value); \
        PAS_ASSERT(config.is_enabled); \
        \
        switch (arguments.header_placement_mode) { \
        case pas_page_header_at_head_of_page: { \
            PAS_PROFILE(boundary, PAGE_HEADER); \
            return (pas_page_base*)boundary; \
        } \
        \
        case pas_page_header_in_table: { \
            pas_page_base* page_base; \
            \
            page_base = pas_page_header_table_get_for_boundary( \
                arguments.header_table, config.page_size, boundary); \
            PAS_TESTING_ASSERT(page_base); \
            return page_base; \
        } } \
        \
        PAS_ASSERT(!"Should not be reached"); \
        return NULL; \
    } \
    \
    static PAS_ALWAYS_INLINE void* \
    name ## _boundary_for_page_header(pas_page_base* page) \
    { \
        pas_basic_page_base_config_declarations_arguments arguments = { .header_placement_mode = (header_placement_mode_value), .header_table = (header_table_value) }; \
        pas_page_base_config config; \
        \
        config = (config_value); \
        PAS_ASSERT(config.is_enabled); \
        \
        switch (name ## _header_placement_mode) { \
        case pas_page_header_at_head_of_page: { \
            return page; \
        } \
        \
        case pas_page_header_in_table: { \
            void* boundary; \
            \
            boundary = pas_page_header_table_get_boundary( \
                arguments.header_table, config.page_size, page); \
            PAS_TESTING_ASSERT(boundary); \
            return boundary; \
        } } \
        \
        PAS_ASSERT(!"Should not be reached"); \
        return NULL; \
    } \
    \
    struct pas_dummy

    PAS_END_EXTERN_C;

#endif /* PAS_PAGE_BASE_CONFIG_UTILS_H */

