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

#ifndef PAS_LARGE_HEAP_PHYSICAL_PAGE_SHARING_CACHE_H
#define PAS_LARGE_HEAP_PHYSICAL_PAGE_SHARING_CACHE_H

#include "pas_bootstrap_heap_page_provider.h"
#include "pas_commit_mode.h"
#include "pas_enumerable_range_list.h"
#include "pas_simple_large_free_heap.h"
#include "pas_utils.h"

PAS_BEGIN_EXTERN_C;

struct pas_heap_config;
struct pas_large_heap_physical_page_sharing_cache;
typedef struct pas_heap_config pas_heap_config;
typedef struct pas_large_heap_physical_page_sharing_cache pas_large_heap_physical_page_sharing_cache;

struct pas_large_heap_physical_page_sharing_cache {
    pas_simple_large_free_heap free_heap;
    pas_heap_page_provider provider;
    void* provider_arg;
    /* Commit state of memory that the provider hands back. pas_committed means the provider
       returns memory that is already physically committed (the common case — e.g. providers
       backed by the standard page allocator). pas_decommitted means the provider returns
       memory that is reserved but not yet physically committed (e.g. a provider that hands
       out chunks of a client-supplied reserved region). The cache passes this through to
       pas_large_sharing_pool_boot_free as the initial commit mode when registering a new
       chunk. */
    pas_commit_mode provider_commit_mode;
};

#define PAS_MEGAPAGE_LARGE_FREE_HEAP_PHYSICAL_PAGE_SHARING_CACHE_INITIALIZER \
    ((pas_large_heap_physical_page_sharing_cache){ \
         .free_heap = PAS_SIMPLE_LARGE_FREE_HEAP_INITIALIZER, \
         .provider = pas_small_medium_bootstrap_heap_page_provider, \
         .provider_arg = NULL, \
         .provider_commit_mode = pas_committed \
     })

#define PAS_LARGE_FREE_HEAP_PHYSICAL_PAGE_SHARING_CACHE_INITIALIZER \
    ((pas_large_heap_physical_page_sharing_cache){ \
         .free_heap = PAS_SIMPLE_LARGE_FREE_HEAP_INITIALIZER, \
         .provider = pas_bootstrap_heap_page_provider, \
         .provider_arg = NULL, \
         .provider_commit_mode = pas_committed \
     })

PAS_API extern pas_enumerable_range_list pas_large_heap_physical_page_sharing_cache_page_list;

PAS_API void
pas_large_heap_physical_page_sharing_cache_construct(
    pas_large_heap_physical_page_sharing_cache* cache,
    pas_heap_page_provider provider,
    void* provider_arg,
    pas_commit_mode provider_commit_mode);

/* NOTE: should_zero should have a consistent value for all calls to try_allocate for a given
   cache. */
PAS_API pas_allocation_result
pas_large_heap_physical_page_sharing_cache_try_allocate_with_alignment(
    pas_large_heap_physical_page_sharing_cache* cache,
    size_t size,
    pas_alignment alignment,
    const pas_heap_config* config,
    bool should_zero);

PAS_END_EXTERN_C;

#endif /* PAS_LARGE_HEAP_PHYSICAL_PAGE_SHARING_CACHE_H */

