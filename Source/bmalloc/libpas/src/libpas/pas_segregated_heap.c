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

#include "pas_segregated_heap.h"

#include "pas_all_heaps.h"
#include "pas_bitfit_heap.h"
#include "pas_heap.h"
#include "pas_heap_ref.h"
#include "pas_segregated_heap_inlines.h"
#include "pas_segregated_global_size_directory.h"
#include "pas_segregated_page.h"
#include "pas_thread_local_cache.h"
#include "pas_thread_local_cache_layout.h"
#include "pas_utility_heap_config.h"

static size_t min_object_size_for_heap_config(pas_heap_config* config)
{
    pas_segregated_page_config_variant segregated_variant;
    pas_bitfit_page_config_variant bitfit_variant;
    for (PAS_EACH_SEGREGATED_PAGE_CONFIG_VARIANT_ASCENDING(segregated_variant)) {
        pas_segregated_page_config* page_config;
        page_config =
            pas_heap_config_segregated_page_config_ptr_for_variant(config, segregated_variant);
        if (!pas_segregated_page_config_is_enabled(*page_config))
            continue;
        return pas_segregated_page_config_min_align(*page_config);
    }
    /* If segregated page configs are totally disabled then we want to give the minimum size according
       to bitfit. */
    for (PAS_EACH_BITFIT_PAGE_CONFIG_VARIANT_ASCENDING(bitfit_variant)) {
        pas_bitfit_page_config* page_config;
        page_config =
            pas_heap_config_bitfit_page_config_ptr_for_variant(config, bitfit_variant);
        if (!pas_bitfit_page_config_is_enabled(*page_config))
            continue;
        return pas_page_base_config_min_align(page_config->base);
    }
    /* Allow segregated and bitfit to both be disabled. To make this work, we lie and say that the
       minimum object size is whatever would be minimum to the large heap. */
    return config->large_alignment;
}

static size_t max_count_for_page_config(pas_heap* parent_heap,
                                        pas_page_base_config* page_config)
{
    size_t result = pas_round_down_to_power_of_2(
        page_config->max_object_size,
        pas_page_base_config_min_align(*page_config))
        / pas_heap_get_type_size(parent_heap);
    
    PAS_ASSERT(result * pas_heap_get_type_size(parent_heap) <= page_config->max_object_size);
    
    return result;
}

static size_t max_object_size_for_page_config(pas_heap* parent_heap,
                                              pas_page_base_config* page_config)
{
    return max_count_for_page_config(parent_heap, page_config)
        * pas_heap_get_type_size(parent_heap);
}

static size_t max_small_count_for_heap_config(pas_heap* parent_heap,
                                              pas_heap_config* config)
{
    size_t result = config->small_lookup_size_upper_bound / pas_heap_get_type_size(parent_heap);
    
    PAS_ASSERT(result * pas_heap_get_type_size(parent_heap)
               <= config->small_lookup_size_upper_bound);
    
    return result;
}

static size_t max_segregated_count_for_heap_config(pas_heap* parent_heap,
                                                   pas_segregated_heap* heap,
                                                   pas_heap_config* config)
{
    pas_segregated_page_config_variant variant;
    for (PAS_EACH_SEGREGATED_PAGE_CONFIG_VARIANT_DESCENDING(variant)) {
        pas_segregated_page_config* page_config;
        page_config = pas_heap_config_segregated_page_config_ptr_for_variant(config, variant);
        if (!pas_segregated_page_config_is_enabled(*page_config))
            continue;
        
        return PAS_MIN((size_t)heap->runtime_config->max_segregated_object_size,
                       max_count_for_page_config(parent_heap, &page_config->base));
    }
    return 0;
}

static size_t max_bitfit_count_for_heap_config(pas_heap* parent_heap,
                                               pas_segregated_heap* heap,
                                               pas_heap_config* config)
{
    static const bool verbose = false;
    
    pas_bitfit_page_config_variant variant;
    
    for (PAS_EACH_BITFIT_PAGE_CONFIG_VARIANT_DESCENDING(variant)) {
        pas_bitfit_page_config* page_config;
        page_config = pas_heap_config_bitfit_page_config_ptr_for_variant(config, variant);
        
        if (verbose) {
            pas_log("Considering %s.\n",
                    pas_bitfit_page_config_kind_get_string(page_config->kind));
        }

        if (!pas_bitfit_page_config_is_enabled(*page_config)) {
            if (verbose) {
                pas_log("Not considering %s because it's disabled.\n",
                        pas_bitfit_page_config_kind_get_string(page_config->kind));
            }
            continue;
        }

        if (verbose) {
            pas_log("Returning the min of %u and %zu\n",
                    heap->runtime_config->max_bitfit_object_size,
                    max_count_for_page_config(parent_heap, &page_config->base));
        }
        
        return PAS_MIN((size_t)heap->runtime_config->max_bitfit_object_size,
                       max_count_for_page_config(parent_heap, &page_config->base));
    }

    if (verbose)
        pas_log("Bitfit has a max count of zero because it's all disabled.\n");
    
    return 0;
}

static size_t max_count_for_heap_config(pas_heap* parent_heap,
                                        pas_segregated_heap* heap,
                                        pas_heap_config* config)
{
    return PAS_MAX(max_segregated_count_for_heap_config(parent_heap, heap, config),
                   max_bitfit_count_for_heap_config(parent_heap, heap, config));
}

static size_t max_bitfit_object_size_for_heap_config(pas_heap* parent_heap,
                                                     pas_segregated_heap* heap,
                                                     pas_heap_config* config)
{
    return max_bitfit_count_for_heap_config(parent_heap, heap, config)
        * pas_heap_get_type_size(parent_heap);
}

static size_t max_object_size_for_heap_config(pas_heap* parent_heap,
                                              pas_segregated_heap* heap,
                                              pas_heap_config* config)
{
    return max_count_for_heap_config(parent_heap, heap, config)
        * pas_heap_get_type_size(parent_heap);
}

void pas_segregated_heap_construct(pas_segregated_heap* segregated_heap,
                                   pas_heap* parent_heap,
                                   pas_heap_config* config,
                                   pas_heap_runtime_config* runtime_config)
{
    /* NOTE: the various primitive and utility heaps are constructed
       specially. */

    PAS_ASSERT(runtime_config->sharing_mode != pas_invalid_sharing_mode);

    segregated_heap->runtime_config = runtime_config;
    
    pas_compact_atomic_segregated_global_size_directory_ptr_store(
        &segregated_heap->basic_size_directory_and_head, NULL);
    
    segregated_heap->small_index_upper_bound = 0;
    segregated_heap->index_to_small_size_directory = NULL;

    pas_segregated_heap_rare_data_ptr_store(&segregated_heap->rare_data, NULL);
    pas_compact_atomic_bitfit_heap_ptr_store(&segregated_heap->bitfit_heap, NULL);

    segregated_heap->index_to_small_allocator_index = NULL;

    PAS_ASSERT(!runtime_config->statically_allocated);
    PAS_ASSERT(runtime_config->is_part_of_heap == !!parent_heap);
    PAS_ASSERT(pas_heap_config_is_utility(config) == !runtime_config->is_part_of_heap);
    
    PAS_ASSERT(pas_heap_for_segregated_heap(segregated_heap) == parent_heap);
}

pas_bitfit_heap* pas_segregated_heap_get_bitfit(pas_segregated_heap* heap,
                                                pas_heap_config* heap_config,
                                                pas_lock_hold_mode heap_lock_hold_mode)
{
    /* NOTE: This will never get called for utility heaps and we have no good way to assert this:
       we grab the heap lock unconditionally. */

    pas_bitfit_heap* result;

    result = pas_compact_atomic_bitfit_heap_ptr_load(&heap->bitfit_heap);
    if (result)
        return result;

    pas_heap_lock_lock_conditionally(heap_lock_hold_mode);
    result = pas_compact_atomic_bitfit_heap_ptr_load(&heap->bitfit_heap);
    if (!result) {
        result = pas_bitfit_heap_create(heap, heap_config);
        pas_compact_atomic_bitfit_heap_ptr_store(&heap->bitfit_heap, result);
    }
    pas_heap_lock_unlock_conditionally(heap_lock_hold_mode);

    return result;
}

typedef struct {
    pas_segregated_heap_medium_directory_tuple* tuple;
    uintptr_t dependency;
} medium_directory_tuple_for_index_impl_result;

static PAS_ALWAYS_INLINE medium_directory_tuple_for_index_impl_result
medium_directory_tuple_for_index_impl(
    pas_segregated_heap_rare_data* rare_data,
    pas_segregated_heap_medium_directory_tuple* medium_directories,
    unsigned num_medium_directories,
    size_t index,
    pas_segregated_heap_medium_size_directory_search_mode search_mode)
{
    static const bool verbose = false;
    
    unsigned begin;
    unsigned end;
    pas_segregated_heap_medium_directory_tuple* best;
    medium_directory_tuple_for_index_impl_result result;
    
    PAS_ASSERT(rare_data);

    if (verbose)
        pas_log("In rare_data = %p, Looking for index = %zu\n", rare_data, index);
    
    begin = 0;
    end = num_medium_directories;
    best = NULL;

    result.dependency = (uintptr_t)medium_directories;
    
    while (end > begin) {
        unsigned middle;
        pas_segregated_heap_medium_directory_tuple* directory;
        unsigned begin_index;
        unsigned end_index;

        middle = (begin + end) >> 1;
        
        directory = medium_directories + middle;
        
        if (verbose) {
            pas_log("begin = %u, end = %u, middle = %u, begin_index = %u, end_index = %u\n",
                    begin, end, middle, directory->begin_index, directory->end_index);
        }

        begin_index = directory->begin_index;
        end_index = directory->end_index;
        
        result.dependency += begin_index + end_index;
        
        if (index < begin_index) {
            end = middle;
            best = directory;
            continue;
        }
        
        if (index > end_index) {
            begin = middle + 1;
            continue;
        }

        result.tuple = directory;
        return result;
    }

    switch (search_mode) {
    case pas_segregated_heap_medium_size_directory_search_within_size_class_progression:
        result.tuple = NULL;
        return result;
        
    case pas_segregated_heap_medium_size_directory_search_least_greater_equal:
        result.tuple = best;
        return result;
    }

    PAS_ASSERT(!"Should not be reached");
    result.tuple = NULL;
    return result;
}

static pas_segregated_heap_medium_directory_tuple*
medium_directory_tuple_for_index_with_lock(
    pas_segregated_heap* heap,
    size_t index,
    pas_segregated_heap_medium_size_directory_search_mode search_mode,
    pas_lock_hold_mode heap_lock_hold_mode)
{
    pas_segregated_heap_medium_directory_tuple* result;
    pas_segregated_heap_rare_data* rare_data;
    pas_segregated_heap_medium_directory_tuple* medium_directories;
    
    pas_heap_lock_lock_conditionally(heap_lock_hold_mode);

    rare_data = pas_segregated_heap_rare_data_ptr_load(&heap->rare_data);
    medium_directories =
        pas_segregated_heap_medium_directory_tuple_ptr_load(&rare_data->medium_directories);
    
    result = medium_directory_tuple_for_index_impl(
        rare_data,
        medium_directories,
        rare_data->num_medium_directories,
        index,
        search_mode).tuple;
    
    pas_heap_lock_unlock_conditionally(heap_lock_hold_mode);
    
    return result;
}

pas_segregated_heap_medium_directory_tuple*
pas_segregated_heap_medium_directory_tuple_for_index(
    pas_segregated_heap* heap,
    size_t index,
    pas_segregated_heap_medium_size_directory_search_mode search_mode,
    pas_lock_hold_mode heap_lock_hold_mode)
{
    pas_segregated_heap_rare_data* rare_data;
    pas_mutation_count saved_count;
    pas_segregated_heap_medium_directory_tuple* medium_directories;
    unsigned num_medium_directories;
    medium_directory_tuple_for_index_impl_result result;
    
    rare_data = pas_segregated_heap_rare_data_ptr_load(&heap->rare_data);
    if (!rare_data)
        return NULL;
    
    if (heap_lock_hold_mode == pas_lock_is_held) {
        return medium_directory_tuple_for_index_with_lock(
            heap, index, search_mode, pas_lock_is_held);
    }
    
    saved_count = rare_data->mutation_count;
    if (PAS_UNLIKELY(pas_mutation_count_is_mutating(saved_count))) {
        return medium_directory_tuple_for_index_with_lock(
            heap, index, search_mode, pas_lock_is_not_held);
    }
    
    num_medium_directories =
        rare_data[pas_mutation_count_depend(saved_count)].num_medium_directories;
    medium_directories = pas_segregated_heap_medium_directory_tuple_ptr_load(
        &rare_data[pas_depend(num_medium_directories)].medium_directories);
    
    result = medium_directory_tuple_for_index_impl(
        rare_data, medium_directories, num_medium_directories, index, search_mode);
    
    if (pas_mutation_count_matches_with_dependency(
            &rare_data->mutation_count, saved_count, result.dependency))
        return result.tuple;
    
    return medium_directory_tuple_for_index_with_lock(
        heap, index, search_mode, pas_lock_is_not_held);
}

unsigned pas_segregated_heap_medium_allocator_index_for_index(
    pas_segregated_heap* heap,
    size_t index,
    pas_segregated_heap_medium_size_directory_search_mode search_mode,
    pas_lock_hold_mode heap_lock_hold_mode)
{
    pas_segregated_heap_medium_directory_tuple* medium_directory;
    
    medium_directory = pas_segregated_heap_medium_directory_tuple_for_index(
        heap, index, search_mode, heap_lock_hold_mode);
    
    if (medium_directory)
        return medium_directory->allocator_index;
    
    return UINT_MAX;
}

pas_segregated_global_size_directory* pas_segregated_heap_medium_size_directory_for_index(
    pas_segregated_heap* heap,
    size_t index,
    pas_segregated_heap_medium_size_directory_search_mode search_mode,
    pas_lock_hold_mode heap_lock_hold_mode)
{
    pas_segregated_heap_medium_directory_tuple* medium_directory;
    
    medium_directory = pas_segregated_heap_medium_directory_tuple_for_index(
        heap, index, search_mode, heap_lock_hold_mode);
    
    if (medium_directory)
        return pas_compact_atomic_segregated_global_size_directory_ptr_load(&medium_directory->directory);
    
    return NULL;
}

static size_t compute_small_index_upper_bound(pas_segregated_heap* heap,
                                              pas_heap_config* config)
{
    size_t max_count;
    
    if (heap->small_index_upper_bound) {
        /* This is important: some clients, like the intrinsic_heap, will initialize
           small_index_upper_bound to something specific. */
        return heap->small_index_upper_bound;
    }
    
    max_count = max_small_count_for_heap_config(pas_heap_for_segregated_heap(heap),
                                                config);
    
    return pas_segregated_heap_index_for_count(heap, max_count, *config) + 1;
}

static void ensure_count_lookup(pas_segregated_heap* heap,
                                pas_heap_config* config)
{
    size_t index_upper_bound;
    pas_compact_atomic_segregated_global_size_directory_ptr* index_to_size_directory;
    pas_allocator_index* index_to_allocator_index;
    unsigned index;
    
    if (heap->small_index_upper_bound)
        return;

    PAS_ASSERT(!heap->runtime_config->statically_allocated);
    PAS_ASSERT(!pas_heap_config_is_utility(config));
    
    index_upper_bound = compute_small_index_upper_bound(heap, config);
    
    index_to_size_directory = pas_immortal_heap_allocate(
        sizeof(pas_compact_atomic_segregated_global_size_directory_ptr) * index_upper_bound,
        "pas_segregated_heap/index_to_size_directory",
        pas_object_allocation);
    index_to_allocator_index = pas_immortal_heap_allocate(
        sizeof(pas_allocator_index) * index_upper_bound,
        "pas_segregated_heap/index_to_allocator_index",
        pas_object_allocation);
    
    for (index = 0; index < index_upper_bound; ++index) {
        pas_compact_atomic_segregated_global_size_directory_ptr_store(
            index_to_size_directory + index, NULL);
        index_to_allocator_index[index] = (pas_allocator_index)UINT_MAX;
    }
    
    pas_fence();

    PAS_ASSERT((unsigned)index_upper_bound == index_upper_bound);
    heap->small_index_upper_bound = (unsigned)index_upper_bound;
    heap->index_to_small_size_directory = index_to_size_directory;
    heap->index_to_small_allocator_index = index_to_allocator_index;
}

unsigned
pas_segregated_heap_ensure_allocator_index(
    pas_segregated_heap* heap,
    pas_segregated_global_size_directory* directory,
    size_t count,
    pas_count_lookup_mode count_lookup_mode,
    pas_heap_config* config,
    unsigned* cached_index)
{
    static const bool verbose = false;

    size_t index;
    unsigned allocator_index;
    bool did_cache_allocator_index;
    pas_heap* parent_heap;

    pas_heap_lock_assert_held();
    
    PAS_ASSERT(directory->object_size >= min_object_size_for_heap_config(config));

    parent_heap = pas_heap_for_segregated_heap(heap);
    
    PAS_ASSERT(count * pas_heap_get_type_size(parent_heap)
               <= directory->object_size);

    PAS_ASSERT(!pas_heap_config_is_utility(config));
    
    if (verbose) {
        printf("In pas_segregated_heap_ensure_allocator_index\n");
        printf("count = %zu\n", count);
    }
    index = pas_segregated_heap_index_for_count(heap, count, *config);
    if (verbose)
        printf("index = %zu\n", index);

    allocator_index =
        pas_segregated_global_size_directory_data_ptr_load(&directory->data)->allocator_index;
    PAS_ASSERT((pas_allocator_index)allocator_index == allocator_index);
    PAS_ASSERT(allocator_index < (unsigned)(pas_allocator_index)UINT_MAX);
    
    if (verbose)
        printf("allocator_index = %u\n", allocator_index);
    
    did_cache_allocator_index = false;
    
    if ((cached_index ? index == *cached_index : index == 1)
        && parent_heap && parent_heap->heap_ref) {
        if (verbose) {
            printf("pas_segregated_heap_ensure_allocator_index_for_size_directory: "
                   "Caching as cached index!\n");
        }
        PAS_ASSERT(parent_heap->heap_ref->allocator_index == UINT_MAX ||
                   parent_heap->heap_ref->allocator_index == allocator_index);
        parent_heap->heap_ref->allocator_index = allocator_index;
        did_cache_allocator_index = true;
    }
    
    if (index < compute_small_index_upper_bound(heap, config)) {
        if (!did_cache_allocator_index
            || count_lookup_mode == pas_force_count_lookup
            || heap->small_index_upper_bound) {
            pas_allocator_index* allocator_index_ptr;
            pas_allocator_index old_allocator_index;
            ensure_count_lookup(heap, config);
            PAS_ASSERT(index < heap->small_index_upper_bound);
            allocator_index_ptr = heap->index_to_small_allocator_index + index;
            old_allocator_index = *allocator_index_ptr;
            PAS_ASSERT(old_allocator_index == (pas_allocator_index)UINT_MAX ||
                       old_allocator_index == allocator_index);
            *allocator_index_ptr = (pas_allocator_index)allocator_index;
        }
    } else {
        pas_segregated_heap_medium_directory_tuple* medium_directory;
        
        medium_directory =
            pas_segregated_heap_medium_directory_tuple_for_index(
                heap, index,
                pas_segregated_heap_medium_size_directory_search_within_size_class_progression,
                pas_lock_is_held);
        PAS_ASSERT(medium_directory);
        PAS_ASSERT(
            pas_compact_atomic_segregated_global_size_directory_ptr_load(&medium_directory->directory)
            == directory);
        
        medium_directory->allocator_index = (pas_allocator_index)allocator_index;
    }
    
    return allocator_index;
}

static size_t compute_ideal_object_size(pas_segregated_heap* heap,
                                        size_t object_size,
                                        size_t alignment,
                                        pas_segregated_page_config* page_config_ptr)
{
    unsigned num_objects;
    pas_segregated_page_config page_config;
    pas_heap* parent_heap;
    
    page_config = *page_config_ptr;

    object_size = pas_round_up_to_power_of_2(
        object_size, pas_segregated_page_config_min_align(page_config));
    
    PAS_ASSERT(pas_is_power_of_2(alignment));
    
    alignment = PAS_MAX(alignment, pas_segregated_page_config_min_align(page_config));
    
    num_objects = pas_segregated_page_number_of_objects((unsigned)object_size,
                                                        page_config);
    
    parent_heap = pas_heap_for_segregated_heap(heap);

    /* Find the largest object_size that fits the same number of objects as we're asking for
       while maintaining the alignment we're asking for. */
    for (;;) {
        size_t next_object_size;
        
        next_object_size = object_size + pas_segregated_page_config_min_align(page_config);
        
        if (!pas_is_aligned(next_object_size, alignment))
            break;
        if (next_object_size > max_object_size_for_page_config(parent_heap,
                                                               &page_config_ptr->base))
            break;
        if (pas_segregated_page_number_of_objects((unsigned)next_object_size,
                                                  page_config) != num_objects)
            break;
        
        object_size = next_object_size;
    }
    
    return object_size;
}

static void check_medium_directories(pas_segregated_heap* heap)
{
    size_t index;
    pas_segregated_heap_rare_data* rare_data;
    pas_segregated_heap_medium_directory_tuple* medium_directories;

    rare_data = pas_segregated_heap_rare_data_ptr_load(&heap->rare_data);
    
    if (!rare_data)
        return;

    medium_directories =
        pas_segregated_heap_medium_directory_tuple_ptr_load(&rare_data->medium_directories);
    
    for (index = rare_data->num_medium_directories; index--;) {
        PAS_ASSERT(medium_directories[index].begin_index
                   <= medium_directories[index].end_index);
    }
    
    for (index = rare_data->num_medium_directories; index-- > 1;) {
        PAS_ASSERT(medium_directories[index - 1].end_index
                   < medium_directories[index].begin_index);
    }
}

typedef struct {
    bool did_find;
    pas_segregated_heap* heap;
} check_part_of_all_heaps_data;

static bool check_part_of_all_heaps_callback(pas_heap* heap, void* arg)
{
    static const bool verbose = false;
    
    check_part_of_all_heaps_data* data;

    data = arg;

    if (verbose) {
        pas_log("all_heaps told us about %p (segregated = %p), we expect %p\n",
                heap, &heap->segregated_heap, data->heap);
    }
    
    if (&heap->segregated_heap == data->heap) {
        data->did_find = true;
        return false;
    }

    return true;
}

static bool check_part_of_all_segregated_heaps_callback(
    pas_segregated_heap* heap, pas_heap_config* config, void* arg)
{
    check_part_of_all_heaps_data* data;

    PAS_UNUSED_PARAM(config);

    data = arg;

    if (heap == data->heap) {
        data->did_find = true;
        return false;
    }

    return true;
}

static void
ensure_count_lookup_if_necessary(pas_segregated_heap* heap,
                                 pas_count_lookup_mode count_lookup_mode,
                                 pas_heap_config* config,
                                 unsigned* cached_index,
                                 size_t index)
{
    if (count_lookup_mode == pas_force_count_lookup
        || ((cached_index
             ? *cached_index != UINT_MAX && index != *cached_index
             : index != 1)
            && index < max_small_count_for_heap_config(pas_heap_for_segregated_heap(heap),
                                                       config)))
        ensure_count_lookup(heap, config);
}

pas_segregated_global_size_directory*
pas_segregated_heap_ensure_size_directory_for_count(pas_segregated_heap* heap,
                                                    size_t count,
                                                    size_t alignment,
                                                    pas_count_lookup_mode count_lookup_mode,
                                                    pas_heap_config* config,
                                                    unsigned* cached_index)
{
    static const bool verbose = false;

    pas_heap* parent_heap;
    pas_segregated_global_size_directory* result;
    size_t index;
    size_t object_size;
    pas_segregated_heap_medium_directory_tuple* medium_tuple;
    bool is_utility;

    pas_heap_lock_assert_held();

    pas_heap_config_activate(config);

    result = NULL;

    if (verbose) {
        pas_log("%p: being asked for directory with count = %zu, alignment = %zu.\n",
                heap, count, alignment);
    }

    if (PAS_ENABLE_TESTING) {
        check_part_of_all_heaps_data data;
        data.heap = heap;

        if (verbose)
            pas_log("Checking that the heap belongs to all heaps.\n");
        
        if (heap->runtime_config->statically_allocated) {
            if (heap->runtime_config->is_part_of_heap) {
                if (verbose)
                    pas_log("Checking that the heap belongs to static heaps.\n");
                data.did_find = false;
                pas_all_heaps_for_each_static_heap(check_part_of_all_heaps_callback, &data);
                PAS_ASSERT(data.did_find);
            }

            if (verbose)
                pas_log("Checking that the heap belongs to static segregated heaps.\n");
            data.did_find = false;
            pas_all_heaps_for_each_static_segregated_heap(
                check_part_of_all_segregated_heaps_callback, &data);
            PAS_ASSERT(data.did_find);
        }

        if (heap->runtime_config->is_part_of_heap) {
            if (verbose)
                pas_log("Checking that the heap belongs to heaps.\n");
            data.did_find = false;
            pas_all_heaps_for_each_heap(check_part_of_all_heaps_callback, &data);
            PAS_ASSERT(data.did_find);
        }

        if (verbose)
            pas_log("Checking that the heap belongs to segregated heaps.\n");
        data.did_find = false;
        pas_all_heaps_for_each_segregated_heap(check_part_of_all_segregated_heaps_callback, &data);
        PAS_ASSERT(data.did_find);
    }

    parent_heap = pas_heap_for_segregated_heap(heap);

    index = pas_segregated_heap_index_for_count(heap, count, *config);

    if (__builtin_umull_overflow(pas_segregated_heap_count_for_index(heap,
                                                                     index,
                                                                     *config),
                                 pas_heap_get_type_size(parent_heap),
                                 &object_size))
        return NULL;

    object_size = PAS_MAX(min_object_size_for_heap_config(config), object_size);
    
    /* Doing the alignment round-up here has the effect that once we do create a size class for this
       count, it ends up being aligned appropriately.
    
       One outcome of this is that we will ensure this larger size at the original index. For this
       reason, the callers of this function should be sure that they round up their count to the
       nearest count to the aligned size. */
    object_size = pas_round_up_to_power_of_2(object_size, alignment);
    
    object_size = pas_round_up_to_power_of_2(
        object_size, pas_segregated_page_config_min_align(config->small_segregated_config));
    alignment = PAS_MAX(alignment,
                        pas_segregated_page_config_min_align(config->small_segregated_config));

    /* If we have overflowed already then bail. */
    if ((unsigned)object_size != object_size ||
        (unsigned)alignment != alignment)
        return NULL;

    if (verbose)
        pas_log("Considering whether object_size = %zu is appropriate.\n", object_size);
    
    /* If it's impossible to use the largest segregated heap to allocate the request then
       immediately give up. Do this before ensure_count_lookup so that heaps that are only used
       for large object allocation don't allocate any small heap meta-data. */
    if (object_size > max_object_size_for_heap_config(parent_heap, heap, config)) {
        if (verbose)
            pas_log("It's too big.\n");
        return NULL;
    }
    
    if (verbose)
        pas_log("Proceeding.\n");
    
    ensure_count_lookup_if_necessary(heap, count_lookup_mode, config, cached_index, index);
    
    result = pas_segregated_heap_size_directory_for_index(heap, index, cached_index);

    PAS_ASSERT(
        !result ||
        pas_is_aligned(result->object_size, pas_segregated_global_size_directory_alignment(result)));

    is_utility = pas_heap_config_is_utility(config);
    
    if (result && pas_segregated_global_size_directory_alignment(result) < alignment) {
        size_t victim_index;
        
        /* In this case, result is a size class for at least as big of a size
           as we want and it's cached at the index we wanted plus some number
           of smaller indices. We will create a new size class for this index
           and we wanted smaller indices to consider this one instead of the
           old one. */
        
        /* Need to clear out this size class from this size and smaller sizes.
           This loop starts at index (not index + 1 despite what the initial
           condition seems to do) and travels down. */
        if (heap->small_index_upper_bound) {
            pas_compact_atomic_segregated_global_size_directory_ptr* index_to_small_size_directory;
            pas_allocator_index* index_to_small_allocator_index;

            index_to_small_size_directory = heap->index_to_small_size_directory;
            index_to_small_allocator_index = heap->index_to_small_allocator_index;
            
            for (victim_index = index + 1; victim_index--;) {
                pas_segregated_global_size_directory* directory;

                directory = pas_compact_atomic_segregated_global_size_directory_ptr_load(
                    index_to_small_size_directory + victim_index);
                
                if (!directory)
                    continue;
                
                if (directory != result)
                    break;

                pas_compact_atomic_segregated_global_size_directory_ptr_store(
                    index_to_small_size_directory + victim_index, NULL);
                if (!is_utility)
                    index_to_small_allocator_index[victim_index] = (pas_allocator_index)UINT_MAX;
            }
        }
        
        medium_tuple = pas_segregated_heap_medium_directory_tuple_for_index(
            heap, index,
            pas_segregated_heap_medium_size_directory_search_within_size_class_progression,
            pas_lock_is_held);
        if (medium_tuple) {
            size_t medium_tuple_index;
            pas_segregated_heap_rare_data* rare_data;

            if (verbose) {
                pas_log("Found medium_tuple begin_index = %u, end_index = %u\n",
                        medium_tuple->begin_index, medium_tuple->end_index);
            }
            
            PAS_ASSERT(index >= medium_tuple->begin_index);
            PAS_ASSERT(index <= medium_tuple->end_index);

            rare_data = pas_segregated_heap_rare_data_ptr_load(&heap->rare_data);
            PAS_ASSERT(rare_data);
            
            pas_mutation_count_start_mutating(&rare_data->mutation_count);
            
            if (index < medium_tuple->end_index) {
                size_t begin_index;
                begin_index = index + 1;
                PAS_ASSERT((pas_segregated_heap_medium_directory_index)begin_index == begin_index);
                medium_tuple->begin_index = (pas_segregated_heap_medium_directory_index)begin_index;
            } else {
                pas_segregated_heap_medium_directory_tuple* medium_directories;
                
                PAS_ASSERT(index == medium_tuple->end_index);

                medium_directories = pas_segregated_heap_medium_directory_tuple_ptr_load(
                    &rare_data->medium_directories);
                
                medium_tuple_index = medium_tuple - medium_directories;
                PAS_ASSERT(medium_tuple_index < rare_data->num_medium_directories);
                
                memmove(medium_directories + medium_tuple_index,
                        medium_directories + medium_tuple_index + 1,
                        sizeof(pas_segregated_heap_medium_directory_tuple) *
                        (rare_data->num_medium_directories - medium_tuple_index - 1));
                
                rare_data->num_medium_directories--;
            }
            
            check_medium_directories(heap);

            pas_mutation_count_stop_mutating(&rare_data->mutation_count);
        }
        
        /* The basic size class could be for our index or for a larger or
           smaller index.
           
           Our index: obviously we should clear it out since we'll create a
           new size class.
           
           Larger index: don't clear it since we still want those larger
           allocations to find this.
           
           Smaller index: clear it if it matches, since we *may* want those
           smaller allocations to consider our new one. */
        if (pas_compact_atomic_segregated_global_size_directory_ptr_load(
                &heap->basic_size_directory_and_head) == result
            && result->base.is_basic_size_directory
            && index >= (cached_index ? *cached_index : 1)) {
            result->base.is_basic_size_directory = false;
            if (parent_heap && parent_heap->heap_ref)
                parent_heap->heap_ref->allocator_index = UINT_MAX;
        }
        
        /* We'll create a new size class that aligns properly. The indices that we cleared will
           reconfigure to use our new size class. */
        result = NULL;
    }
    
    if (!result) {
        size_t candidate_index;
        pas_segregated_global_size_directory* candidate;
        size_t medium_install_index;
        size_t medium_install_count;
        double best_bytes_dirtied_per_object;
        pas_segregated_page_config* best_page_config;
        pas_segregated_page_config_variant variant;
        pas_compact_atomic_segregated_global_size_directory_ptr* index_to_small_size_directory;
        pas_segregated_global_size_directory* basic_size_directory_and_head;

        index_to_small_size_directory = heap->index_to_small_size_directory;
        
        /* Should try to find a size class that is bigger but small enough before creating a
           new one. */
    
        candidate = NULL;
    
        for (candidate_index = index;
             candidate_index < heap->small_index_upper_bound;
             ++candidate_index) {
            candidate = pas_compact_atomic_segregated_global_size_directory_ptr_load(
                index_to_small_size_directory + candidate_index);
            if (candidate)
                break;
        }
    
        /* It's possible that the basic size class is a candidate. That could
           happen if we did not force count lookup when creating that basic size
           class. */
        basic_size_directory_and_head = pas_compact_atomic_segregated_global_size_directory_ptr_load(
            &heap->basic_size_directory_and_head);
        if ((!candidate || candidate_index > (cached_index ? *cached_index : 1))
            && basic_size_directory_and_head
            && basic_size_directory_and_head->base.is_basic_size_directory
            && basic_size_directory_and_head->object_size >= object_size)
            candidate = basic_size_directory_and_head;
    
        /* And maybe the candidate we would have gotten from the medium page search list is a
           closer fit. That would happen if the candidate we selected was based on the basic page
           directory. Obviously, if we had found our candidate in the small lookup table, then this
           will fail. */
        medium_tuple = pas_segregated_heap_medium_directory_tuple_for_index(
            heap, index,
            pas_segregated_heap_medium_size_directory_search_least_greater_equal,
            pas_lock_is_held);
        if (medium_tuple) {
            pas_segregated_global_size_directory* directory;
            directory = pas_compact_atomic_segregated_global_size_directory_ptr_load(
                &medium_tuple->directory);
            if (!candidate || directory->object_size < candidate->object_size)
                candidate = directory;
        }

        /* Figure out the best case scenario if we did create a page directory for this size. */
        best_bytes_dirtied_per_object = PAS_INFINITY;
        best_page_config = NULL;
        if (object_size <= heap->runtime_config->max_segregated_object_size) {
            for (PAS_EACH_SEGREGATED_PAGE_CONFIG_VARIANT_DESCENDING(variant)) {
                pas_segregated_page_config* page_config_ptr;
                pas_segregated_page_config page_config;
                double bytes_dirtied_per_object;
                unsigned object_size_for_config;
                
                page_config_ptr =
                    pas_heap_config_segregated_page_config_ptr_for_variant(config, variant);
                if (!pas_segregated_page_config_is_enabled(*page_config_ptr))
                    continue;
                
                page_config = *page_config_ptr;
                
                object_size_for_config = (unsigned)pas_round_up_to_power_of_2(
                    object_size,
                    pas_segregated_page_config_min_align(page_config));
                
                if (object_size_for_config > max_object_size_for_page_config(parent_heap,
                                                                             &page_config_ptr->base))
                    continue;
                
                bytes_dirtied_per_object =
                    pas_segregated_page_bytes_dirtied_per_object(object_size_for_config, page_config);
                
                if (verbose) {
                    pas_log("Bytes dirtied per object for %s, %zu: %lf.\n",
                            pas_segregated_page_config_variant_get_string(variant),
                            object_size,
                            bytes_dirtied_per_object);
                }
                
                if (bytes_dirtied_per_object < best_bytes_dirtied_per_object) {
                    best_bytes_dirtied_per_object = bytes_dirtied_per_object;
                    best_page_config = page_config_ptr;
                }
            }
        }

        if (best_page_config) {
            PAS_ASSERT(best_bytes_dirtied_per_object >= 0.);
            PAS_ASSERT(best_bytes_dirtied_per_object < PAS_INFINITY);
        } else {
            PAS_ASSERT(object_size <= heap->runtime_config->max_bitfit_object_size);
            PAS_TESTING_ASSERT(
                object_size <= max_bitfit_object_size_for_heap_config(parent_heap, heap, config));
            best_bytes_dirtied_per_object =
                pas_bitfit_heap_select_variant(object_size, config).object_size;
            PAS_ASSERT(!is_utility);
        }

        if (candidate && pas_segregated_global_size_directory_alignment(candidate) >= alignment) {
            double bytes_dirtied_per_object_by_candidate;
        
            if (verbose) {
                printf("object_size = %lu\n", object_size);
                printf("candidate->object_size = %u\n", candidate->object_size);
            }

            if (candidate->base.page_config_kind != pas_segregated_page_config_kind_null) {
                bytes_dirtied_per_object_by_candidate =
                    pas_segregated_page_bytes_dirtied_per_object(
                        candidate->object_size,
                        *pas_segregated_page_config_kind_get_config(candidate->base.page_config_kind));
            } else
                bytes_dirtied_per_object_by_candidate = candidate->object_size;

            if (best_bytes_dirtied_per_object * PAS_SIZE_CLASS_PROGRESSION
                > bytes_dirtied_per_object_by_candidate)
                result = candidate;
        }
    
        if (!result) {
            pas_compact_atomic_segregated_global_size_directory_ptr* head;
            pas_segregated_global_size_directory* basic_size_directory_and_head;

            if (best_page_config) {
                object_size = compute_ideal_object_size(
                    heap,
                    PAS_MAX(object_size,
                            pas_segregated_page_config_min_align(*best_page_config)),
                    alignment, best_page_config);
            } else {
                PAS_ASSERT(!is_utility);
                
                /* best_bytes_dirtied_per_object has the right object size computed by the bitfit heap,
                   so just reuse that. */
                object_size = best_bytes_dirtied_per_object;
            }
        
            if (best_page_config)
                alignment = PAS_MAX(alignment, pas_segregated_page_config_min_align(*best_page_config));

            PAS_ASSERT((unsigned)object_size == object_size);
            PAS_ASSERT((unsigned)alignment == alignment);

            if (verbose) {
                pas_log("Going to create a directory for object_size = %zu, alignment = %zu, "
                        "best_page_config = %s.\n",
                        object_size, alignment,
                        best_page_config
                        ? pas_segregated_page_config_kind_get_string(best_page_config->kind)
                        : "null");
            }
            
            while (pas_is_aligned(object_size, alignment << 1))
                alignment <<= 1;

            if (verbose)
                pas_log("Bumped alignment up to %zu.\n", alignment);
        
            result = pas_segregated_global_size_directory_create(
                heap,
                (unsigned)object_size,
                (unsigned)alignment,
                config,
                best_page_config);
            if (verbose)
                printf("Created size class = %p\n", result);

            basic_size_directory_and_head = pas_compact_atomic_segregated_global_size_directory_ptr_load(
                &heap->basic_size_directory_and_head);
            if (basic_size_directory_and_head
                && basic_size_directory_and_head->base.is_basic_size_directory)
                head = &basic_size_directory_and_head->next_for_heap;
            else
                head = &heap->basic_size_directory_and_head;

            pas_compact_atomic_segregated_global_size_directory_ptr_store(
                &result->next_for_heap,
                pas_compact_atomic_segregated_global_size_directory_ptr_load(head));
            pas_compact_atomic_segregated_global_size_directory_ptr_store(head, result);
        }
        
        if (cached_index
            ? *cached_index == UINT_MAX || *cached_index == index
            : index == 1) {
            pas_compact_atomic_segregated_global_size_directory_ptr* prev_next_ptr;
            bool did_find_result;
            pas_segregated_global_size_directory* directory;
            
            if (verbose) {
                printf("pas_segregated_heap_ensure_size_directory_for_count: "
                       "Caching as basic size class!\n");
            }
            directory = pas_compact_atomic_segregated_global_size_directory_ptr_load(
                &heap->basic_size_directory_and_head);
            PAS_ASSERT(!directory || !directory->base.is_basic_size_directory); 
            if (cached_index) {
                PAS_ASSERT(*cached_index == UINT_MAX || *cached_index == index);
                *cached_index = (unsigned)index;
            }

            /* Gotta move `result` to the head of the list and set its bit. NOTE: If we just added
               created and `result` then it will be at the start of the list. It's unusual for us
               to experience the O(n) case of this loop. */
            did_find_result = false;
            for (prev_next_ptr = &heap->basic_size_directory_and_head;
                 !pas_compact_atomic_segregated_global_size_directory_ptr_is_null(prev_next_ptr);
                 prev_next_ptr =
                     &pas_compact_atomic_segregated_global_size_directory_ptr_load(
                         prev_next_ptr)->next_for_heap) {
                if (pas_compact_atomic_segregated_global_size_directory_ptr_load(prev_next_ptr)
                    == result) {
                    pas_compact_atomic_segregated_global_size_directory_ptr_store(
                        prev_next_ptr,
                        pas_compact_atomic_segregated_global_size_directory_ptr_load(
                            &result->next_for_heap));
                    pas_compact_atomic_segregated_global_size_directory_ptr_store(
                        &result->next_for_heap, NULL);
                    did_find_result = true;
                    break;
                }
                /* If we're going to go through the effort of scanning this list we might as
                   well check some basic properties of it. */
                PAS_ASSERT(!pas_compact_atomic_segregated_global_size_directory_ptr_load(
                               prev_next_ptr)->base.is_basic_size_directory);
            }
            PAS_ASSERT(did_find_result);
            PAS_ASSERT(pas_compact_atomic_segregated_global_size_directory_ptr_is_null(
                           &result->next_for_heap));
            pas_compact_atomic_segregated_global_size_directory_ptr_store(
                &result->next_for_heap,
                pas_compact_atomic_segregated_global_size_directory_ptr_load(
                    &heap->basic_size_directory_and_head));
            pas_compact_atomic_segregated_global_size_directory_ptr_store(
                &heap->basic_size_directory_and_head, result);
            result->base.is_basic_size_directory = true;
        } else {
            if (verbose) {
                printf("pas_segregated_heap_ensure_size_directory_for_count: "
                       "NOT caching as basic size class!\n");
            }
        }

        if (verbose) {
            pas_log("index = %zu, object_size = %zu, index upper bound = %u.\n",
                    index, object_size, heap->small_index_upper_bound);
        }
    
        if (index < heap->small_index_upper_bound) {
            size_t candidate_index;
            pas_compact_atomic_segregated_global_size_directory_ptr* index_to_small_size_directory;
            pas_allocator_index* index_to_small_allocator_index;

            if (verbose) {
                pas_log("Installing starting at index = %zu up to %u\n",
                        index, heap->small_index_upper_bound);
            }

            index_to_small_size_directory = heap->index_to_small_size_directory;
            index_to_small_allocator_index = heap->index_to_small_allocator_index;
        
            PAS_ASSERT(pas_compact_atomic_segregated_global_size_directory_ptr_is_null(
                           index_to_small_size_directory + index));
            if (!is_utility)
                PAS_ASSERT(index_to_small_allocator_index[index] == (pas_allocator_index)UINT_MAX);
        
            /* Install this result in all indices starting with this one that don't already have a
               size class and where this size class would be big enough. */
            for (candidate_index = index;
                 candidate_index < heap->small_index_upper_bound
                 && (pas_heap_get_type_size(parent_heap) *
                     pas_segregated_heap_count_for_index(heap,
                                                         candidate_index,
                                                         *config) <= object_size);
                 ++candidate_index) {
                pas_segregated_global_size_directory* candidate;

                if (verbose)
                    pas_log("Installing at index candidate_index = %zu.\n", candidate_index);
            
                candidate = pas_compact_atomic_segregated_global_size_directory_ptr_load(
                    index_to_small_size_directory + candidate_index);
                if (candidate) {
                    if (verbose)
                        pas_log("Have candidate with size = %zu\n", candidate->object_size);
                    
                    /* If the candidate at this index has an object size that is no larger than the
                       one we picked, then we should have just simply used this candidate for our
                       allocations! */
                    PAS_ASSERT(candidate == result || candidate->object_size > object_size);
                    break;
                }

                pas_compact_atomic_segregated_global_size_directory_ptr_store(
                    heap->index_to_small_size_directory + candidate_index, result);
            }
        }
    
        medium_install_count = object_size / pas_heap_get_type_size(parent_heap);
        medium_install_index = pas_segregated_heap_index_for_count(
            heap, medium_install_count, *config);

        if (verbose)
            pas_log("medium_install_index = %zu\n", medium_install_index);
        
        if (medium_install_index >= compute_small_index_upper_bound(heap, config)) {
            pas_segregated_heap_rare_data* rare_data;
            pas_segregated_heap_medium_directory_tuple* next_tuple;
            size_t tuple_insertion_index;
        
            if (heap->small_index_upper_bound)
                PAS_ASSERT(medium_install_index >= heap->small_index_upper_bound);

            rare_data = pas_segregated_heap_rare_data_ptr_load(&heap->rare_data);
            if (!rare_data) {
                rare_data = pas_immortal_heap_allocate(
                    sizeof(pas_segregated_heap_rare_data),
                    "pas_segregated_heap_rare_data",
                    pas_object_allocation);
            
                pas_zero_memory(rare_data, sizeof(pas_segregated_heap_rare_data));
            
                rare_data->mutation_count = PAS_MUTATION_COUNT_INITIALIZER;
            
                pas_fence();

                pas_segregated_heap_rare_data_ptr_store(&heap->rare_data, rare_data);
            }
        
            next_tuple = pas_segregated_heap_medium_directory_tuple_for_index(
                heap, index,
                pas_segregated_heap_medium_size_directory_search_least_greater_equal,
                pas_lock_is_held);
        
            if (next_tuple &&
                pas_compact_atomic_segregated_global_size_directory_ptr_load(
                    &next_tuple->directory) == result) {
                size_t begin_index;
                begin_index = PAS_MIN(index, (size_t)next_tuple->begin_index);
                PAS_ASSERT((pas_segregated_heap_medium_directory_index)begin_index == begin_index);
                next_tuple->begin_index = (pas_segregated_heap_medium_directory_index)begin_index;
            } else {
                pas_segregated_heap_medium_directory_tuple* medium_directories;
                pas_segregated_heap_medium_directory_tuple* medium_directory;

                medium_directories = pas_segregated_heap_medium_directory_tuple_ptr_load(
                    &rare_data->medium_directories);
                
                if (next_tuple) {
                    /* Whatever we find has to have a larger object size than what we picked because
                       otherwise we should have just returned that.
               
                       We assert that in two parts so that we can tell how bad things got if the assert
                       starts failing.
            
                       First, obviously, it should not be an exact match to the original index that we
                       asked for.*/
                    PAS_ASSERT(index < next_tuple->end_index);
            
                    /* And it should be a larger index than what we are adding. */
                    PAS_ASSERT(medium_install_index < next_tuple->end_index);
            
                    /* Obviously that means that our object_size has to be smaller than the existing
                       one. */
                    PAS_ASSERT(
                        object_size <
                        pas_compact_atomic_segregated_global_size_directory_ptr_load(
                            &next_tuple->directory)->object_size);
            
                    PAS_ASSERT(next_tuple - medium_directories
                               < rare_data->num_medium_directories);
            
                    tuple_insertion_index = next_tuple - medium_directories;
                } else
                    tuple_insertion_index = rare_data->num_medium_directories;
        
                pas_mutation_count_start_mutating(&rare_data->mutation_count);
                
                if (rare_data->num_medium_directories
                    >= rare_data->medium_directories_capacity) {
                    pas_segregated_heap_medium_directory_tuple* new_directories;
                    unsigned new_capacity;
            
                    new_capacity = (rare_data->medium_directories_capacity + 1) << 1;
            
                    new_directories = pas_immortal_heap_allocate(
                        sizeof(pas_segregated_heap_medium_directory_tuple) * new_capacity,
                        "pas_segregated_heap_rare_data/medium_directories",
                        pas_object_allocation);
            
                    memcpy(new_directories, medium_directories,
                           sizeof(pas_segregated_heap_medium_directory_tuple)
                           * rare_data->num_medium_directories);
            
                    pas_fence(); /* This fences is probably not needed but it's cheap in this context
                                    and adds some sanity. */

                    pas_segregated_heap_medium_directory_tuple_ptr_store(
                        &rare_data->medium_directories, new_directories);

                    medium_directories = new_directories;
            
                    pas_fence(); /* This fence ensures that we don't see updated length until after we
                                    see the bigger directories pointer. It's an essential fence. */
            
                    rare_data->medium_directories_capacity = new_capacity;

                    check_medium_directories(heap);
                }
                
                PAS_ASSERT(rare_data->num_medium_directories
                           < rare_data->medium_directories_capacity);
                
                memmove(medium_directories + tuple_insertion_index + 1,
                        medium_directories + tuple_insertion_index,
                        sizeof(pas_segregated_heap_medium_directory_tuple) *
                        (rare_data->num_medium_directories - tuple_insertion_index));
                
                PAS_ASSERT((pas_segregated_heap_medium_directory_index)index == index);
                PAS_ASSERT((pas_segregated_heap_medium_directory_index)medium_install_index
                           == medium_install_index);
                medium_directory = medium_directories + tuple_insertion_index;
                pas_compact_atomic_segregated_global_size_directory_ptr_store(
                    &medium_directory->directory, result);
                medium_directory->allocator_index = (pas_allocator_index)UINT_MAX;

                if (verbose) {
                    pas_log("In rare_data = %p, Installing medium tuple %zu...%zu\n",
                            rare_data, index, medium_install_index);
                }
                
                medium_directory->begin_index = (pas_segregated_heap_medium_directory_index)index;
                medium_directory->end_index =
                    (pas_segregated_heap_medium_directory_index)medium_install_index;
                
                pas_fence(); /* This fence is probably not needed but it's cheap in this context
                                and adds some sanity. */
                
                rare_data->num_medium_directories++;
                
                check_medium_directories(heap);
                
                pas_mutation_count_stop_mutating(&rare_data->mutation_count);
            }
        }
        
        PAS_ASSERT(pas_segregated_heap_size_directory_for_index(heap, index, cached_index)
                   == result);
    }

    return result;
}

size_t pas_segregated_heap_get_num_free_bytes(pas_segregated_heap* heap)
{
    return pas_segregated_heap_compute_summary(heap).free;
}

bool pas_segregated_heap_for_each_global_size_directory(
    pas_segregated_heap* heap,
    pas_segregated_heap_for_each_global_size_directory_callback callback,
    void *arg)
{
    pas_segregated_global_size_directory* directory;

    for (directory = pas_compact_atomic_segregated_global_size_directory_ptr_load(
             &heap->basic_size_directory_and_head);
         directory;
         directory = pas_compact_atomic_segregated_global_size_directory_ptr_load(
             &directory->next_for_heap)) {
        if (!callback(heap, directory, arg))
            return false;
    }
    
    return true;
}

typedef struct {
    pas_segregated_heap* heap;
    pas_segregated_heap_for_each_committed_view_callback callback;
    void *arg;
} for_each_committed_page_data;

static bool for_each_committed_size_directory_callback(
    pas_segregated_heap* heap,
    pas_segregated_global_size_directory* size_directory,
    void* arg)
{
    for_each_committed_page_data* data;
    pas_segregated_directory* directory;
    size_t index;
    PAS_UNUSED_PARAM(heap);
    data = arg;
    directory = &size_directory->base;
    for (index = 0; index < pas_segregated_directory_size(directory); ++index) {
        if (pas_segregated_directory_is_committed(directory, index)) {
            if (!data->callback(
                    data->heap,
                    size_directory,
                    pas_segregated_directory_get(directory, index),
                    data->arg))
                return false;
        }
    }
    return true;
}

bool pas_segregated_heap_for_each_committed_view(
    pas_segregated_heap* heap,
    pas_segregated_heap_for_each_committed_view_callback callback,
    void* arg)
{
    for_each_committed_page_data data;
    data.heap = heap;
    data.callback = callback;
    data.arg = arg;
    return pas_segregated_heap_for_each_global_size_directory(
        heap,
        for_each_committed_size_directory_callback,
        &data);
}

typedef struct {
    pas_segregated_heap* heap;
    pas_segregated_heap_for_each_view_index_callback callback;
    void *arg;
} for_each_view_index_data;

static bool for_each_view_index_directory_callback(pas_segregated_heap* heap,
                                                   pas_segregated_global_size_directory* size_directory,
                                                   void* arg)
{
    for_each_view_index_data* data;
    pas_segregated_directory* directory;
    size_t index;
    PAS_UNUSED_PARAM(heap);
    data = arg;
    directory = &size_directory->base;
    for (index = 0; index < pas_segregated_directory_size(directory); ++index) {
        if (!data->callback(
                data->heap,
                size_directory,
                index,
                pas_segregated_directory_get(directory, index),
                data->arg))
            return false;
    }
    return true;
}

bool pas_segregated_heap_for_each_view_index(
    pas_segregated_heap* heap,
    pas_segregated_heap_for_each_view_index_callback callback,
    void* arg)
{
    for_each_view_index_data data;
    data.heap = heap;
    data.callback = callback;
    data.arg = arg;
    return pas_segregated_heap_for_each_global_size_directory(
        heap,
        for_each_view_index_directory_callback,
        &data);
}

typedef struct {
    pas_segregated_heap* heap;
    pas_segregated_heap_for_each_live_object_callback callback;
    void* arg;
} for_each_live_object_data;

static bool for_each_live_object_object_callback(pas_segregated_global_size_directory* directory,
                                                 pas_segregated_view view,
                                                 uintptr_t begin,
                                                 void* arg)
{
    for_each_live_object_data* data = arg;
    PAS_UNUSED_PARAM(view);
    return data->callback(data->heap, begin, directory->object_size, data->arg);
}

static bool for_each_live_object_directory_callback(
    pas_segregated_heap* heap,
    pas_segregated_global_size_directory* directory,
    void* arg)
{
    PAS_UNUSED_PARAM(heap);
    return pas_segregated_global_size_directory_for_each_live_object(
        directory,
        for_each_live_object_object_callback,
        arg);
}

static bool for_each_live_object_bitfit_callback(
    pas_bitfit_heap* heap,
    pas_bitfit_view* view,
    uintptr_t begin,
    size_t size,
    void* arg)
{
    for_each_live_object_data* data;

    PAS_UNUSED_PARAM(heap);
    PAS_UNUSED_PARAM(view);
    data = arg;
    
    return data->callback(data->heap, begin, size, data->arg);
}

bool pas_segregated_heap_for_each_live_object(
    pas_segregated_heap* heap,
    pas_segregated_heap_for_each_live_object_callback callback,
    void* arg)
{
    for_each_live_object_data data;
    pas_bitfit_heap* bitfit_heap;
    
    data.heap = heap;
    data.callback = callback;
    data.arg = arg;
    
    if (!pas_segregated_heap_for_each_global_size_directory(
            heap,
            for_each_live_object_directory_callback,
            &data))
        return false;

    bitfit_heap = pas_compact_atomic_bitfit_heap_ptr_load(&heap->bitfit_heap);
    if (!bitfit_heap)
        return true;

    return pas_bitfit_heap_for_each_live_object(
        bitfit_heap,
        for_each_live_object_bitfit_callback,
        &data);
}

typedef struct {
    size_t result;
} num_committed_views_data;

static bool num_committed_views_directory_callback(
    pas_segregated_heap* heap,
    pas_segregated_global_size_directory* directory,
    void* arg)
{
    num_committed_views_data* data;
    
    PAS_UNUSED_PARAM(heap);

    data = arg;
    
    data->result += pas_segregated_directory_num_committed_views(&directory->base);
    
    return true;
}

size_t pas_segregated_heap_num_committed_views(pas_segregated_heap* heap)
{
    num_committed_views_data data;

    data.result = 0;
    
    pas_segregated_heap_for_each_global_size_directory(
        heap, num_committed_views_directory_callback, &data);
    
    return data.result;
}

typedef struct {
    size_t result;
} num_empty_views_data;

static bool num_empty_views_directory_callback(
    pas_segregated_heap* heap,
    pas_segregated_global_size_directory* directory,
    void* arg)
{
    num_empty_views_data* data;
    
    PAS_UNUSED_PARAM(heap);

    data = arg;
    
    data->result += pas_segregated_directory_num_empty_views(&directory->base);
    
    return true;
}

size_t pas_segregated_heap_num_empty_views(pas_segregated_heap* heap)
{
    num_empty_views_data data;

    data.result = 0;
    
    pas_segregated_heap_for_each_global_size_directory(
        heap, num_empty_views_directory_callback, &data);
    
    return data.result;
}

typedef struct {
    size_t result;
} num_empty_granules_data;

static bool num_empty_granules_directory_callback(
    pas_segregated_heap* heap,
    pas_segregated_global_size_directory* directory,
    void* arg)
{
    num_empty_granules_data* data;

    PAS_UNUSED_PARAM(heap);

    data = arg;

    data->result += pas_segregated_directory_num_empty_granules(&directory->base);

    return true;
}

size_t pas_segregated_heap_num_empty_granules(pas_segregated_heap* heap)
{
    num_empty_granules_data data;

    data.result = 0;

    pas_segregated_heap_for_each_global_size_directory(
        heap, num_empty_granules_directory_callback, &data);

    return data.result;
}

typedef struct {
    size_t result;
} num_views_data;

static bool num_views_directory_callback(
    pas_segregated_heap* heap,
    pas_segregated_global_size_directory* directory,
    void* arg)
{
    num_views_data* data;
    
    PAS_UNUSED_PARAM(heap);

    data = arg;
    
    data->result += pas_segregated_directory_size(&directory->base);
    
    return true;
}

size_t pas_segregated_heap_num_views(pas_segregated_heap* heap)
{
    num_views_data data;
    
    data.result = 0;
    
    pas_segregated_heap_for_each_global_size_directory(
        heap, num_views_directory_callback, &data);
    
    return data.result;
}

static bool compute_summary_directory_callback(
    pas_segregated_heap* heap,
    pas_segregated_global_size_directory* directory,
    void* arg)
{
    pas_heap_summary* summary;
    
    PAS_UNUSED_PARAM(heap);

    summary = arg;
    
    *summary = pas_heap_summary_add(
        *summary,
        pas_segregated_directory_compute_summary(&directory->base));
    
    return true;
}

pas_heap_summary pas_segregated_heap_compute_summary(pas_segregated_heap* heap)
{
    pas_heap_summary result;
    pas_bitfit_heap* bitfit_heap;
    
    result = pas_heap_summary_create_empty();
    
    pas_segregated_heap_for_each_global_size_directory(
        heap, compute_summary_directory_callback, &result);

    bitfit_heap = pas_compact_atomic_bitfit_heap_ptr_load(&heap->bitfit_heap);
    if (bitfit_heap)
        result = pas_heap_summary_add(result, pas_bitfit_heap_compute_summary(bitfit_heap));
    
    return result;
}

#endif /* LIBPAS_ENABLED */
