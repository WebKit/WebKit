/*
 * Copyright (c) 2018-2022 Apple Inc. All rights reserved.
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
#include "pas_compact_expendable_memory.h"
#include "pas_fd_stream.h"
#include "pas_heap.h"
#include "pas_heap_ref.h"
#include "pas_large_expendable_memory.h"
#include "pas_large_utility_free_heap.h"
#include "pas_min_heap.h"
#include "pas_segregated_heap_inlines.h"
#include "pas_segregated_size_directory.h"
#include "pas_segregated_page.h"
#include "pas_thread_local_cache.h"
#include "pas_thread_local_cache_layout.h"
#include "pas_utility_heap_config.h"

unsigned pas_segregated_heap_num_size_lookup_rematerializations;

static void check_size_lookup_recomputation_if_appropriate(pas_segregated_heap* heap,
                                                           const pas_heap_config* config,
                                                           unsigned *cached_index,
                                                           const char* where);

static size_t min_align_for_heap(pas_segregated_heap* heap,
                                 const pas_heap_config* config)
{
    pas_segregated_page_config_variant segregated_variant;
    pas_bitfit_page_config_variant bitfit_variant;
    for (PAS_EACH_SEGREGATED_PAGE_CONFIG_VARIANT_ASCENDING(segregated_variant)) {
        const pas_segregated_page_config* page_config;
        page_config =
            pas_heap_config_segregated_page_config_ptr_for_variant(config, segregated_variant);
        if (!pas_segregated_page_config_is_enabled(*page_config, heap->runtime_config))
            continue;
        return pas_segregated_page_config_min_align(*page_config);
    }
    /* If segregated page configs are totally disabled then we want to give the minimum size according
       to bitfit. */
    for (PAS_EACH_BITFIT_PAGE_CONFIG_VARIANT_ASCENDING(bitfit_variant)) {
        const pas_bitfit_page_config* page_config;
        page_config =
            pas_heap_config_bitfit_page_config_ptr_for_variant(config, bitfit_variant);
        if (!pas_bitfit_page_config_is_enabled(*page_config, heap->runtime_config))
            continue;
        return pas_page_base_config_min_align(page_config->base);
    }
    /* Allow segregated and bitfit to both be disabled. To make this work, we lie and say that the
       minimum object size is whatever would be minimum to the large heap. */
    return config->large_alignment;
}

static size_t min_object_size_for_heap(pas_segregated_heap* heap,
                                       const pas_heap_config* config)
{
    return min_align_for_heap(heap, config);
}

static size_t max_object_size_for_page_config(pas_heap* parent_heap,
                                              const pas_page_base_config* page_config)
{
    static const bool verbose = false;
    
    size_t result;

    if (verbose) {
        pas_log("max_object_size for %p (%s): %zu\n", &parent_heap->segregated_heap,
                pas_page_base_config_get_kind_string(page_config), page_config->max_object_size);
    }
    
    result = pas_round_down_to_power_of_2(
        page_config->max_object_size,
        pas_page_base_config_min_align(*page_config));
    
    return result;
}

static size_t max_segregated_object_size_for_heap(pas_heap* parent_heap,
                                                  pas_segregated_heap* heap,
                                                  const pas_heap_config* config)
{
    static const bool verbose = false;
    
    pas_segregated_page_config_variant variant;
    for (PAS_EACH_SEGREGATED_PAGE_CONFIG_VARIANT_DESCENDING(variant)) {
        const pas_segregated_page_config* page_config;
        size_t max_size_per_config;
        size_t result;
        
        page_config = pas_heap_config_segregated_page_config_ptr_for_variant(config, variant);
        if (!pas_segregated_page_config_is_enabled(*page_config, heap->runtime_config))
            continue;

        max_size_per_config = max_object_size_for_page_config(parent_heap, &page_config->base);
        
        result = PAS_MIN((size_t)heap->runtime_config->max_segregated_object_size,
                         max_size_per_config);
        if (verbose)
            pas_log("returning max for segregated for heap = %p: %zu\n", heap, (size_t)result);
        return result;
    }
    return 0;
}

static size_t max_bitfit_object_size_for_heap(pas_heap* parent_heap,
                                              pas_segregated_heap* heap,
                                              const pas_heap_config* config)
{
    static const bool verbose = false;
    
    pas_bitfit_page_config_variant variant;
    
    for (PAS_EACH_BITFIT_PAGE_CONFIG_VARIANT_DESCENDING(variant)) {
        const pas_bitfit_page_config* page_config;
        size_t max_size_per_config;
        size_t result;
        
        page_config = pas_heap_config_bitfit_page_config_ptr_for_variant(config, variant);
        
        if (verbose) {
            pas_log("Considering %s.\n",
                    pas_bitfit_page_config_kind_get_string(page_config->kind));
        }

        if (!pas_bitfit_page_config_is_enabled(*page_config, heap->runtime_config)) {
            if (verbose) {
                pas_log("Not considering %s because it's disabled.\n",
                        pas_bitfit_page_config_kind_get_string(page_config->kind));
            }
            continue;
        }

        if (verbose) {
            pas_log("Returning the min of %u and %zu\n",
                    heap->runtime_config->max_bitfit_object_size,
                    max_object_size_for_page_config(parent_heap, &page_config->base));
        }

        if (verbose) {
            pas_log("Going to return max for %s but heap = %p, config = %p, max = %zu\n",
                    pas_bitfit_page_config_kind_get_string(page_config->kind),
                    heap, heap->runtime_config, (size_t)heap->runtime_config->max_bitfit_object_size);
        }

        max_size_per_config = max_object_size_for_page_config(parent_heap, &page_config->base);
        
        result = PAS_MIN((size_t)heap->runtime_config->max_bitfit_object_size,
                         max_size_per_config);

        if (verbose)
            pas_log("returning max for bitfit for heap = %p: %zu\n", heap, result);

        return result;
    }

    if (verbose)
        pas_log("Bitfit has a max object size of zero because it's all disabled.\n");
    
    return 0;
}

static size_t max_object_size_for_heap(pas_heap* parent_heap,
                                       pas_segregated_heap* heap,
                                       const pas_heap_config* config)
{
    return PAS_MAX(max_segregated_object_size_for_heap(parent_heap, heap, config),
                   max_bitfit_object_size_for_heap(parent_heap, heap, config));
}

void pas_segregated_heap_construct(pas_segregated_heap* segregated_heap,
                                   pas_heap* parent_heap,
                                   const pas_heap_config* config,
                                   pas_heap_runtime_config* runtime_config)
{
    /* NOTE: the various primitive and utility heaps are constructed
       specially. */

    PAS_ASSERT(runtime_config);
    PAS_ASSERT(runtime_config->sharing_mode != pas_invalid_sharing_mode);

    segregated_heap->runtime_config = runtime_config;
    
    pas_compact_atomic_segregated_size_directory_ptr_store(
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
                                                const pas_heap_config* heap_config,
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

size_t pas_segregated_heap_get_cached_index_for_heap_type(pas_segregated_heap* heap,
                                                          const pas_heap_config* config)
{
    return pas_segregated_heap_index_for_size(
        pas_heap_get_type_size(pas_heap_for_segregated_heap(heap)), *config);
}

bool pas_segregated_heap_cached_index_is_set(unsigned* cached_index)
{
    if (!cached_index)
        return true;
    return *cached_index != UINT_MAX;
}

size_t pas_segregated_heap_get_cached_index(pas_segregated_heap* heap,
                                            unsigned* cached_index,
                                            const pas_heap_config* config)
{
    if (cached_index)
        return *cached_index;
    return pas_segregated_heap_get_cached_index_for_heap_type(heap, config);
}

bool pas_segregated_heap_index_is_cached_index_and_cached_index_is_set(pas_segregated_heap* heap,
                                                                       unsigned* cached_index,
                                                                       size_t index,
                                                                       const pas_heap_config* config)
{
    return pas_segregated_heap_cached_index_is_set(cached_index)
        && index == pas_segregated_heap_get_cached_index(heap, cached_index, config);
}

bool pas_segregated_heap_index_is_cached_index_or_cached_index_is_unset(pas_segregated_heap* heap,
                                                                        unsigned* cached_index,
                                                                        size_t index,
                                                                        const pas_heap_config* config)
{
    return !pas_segregated_heap_cached_index_is_set(cached_index)
        || index == pas_segregated_heap_get_cached_index(heap, cached_index, config);
}

bool pas_segregated_heap_index_is_not_cached_index_and_cached_index_is_set(pas_segregated_heap* heap,
                                                                           unsigned* cached_index,
                                                                           size_t index,
                                                                           const pas_heap_config* config)
{
    return pas_segregated_heap_cached_index_is_set(cached_index)
        && index != pas_segregated_heap_get_cached_index(heap, cached_index, config);
}

bool pas_segregated_heap_index_is_greater_than_cached_index_and_cached_index_is_set(pas_segregated_heap* heap,
                                                                                    unsigned* cached_index,
                                                                                    size_t index,
                                                                                    const pas_heap_config* config)
{
    return pas_segregated_heap_cached_index_is_set(cached_index)
        && index > pas_segregated_heap_get_cached_index(heap, cached_index, config);
}

bool pas_segregated_heap_index_is_greater_equal_cached_index_and_cached_index_is_set(pas_segregated_heap* heap,
                                                                                     unsigned* cached_index,
                                                                                     size_t index,
                                                                                     const pas_heap_config* config)
{
    return pas_segregated_heap_cached_index_is_set(cached_index)
        && index >= pas_segregated_heap_get_cached_index(heap, cached_index, config);
}

pas_segregated_size_directory* pas_segregated_heap_size_directory_for_index_slow(
    pas_segregated_heap* heap,
    size_t index,
    unsigned* cached_index,
    const pas_heap_config* config)
{
    if (pas_segregated_heap_index_is_cached_index_and_cached_index_is_set(heap, cached_index, index, config)) {
        pas_segregated_size_directory* result;
        result = pas_compact_atomic_segregated_size_directory_ptr_load(
            &heap->basic_size_directory_and_head);
        if (result && result->base.is_basic_size_directory)
            return result;
    }

    if (index < (size_t)heap->small_index_upper_bound)
        return NULL;
    
    return pas_segregated_heap_medium_size_directory_for_index(
        heap, index,
        pas_segregated_heap_medium_size_directory_search_within_size_class_progression,
        pas_lock_is_held);
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

        /* This is necessary to guard against the medium tuple array being decommitted at the wrong time,
           or the tuple straddling page boundary, leading to the begin index being zero and the end_index
           having its original value. */
        if (!begin_index) {
            result.tuple = NULL;
            return result;
        }
        
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
    static const bool verbose = false;
    
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
            &rare_data->mutation_count, saved_count, result.dependency)) {
        if (verbose && !result.tuple)
            pas_log("did not find tuple\n");
        return result.tuple;
    }
    
    return medium_directory_tuple_for_index_with_lock(
        heap, index, search_mode, pas_lock_is_not_held);
}

unsigned pas_segregated_heap_medium_allocator_index_for_index(
    pas_segregated_heap* heap,
    size_t index,
    pas_segregated_heap_medium_size_directory_search_mode search_mode,
    pas_lock_hold_mode heap_lock_hold_mode)
{
    static const bool verbose = false;
    
    pas_segregated_heap_medium_directory_tuple* medium_directory;
    
    medium_directory = pas_segregated_heap_medium_directory_tuple_for_index(
        heap, index, search_mode, heap_lock_hold_mode);
    
    if (medium_directory) {
        unsigned result;
        result = medium_directory->allocator_index;
        if (verbose && !result)
            pas_log("found null allocator index\n");
        return result;
    }
    
    return 0;
}

pas_segregated_size_directory* pas_segregated_heap_medium_size_directory_for_index(
    pas_segregated_heap* heap,
    size_t index,
    pas_segregated_heap_medium_size_directory_search_mode search_mode,
    pas_lock_hold_mode heap_lock_hold_mode)
{
    pas_segregated_heap_medium_directory_tuple* medium_directory;
    
    medium_directory = pas_segregated_heap_medium_directory_tuple_for_index(
        heap, index, search_mode, heap_lock_hold_mode);
    
    if (medium_directory)
        return pas_compact_atomic_segregated_size_directory_ptr_load(&medium_directory->directory);
    
    return NULL;
}

static size_t compute_small_index_upper_bound(pas_segregated_heap* heap,
                                              const pas_heap_config* config)
{
    if (heap->small_index_upper_bound) {
        /* This is important: some clients, like the intrinsic_heap, will initialize
           small_index_upper_bound to something specific. */
        return heap->small_index_upper_bound;
    }
    
    return pas_segregated_heap_index_for_size(config->small_lookup_size_upper_bound, *config) + 1;
}

static void ensure_size_lookup(pas_segregated_heap* heap,
                               const pas_heap_config* config)
{
    static const bool verbose = false;
    
    size_t index_upper_bound;
    pas_compact_atomic_segregated_size_directory_ptr* index_to_size_directory;
    pas_allocator_index* index_to_allocator_index;
    unsigned index;
    
    if (heap->small_index_upper_bound)
        return;

    if (verbose)
        pas_log("%p(%s): Ensuring size lookup!\n", heap, pas_heap_config_kind_get_string(config->kind));

    PAS_ASSERT(!heap->runtime_config->statically_allocated);
    PAS_ASSERT(!pas_heap_config_is_utility(config));
    
    index_upper_bound = compute_small_index_upper_bound(heap, config);
    
    index_to_size_directory = pas_large_expendable_memory_allocate(
        sizeof(pas_compact_atomic_segregated_size_directory_ptr) * index_upper_bound,
        sizeof(pas_compact_atomic_segregated_size_directory_ptr),
        "pas_segregated_heap/index_to_size_directory");
    index_to_allocator_index = pas_large_expendable_memory_allocate(
        sizeof(pas_allocator_index) * index_upper_bound,
        sizeof(pas_allocator_index),
        "pas_segregated_heap/index_to_allocator_index");
    
    for (index = 0; index < index_upper_bound; ++index) {
        pas_compact_atomic_segregated_size_directory_ptr_store(
            index_to_size_directory + index, NULL);
        index_to_allocator_index[index] = 0;
    }
    
    pas_fence();

    PAS_ASSERT((unsigned)index_upper_bound == index_upper_bound);
    heap->small_index_upper_bound = (unsigned)index_upper_bound;
    heap->index_to_small_size_directory = index_to_size_directory;
    heap->index_to_small_allocator_index = index_to_allocator_index;
}

static inline int size_directory_min_heap_compare(pas_segregated_size_directory** a_ptr,
                                                  pas_segregated_size_directory** b_ptr)
{
    pas_segregated_size_directory* a;
    pas_segregated_size_directory* b;

    a = *a_ptr;
    b = *b_ptr;

    if (a->object_size < b->object_size)
        return -1;
    if (a->object_size > b->object_size)
        return 1;
    return 0;
}

static inline size_t size_directory_min_heap_get_index(pas_segregated_size_directory** entry_ptr)
{
    PAS_UNUSED_PARAM(entry_ptr);
    PAS_ASSERT(!"Should not be reached");
    return 0;
}

static inline void size_directory_min_heap_set_index(pas_segregated_size_directory** entry_ptr,
                                                     size_t index)
{
    PAS_UNUSED_PARAM(entry_ptr);
    PAS_UNUSED_PARAM(index);
}

PAS_CREATE_MIN_HEAP(size_directory_min_heap, pas_segregated_size_directory*, 200,
                    .compare = size_directory_min_heap_compare,
                    .get_index = size_directory_min_heap_get_index,
                    .set_index = size_directory_min_heap_set_index);

/* NOTE: It's possible for this to call set_index_to_small_allocator_index for values of index that previously
   didn't have an allocator_index set. */
static void recompute_size_lookup(pas_segregated_heap* heap,
                                  const pas_heap_config* config,
                                  unsigned* cached_index,
                                  void (*set_index_to_small_allocator_index)(
                                      size_t index, pas_allocator_index value, void* arg),
                                  void (*set_index_to_small_size_directory)(
                                      size_t index, pas_segregated_size_directory* directory, void* arg),
                                  void (*set_medium_directory_tuple)(
                                      size_t medium_tuple_index,
                                      pas_segregated_heap_medium_directory_tuple* value,
                                      void* arg),
                                  void* arg)
{
    pas_segregated_size_directory* directory;
    size_directory_min_heap min_heap;
    size_t medium_tuple_index;

    if (!heap->small_index_upper_bound) {
        pas_segregated_heap_rare_data* rare_data;
        rare_data = pas_segregated_heap_rare_data_ptr_load(&heap->rare_data);
        if (!rare_data)
            return;
        if (!rare_data->num_medium_directories) {
            PAS_ASSERT(!rare_data->medium_directories_capacity);
            return;
        }
    }
    
    size_directory_min_heap_construct(&min_heap);

    for (directory = pas_compact_atomic_segregated_size_directory_ptr_load(
             &heap->basic_size_directory_and_head);
         directory;
         directory = pas_compact_atomic_segregated_size_directory_ptr_load(&directory->next_for_heap)) {
        size_t index;
        pas_allocator_index allocator_index;
        size_t extra_index_for_allocator;
        bool have_extra_index_for_allocator;

        allocator_index = directory->allocator_index;

        PAS_ASSERT(allocator_index != (pas_allocator_index)UINT_MAX);

        have_extra_index_for_allocator = false;
        extra_index_for_allocator = 0;
        if (allocator_index
            && directory->base.is_basic_size_directory
            && pas_segregated_heap_cached_index_is_set(cached_index)) {
            index = pas_segregated_heap_get_cached_index(heap, cached_index, config);
            if (index < heap->small_index_upper_bound) {
                /* It's possible that we've put the basic size directory into the allocator_index table even
                   though we haven't put it into the size directory table because array allocation could
                   find the basic size directory by looking at the basic directory pointer and then
                   ensure_size_lookup and stash the allocator_index in the allocator_index table. */
                have_extra_index_for_allocator = true;
                extra_index_for_allocator = index;
            }
        }
        
        if (pas_segregated_size_directory_min_index(directory) != UINT_MAX) {
            PAS_ASSERT(pas_segregated_size_directory_min_index(directory)
                       <= pas_segregated_heap_index_for_size(directory->object_size, *config));
            
            for (index = pas_segregated_size_directory_min_index(directory);
                 index < PAS_MIN(pas_segregated_heap_index_for_size(directory->object_size, *config) + 1,
                                 heap->small_index_upper_bound);
                 index++) {
                set_index_to_small_size_directory(index, directory, arg);
                
                if (allocator_index) {
                    set_index_to_small_allocator_index(index, allocator_index, arg);
                    
                    if (have_extra_index_for_allocator
                        && extra_index_for_allocator == index)
                        have_extra_index_for_allocator = false;
                }
            }

            if (pas_segregated_heap_index_for_size(directory->object_size, *config)
                >= heap->small_index_upper_bound) {
                size_directory_min_heap_add(
                    &min_heap, directory, &pas_large_utility_free_heap_allocation_config);
            }
        }
        
        if (have_extra_index_for_allocator)
            set_index_to_small_allocator_index(extra_index_for_allocator, allocator_index, arg);
    }

    for (medium_tuple_index = 0;
         (directory = size_directory_min_heap_take_min(&min_heap));
         medium_tuple_index++) {
        pas_segregated_heap_medium_directory_tuple tuple;

        pas_compact_atomic_segregated_size_directory_ptr_store(&tuple.directory, directory);
        tuple.allocator_index = directory->allocator_index;
        PAS_ASSERT(tuple.allocator_index != (pas_allocator_index)UINT_MAX);
        tuple.begin_index = pas_segregated_size_directory_min_index(directory);
        PAS_ASSERT(tuple.begin_index);
        tuple.end_index = (unsigned)pas_segregated_heap_index_for_size(directory->object_size, *config);
        
        set_medium_directory_tuple(medium_tuple_index, &tuple, arg);
    }

    size_directory_min_heap_destruct(&min_heap, &pas_large_utility_free_heap_allocation_config);
}

static void rematerialize_size_lookup_set_index_to_small_allocator_index(
    size_t index, pas_allocator_index value, void* arg)
{
    pas_segregated_heap* heap;

    heap = arg;

    PAS_ASSERT(heap->index_to_small_allocator_index);
    PAS_ASSERT(index < heap->small_index_upper_bound);
    heap->index_to_small_allocator_index[index] = value;
}

static void rematerialize_size_lookup_set_index_to_small_size_directory(
    size_t index, pas_segregated_size_directory* directory, void* arg)
{
    pas_segregated_heap* heap;

    heap = arg;

    PAS_ASSERT(heap->index_to_small_size_directory);
    PAS_ASSERT(index < heap->small_index_upper_bound);
    pas_compact_atomic_segregated_size_directory_ptr_store(
        heap->index_to_small_size_directory + index, directory);
}

static void rematerialize_size_lookup_set_medium_directory_tuple(
    size_t medium_tuple_index, pas_segregated_heap_medium_directory_tuple* tuple, void* arg)
{
    pas_segregated_heap* heap;
    pas_segregated_heap_rare_data* data;
    pas_segregated_heap_medium_directory_tuple* tuples;

    heap = arg;
    data = pas_segregated_heap_rare_data_ptr_load(&heap->rare_data);
    PAS_ASSERT(data);

    PAS_ASSERT(medium_tuple_index < data->num_medium_directories);
    PAS_ASSERT(medium_tuple_index < data->medium_directories_capacity);
    
    tuples = pas_segregated_heap_medium_directory_tuple_ptr_load(&data->medium_directories);
    PAS_ASSERT(tuples);
    PAS_ASSERT(tuple->begin_index);
    tuples[medium_tuple_index] = *tuple;
}

static void rematerialize_size_lookup_if_necessary(pas_segregated_heap* heap,
                                                   const pas_heap_config* config,
                                                   unsigned* cached_index)
{
    pas_segregated_heap_rare_data* data;

    pas_heap_lock_assert_held();

    if (!pas_segregated_heap_touch_lookup_tables(heap, pas_expendable_memory_touch_to_commit_if_necessary))
        return;

    pas_segregated_heap_num_size_lookup_rematerializations++;

    data = pas_segregated_heap_rare_data_ptr_load(&heap->rare_data);
    if (data)
        pas_mutation_count_start_mutating(&data->mutation_count);
    pas_compiler_fence();

    recompute_size_lookup(heap, config, cached_index,
                          rematerialize_size_lookup_set_index_to_small_allocator_index,
                          rematerialize_size_lookup_set_index_to_small_size_directory,
                          rematerialize_size_lookup_set_medium_directory_tuple,
                          heap);

    pas_compiler_fence();
    if (data)
        pas_mutation_count_stop_mutating(&data->mutation_count);
}

unsigned
pas_segregated_heap_ensure_allocator_index(
    pas_segregated_heap* heap,
    pas_segregated_size_directory* directory,
    size_t size,
    pas_size_lookup_mode size_lookup_mode,
    const pas_heap_config* config,
    unsigned* cached_index)
{
    static const bool verbose = false;

    size_t index;
    unsigned allocator_index;
    bool did_cache_allocator_index;
    pas_heap* parent_heap;

    pas_heap_lock_assert_held();
    
    PAS_ASSERT(directory->object_size >= min_object_size_for_heap(heap, config), directory->object_size, min_object_size_for_heap(heap, config));

    rematerialize_size_lookup_if_necessary(heap, config, cached_index);

    check_size_lookup_recomputation_if_appropriate(
        heap, config, cached_index, "start of pas_segregated_heap_ensure_allocator_index");

    parent_heap = pas_heap_for_segregated_heap(heap);
    
    PAS_ASSERT(size <= directory->object_size, size, directory->object_size);
    PAS_ASSERT(!pas_heap_config_is_utility(config), pas_heap_config_is_utility(config));
    
    if (verbose)
        pas_log("%p: In pas_segregated_heap_ensure_allocator_index, size = %zu\n", (void*)pthread_self(), size);
    index = pas_segregated_heap_index_for_size(size, *config);
    if (verbose)
        pas_log("index = %zu\n", index);

    allocator_index = directory->allocator_index;
    PAS_ASSERT(allocator_index, allocator_index);
    PAS_ASSERT((pas_allocator_index)allocator_index == allocator_index, allocator_index);
    PAS_ASSERT(allocator_index < (unsigned)(pas_allocator_index)UINT_MAX, allocator_index);
    
    if (verbose)
        pas_log("allocator_index = %u\n", allocator_index);
    
    did_cache_allocator_index = false;
    
    if (pas_segregated_heap_index_is_cached_index_and_cached_index_is_set(heap, cached_index, index, config)
        && parent_heap && parent_heap->heap_ref) {
        if (verbose) {
            pas_log("pas_segregated_heap_ensure_allocator_index_for_size_directory: "
                   "Caching as cached index!\n");
        }
        PAS_ASSERT(!parent_heap->heap_ref->allocator_index ||
            parent_heap->heap_ref->allocator_index == allocator_index, parent_heap->heap_ref->allocator_index, allocator_index);
        parent_heap->heap_ref->allocator_index = allocator_index;
        did_cache_allocator_index = true;
    }
    
    if (index < compute_small_index_upper_bound(heap, config)) {
        if (!did_cache_allocator_index
            || size_lookup_mode == pas_force_size_lookup
            || heap->small_index_upper_bound) {
            pas_allocator_index* allocator_index_ptr;
            pas_allocator_index old_allocator_index;
            ensure_size_lookup(heap, config);
            PAS_ASSERT(index < heap->small_index_upper_bound, index, heap->small_index_upper_bound);
            allocator_index_ptr = heap->index_to_small_allocator_index + index;
            old_allocator_index = *allocator_index_ptr;
            PAS_ASSERT(!old_allocator_index ||
                old_allocator_index == allocator_index, old_allocator_index, allocator_index);
            *allocator_index_ptr = (pas_allocator_index)allocator_index;
        }
    } else {
        pas_segregated_heap_medium_directory_tuple* medium_directory;
        
        medium_directory =
            pas_segregated_heap_medium_directory_tuple_for_index(
                heap, index,
                pas_segregated_heap_medium_size_directory_search_within_size_class_progression,
                pas_lock_is_held);
        PAS_ASSERT(medium_directory, medium_directory);
        PAS_ASSERT(
            pas_compact_atomic_segregated_size_directory_ptr_load(&medium_directory->directory)
            == directory, pas_compact_atomic_segregated_size_directory_ptr_load(&medium_directory->directory), directory);
        
        medium_directory->allocator_index = (pas_allocator_index)allocator_index;
    }
    
    check_size_lookup_recomputation_if_appropriate(
        heap, config, cached_index, "end of pas_segregated_heap_ensure_allocator_index");

    return allocator_index;
}

static size_t compute_ideal_object_size(pas_segregated_heap* heap,
                                        size_t object_size,
                                        size_t alignment,
                                        const pas_segregated_page_config* page_config_ptr)
{
    static const bool verbose = false;
    
    unsigned num_objects;
    pas_segregated_page_config page_config;
    pas_heap* parent_heap;
    
    page_config = *page_config_ptr;

    object_size = pas_round_up_to_power_of_2(
        object_size, pas_segregated_page_config_min_align(page_config));
    
    PAS_ASSERT(pas_is_power_of_2(alignment));
    
    alignment = PAS_MAX(alignment, pas_segregated_page_config_min_align(page_config));
    
    num_objects = pas_segregated_page_number_of_objects((unsigned)object_size,
                                                        page_config,
                                                        pas_segregated_page_exclusive_role);
    
    parent_heap = pas_heap_for_segregated_heap(heap);

    /* Find the largest object_size that fits the same number of objects as we're asking for
       while maintaining the alignment we're asking for. */
    for (;;) {
        size_t next_object_size;
        
        next_object_size = object_size + pas_segregated_page_config_min_align(page_config);
        
        if (!pas_is_aligned(next_object_size, alignment))
            break;
        if (next_object_size > max_object_size_for_page_config(parent_heap,
                                                               &page_config_ptr->base)) {
            if (verbose) {
                pas_log("Rejecting %zu because it's bifgfer than the max for page config.\n",
                        next_object_size);
            }
            break;
        }
        if (pas_segregated_page_number_of_objects((unsigned)next_object_size,
                                                  page_config,
                                                  pas_segregated_page_exclusive_role) != num_objects)
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
    pas_segregated_heap* heap, const pas_heap_config* config, void* arg)
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
ensure_size_lookup_if_necessary(pas_segregated_heap* heap,
                                pas_size_lookup_mode size_lookup_mode,
                                const pas_heap_config* config,
                                unsigned* cached_index,
                                size_t index)
{
    if ((size_lookup_mode == pas_force_size_lookup
         || pas_segregated_heap_index_is_not_cached_index_and_cached_index_is_set(
             heap, cached_index, index, config))
        && index < compute_small_index_upper_bound(heap, config))
        ensure_size_lookup(heap, config);
}

typedef struct {
    pas_segregated_heap* heap;
    bool is_all_good;
    unsigned* seen_index_to_small_allocator_index;
    unsigned* seen_index_to_small_size_directory;
    unsigned num_medium_directories;
} check_size_lookup_recomputation_data;

static void check_size_lookup_recomputation_did_become_not_all_good(check_size_lookup_recomputation_data* data)
{
    static const bool crash_immediately = false;

    PAS_ASSERT(!crash_immediately);
    
    data->is_all_good = false;
}

static void check_size_lookup_recomputation_set_index_to_small_allocator_index(
    size_t index, pas_allocator_index value, void* arg)
{
    check_size_lookup_recomputation_data* data;

    data = arg;

    PAS_ASSERT(index < data->heap->small_index_upper_bound);
    PAS_ASSERT(data->heap->index_to_small_allocator_index);

    if (data->heap->index_to_small_allocator_index[index] != value
        && data->heap->index_to_small_allocator_index[index]) {
        pas_log("Size lookup recomputation error in set_index_to_small_allocator_index: "
                "index_to_small_allocator_index[%zu] = %u, value = %u\n",
                index, data->heap->index_to_small_allocator_index[index], value);
        check_size_lookup_recomputation_did_become_not_all_good(data);
    }

    pas_bitvector_set(data->seen_index_to_small_allocator_index, index, true);
}

static void check_size_lookup_recomputation_set_index_to_small_size_directory(
    size_t index, pas_segregated_size_directory* directory, void* arg)
{
    check_size_lookup_recomputation_data* data;

    data = arg;

    PAS_ASSERT(index < data->heap->small_index_upper_bound);
    PAS_ASSERT(data->heap->index_to_small_size_directory);

    if (pas_compact_atomic_segregated_size_directory_ptr_load(
            data->heap->index_to_small_size_directory + index) != directory) {
        pas_log("Size lookup recomputation error in set_index_to_small_size_directory: "
                "index_to_small_size_directory[%zu] = ", index);
        pas_segregated_size_directory_dump_reference(
            pas_compact_atomic_segregated_size_directory_ptr_load(
                data->heap->index_to_small_size_directory + index), &pas_log_stream.base);
        pas_log(", value = ");
        pas_segregated_size_directory_dump_reference(directory, &pas_log_stream.base);
        pas_log("\n");
        check_size_lookup_recomputation_did_become_not_all_good(data);
    }

    pas_bitvector_set(data->seen_index_to_small_size_directory, index, true);
}

static void check_size_lookup_recomputation_set_medium_directory_tuple(
    size_t medium_tuple_index, pas_segregated_heap_medium_directory_tuple* value, void* arg)
{
    check_size_lookup_recomputation_data* data;
    pas_segregated_heap_rare_data* rare_data;
    pas_segregated_heap_medium_directory_tuple* tuples;

    data = arg;

    PAS_ASSERT(value->begin_index);
    PAS_ASSERT(medium_tuple_index == data->num_medium_directories);
    PAS_ASSERT((unsigned)(medium_tuple_index + 1u) == medium_tuple_index + 1u);
    data->num_medium_directories = (unsigned)(medium_tuple_index + 1u);

    rare_data = pas_segregated_heap_rare_data_ptr_load(&data->heap->rare_data);
    PAS_ASSERT(rare_data);
    PAS_ASSERT(medium_tuple_index < rare_data->num_medium_directories);
    PAS_ASSERT(medium_tuple_index < rare_data->medium_directories_capacity);
    tuples = pas_segregated_heap_medium_directory_tuple_ptr_load(&rare_data->medium_directories);
    PAS_ASSERT(tuples);

    if (pas_compact_atomic_segregated_size_directory_ptr_load(&tuples[medium_tuple_index].directory)
        != pas_compact_atomic_segregated_size_directory_ptr_load(&value->directory)) {
        pas_log("Size lookup recomputation error in set_medium_directory_tuple: tuples[%zu].directory = ",
                medium_tuple_index);
        pas_segregated_size_directory_dump_reference(
            pas_compact_atomic_segregated_size_directory_ptr_load(&tuples[medium_tuple_index].directory),
            &pas_log_stream.base);
        pas_log(", value->directory = ");
        pas_segregated_size_directory_dump_reference(
            pas_compact_atomic_segregated_size_directory_ptr_load(&value->directory), &pas_log_stream.base);
        pas_log("\n");
        check_size_lookup_recomputation_did_become_not_all_good(data);
    }

    if (tuples[medium_tuple_index].allocator_index != value->allocator_index
        && tuples[medium_tuple_index].allocator_index) {
        pas_log("Size lookup recomputation error in set_medium_directory_tuple: tuples[%zu].allocator_index = "
                "%u, value->allocator_index = %u for directory = ",
                medium_tuple_index, tuples[medium_tuple_index].allocator_index, value->allocator_index);
        pas_segregated_size_directory_dump_reference(
            pas_compact_atomic_segregated_size_directory_ptr_load(&value->directory), &pas_log_stream.base);
        pas_log("\n");
        check_size_lookup_recomputation_did_become_not_all_good(data);
    }

    if (tuples[medium_tuple_index].begin_index != value->begin_index) {
        pas_log("Size lookup recomputation error in set_medium_directory_tuple: tuples[%zu].begin_index = %u, "
                "value->begin_index = %u for directory = ",
                medium_tuple_index, tuples[medium_tuple_index].begin_index, value->begin_index);
        pas_segregated_size_directory_dump_reference(
            pas_compact_atomic_segregated_size_directory_ptr_load(&value->directory), &pas_log_stream.base);
        pas_log("\n");
        check_size_lookup_recomputation_did_become_not_all_good(data);
    }

    if (tuples[medium_tuple_index].end_index != value->end_index) {
        pas_log("Size lookup recomputation error in set_medium_directory_tuple: tuples[%zu].end_index = %u, "
                "value->end_index = %u for directory = ",
                medium_tuple_index, tuples[medium_tuple_index].end_index, value->end_index);
        pas_segregated_size_directory_dump_reference(
            pas_compact_atomic_segregated_size_directory_ptr_load(&value->directory), &pas_log_stream.base);
        pas_log("\n");
        check_size_lookup_recomputation_did_become_not_all_good(data);
    }
}

static bool check_size_lookup_recomputation_dump_directory(pas_segregated_heap* heap,
                                                           pas_segregated_size_directory* directory,
                                                           void* arg)
{
    PAS_UNUSED_PARAM(heap);
    PAS_ASSERT(!arg);

    pas_log("    ");
    pas_segregated_size_directory_dump_reference(directory, &pas_log_stream.base);
    pas_log(": min_index = %u, object_size = %u, allocator_index = %u",
            pas_segregated_size_directory_min_index(directory),
            directory->object_size,
            pas_segregated_size_directory_get_tlc_allocator_index(directory));
    if (directory->base.is_basic_size_directory)
        pas_log(", is basic");
    pas_log("\n");
    
    return true;
}

static void check_size_lookup_recomputation(pas_segregated_heap* heap,
                                            const pas_heap_config* config,
                                            unsigned *cached_index,
                                            const char* where)
{
    check_size_lookup_recomputation_data data;
    size_t bitvector_size;
    size_t index;
    unsigned expected_num_medium_directories;
    pas_segregated_heap_rare_data* rare_data;

    bitvector_size = PAS_BITVECTOR_NUM_BYTES(heap->small_index_upper_bound);

    data.heap = heap;
    data.is_all_good = true;
    data.seen_index_to_small_allocator_index = pas_large_utility_free_heap_allocate(
        bitvector_size, "check_size_lookup_recomputation/seen_index_to_small_allocator_index");
    data.seen_index_to_small_size_directory = pas_large_utility_free_heap_allocate(
        bitvector_size, "check_size_lookup_recomputation/seen_index_to_small_size_directory");
    data.num_medium_directories = 0;

    pas_zero_memory(data.seen_index_to_small_allocator_index, bitvector_size);
    pas_zero_memory(data.seen_index_to_small_size_directory, bitvector_size);

    recompute_size_lookup(
        heap,
        config,
        cached_index,
        check_size_lookup_recomputation_set_index_to_small_allocator_index,
        check_size_lookup_recomputation_set_index_to_small_size_directory,
        check_size_lookup_recomputation_set_medium_directory_tuple,
        &data);

    for (index = 0; index < heap->small_index_upper_bound; ++index) {
        if (heap->index_to_small_allocator_index[index]
            && !pas_bitvector_get(data.seen_index_to_small_allocator_index, index)) {
            pas_log("Size lookup recomputation error in set_index_to_small_allocator_index: "
                    "index_to_small_allocator_index[%zu] = %u, value never set\n",
                    index, heap->index_to_small_allocator_index[index]);
            check_size_lookup_recomputation_did_become_not_all_good(&data);
        }
        
        if (pas_compact_atomic_segregated_size_directory_ptr_load(heap->index_to_small_size_directory + index)
            && !pas_bitvector_get(data.seen_index_to_small_size_directory, index)) {
            pas_log("Size lookup recomputation error in set_index_to_small_size_directory: "
                    "index_to_small_size_directory[%zu] = ", index);
            pas_segregated_size_directory_dump_reference(
                pas_compact_atomic_segregated_size_directory_ptr_load(
                    heap->index_to_small_size_directory + index), &pas_log_stream.base);
            pas_log(", value not set\n");
            check_size_lookup_recomputation_did_become_not_all_good(&data);
        }
    }

    rare_data = pas_segregated_heap_rare_data_ptr_load(&heap->rare_data);
    if (rare_data)
        expected_num_medium_directories = rare_data->num_medium_directories;
    else
        expected_num_medium_directories = 0;
    if (expected_num_medium_directories != data.num_medium_directories) {
        pas_log("Size lookup recomputation error in set_medium_directory_tuple: "
                "num_medium_directories in rare_data = %u, but got num_medium_directories = %u\n",
                expected_num_medium_directories, data.num_medium_directories);
        check_size_lookup_recomputation_did_become_not_all_good(&data);
    }

    if (!data.is_all_good) {
        pas_heap* parent_heap;
        pas_log("Encountered size recomputation failure for heap %p (%s, ",
                heap, pas_heap_config_kind_get_string(config->kind));
        parent_heap = pas_heap_for_segregated_heap(heap);
        if (parent_heap)
            config->dump_type(parent_heap->type, &pas_log_stream.base);
        else
            pas_log("no type");
        pas_log(") at %s.\n", where);

        pas_log("Directories:\n");
        pas_segregated_heap_for_each_size_directory(heap, check_size_lookup_recomputation_dump_directory, NULL);
    }

    PAS_ASSERT(data.is_all_good);

    pas_large_utility_free_heap_deallocate(data.seen_index_to_small_allocator_index, bitvector_size);
    pas_large_utility_free_heap_deallocate(data.seen_index_to_small_size_directory, bitvector_size);
}

static void check_size_lookup_recomputation_if_appropriate(pas_segregated_heap* heap,
                                                           const pas_heap_config* config,
                                                           unsigned *cached_index,
                                                           const char* where)
{
    if (!PAS_ENABLE_TESTING)
        return;
    if (pas_heap_config_is_utility(config))
        return;
    check_size_lookup_recomputation(heap, config, cached_index, where);
}

pas_segregated_size_directory*
pas_segregated_heap_ensure_size_directory_for_size(
    pas_segregated_heap* heap,
    size_t size,
    size_t alignment,
    pas_size_lookup_mode size_lookup_mode,
    const pas_heap_config* config,
    unsigned* cached_index,
    pas_segregated_size_directory_creation_mode creation_mode)
{
    static const bool verbose = false;

    pas_heap* parent_heap;
    pas_segregated_size_directory* result;
    size_t index;
    size_t object_size;
    pas_segregated_heap_medium_directory_tuple* medium_tuple;
    bool is_utility;
    size_t dynamic_min_align;
    size_t static_min_align;
    size_t type_alignment;

    pas_heap_lock_assert_held();

    pas_heap_config_activate(config);

    result = NULL;

    if (verbose) {
        pas_log("%p(%s): being asked for directory with size = %zu, alignment = %zu.\n",
                heap, pas_heap_config_kind_get_string(config->kind), size, alignment);
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

    rematerialize_size_lookup_if_necessary(heap, config, cached_index);

    is_utility = pas_heap_config_is_utility(config);

    check_size_lookup_recomputation_if_appropriate(
        heap, config, cached_index, "start of pas_segregated_heap_ensure_size_directory_for_size");

    parent_heap = pas_heap_for_segregated_heap(heap);

    /* Say that the heap's type has an alignment of 128 and that someone calls something like
       bmalloc_iso_allocate_array_by_size(&some_heap, 5163).
       
       Before considering what the possible behaviors could be, let's take note of a constraint: it's not
       reasonable to require the fast path of allocation to align 5163 to 128, since that would require loading
       the alignment from the type and doing it dynamically, which involves more cycles for the fast path than
       we'd like.
       
       So what behavior do we want this to have?
       
       1) We could make this an error. But, since we cannot do the alignment computation on the fast path, the
          error would only manifest if we hit a slow path.
       
       2) We could just silently align the size for the user. To make this work, we just have to make sure that
          when the fast path asks for 5163 again, they will get an allocator that is aligned appropriately.
       
       The first option seems doubly bad: it will only manifest sometimes, and it's unnecessarily restrictive.
       Who is to say that an array of that length is really wrong? Libpas generally takes the approach of
       permitting the user to request a misaligned size, and it will align the size for the user.

       To make the second option work, we need to ensure that the `index` is computed from the size that the
       user thought they were requesting, and that this function gets called with size being exactly the size
       that the allocator saw.
       
       Note that this size may be already aligned to the dynamically requested alignment (as opposed to the
       type's alignment), and that's great! That behavior ensures that if we later try to allocate this size
       without that alignment, we can get a directory that is better suited. The `alignment` we are passed here
       is the max of the type's alignment and the requested alignment, so if the size is not aligned to
       `alignment`, then we know it's because `alignment` is the type's alignment. */

    type_alignment = pas_heap_get_type_alignment(parent_heap);
    PAS_ASSERT(alignment >= type_alignment);
    
    index = pas_segregated_heap_index_for_size(size, *config);

    if (verbose)
        pas_log("index = %zu\n", index);
    
    object_size = pas_segregated_heap_size_for_index(index, *config);

    if (verbose)
        pas_log("object_size = %zu\n", object_size);

    if (object_size < size) {
        /* This'll only happen in certain kinds of overflows, in which case the size must be way too big. */
        PAS_ASSERT(!object_size);
        return NULL;
    }

    object_size = PAS_MAX(min_object_size_for_heap(heap, config), object_size);

    if (verbose)
        pas_log("object_size after accounting for min_object_size = %zu\n", object_size);

    static_min_align = pas_heap_config_segregated_heap_min_align(*config);
    dynamic_min_align = min_align_for_heap(heap, config);
    PAS_ASSERT(dynamic_min_align >= static_min_align);

    PAS_ASSERT(pas_is_aligned(object_size, alignment));
    PAS_ASSERT(pas_is_aligned(object_size, static_min_align));

    object_size = pas_round_up_to_power_of_2(object_size, dynamic_min_align);
    
    alignment = PAS_MAX(alignment, dynamic_min_align);

    if (alignment > type_alignment) {
        /* If the alignment is bigger than type_alignment then it means that this is a dynamically requested
           alignment. So, we expect that the object size was already aligned to it by the allocator fast
           path. We should never get here with a size that isn't already aligned to the dynamically requested
           alignment! */
        PAS_ASSERT(pas_is_aligned(object_size, alignment));
    } else {
        /* If the alignment is exactly the type_alignment, then it might be that we are allocating without
           any dynamically requested alignment, or a dynamically requested alignment that is smaller than the
           type alignment. In that case, we align the object_size here, so that in this heap, all object sizes
           are aligned to the type's alignment. */
        object_size = pas_round_up_to_power_of_2(object_size, alignment);
    }

    PAS_ASSERT(object_size >= size);

    /* If we have overflowed already then bail. */
    if ((unsigned)object_size != object_size ||
        (unsigned)alignment != alignment)
        return NULL;

    if (verbose)
        pas_log("Considering whether object_size = %zu is appropriate.\n", object_size);
    
    /* If it's impossible to use the largest segregated heap to allocate the request then
       immediately give up. Do this before ensure_size_lookup so that heaps that are only used
       for large object allocation don't allocate any small heap meta-data. */
    if (object_size > max_object_size_for_heap(parent_heap, heap, config)) {
        if (verbose)
            pas_log("It's too big.\n");
        return NULL;
    }
    
    if (verbose)
        pas_log("Proceeding.\n");
    
    ensure_size_lookup_if_necessary(heap, size_lookup_mode, config, cached_index, index);
    
    result = pas_segregated_heap_size_directory_for_index(heap, index, cached_index, config);

    PAS_ASSERT(
        !result
        || pas_is_aligned(result->object_size, pas_segregated_size_directory_alignment(result))
        || pas_segregated_size_directory_is_bitfit(result));

    if (verbose)
        pas_log("Small index upper bound = %u\n", heap->small_index_upper_bound);
    
    if (result && pas_segregated_size_directory_alignment(result) < alignment) {
        /* In this case, result is a size class that's big enough for the size
           we want and it's cached at the index we wanted plus some number
           of smaller indices. We will create a new size class for this index
           and we wanted smaller indices to consider this one instead of the
           old one. */

        if (verbose)
            pas_log("Found result = %p, but we need to evict it starting at index = %zu\n", result, index);

        /* Bitfit size directories claim super high alignment, so they should never get replaced. This is a
           hard requirement, since:

           - pas_bitfit_size_class is allocated as part of the pas_segregated_size_directory.
           - We cannot add duplicate bitfit_size_classes, so if we ever tried to replace a
             segregated_size_directory with another one of the same size, and they both had bitfit_size_classes,
             then we'd be in trouble. */
        PAS_ASSERT(!pas_segregated_size_directory_is_bitfit(result));
        
        PAS_ASSERT(result->object_size >= object_size);
        PAS_ASSERT(result->object_size >= pas_segregated_heap_size_for_index(index, *config));
        PAS_ASSERT(pas_segregated_heap_index_for_size(result->object_size, *config) >= index);

        if (pas_segregated_heap_index_for_size(result->object_size, *config) == index)
            pas_segregated_size_directory_set_min_index(result, UINT_MAX);
        else {
            PAS_ASSERT(index + 1 > index);
            PAS_ASSERT(pas_segregated_heap_index_for_size(result->object_size, *config) >= index + 1);
            PAS_ASSERT((unsigned)(index + 1) == index + 1);
            PAS_ASSERT(pas_segregated_size_directory_min_index(result) != UINT_MAX);
            pas_segregated_size_directory_set_min_index(result, (unsigned)(index + 1));
        }

        /* Need to clear out this size class from this size and smaller sizes.
           This loop starts at index (not index + 1 despite what the initial
           condition seems to do) and travels down. */
        if (heap->small_index_upper_bound) {
            pas_compact_atomic_segregated_size_directory_ptr* index_to_small_size_directory;
            pas_allocator_index* index_to_small_allocator_index;
            size_t victim_index;

            if (verbose)
                pas_log("Evicting starting with index = %zu\n", index);

            index_to_small_size_directory = heap->index_to_small_size_directory;
            index_to_small_allocator_index = heap->index_to_small_allocator_index;

            if (index < heap->small_index_upper_bound) {
                PAS_ASSERT(pas_compact_atomic_segregated_size_directory_ptr_load(
                               index_to_small_size_directory + index) == result);
            }

            /* FIXME: Need a test for the reason why we need this PAS_MIN(). */
            for (victim_index = PAS_MIN(index + 1, heap->small_index_upper_bound); victim_index--;) {
                pas_segregated_size_directory* directory;

                directory = pas_compact_atomic_segregated_size_directory_ptr_load(
                    index_to_small_size_directory + victim_index);

                if (verbose)
                    pas_log("Considering victim_index = %zu, directory = %p.\n", victim_index, directory);

                if (directory != result)
                    break;

                pas_compact_atomic_segregated_size_directory_ptr_store(
                    index_to_small_size_directory + victim_index, NULL);
                if (!is_utility)
                    index_to_small_allocator_index[victim_index] = 0;
            }
        }
        
        medium_tuple = pas_segregated_heap_medium_directory_tuple_for_index(
            heap, index,
            pas_segregated_heap_medium_size_directory_search_within_size_class_progression,
            pas_lock_is_held);
        if (medium_tuple) {
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
                PAS_ASSERT(medium_tuple->begin_index);
            } else {
                pas_segregated_heap_medium_directory_tuple* medium_directories;
                size_t medium_tuple_index;
                
                PAS_ASSERT(index == medium_tuple->end_index);

                medium_directories = pas_segregated_heap_medium_directory_tuple_ptr_load(
                    &rare_data->medium_directories);
                
                medium_tuple_index = (size_t)(medium_tuple - medium_directories);
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
        if (pas_compact_atomic_segregated_size_directory_ptr_load(
                &heap->basic_size_directory_and_head) == result &&
            result->base.is_basic_size_directory &&
            pas_segregated_heap_index_is_greater_equal_cached_index_and_cached_index_is_set(
                heap, cached_index, index, config)) {
            result->base.is_basic_size_directory = false;
            if (parent_heap && parent_heap->heap_ref)
                parent_heap->heap_ref->allocator_index = 0;
        }
        
        /* We'll create a new size class that aligns properly. The indices that we cleared will
           reconfigure to use our new size class. */
        result = NULL;
    }
    
    if (!result) {
        size_t candidate_index;
        pas_segregated_size_directory* candidate;
        size_t medium_install_index;
        size_t medium_install_size;
        double best_bytes_dirtied_per_object;
        const pas_segregated_page_config* best_page_config;
        pas_segregated_page_config_variant variant;
        pas_compact_atomic_segregated_size_directory_ptr* index_to_small_size_directory;
        pas_segregated_size_directory* basic_size_directory_and_head;
        bool did_add_to_size_lookup;

        index_to_small_size_directory = heap->index_to_small_size_directory;
        
        /* Should try to find a size class that is bigger but small enough before creating a
           new one. */
    
        candidate = NULL;
    
        for (candidate_index = index;
             candidate_index < heap->small_index_upper_bound;
             ++candidate_index) {
            candidate = pas_compact_atomic_segregated_size_directory_ptr_load(
                index_to_small_size_directory + candidate_index);
            if (candidate)
                break;
        }
    
        /* It's possible that the basic size class is a candidate. That could
           happen if we did not force count lookup when creating that basic size
           class. */
        basic_size_directory_and_head = pas_compact_atomic_segregated_size_directory_ptr_load(
            &heap->basic_size_directory_and_head);
        if ((!candidate || pas_segregated_heap_index_is_greater_than_cached_index_and_cached_index_is_set(
                 heap, cached_index, candidate_index, config))
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
            pas_segregated_size_directory* directory;
            directory = pas_compact_atomic_segregated_size_directory_ptr_load(
                &medium_tuple->directory);
            if (!candidate || directory->object_size < candidate->object_size)
                candidate = directory;
        }

        /* Figure out the best case scenario if we did create a page directory for this size. */
        best_bytes_dirtied_per_object = PAS_INFINITY;
        best_page_config = NULL;
        if (object_size <= heap->runtime_config->max_segregated_object_size) {
            for (PAS_EACH_SEGREGATED_PAGE_CONFIG_VARIANT_DESCENDING(variant)) {
                const pas_segregated_page_config* page_config_ptr;
                pas_segregated_page_config page_config;
                double bytes_dirtied_per_object;
                unsigned object_size_for_config;
                
                page_config_ptr =
                    pas_heap_config_segregated_page_config_ptr_for_variant(config, variant);
                if (!pas_segregated_page_config_is_enabled(*page_config_ptr, heap->runtime_config))
                    continue;
                
                page_config = *page_config_ptr;
                
                object_size_for_config = (unsigned)pas_round_up_to_power_of_2(
                    object_size,
                    pas_segregated_page_config_min_align(page_config));

                if (verbose)
                    pas_log("object size for config: %u\n", object_size_for_config);
                
                if (object_size_for_config > max_object_size_for_page_config(parent_heap,
                                                                             &page_config_ptr->base))
                    continue;
                
                bytes_dirtied_per_object =
                    pas_segregated_page_bytes_dirtied_per_object(
                        object_size_for_config, page_config, pas_segregated_page_exclusive_role);
                
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
            if (verbose) {
                pas_log("object_size = %zu\n", (size_t)object_size);
                pas_log("max bitfit size = %zu (heap %p, config %p)\n", (size_t)heap->runtime_config->max_bitfit_object_size, heap, heap->runtime_config);
            }
            PAS_ASSERT(object_size <= heap->runtime_config->max_bitfit_object_size);
            PAS_TESTING_ASSERT(
                object_size <= max_bitfit_object_size_for_heap(parent_heap, heap, config));
            best_bytes_dirtied_per_object =
                pas_bitfit_heap_select_variant(object_size, config, heap->runtime_config).object_size;
            PAS_ASSERT(!is_utility);
        }

        if (candidate && pas_segregated_size_directory_alignment(candidate) >= alignment) {
            double bytes_dirtied_per_object_by_candidate;
        
            if (verbose) {
                pas_log("object_size = %lu\n", object_size);
                pas_log("candidate->object_size = %u\n", candidate->object_size);
            }

            if (candidate->base.page_config_kind != pas_segregated_page_config_kind_null) {
                bytes_dirtied_per_object_by_candidate =
                    pas_segregated_page_bytes_dirtied_per_object(
                        candidate->object_size,
                        *pas_segregated_page_config_kind_get_config(candidate->base.page_config_kind),
                        pas_segregated_page_exclusive_role);
            } else
                bytes_dirtied_per_object_by_candidate = candidate->object_size;

            if (best_bytes_dirtied_per_object * PAS_SIZE_CLASS_PROGRESSION
                > bytes_dirtied_per_object_by_candidate)
                result = candidate;
        }

        if (result)
            object_size = result->object_size;
        else {
            pas_compact_atomic_segregated_size_directory_ptr* head;
            pas_segregated_size_directory* basic_size_directory_and_head;

            if (verbose)
                pas_log("About to compute ideal object size; object_size = %zu\n", object_size);

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
                object_size = (size_t)best_bytes_dirtied_per_object;
            }
        
            if (verbose)
                pas_log("Did compute ideal object size; object_size = %zu\n", object_size);

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

            /* If we have partial views, then we want the directory to have the most conservative possible
               alignment, which is what we would have so far: it's the alignment the user asked for, possibly
               bumped up to minalign. That's because we bump-allocate partial view memory out of shared views,
               and it's possible (for example) to have a 256-size directory that wants to bump-allocate at an
               offset of 112 with alignment=16. In that case, it's ideal for that 256-byte object to be placed
               at offset=112 with no gaps, so as to not create internal fragmentation. Had we executed the code
               below, we would have given the 256-size directory alignment=256, and so we would be forced to
               allocate at offset=256, creating a gap of 144 bytes. Yuck!
               
               On the other hand, not executing this code creates this weird situation where if we had once upon
               a time allocated 256 bytes with no particular alignment, and later memaligned 256 bytes with
               256-byte alignment, then we would create a second directory, and the old directory's memory will
               be "retired". That retired memory will never be able to be used for any future allocations. That
               doesn't mean that the memory is wasted; if all of the objects in the pages of that directory get
               freed then those pages will get decommitted. But this creates a new unique source of external
               fragmentation. I suspect that this problem is super unlikely since memalign is rare to begin with.
               
               So, currently we just execute the code below if we will never have partial views. No partial views
               means no possibility of the internal fragmentation problem, so then we just want to avoid the
               external fragmentation problem. */
            if (!heap->runtime_config->directory_size_bound_for_partial_views) {
                alignment = (size_t)1 << __builtin_ctzl(object_size);

                if (verbose)
                    pas_log("Bumped alignment for object_size = %zu up to %zu.\n", object_size, alignment);

                PAS_ASSERT(pas_is_aligned(object_size, alignment));
                PAS_ASSERT(!pas_is_aligned(object_size, alignment << (size_t)1));
            }
        
            result = pas_segregated_size_directory_create(
                heap,
                (unsigned)object_size,
                (unsigned)alignment,
                config,
                best_page_config,
                creation_mode);
            if (verbose)
                pas_log("Created size class = %p\n", result);

            basic_size_directory_and_head = pas_compact_atomic_segregated_size_directory_ptr_load(
                &heap->basic_size_directory_and_head);
            if (basic_size_directory_and_head
                && basic_size_directory_and_head->base.is_basic_size_directory)
                head = &basic_size_directory_and_head->next_for_heap;
            else
                head = &heap->basic_size_directory_and_head;

            pas_compact_atomic_segregated_size_directory_ptr_store(
                &result->next_for_heap,
                pas_compact_atomic_segregated_size_directory_ptr_load(head));
            pas_compact_atomic_segregated_size_directory_ptr_store(head, result);
        }

        did_add_to_size_lookup = false;
        
        if (pas_segregated_heap_index_is_cached_index_or_cached_index_is_unset(
                heap, cached_index, index, config)) {
            pas_compact_atomic_segregated_size_directory_ptr* prev_next_ptr;
            bool did_find_result;
            pas_segregated_size_directory* directory;
            
            if (verbose) {
                pas_log("pas_segregated_heap_ensure_size_directory_for_size: "
                       "Caching as basic size class!\n");
            }
            directory = pas_compact_atomic_segregated_size_directory_ptr_load(
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
                 !pas_compact_atomic_segregated_size_directory_ptr_is_null(prev_next_ptr);
                 prev_next_ptr =
                     &pas_compact_atomic_segregated_size_directory_ptr_load(
                         prev_next_ptr)->next_for_heap) {
                if (pas_compact_atomic_segregated_size_directory_ptr_load(prev_next_ptr)
                    == result) {
                    pas_compact_atomic_segregated_size_directory_ptr_store(
                        prev_next_ptr,
                        pas_compact_atomic_segregated_size_directory_ptr_load(
                            &result->next_for_heap));
                    pas_compact_atomic_segregated_size_directory_ptr_store(
                        &result->next_for_heap, NULL);
                    did_find_result = true;
                    break;
                }
                /* If we're going to go through the effort of scanning this list we might as
                   well check some basic properties of it. */
                PAS_ASSERT(!pas_compact_atomic_segregated_size_directory_ptr_load(
                               prev_next_ptr)->base.is_basic_size_directory);
            }
            PAS_ASSERT(did_find_result);
            PAS_ASSERT(pas_compact_atomic_segregated_size_directory_ptr_is_null(
                           &result->next_for_heap));
            pas_compact_atomic_segregated_size_directory_ptr_store(
                &result->next_for_heap,
                pas_compact_atomic_segregated_size_directory_ptr_load(
                    &heap->basic_size_directory_and_head));
            pas_compact_atomic_segregated_size_directory_ptr_store(
                &heap->basic_size_directory_and_head, result);
            result->base.is_basic_size_directory = true;
        } else {
            if (verbose) {
                pas_log("pas_segregated_heap_ensure_size_directory_for_count: "
                        "NOT caching as basic size class!\n");
            }
        }

        if (verbose) {
            pas_log("index = %zu, object_size = %zu, index upper bound = %u.\n",
                    index, object_size, heap->small_index_upper_bound);
        }
    
        if (index < heap->small_index_upper_bound) {
            size_t candidate_index;
            pas_compact_atomic_segregated_size_directory_ptr* index_to_small_size_directory;
            pas_allocator_index* index_to_small_allocator_index;

            if (verbose) {
                pas_log("Installing starting at index = %zu up to %u\n",
                        index, heap->small_index_upper_bound);
            }

            index_to_small_size_directory = heap->index_to_small_size_directory;
            index_to_small_allocator_index = heap->index_to_small_allocator_index;
        
            PAS_ASSERT(pas_compact_atomic_segregated_size_directory_ptr_is_null(
                           index_to_small_size_directory + index));
            if (!is_utility)
                PAS_ASSERT(index_to_small_allocator_index[index] == 0);
        
            /* Install this result in all indices starting with this one that don't already have a
               size class and where this size class would be big enough. */
            for (candidate_index = index;
                 candidate_index < heap->small_index_upper_bound
                 && pas_segregated_heap_size_for_index(candidate_index, *config) <= result->object_size;
                 ++candidate_index) {
                pas_segregated_size_directory* candidate;

                if (verbose)
                    pas_log("Installing at index candidate_index = %zu.\n", candidate_index);
            
                candidate = pas_compact_atomic_segregated_size_directory_ptr_load(
                    index_to_small_size_directory + candidate_index);
                if (candidate) {
                    if (verbose)
                        pas_log("Have candidate with size = %d\n", candidate->object_size);
                    
                    /* If the candidate at this index has an object size that is no larger than the
                       one we picked, then we should have just simply used this candidate for our
                       allocations! */
                    PAS_ASSERT(candidate == result || candidate->object_size > object_size);
                    break;
                }

                pas_compact_atomic_segregated_size_directory_ptr_store(
                    heap->index_to_small_size_directory + candidate_index, result);
            }

            did_add_to_size_lookup = true;
        }

        PAS_ASSERT(object_size == result->object_size);

        medium_install_size = object_size;
        medium_install_index = pas_segregated_heap_index_for_size(medium_install_size, *config);

        if (verbose)
            pas_log("object_size = %zu, medium_install_index = %zu\n", object_size, medium_install_index);
        
        PAS_ASSERT(index <= medium_install_index);

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
                pas_compact_atomic_segregated_size_directory_ptr_load(
                    &next_tuple->directory) == result) {
                size_t begin_index;
                begin_index = PAS_MIN(index, (size_t)next_tuple->begin_index);
                PAS_ASSERT(begin_index);
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
                        pas_compact_atomic_segregated_size_directory_ptr_load(
                            &next_tuple->directory)->object_size);
            
                    PAS_ASSERT(next_tuple - medium_directories
                               < rare_data->num_medium_directories);
            
                    tuple_insertion_index = (size_t)(next_tuple - medium_directories);
                } else
                    tuple_insertion_index = rare_data->num_medium_directories;
        
                pas_mutation_count_start_mutating(&rare_data->mutation_count);
                
                if (rare_data->num_medium_directories
                    >= rare_data->medium_directories_capacity) {
                    pas_segregated_heap_medium_directory_tuple* new_directories;
                    unsigned new_capacity;
            
                    new_capacity = (rare_data->medium_directories_capacity + 1) << 1;
            
                    new_directories = pas_compact_expendable_memory_allocate(
                        sizeof(pas_segregated_heap_medium_directory_tuple) * new_capacity,
                        PAS_ALIGNOF(pas_segregated_heap_medium_directory_tuple),
                        "pas_segregated_heap_rare_data/medium_directories");
            
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
                pas_compact_atomic_segregated_size_directory_ptr_store(
                    &medium_directory->directory, result);
                medium_directory->allocator_index = 0;

                if (verbose) {
                    pas_log("In rare_data = %p, Installing medium tuple %zu...%zu\n",
                            rare_data, index, medium_install_index);
                }

                PAS_ASSERT(index);
                medium_directory->begin_index = (pas_segregated_heap_medium_directory_index)index;
                medium_directory->end_index =
                    (pas_segregated_heap_medium_directory_index)medium_install_index;
                
                pas_fence(); /* This fence is probably not needed but it's cheap in this context
                                and adds some sanity. */
                
                rare_data->num_medium_directories++;
                
                check_medium_directories(heap);
                
                pas_mutation_count_stop_mutating(&rare_data->mutation_count);
            }

            did_add_to_size_lookup = true;
        }

        if (did_add_to_size_lookup) {
            /* Make sure that the directory knows that it's participating in size class lookup, and that it
               might be found as early in that lookup as the `index` that initiated this whole function call.
               Note that the code that follows will never install the directory anywhere lower than `index`.
               
               We only ever set min_index under heap_lock, so we don't have to do any kind of CAS, though the
               set function is implemented using a CAS because min_index is part of an encoded field that
               contains other things that aren't protected by heap_lock.
               
               FIXME: Can't we assert that the old min_index is bigger than index? */
            if (pas_segregated_size_directory_min_index(result) > index) {
                PAS_ASSERT((unsigned)index == index);
                pas_segregated_size_directory_set_min_index(result, (unsigned)index);
            }
        }
        
        PAS_ASSERT(pas_segregated_heap_size_directory_for_index(heap, index, cached_index, config)
                   == result);
    }

    check_size_lookup_recomputation_if_appropriate(
        heap, config, cached_index, "end of pas_segregated_heap_ensure_size_directory_for_size");

    return result;
}

size_t pas_segregated_heap_get_num_free_bytes(pas_segregated_heap* heap)
{
    return pas_segregated_heap_compute_summary(heap).free;
}

bool pas_segregated_heap_for_each_size_directory(
    pas_segregated_heap* heap,
    pas_segregated_heap_for_each_size_directory_callback callback,
    void *arg)
{
    pas_segregated_size_directory* directory;

    for (directory = pas_compact_atomic_segregated_size_directory_ptr_load(
             &heap->basic_size_directory_and_head);
         directory;
         directory = pas_compact_atomic_segregated_size_directory_ptr_load(
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
    pas_segregated_size_directory* size_directory,
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
    return pas_segregated_heap_for_each_size_directory(
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
                                                   pas_segregated_size_directory* size_directory,
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
    return pas_segregated_heap_for_each_size_directory(
        heap,
        for_each_view_index_directory_callback,
        &data);
}

typedef struct {
    pas_segregated_heap* heap;
    pas_segregated_heap_for_each_live_object_callback callback;
    void* arg;
} for_each_live_object_data;

static bool for_each_live_object_object_callback(pas_segregated_size_directory* directory,
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
    pas_segregated_size_directory* directory,
    void* arg)
{
    PAS_UNUSED_PARAM(heap);
    return pas_segregated_size_directory_for_each_live_object(
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
    
    if (!pas_segregated_heap_for_each_size_directory(
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
    pas_segregated_size_directory* directory,
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
    
    pas_segregated_heap_for_each_size_directory(
        heap, num_committed_views_directory_callback, &data);
    
    return data.result;
}

typedef struct {
    size_t result;
} num_empty_views_data;

static bool num_empty_views_directory_callback(
    pas_segregated_heap* heap,
    pas_segregated_size_directory* directory,
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
    
    pas_segregated_heap_for_each_size_directory(
        heap, num_empty_views_directory_callback, &data);
    
    return data.result;
}

typedef struct {
    size_t result;
} num_empty_granules_data;

static bool num_empty_granules_directory_callback(
    pas_segregated_heap* heap,
    pas_segregated_size_directory* directory,
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

    pas_segregated_heap_for_each_size_directory(
        heap, num_empty_granules_directory_callback, &data);

    return data.result;
}

typedef struct {
    size_t result;
} num_views_data;

static bool num_views_directory_callback(
    pas_segregated_heap* heap,
    pas_segregated_size_directory* directory,
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
    
    pas_segregated_heap_for_each_size_directory(
        heap, num_views_directory_callback, &data);
    
    return data.result;
}

static bool compute_summary_directory_callback(
    pas_segregated_heap* heap,
    pas_segregated_size_directory* directory,
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
    
    pas_segregated_heap_for_each_size_directory(
        heap, compute_summary_directory_callback, &result);

    bitfit_heap = pas_compact_atomic_bitfit_heap_ptr_load(&heap->bitfit_heap);
    if (bitfit_heap)
        result = pas_heap_summary_add(result, pas_bitfit_heap_compute_summary(bitfit_heap));
    
    return result;
}

#endif /* LIBPAS_ENABLED */
