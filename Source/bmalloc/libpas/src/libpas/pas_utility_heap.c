/*
 * Copyright (c) 2018-2021 Apple Inc. All rights reserved.
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

#include "pas_utility_heap.h"

#include "pas_allocation_callbacks.h"
#include "pas_deallocate.h"
#include "pas_get_allocation_size.h"
#include "pas_local_allocator_inlines.h"
#include "pas_utility_heap_config.h"

pas_utility_heap_support pas_utility_heap_support_instance = PAS_UTILITY_HEAP_SUPPORT_INITIALIZER;

pas_heap_runtime_config pas_utility_heap_runtime_config = {
    .lookup_kind = pas_segregated_heap_lookup_primitive,
    .sharing_mode = pas_share_pages,
    .statically_allocated = true,
    .is_part_of_heap = false,
    .directory_size_bound_for_partial_views = PAS_UTILITY_BOUND_FOR_PARTIAL_VIEWS,
    .directory_size_bound_for_baseline_allocators = PAS_UTILITY_BOUND_FOR_BASELINE_ALLOCATORS,
    .directory_size_bound_for_no_view_cache = PAS_UTILITY_BOUND_FOR_NO_VIEW_CACHE,
    .max_segregated_object_size = PAS_UTILITY_MAX_SEGREGATED_OBJECT_SIZE,
    .max_bitfit_object_size = PAS_UTILITY_MAX_BITFIT_OBJECT_SIZE,
    .view_cache_capacity_for_object_size = pas_heap_runtime_config_zero_view_cache_capacity
};

pas_segregated_heap pas_utility_segregated_heap = {
    .runtime_config = &pas_utility_heap_runtime_config,
    .basic_size_directory_and_head = PAS_COMPACT_ATOMIC_PTR_INITIALIZER,
    .index_to_small_size_directory = pas_utility_heap_support_instance.index_to_size_directory,
    .rare_data = PAS_COMPACT_ATOMIC_PTR_INITIALIZER,
    .index_to_small_allocator_index = NULL,
    .small_index_upper_bound = PAS_NUM_UTILITY_SIZE_CLASSES,
};

pas_allocator_counts pas_utility_allocator_counts;

void* pas_utility_heap_try_allocate_with_alignment(
    size_t size,
    size_t alignment,
    const char* name)
{
    size_t aligned_size;
    size_t index;
    pas_local_allocator* allocator;
    void* result;
    pas_utility_heap_allocator* allocators;

    pas_heap_lock_assert_held();

    aligned_size = pas_round_up_to_power_of_2(size, alignment);

    index = pas_segregated_heap_index_for_size(aligned_size, PAS_UTILITY_HEAP_CONFIG);

    if (index >= PAS_NUM_UTILITY_SIZE_CLASSES) {
        pas_log("Cannot allocate size = %zu (alignment = %zu, aligned_size = %zu, index = %zu) "
                "with utility heap.\n",
                size, alignment, aligned_size, index);
        PAS_ASSERT(index < PAS_NUM_UTILITY_SIZE_CLASSES);
    }

    allocators = pas_utility_heap_support_instance.allocators;
    if (!allocators) {
        size_t index_to_init;
        
        allocators = pas_immortal_heap_allocate(
            sizeof(pas_utility_heap_allocator) * PAS_NUM_UTILITY_SIZE_CLASSES,
            "pas_utility_heap_allocators",
            pas_object_allocation);

        for (index_to_init = PAS_NUM_UTILITY_SIZE_CLASSES; index_to_init--;)
            allocators[index_to_init].allocator = PAS_LOCAL_ALLOCATOR_NULL_INITIALIZER;
        
        pas_utility_heap_support_instance.allocators = allocators;
    }
    allocator = &allocators[index].allocator;

    if (pas_local_allocator_is_null(allocator)
        || alignment > pas_local_allocator_alignment(allocator)) {
        pas_segregated_size_directory* directory;
        
        pas_utility_heap_support_instance.slow_path_count++;

        directory = pas_segregated_heap_ensure_size_directory_for_size(
            &pas_utility_segregated_heap, aligned_size, alignment,
            pas_force_size_lookup, &pas_utility_heap_config, NULL,
            pas_segregated_size_directory_full_creation_mode);

        PAS_ASSERT(directory);

        pas_local_allocator_construct(allocator, directory);
    }

    result = (void*)pas_local_allocator_try_allocate(
        allocator,
        aligned_size,
        alignment,
        pas_compact_allocation_mode,
        PAS_UTILITY_HEAP_CONFIG,
        &pas_utility_allocator_counts,
        pas_allocation_result_identity).begin;

    pas_did_allocate(result, size, pas_utility_heap_kind, name, pas_object_allocation);

    return result;
}

void* pas_utility_heap_allocate_with_alignment(
    size_t size,
    size_t alignment,
    const char* name)
{
    void* result = pas_utility_heap_try_allocate_with_alignment(size, alignment, name);
    PAS_ASSERT(result);
    return result;
}

void* pas_utility_heap_try_allocate(size_t size, const char* name)
{
    return pas_utility_heap_try_allocate_with_alignment(size, 1, name);
}

void* pas_utility_heap_allocate(size_t size, const char* name)
{
    void* result = pas_utility_heap_try_allocate(size, name);
    PAS_ASSERT(result);
    return result;
}

void pas_utility_heap_deallocate(void* ptr)
{
    uintptr_t begin;

    pas_heap_lock_assert_held();

    if (!ptr)
        return;

    pas_will_deallocate(ptr, 0, pas_utility_heap_kind, pas_object_allocation);

    begin = (uintptr_t)ptr;

    pas_segregated_page_deallocate(
        begin, NULL, pas_segregated_deallocation_direct_mode, NULL,
        PAS_UTILITY_HEAP_CONFIG.small_segregated_config, pas_segregated_page_exclusive_role);
}

size_t pas_utility_heap_get_num_free_bytes(void)
{
    return pas_segregated_heap_get_num_free_bytes(&pas_utility_segregated_heap);
}

typedef struct {
    pas_utility_heap_for_each_live_object_callback callback;
    void* arg;
} for_each_live_object_data;

static bool for_each_live_object_small_object_callback(pas_segregated_heap* heap,
                                                       uintptr_t begin,
                                                       size_t size,
                                                       void* arg)
{
    for_each_live_object_data* data;

    PAS_UNUSED_PARAM(heap);
    
    data = arg;
    
    return data->callback(begin, size, data->arg);
}

bool pas_utility_heap_for_each_live_object(pas_utility_heap_for_each_live_object_callback callback,
                                           void* arg)
{
    for_each_live_object_data data;
    
    pas_heap_lock_assert_held();
    
    data.callback = callback;
    data.arg = arg;
    
    return pas_segregated_heap_for_each_live_object(&pas_utility_segregated_heap,
                                                    for_each_live_object_small_object_callback,
                                                    &data);
}

bool pas_utility_heap_for_all_allocators(pas_allocator_scavenge_action action,
                                         pas_lock_hold_mode heap_lock_hold_mode)
{
    size_t index;
    bool result;

    result = false;
    
    pas_heap_lock_lock_conditionally(heap_lock_hold_mode);

    if (pas_utility_heap_support_instance.allocators) {
        for (index = PAS_NUM_UTILITY_SIZE_CLASSES; index--;) {
            result |= pas_local_allocator_scavenge(
                &pas_utility_heap_support_instance.allocators[index].allocator,
                action);
        }
    }

    pas_heap_lock_unlock_conditionally(heap_lock_hold_mode);

    return result;
}

#endif /* LIBPAS_ENABLED */
