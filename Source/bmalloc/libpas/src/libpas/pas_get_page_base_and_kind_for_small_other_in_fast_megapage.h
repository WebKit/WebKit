/*
 * Copyright (c) 2021 Apple Inc. All rights reserved.
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

#ifndef PAS_GET_PAGE_BASE_AND_KIND_FOR_SMALL_OTHER_IN_FAST_MEGAPAGE_H
#define PAS_GET_PAGE_BASE_AND_KIND_FOR_SMALL_OTHER_IN_FAST_MEGAPAGE_H

#include "pas_heap_config.h"
#include "pas_page_base_and_kind.h"

PAS_BEGIN_EXTERN_C;

static PAS_ALWAYS_INLINE pas_page_base_and_kind
pas_get_page_base_and_kind_for_small_other_in_fast_megapage(uintptr_t begin, pas_heap_config config)
{
    if (config.small_bitfit_config.base.is_enabled
        && config.small_bitfit_is_in_megapage) {
        pas_page_base* page_base;
        page_base = pas_page_base_for_address_and_page_config(begin, config.small_bitfit_config.base);
        if (config.small_segregated_config.base.is_enabled
            && config.small_segregated_is_in_megapage) {
            PAS_ASSERT(config.small_bitfit_config.base.page_size
                       == config.small_segregated_config.base.page_size);
            PAS_ASSERT(
                pas_page_base_for_address_and_page_config(begin, config.small_segregated_config.base)
                == page_base);
            return pas_page_base_and_kind_create(page_base, pas_page_base_get_kind(page_base));
        }
        return pas_page_base_and_kind_create(page_base, pas_small_bitfit_page_kind);
    }

    /* We shouldn't get here unless we think that either small bitfit or small segregated is in megapage.
       So, if small bitfit isn't, then small segregated must be. */
    PAS_ASSERT(config.small_segregated_config.base.is_enabled
               && config.small_segregated_is_in_megapage);
    
    return pas_page_base_and_kind_create(
        pas_page_base_for_address_and_page_config(begin, config.small_segregated_config.base),
        pas_small_shared_segregated_page_kind);
}

PAS_END_EXTERN_C;

#endif /* PAS_GET_PAGE_BASE_AND_KIND_FOR_SMALL_OTHER_IN_FAST_MEGAPAGE_H */

