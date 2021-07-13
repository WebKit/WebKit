/*
 * Copyright (c) 2020 Apple Inc. All rights reserved.
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

#ifndef PAS_BASIC_SEGREGATED_PAGE_CACHES_H
#define PAS_BASIC_SEGREGATED_PAGE_CACHES_H

#include "pas_megapage_cache.h"
#include "pas_segregated_shared_page_directory.h"

PAS_BEGIN_EXTERN_C;

struct pas_basic_segregated_page_caches;
typedef struct pas_basic_segregated_page_caches pas_basic_segregated_page_caches;

struct pas_basic_segregated_page_caches {
    pas_megapage_cache megapage_cache;
    pas_segregated_shared_page_directory shared_page_directory[PAS_HEAP_SURVIVAL_MODE_NUM_INDICES];
};

#define PAS_BASIC_SEGREGATED_PAGE_CACHES_INITIALIZER(page_config) \
    ((pas_basic_segregated_page_caches){ \
        .megapage_cache = PAS_MEGAPAGE_CACHE_INITIALIZER, \
        .shared_page_directory = { \
            [0 ... PAS_HEAP_SURVIVAL_MODE_NUM_INDICES - 1] = \
                PAS_SEGREGATED_SHARED_PAGE_DIRECTORY_INITIALIZER( \
                    (page_config), \
                    pas_segregated_heap_share_pages_physically) \
        } \
    })

PAS_END_EXTERN_C;

#endif /* PAS_BASIC_SEGREGATED_PAGE_CACHES_H */

