/*
 * Copyright (c) 2019-2021 Apple Inc. All rights reserved.
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

#ifndef PAS_GET_OBJECT_KIND_H
#define PAS_GET_OBJECT_KIND_H

#include "pas_get_page_base_and_kind_for_small_other_in_fast_megapage.h"
#include "pas_heap_config.h"
#include "pas_object_kind.h"
#include "pas_page_base.h"
#include "pas_large_map.h"

PAS_BEGIN_EXTERN_C;

static PAS_ALWAYS_INLINE pas_object_kind pas_get_object_kind(void* ptr,
                                                             pas_heap_config config)
{
    uintptr_t begin;
    pas_page_kind page_kind;
    
    begin = (uintptr_t)ptr;
    
    switch (config.fast_megapage_kind_func(begin)) {
    case pas_small_exclusive_segregated_fast_megapage_kind:
        return pas_small_segregated_object_kind;
    case pas_small_other_fast_megapage_kind:
        page_kind = pas_get_page_base_and_kind_for_small_other_in_fast_megapage(begin, config).page_kind;
        goto use_page_kind;
    case pas_not_a_fast_megapage_kind: {
        pas_large_map_entry entry;
        pas_page_base* page_base;

        page_base = config.page_header_func(begin);
        if (page_base) {
            page_kind = pas_page_base_get_kind(page_base);
            goto use_page_kind;
        }
        
        pas_heap_lock_lock();
        
        entry = pas_large_map_find(begin);
        
        pas_heap_lock_unlock();
        
        if (pas_large_map_entry_is_empty(entry))
            return pas_not_an_object_kind;
        return pas_large_object_kind;
    } }
    
    PAS_ASSERT(!"Should not be reached");
    return pas_not_an_object_kind;

use_page_kind:
    return pas_object_kind_for_page_kind(page_kind);
}

PAS_END_EXTERN_C;

#endif /* PAS_GET_OBJECT_KIND */

