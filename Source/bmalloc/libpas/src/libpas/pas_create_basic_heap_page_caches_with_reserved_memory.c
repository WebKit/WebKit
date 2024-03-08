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

#include "pas_config.h"

#if LIBPAS_ENABLED

#include "pas_create_basic_heap_page_caches_with_reserved_memory.h"

#include "pas_basic_heap_page_caches.h"
#include "pas_basic_heap_runtime_config.h"
#include "pas_immortal_heap.h"
#include "pas_large_heap_physical_page_sharing_cache.h"
#include "pas_megapage_cache.h"
#include "pas_reserved_memory_provider.h"
#include "pas_segregated_shared_page_directory.h"

static pas_allocation_result allocate_from_large(
    size_t size,
    pas_alignment alignment,
    const char* name,
    pas_heap* heap,
    pas_physical_memory_transaction* transaction,
    void* arg)
{
    PAS_UNUSED_PARAM(name);
    PAS_ASSERT(heap);
    PAS_ASSERT(transaction);
    PAS_ASSERT(!arg);
    PAS_ASSERT(!alignment.alignment_begin);

    return pas_large_heap_try_allocate_and_forget(
        &heap->large_heap, size, alignment.alignment, pas_non_compact_allocation_mode,
        pas_heap_config_kind_get_config(heap->config_kind),
        transaction);
}

/* Warning: This creates caches that allow type confusion. Only use this for primitive heaps! */
pas_basic_heap_page_caches* pas_create_basic_heap_page_caches_with_reserved_memory(
    pas_basic_heap_runtime_config* template_runtime_config,
    uintptr_t begin,
    uintptr_t end)
{
    pas_reserved_memory_provider* provider;
    pas_basic_heap_page_caches* caches;
    pas_segregated_page_config_variant segregated_variant;

    pas_heap_lock_lock();

    provider = pas_immortal_heap_allocate(
        sizeof(pas_reserved_memory_provider),
        "pas_reserved_memory_provider",
        pas_object_allocation);

    pas_reserved_memory_provider_construct(provider, begin, end);

    caches = pas_immortal_heap_allocate(
        sizeof(pas_basic_heap_page_caches),
        "pas_basic_heap_page_caches",
        pas_object_allocation);

    pas_large_heap_physical_page_sharing_cache_construct(
        &caches->large_heap_cache,
        pas_reserved_memory_provider_try_allocate,
        provider);
    
    pas_megapage_cache_construct(
        &caches->small_exclusive_segregated_megapage_cache,
        allocate_from_large,
        NULL);

    pas_megapage_cache_construct(
        &caches->small_other_megapage_cache,
        allocate_from_large,
        NULL);

    pas_megapage_cache_construct(
        &caches->medium_megapage_cache,
        allocate_from_large,
        NULL);

    for (PAS_EACH_SEGREGATED_PAGE_CONFIG_VARIANT_ASCENDING(segregated_variant)) {
        pas_shared_page_directory_by_size* directories;

        directories = pas_basic_heap_page_caches_get_shared_page_directories(caches,
                                                                             segregated_variant);

        *directories = PAS_SHARED_PAGE_DIRECTORY_BY_SIZE_INITIALIZER(
            pas_basic_heap_page_caches_get_shared_page_directories(
                template_runtime_config->page_caches,
                segregated_variant)->log_shift,
            pas_share_pages);
    }

    pas_heap_lock_unlock();

    return caches;
}

#endif /* LIBPAS_ENABLED */
