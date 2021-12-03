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

#ifndef PAS_SEGREGATED_PAGE_CONFIG_INLINES_H
#define PAS_SEGREGATED_PAGE_CONFIG_INLINES_H

#include "pas_config.h"
#include "pas_local_allocator_inlines.h"
#include "pas_segregated_page_config.h"

PAS_BEGIN_EXTERN_C;

#define PAS_SEGREGATED_PAGE_CONFIG_TLC_SPECIALIZATION_DEFINITIONS(lower_case_page_config_name, page_config_value) \
    PAS_NEVER_INLINE pas_allocation_result \
    lower_case_page_config_name ## _specialized_local_allocator_try_allocate_in_primordial_partial_view( \
        pas_local_allocator* allocator) \
    { \
        return pas_local_allocator_try_allocate_in_primordial_partial_view( \
            allocator, (page_config_value)); \
    } \
    \
    PAS_NEVER_INLINE bool lower_case_page_config_name ## _specialized_local_allocator_start_allocating_in_primordial_partial_view( \
        pas_local_allocator* allocator, \
        pas_segregated_partial_view* partial, \
        pas_segregated_size_directory* size_directory) \
    { \
        return pas_local_allocator_start_allocating_in_primordial_partial_view( \
            allocator, partial, size_directory, (page_config_value)); \
    } \
    \
    PAS_NEVER_INLINE bool lower_case_page_config_name ## _specialized_local_allocator_refill( \
        pas_local_allocator* allocator, \
        pas_allocator_counts* counts) \
    { \
        return pas_local_allocator_refill_with_known_config(allocator, counts, (page_config_value)); \
    } \
    \
    void lower_case_page_config_name ## _specialized_local_allocator_return_memory_to_page( \
        pas_local_allocator* allocator, \
        pas_segregated_view view, \
        pas_segregated_page* page, \
        pas_segregated_size_directory* directory, \
        pas_lock_hold_mode heap_lock_hold_mode) \
    { \
        pas_local_allocator_return_memory_to_page( \
            allocator, view, page, directory, heap_lock_hold_mode, (page_config_value)); \
    } \
    \
    struct pas_dummy

#define PAS_SEGREGATED_PAGE_CONFIG_SPECIALIZATION_DEFINITIONS(lower_case_page_config_name, page_config_value) \
    PAS_SEGREGATED_PAGE_CONFIG_TLC_SPECIALIZATION_DEFINITIONS(lower_case_page_config_name, page_config_value)

PAS_END_EXTERN_C;

#endif /* PAS_SEGREGATD_PAGE_CONFIG_INLINES_H */

