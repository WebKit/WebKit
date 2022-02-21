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

#ifndef PAS_BASIC_HEAP_PAGE_CACHES_H
#define PAS_BASIC_HEAP_PAGE_CACHES_H

#include "pas_large_heap_physical_page_sharing_cache.h"
#include "pas_megapage_cache.h"
#include "pas_segregated_page_config_variant.h"
#include "pas_shared_page_directory_by_size.h"

PAS_BEGIN_EXTERN_C;

struct pas_basic_heap_page_caches;
typedef struct pas_basic_heap_page_caches pas_basic_heap_page_caches;

struct pas_basic_heap_page_caches {
    pas_large_heap_physical_page_sharing_cache large_heap_cache;
    pas_megapage_cache small_exclusive_segregated_megapage_cache;
    pas_shared_page_directory_by_size small_shared_page_directories;
    pas_megapage_cache small_other_megapage_cache;
    pas_megapage_cache medium_megapage_cache; /* The purpose of this is not for the fast megapage
                                                 table, but to make sure that medium pages are not
                                                 allocated one-at-a-time from bootstrap, since that
                                                 would create unnecessary fragmentation in the large
                                                 heap. */
    pas_shared_page_directory_by_size medium_shared_page_directories;
};

#define PAS_BASIC_HEAP_PAGE_CACHES_INITIALIZER(small_log_shift, medium_log_shift) \
    ((pas_basic_heap_page_caches){ \
        .large_heap_cache = PAS_LARGE_FREE_HEAP_PHYSICAL_PAGE_SHARING_CACHE_INITIALIZER, \
        .small_exclusive_segregated_megapage_cache = PAS_MEGAPAGE_CACHE_INITIALIZER, \
        .small_shared_page_directories = PAS_SHARED_PAGE_DIRECTORY_BY_SIZE_INITIALIZER( \
            (small_log_shift), pas_share_pages), \
        .small_other_megapage_cache = PAS_MEGAPAGE_CACHE_INITIALIZER, \
        .medium_megapage_cache = PAS_MEGAPAGE_CACHE_INITIALIZER, \
        .medium_shared_page_directories = PAS_SHARED_PAGE_DIRECTORY_BY_SIZE_INITIALIZER( \
            (medium_log_shift), pas_share_pages) \
    })

static inline pas_shared_page_directory_by_size*
pas_basic_heap_page_caches_get_shared_page_directories(
    pas_basic_heap_page_caches* caches,
    pas_segregated_page_config_variant variant)
{
    switch (variant) {
    case pas_medium_segregated_page_config_variant:
        return &caches->medium_shared_page_directories;
    case pas_small_segregated_page_config_variant:
        return &caches->small_shared_page_directories;
    }
    PAS_ASSERT(!"Should not be reached");
    return NULL;
}

PAS_END_EXTERN_C;

#endif /* PAS_BASIC_HEAP_PAGE_CACHES_H */

