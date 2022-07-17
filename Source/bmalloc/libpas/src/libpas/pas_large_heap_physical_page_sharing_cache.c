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

#include "pas_config.h"

#if LIBPAS_ENABLED

#include "pas_large_heap_physical_page_sharing_cache.h"

#include "pas_bootstrap_free_heap.h"
#include "pas_heap_config.h"
#include "pas_heap_lock.h"
#include "pas_large_sharing_pool.h"
#include "pas_page_malloc.h"
#include "pas_page_sharing_pool.h"
#include <stdio.h>

pas_enumerable_range_list pas_large_heap_physical_page_sharing_cache_page_list;

typedef struct {
    pas_large_heap_physical_page_sharing_cache* cache;
    const pas_heap_config* config;
    bool should_zero;
} aligned_allocator_data;

static pas_aligned_allocation_result large_aligned_allocator(size_t size,
                                                             pas_alignment alignment,
                                                             void* arg)
{
    static const bool verbose = false;

    aligned_allocator_data* data;
    size_t page_size;
    size_t aligned_size;
    pas_alignment aligned_alignment;
    pas_allocation_result allocation_result;
    pas_aligned_allocation_result result;
    
    pas_heap_lock_assert_held();
    
    data = arg;
    
    page_size = pas_page_malloc_alignment();
    
    aligned_size = pas_round_up_to_power_of_2(size, page_size);
    aligned_alignment = pas_alignment_round_up(alignment, page_size);
    
    pas_zero_memory(&result, sizeof(result));

    /* We need to take some number of physical bytes to balance this allocation. But we don't want
       to do that until we actually finish servicing this allocation, since the memory we would
       decommit right now might be the memory we are about to use to satisfy the memory allocation
       request.
       
       Because of the cleverness of the large free heaps, the memory we return here may be first
       coalesced with some already-free memory and then returned. So, of we decommit right now, we
       might decommit that memory. That would not be wrong, but it would be a waste of time, since
       we will recommit it.
       
       Instead we rely on two properties:
       
       - Incurring a negative physical page balance here ensures that once we have the memory
         allocation in hand, we will take the right number of physical pages. Since at that point
         we would have already marked the memory we allocated as committed, we will be sure not to
         take that memory.
       
       - It's OK to decommit after allocating in this case. Normally we ensure that we schedule the
         commit to happen after the decommit, because we're All About keeping peak memory low. But
         here, the memory we have gotten from the OS is still clean, so it doesn't count against our
         peak dirty. */
    if (verbose)
        printf("Taking %zu later.\n", aligned_size);
    pas_physical_page_sharing_pool_take_later(aligned_size);
    
    allocation_result = data->cache->provider(
        aligned_size, aligned_alignment,
        "pas_large_heap_physical_page_sharing_cache/chunk",
        NULL, NULL,
        data->cache->provider_arg);

    if (!allocation_result.did_succeed) {
        pas_physical_page_sharing_pool_give_back(aligned_size);
        return result;
    }

    if (data->should_zero) {
        if (verbose) {
            pas_log("Making sure that allocation at %p...%p is zero.\n",
                    (void*)allocation_result.begin,
                    (char*)allocation_result.begin + aligned_size);
        }
        
        allocation_result = pas_allocation_result_zero(allocation_result, aligned_size);
    }

    pas_enumerable_range_list_append(
        &pas_large_heap_physical_page_sharing_cache_page_list,
        pas_range_create(allocation_result.begin, allocation_result.begin + aligned_size));

    if (verbose) {
        pas_log("Large cache allocated %p...%p\n",
                (void*)allocation_result.begin,
                (void*)((uintptr_t)allocation_result.begin + aligned_size));
    }

    /* We are likely to introduce more free memory into this heap than the heap can immediately use.
       But we don't want that memory to then immediately get decommitted, since we believe that this
       memory is likely to be used for the very next allocation our of this heap.
       
       So we use boot_free, which frees while bumping the epoch. */
    pas_large_sharing_pool_boot_free(
        pas_range_create(allocation_result.begin, allocation_result.begin + aligned_size),
        pas_physical_memory_is_locked_by_virtual_range_common_lock,
        data->config->mmap_capability);
    
    result.result = (void*)allocation_result.begin;
    result.result_size = size;
    result.left_padding = (void*)allocation_result.begin;
    result.left_padding_size = 0;
    result.right_padding = (char*)(void*)allocation_result.begin + size;
    result.right_padding_size = aligned_size - size;
    result.zero_mode = allocation_result.zero_mode;
    
    return result;
}

void pas_large_heap_physical_page_sharing_cache_construct(
    pas_large_heap_physical_page_sharing_cache* cache,
    pas_heap_page_provider provider,
    void* provider_arg)
{
    pas_simple_large_free_heap_construct(&cache->free_heap);
    cache->provider = provider;
    cache->provider_arg = provider_arg;
}

pas_allocation_result
pas_large_heap_physical_page_sharing_cache_try_allocate_with_alignment(
    pas_large_heap_physical_page_sharing_cache* cache,
    size_t size,
    pas_alignment alignment,
    const pas_heap_config* heap_config,
    bool should_zero)
{
    static const bool verbose = false;
    
    aligned_allocator_data data;
    pas_large_free_heap_config config;
    
    data.cache = cache;
    data.config = heap_config;
    data.should_zero = should_zero;
    
    config.type_size = 1;
    config.min_alignment = 1;
    config.aligned_allocator = large_aligned_allocator;
    config.aligned_allocator_arg = &data;
    config.deallocator = NULL;
    config.deallocator_arg = NULL;

    if (verbose) {
        pas_log("Allocating large heap physical cache out of simple free heap %p\n",
                &cache->free_heap);
    }
    
    return pas_simple_large_free_heap_try_allocate(&cache->free_heap, size, alignment, &config);
}

#endif /* LIBPAS_ENABLED */
