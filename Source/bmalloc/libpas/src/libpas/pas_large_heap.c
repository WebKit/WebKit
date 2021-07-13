/*
 * Copyright (c) 2018-2020 Apple Inc. All rights reserved.
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

#include "pas_large_heap.h"

#include "pas_bootstrap_free_heap.h"
#include "pas_compute_summary_object_callbacks.h"
#include "pas_heap.h"
#include "pas_heap_config.h"
#include "pas_heap_lock.h"
#include "pas_large_free_heap_config.h"
#include "pas_large_sharing_pool.h"
#include "pas_large_map.h"
#include "pas_page_malloc.h"
#include <stdio.h>

void pas_large_heap_construct(pas_large_heap* heap)
{
    /* Warning: anything you do here must be duplicated in
       pas_try_allocate_intrinsic_primitive.h. */
    
    pas_fast_large_free_heap_construct(&heap->free_heap);
    heap->table_state = pas_heap_table_state_uninitialized;
    heap->index = 0;
}

typedef struct {
    pas_heap_config_aligned_allocator aligned_allocator;
    pas_large_heap* heap;
    pas_heap_config* config;
} aligned_allocator_data;

static pas_aligned_allocation_result aligned_allocator(size_t size,
                                                       pas_alignment alignment,
                                                       void* arg)
{
    aligned_allocator_data* data;
    
    data = arg;
    
    PAS_ASSERT(data);
    
    return data->aligned_allocator(size, alignment, data->heap, data->config);
}

static void initialize_config(pas_large_free_heap_config* config,
                              aligned_allocator_data* data,
                              pas_large_heap* heap,
                              pas_heap_config* heap_config)
{
    if (data) {
        data->aligned_allocator = heap_config->aligned_allocator;
        data->heap = heap;
        data->config = heap_config;
    }
    config->type_size = heap_config->get_type_size(
        pas_heap_for_large_heap(heap)->type);
    config->min_alignment = heap_config->large_alignment;
    config->aligned_allocator = aligned_allocator;
    config->aligned_allocator_arg = data;
    config->deallocator = heap_config->deallocator;
    config->deallocator_arg = heap;
}

pas_allocation_result
pas_large_heap_try_allocate(pas_large_heap* heap,
                            size_t size,
                            size_t alignment,
                            pas_heap_config* heap_config,
                            pas_physical_memory_transaction* transaction)
{
    static const bool verbose = false;
    
    pas_allocation_result result;
    pas_heap_type* type;
    pas_large_free_heap_config config;
    aligned_allocator_data data;
    pas_large_map_entry entry;
    
    PAS_ASSERT(pas_is_power_of_2(alignment));
    pas_heap_lock_assert_held();
    
    type = pas_heap_for_large_heap(heap)->type;
    
    if (!size)
        size = heap_config->get_type_size(type);
    
    alignment = PAS_MAX(alignment, heap_config->get_type_alignment(type));
    alignment = PAS_MAX(alignment, heap_config->large_alignment);
    
    size = pas_round_up_to_power_of_2(size, alignment);
    
    if (verbose) {
        printf("Allocating large object of size %zu\n", size);
        printf("Cartesian tree minimum = %p\n", pas_cartesian_tree_minimum(&heap->free_heap.tree));
        printf("Num mapped bytes = %zu\n", heap->free_heap.num_mapped_bytes);
    }
    
    initialize_config(&config, &data, heap, heap_config);
    
    result = pas_fast_large_free_heap_try_allocate(
        &heap->free_heap,
        size,
        pas_alignment_create_traditional(alignment),
        &config);
    
    if (!result.did_succeed)
        return pas_allocation_result_create_failure();
    
    if (heap_config->aligned_allocator_talks_to_sharing_pool &&
        !pas_large_sharing_pool_allocate_and_commit(
            pas_range_create(result.begin, result.begin + size),
            transaction,
            pas_physical_memory_is_locked_by_virtual_range_common_lock)) {
        pas_fast_large_free_heap_deallocate(
            &heap->free_heap, result.begin, result.begin + size,
            result.zero_mode, &config);
        return pas_allocation_result_create_failure();
    }

    entry.begin = result.begin;
    entry.end = result.begin + size;
    entry.heap = heap;
    pas_large_map_add(entry);
    
    PAS_ASSERT(pas_is_aligned(result.begin, alignment));
    
    return result;
}

bool pas_large_heap_try_deallocate(uintptr_t begin,
                                   pas_heap_config* heap_config)
{
    pas_large_map_entry map_entry;
    pas_large_free_heap_config config;

    pas_heap_lock_assert_held();
    
    map_entry = pas_large_map_take(begin);
    
    if (pas_large_map_entry_is_empty(map_entry))
        return false;
    
    PAS_ASSERT(pas_heap_config_kind_get_config(
                   pas_heap_for_large_heap(map_entry.heap)->config_kind)
               == heap_config);

    if (heap_config->aligned_allocator_talks_to_sharing_pool) {
        pas_large_sharing_pool_free(
            pas_range_create(map_entry.begin, map_entry.end),
            pas_physical_memory_is_locked_by_virtual_range_common_lock);
    }

    initialize_config(&config, NULL, map_entry.heap, heap_config);
    pas_fast_large_free_heap_deallocate(&map_entry.heap->free_heap,
                                        map_entry.begin,
                                        map_entry.end,
                                        pas_zero_mode_may_have_non_zero,
                                        &config);
    
    return true;
}

bool pas_large_heap_try_shrink(uintptr_t begin,
                               size_t new_size,
                               pas_heap_config* heap_config)
{
    /* FIXME: This doesn't play nice with enumeration. I think that's fine for now because shrink()
       isn't a real malloc API. But it would be possible to make this work well with enumeration if
       we put in some effort. */

    pas_large_map_entry map_entry;
    pas_large_free_heap_config config;
    pas_large_heap* heap;
    pas_heap_type* type;
    size_t alignment;

    pas_heap_lock_assert_held();

    map_entry = pas_large_map_take(begin);

    if (pas_large_map_entry_is_empty(map_entry))
        return false;

    heap = map_entry.heap;
    type = pas_heap_for_large_heap(heap)->type;

    if (!new_size)
        new_size = heap_config->get_type_size(type);

    alignment = PAS_MAX(heap_config->get_type_alignment(type),
                        heap_config->large_alignment);

    new_size = pas_round_up_to_power_of_2(new_size, alignment);

    PAS_ASSERT(pas_heap_config_kind_get_config(
                   pas_heap_for_large_heap(heap)->config_kind)
               == heap_config);

    if (heap_config->aligned_allocator_talks_to_sharing_pool) {
        pas_large_sharing_pool_free(
            pas_range_create(map_entry.begin + new_size, map_entry.end),
            pas_physical_memory_is_locked_by_virtual_range_common_lock);
    }

    initialize_config(&config, NULL, heap, heap_config);
    pas_fast_large_free_heap_deallocate(&heap->free_heap,
                                        map_entry.begin + new_size,
                                        map_entry.end,
                                        pas_zero_mode_may_have_non_zero,
                                        &config);

    map_entry.end = map_entry.begin + new_size;
    pas_large_map_add(map_entry);

    return true;
}

void pas_large_heap_shove_into_free(pas_large_heap* heap,
                                    uintptr_t begin,
                                    uintptr_t end,
                                    pas_zero_mode zero_mode,
                                    pas_heap_config* heap_config)
{
    pas_large_free_heap_config config;
    initialize_config(&config, NULL, heap, heap_config);
    pas_fast_large_free_heap_deallocate(&heap->free_heap,
                                        begin,
                                        end,
                                        zero_mode,
                                        &config);
}

typedef struct {
    pas_large_heap* heap;
    pas_large_heap_for_each_live_object_callback callback;
    void *arg;
} for_each_live_object_data;

static bool for_each_live_object_entry_callback(pas_large_map_entry entry,
                                                void* arg)
{
    for_each_live_object_data* data = arg;
    
    if (entry.heap != data->heap)
        return true;
    
    return data->callback(data->heap, entry.begin, entry.end, data->arg);
}

bool pas_large_heap_for_each_live_object(pas_large_heap* heap,
                                         pas_large_heap_for_each_live_object_callback callback,
                                         void *arg)
{
    for_each_live_object_data data;
    
    pas_heap_lock_assert_held();
    
    data.heap = heap;
    data.callback = callback;
    data.arg = arg;
    return pas_large_map_for_each_entry(for_each_live_object_entry_callback, &data);
}

pas_large_heap* pas_large_heap_for_object(uintptr_t begin)
{
    pas_large_map_entry entry;
    
    entry = pas_large_map_find(begin);
    if (pas_large_map_entry_is_empty(entry))
        return NULL;
    
    PAS_ASSERT(entry.heap);
    
    return entry.heap;
}

size_t pas_large_heap_get_num_free_bytes(pas_large_heap* heap)
{
    return pas_fast_large_free_heap_get_num_free_bytes(&heap->free_heap);
}

static bool compute_summary_live_object_callback(pas_large_heap* heap,
                                                 uintptr_t begin,
                                                 uintptr_t end,
                                                 void* arg)
{
    return pas_compute_summary_live_object_callback_for_config(
        pas_heap_config_kind_get_config(
            pas_heap_for_large_heap(heap)->config_kind))(
                begin, end, arg);
}

pas_heap_summary pas_large_heap_compute_summary(pas_large_heap* heap)
{
    pas_heap_summary result;
    
    pas_heap_lock_assert_held();
    
    result = pas_heap_summary_create_empty();
    
    pas_large_heap_for_each_live_object(
        heap, compute_summary_live_object_callback, &result);
    pas_fast_large_free_heap_for_each_free(
        &heap->free_heap,
        pas_compute_summary_dead_object_callback_for_config(
            pas_heap_config_kind_get_config(pas_heap_for_large_heap(heap)->config_kind)),
        &result);
    
    return result;
}

#endif /* LIBPAS_ENABLED */
