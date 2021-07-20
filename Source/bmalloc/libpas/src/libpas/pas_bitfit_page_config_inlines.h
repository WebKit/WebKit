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

#ifndef PAS_BITFIT_PAGE_CONFIG_INLINES_H
#define PAS_BITFIT_PAGE_CONFIG_INLINES_H

#include "pas_bitfit_allocator_inlines.h"
#include "pas_bitfit_page_config.h"
#include "pas_bitfit_page_inlines.h"

PAS_BEGIN_EXTERN_C;

#define PAS_BITFIT_PAGE_CONFIG_SPECIALIZATION_DEFINITIONS(lower_case_page_config_name, page_config_value) \
    pas_fast_path_allocation_result \
    lower_case_page_config_name ## _specialized_allocator_try_allocate( \
        pas_bitfit_allocator* allocator, \
        pas_local_allocator* local, \
        size_t size, \
        size_t alignment) \
    { \
        return pas_bitfit_allocator_try_allocate( \
            allocator, local, size, alignment, (page_config_value)); \
    } \
    \
    void lower_case_page_config_name ## _specialized_page_deallocate_with_page( \
        pas_bitfit_page* page, uintptr_t begin) \
    { \
        pas_bitfit_page_deallocate_with_page(page, begin, (page_config_value)); \
    } \
    \
    size_t lower_case_page_config_name ## _specialized_page_get_allocation_size_with_page( \
        pas_bitfit_page* page, uintptr_t begin) \
    { \
        return pas_bitfit_page_get_allocation_size_with_page(page, begin, (page_config_value)); \
    } \
    \
    void lower_case_page_config_name ## _specialized_page_shrink_with_page( \
        pas_bitfit_page* page, uintptr_t begin, size_t new_size) \
    { \
        return pas_bitfit_page_shrink_with_page(page, begin, new_size, (page_config_value)); \
    } \
    \
    struct pas_dummy

PAS_END_EXTERN_C;

#endif /* PAS_BITFIT_PAGE_CONFIG_INLINES_H */

