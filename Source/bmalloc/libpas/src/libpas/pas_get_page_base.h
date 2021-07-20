/*
 * Copyright (c) 2019-2020 Apple Inc. All rights reserved.
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

#ifndef PAS_GET_PAGE_BASE_H
#define PAS_GET_PAGE_BASE_H

#include "pas_heap_config.h"
#include "pas_page_base.h"
#include "pas_large_map.h"

PAS_BEGIN_EXTERN_C;

static PAS_ALWAYS_INLINE pas_page_base* pas_get_page_base(void* ptr,
                                                          pas_heap_config config)
{
    uintptr_t begin;
    
    begin = (uintptr_t)ptr;
    
    switch (config.fast_megapage_kind_func(begin)) {
    case pas_small_segregated_fast_megapage_kind:
        return pas_page_base_for_address_and_page_config(begin, config.small_segregated_config.base);
    case pas_small_bitfit_fast_megapage_kind:
        return pas_page_base_for_address_and_page_config(begin, config.small_bitfit_config.base);
    case pas_not_a_fast_megapage_kind: {
        pas_page_base* page_base;

        page_base = config.page_header_func(begin);
        if (page_base)
            return page_base;

        return NULL;
    } }
    
    PAS_ASSERT(!"Should not be reached");
    return NULL;
}

PAS_END_EXTERN_C;

#endif /* PAS_GET_PAGE_BASE */

