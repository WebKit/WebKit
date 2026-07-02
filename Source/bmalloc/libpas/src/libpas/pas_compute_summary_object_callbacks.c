/*
 * Copyright (c) 2019 Apple Inc. All rights reserved.
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

#include "pas_compute_summary_object_callbacks.h"

#include "pas_heap_lock.h"
#include "pas_heap_summary.h"
#include "pas_large_sharing_pool.h"

bool pas_compute_summary_live_object_callback(uintptr_t begin,
                                              uintptr_t end,
                                              void* arg)
{
    pas_heap_summary* summary_ptr;
    pas_heap_summary my_summary;
    
    pas_heap_lock_assert_held();
    
    summary_ptr = arg;
    
    my_summary = pas_large_sharing_pool_compute_summary(
        pas_range_create(begin, end),
        pas_large_sharing_pool_compute_summary_known_allocated,
        pas_lock_is_held);
    PAS_ASSERT(!my_summary.free);
    PAS_ASSERT(my_summary.allocated == end - begin);
    
    *summary_ptr = pas_heap_summary_add(*summary_ptr, my_summary);
    
    return true;
}

bool pas_compute_summary_live_object_callback_without_physical_sharing(uintptr_t begin,
                                                                       uintptr_t end,
                                                                       void* arg)
{
    pas_heap_summary* summary_ptr;
    pas_heap_summary my_summary;
    
    pas_heap_lock_assert_held();
    
    summary_ptr = arg;
    
    my_summary = pas_heap_summary_create_empty();
    my_summary.allocated = end - begin;
    my_summary.committed = end - begin;
    
    *summary_ptr = pas_heap_summary_add(*summary_ptr, my_summary);
    
    return true;
}

bool (*pas_compute_summary_live_object_callback_for_config(const pas_heap_config* config))(
    uintptr_t begin,
    uintptr_t end,
    void* arg)
{
    if (config->aligned_allocator_talks_to_sharing_pool)
        return pas_compute_summary_live_object_callback;
    return pas_compute_summary_live_object_callback_without_physical_sharing;
}

bool pas_compute_summary_dead_object_callback(pas_large_free free,
                                              void* arg)
{
    pas_heap_summary* summary_ptr;
    pas_heap_summary my_summary;

    pas_heap_lock_assert_held();
    
    summary_ptr = arg;
    
    my_summary = pas_large_sharing_pool_compute_summary(
        pas_range_create(free.begin, free.end),
        pas_large_sharing_pool_compute_summary_known_free,
        pas_lock_is_held);
    PAS_ASSERT(!my_summary.allocated);
    PAS_ASSERT(my_summary.free == free.end - free.begin);
    
    *summary_ptr = pas_heap_summary_add(*summary_ptr, my_summary);
    
    return true;
}

bool pas_compute_summary_dead_object_callback_without_physical_sharing(pas_large_free free,
                                                                       void* arg)
{
    pas_heap_summary* summary_ptr;
    pas_heap_summary my_summary;

    pas_heap_lock_assert_held();
    
    summary_ptr = arg;
    
    my_summary = pas_heap_summary_create_empty();
    my_summary.free = free.end - free.begin;
    my_summary.committed = free.end - free.begin;
    my_summary.free_ineligible_for_decommit = free.end - free.begin;
    
    *summary_ptr = pas_heap_summary_add(*summary_ptr, my_summary);
    
    return true;
}

bool (*pas_compute_summary_dead_object_callback_for_config(const pas_heap_config* config))(
    pas_large_free free,
    void* arg)
{
    if (config->aligned_allocator_talks_to_sharing_pool)
        return pas_compute_summary_dead_object_callback;
    return pas_compute_summary_dead_object_callback_without_physical_sharing;
}

#endif /* LIBPAS_ENABLED */
