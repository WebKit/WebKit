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

#include "pas_designated_intrinsic_heap.h"

#include "pas_designated_intrinsic_heap_inlines.h"
#include "pas_heap_lock.h"
#include "pas_segregated_heap.h"
#include "pas_thread_local_cache_layout.h"
#include "pas_thread_local_cache_node.h"

typedef struct {
    pas_segregated_heap* heap;
    const pas_heap_config* config_ptr;
    pas_allocator_index num_allocator_indices;
    pas_allocator_index next_index_to_set;
} initialize_data;

static void set_up_range(initialize_data* data,
                         pas_allocator_index designated_begin,
                         pas_allocator_index designated_end_inclusive,
                         size_t size)
{
    pas_allocator_index designated_index;
    pas_segregated_size_directory* directory;
    pas_allocator_index designated_end;

    PAS_ASSERT(
        designated_end_inclusive * pas_segregated_page_config_min_align(
            data->config_ptr->small_segregated_config)
        == size);

    designated_end = designated_end_inclusive + 1;

    PAS_ASSERT(size);
    PAS_ASSERT(designated_end > designated_begin);

    PAS_ASSERT(designated_begin == data->next_index_to_set);

    pas_heap_lock_assert_held();

    if (pas_thread_local_cache_layout_get_last_node()) {
        PAS_ASSERT(
            size
            > pas_thread_local_cache_layout_node_get_directory(pas_thread_local_cache_layout_get_last_node())->object_size);
    } else
        PAS_ASSERT(!designated_begin);

    directory = NULL;
    
    for (designated_index = designated_begin; designated_index < designated_end; ++designated_index) {
        pas_allocator_index target_allocator_index;
        bool result;

        result = __builtin_mul_overflow(
            designated_index, data->num_allocator_indices, &target_allocator_index);
        PAS_ASSERT(!result);
        result = __builtin_add_overflow(
            target_allocator_index, PAS_LOCAL_ALLOCATOR_UNSELECTED_NUM_INDICES, &target_allocator_index);
        PAS_ASSERT(!result);
        
        PAS_ASSERT(target_allocator_index >= pas_thread_local_cache_layout_next_allocator_index);
        pas_thread_local_cache_layout_next_allocator_index = target_allocator_index;

        if (designated_index == designated_begin) {
            PAS_ASSERT(!directory);
            
            directory = pas_segregated_heap_ensure_size_directory_for_size(
                data->heap, size, 1, pas_force_size_lookup, data->config_ptr, NULL,
                pas_segregated_size_directory_initial_creation_mode);

            PAS_ASSERT(directory);

            PAS_ASSERT(
                pas_segregated_size_directory_num_allocator_indices(directory)
                <= pas_designated_intrinsic_heap_num_allocator_indices(*data->config_ptr));
            
            /* This is a weird assert - if it was false then it doesn't literally make this broken.
               It just means that there is a chance of memory wasteage. So, if we hit it, then
               maybe we can just say that that's OK and remove the assert? */
            PAS_ASSERT(directory->object_size == size);
            
            pas_segregated_size_directory_create_tlc_allocator(directory);

            PAS_ASSERT(directory->allocator_index == target_allocator_index);
        } else {
            pas_thread_local_cache_layout_node last_node;
            pas_allocator_index resulting_index;
            
            PAS_ASSERT(directory);
            
            resulting_index = pas_thread_local_cache_layout_duplicate(directory);
            PAS_ASSERT(resulting_index == target_allocator_index);

            last_node = pas_thread_local_cache_layout_get_last_node();
            PAS_ASSERT(pas_thread_local_cache_layout_node_get_directory(last_node) == directory);
            PAS_ASSERT(pas_thread_local_cache_layout_node_get_allocator_index_for_allocator(last_node) == target_allocator_index);
            PAS_ASSERT(directory->allocator_index == PAS_LOCAL_ALLOCATOR_UNSELECTED_NUM_INDICES + designated_begin * data->num_allocator_indices);
        }
    }

    data->next_index_to_set = designated_end;
}

void pas_designated_intrinsic_heap_initialize(pas_segregated_heap* heap,
                                              const pas_heap_config* config_ptr)
{
    pas_heap_config config;
    initialize_data data;
    pas_thread_local_cache_layout_node layout_node;

    config = *config_ptr;

    /* This only works if it's called before anything else happens. */
    pas_heap_lock_assert_held();
    PAS_ASSERT(!pas_thread_local_cache_node_first);
    PAS_ASSERT(
        pas_thread_local_cache_layout_next_allocator_index == PAS_LOCAL_ALLOCATOR_UNSELECTED_NUM_INDICES);
    PAS_ASSERT(pas_compact_atomic_segregated_size_directory_ptr_is_null(
                   &heap->basic_size_directory_and_head));

    data.heap = heap;
    data.config_ptr = config_ptr;

    data.num_allocator_indices = pas_designated_intrinsic_heap_num_allocator_indices(config);

    data.next_index_to_set = 0;

    switch (pas_segregated_page_config_min_align(config.small_segregated_config)) {
    case 8:
        set_up_range(&data,  0,  1, 8);
        set_up_range(&data,  2,  2, 16);
        set_up_range(&data,  3,  3, 24);
        set_up_range(&data,  4,  4, 32);
        set_up_range(&data,  5,  5, 40);
        set_up_range(&data,  6,  6, 48);
        set_up_range(&data,  7,  8, 64);
        set_up_range(&data,  9, 10, 80);
        set_up_range(&data, 11, 12, 96);
        set_up_range(&data, 13, 16, 128);
        set_up_range(&data, 17, 20, 160);
        set_up_range(&data, 21, 24, 192);
        set_up_range(&data, 25, 28, 224);
        set_up_range(&data, 29, 32, 256);
        set_up_range(&data, 33, 38, 304);
        if ((false)) {
            set_up_range(&data, 39, 44, 352);
            set_up_range(&data, 45, 52, 416);
        }
        break;
        
    case 16:
        set_up_range(&data,  0,  1, 16);
        set_up_range(&data,  2,  2, 32);
        set_up_range(&data,  3,  3, 48);
        set_up_range(&data,  4,  4, 64);
        set_up_range(&data,  5,  5, 80);
        set_up_range(&data,  6,  6, 96);
        set_up_range(&data,  7,  8, 128);
        set_up_range(&data,  9, 10, 160);
        set_up_range(&data, 11, 12, 192);
        set_up_range(&data, 13, 14, 224);
        set_up_range(&data, 15, 16, 256);
        set_up_range(&data, 17, 19, 304);
        set_up_range(&data, 20, 22, 352);
        set_up_range(&data, 23, 26, 416);
        break;

    case 32:
        set_up_range(&data,  0,  1, 32);
        set_up_range(&data,  2,  2, 64);
        set_up_range(&data,  3,  3, 96);
        set_up_range(&data,  4,  4, 128);
        set_up_range(&data,  5,  5, 160);
        set_up_range(&data,  6,  6, 192);
        set_up_range(&data,  7,  7, 224);
        set_up_range(&data,  8,  8, 256);
        set_up_range(&data,  9, 10, 320);
        set_up_range(&data, 11, 12, 384);
        set_up_range(&data, 13, 14, 448);
        break;

    default:
        PAS_ASSERT(!"Unsupported minalign");
        break;
    }

    /* Cause the allocation of view caches to happen after we have laid out all of the local allocators. */
    for (PAS_THREAD_LOCAL_CACHE_LAYOUT_EACH_ALLOCATOR(layout_node)) {
        if (!pas_is_wrapped_segregated_size_directory(layout_node))
            continue;

        pas_segregated_size_directory_finish_creation(pas_unwrap_segregated_size_directory(layout_node));
    }

    if (PAS_ENABLE_TESTING) {
        /* Check that we won't return a designated index that is invalid. */

        size_t index;

        for (index = 0; ; ++index) {
            pas_designated_index_result result;

            result = pas_designated_intrinsic_heap_designated_index(
                index, pas_intrinsic_heap_is_designated, config);

            if (!result.did_succeed)
                break;

            PAS_ASSERT(
                result.index * data.num_allocator_indices
                < pas_thread_local_cache_layout_next_allocator_index);
        }
    }
}

#endif /* LIBPAS_ENABLED */
