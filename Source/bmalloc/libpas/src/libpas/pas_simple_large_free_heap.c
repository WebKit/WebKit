/*
 * Copyright (c) 2018-2019 Apple Inc. All rights reserved.
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

#include "pas_simple_large_free_heap.h"

#include "pas_bootstrap_free_heap.h"
#include "pas_generic_large_free_heap.h"
#include "pas_heap_lock.h"
#include "pas_large_free.h"
#include "pas_large_free_heap_config.h"
#include "pas_log.h"
#include <stdio.h>

static const bool verbose = PAS_SHOULD_LOG(PAS_LOG_LARGE_HEAPS);
static const unsigned verbosity = 0;

#define BOOTSTRAP_FREE_LIST_CAPACITY 4
static pas_large_free bootstrap_free_list[BOOTSTRAP_FREE_LIST_CAPACITY];

static PAS_ALWAYS_INLINE pas_large_free* free_list_entry(pas_simple_large_free_heap* free_heap,
                                                         size_t index)
{
    if (PAS_LIKELY(index < free_heap->free_list_capacity))
        return free_heap->free_list + index;

    PAS_ASSERT(free_heap == &pas_bootstrap_free_heap);
    PAS_ASSERT(index - free_heap->free_list_capacity < BOOTSTRAP_FREE_LIST_CAPACITY);
    return bootstrap_free_list + (index - free_heap->free_list_capacity);
}

void pas_simple_large_free_heap_construct(pas_simple_large_free_heap* heap)
{
    pas_simple_large_free_heap heap_initializer = PAS_SIMPLE_LARGE_FREE_HEAP_INITIALIZER;
    *heap = heap_initializer;
}

static void dump_free_list(pas_simple_large_free_heap* heap)
{
    size_t index;
    printf("Free list:\n");
    for (index = 0; index < heap->free_list_size; ++index) {
        pas_large_free* free;

        free = free_list_entry(heap, index);
        
        printf("    %p...%p: size = %zu\n",
               (void*)((uintptr_t)free->begin),
               (void*)((uintptr_t)free->end),
               pas_large_free_size(*free));
        
        PAS_ASSERT(free->begin);
        PAS_ASSERT(free->end > free->begin);
    }
}

static void consider_expanding(pas_simple_large_free_heap* heap, size_t num_things_to_add)
{
    size_t new_capacity;
    pas_large_free* new_free_list;
    size_t new_free_list_size_in_bytes;
    
    PAS_ASSERT(heap != &pas_bootstrap_free_heap);
    
    if (heap->free_list_size + num_things_to_add <= heap->free_list_capacity)
        return;
    
    new_capacity = (heap->free_list_capacity + num_things_to_add) << 1;
    
    new_free_list_size_in_bytes = new_capacity * sizeof(pas_large_free);
    
    new_free_list = pas_bootstrap_free_heap_allocate_simple(
        new_free_list_size_in_bytes,
        "pas_simple_large_free_heap/free_list",
        pas_object_allocation);
    
    memcpy(new_free_list, heap->free_list,
           sizeof(pas_large_free) * heap->free_list_size);
    pas_zero_memory(new_free_list + heap->free_list_size,
                sizeof(pas_large_free) * (new_capacity - heap->free_list_size));
    
    pas_bootstrap_free_heap_deallocate(
        heap->free_list,
        heap->free_list_capacity * sizeof(pas_large_free),
        pas_object_allocation);
    
    heap->free_list = new_free_list;
    heap->free_list_capacity = new_capacity;
}

static void append(pas_simple_large_free_heap* heap, pas_large_free new_free)
{
    if (verbose) {
        pas_log("%p: Appending %p...%p (%s)\n",
                heap, (void*)((uintptr_t)new_free.begin), (void*)((uintptr_t)new_free.end),
                new_free.zero_mode ? "zero" : "non-zero");
    }
    
    PAS_ASSERT(new_free.begin);
    PAS_ASSERT(new_free.end > new_free.begin);
    if (heap != &pas_bootstrap_free_heap) {
        consider_expanding(heap, 1);
        PAS_ASSERT(heap->free_list_size < heap->free_list_capacity);
    } else
        PAS_ASSERT(heap->free_list_size < heap->free_list_capacity + BOOTSTRAP_FREE_LIST_CAPACITY);
    *free_list_entry(heap, heap->free_list_size++) = new_free;
}

PAS_API uint64_t pas_simple_large_free_heap_num_merge_calls;
PAS_API uint64_t pas_simple_large_free_heap_num_merge_nodes_considered;

static void merge(pas_simple_large_free_heap* heap, pas_large_free new_free,
                  pas_large_free_heap_config* config)
{
    enum { max_num_victims = 2 };
    
    size_t victim_indices[max_num_victims];
    size_t num_victims = 0;
    size_t index;
    pas_large_free merger;

    if (verbose) {
        pas_log("%p: Merging %p...%p (%s)\n",
                heap, (void*)((uintptr_t)new_free.begin), (void*)((uintptr_t)new_free.end),
                new_free.zero_mode ? "zero" : "non-zero");
    }
    
    pas_simple_large_free_heap_num_merge_calls++;
    
    for (index = 0; index < heap->free_list_size; ++index) {
        pas_simple_large_free_heap_num_merge_nodes_considered++;
        if (pas_large_free_can_merge(*free_list_entry(heap, index),
                                     new_free,
                                     config)) {
            PAS_ASSERT(num_victims < max_num_victims);
            victim_indices[num_victims++] = index;
        }
    }
    
    PAS_ASSERT(num_victims <= 2);
    
    if (!num_victims) {
        append(heap, new_free);
        return;
    }
    
    PAS_ASSERT(num_victims == 1 || num_victims == 2);
    
    merger = pas_large_free_create_merged_for_sure(
        new_free,
        *free_list_entry(heap, victim_indices[0]),
        config);
    if (num_victims == 2) {
        merger = pas_large_free_create_merged_for_sure(
            merger,
            *free_list_entry(heap, victim_indices[1]),
            config);
    }
    
    *free_list_entry(heap, victim_indices[0]) = merger;
    
    if (num_victims == 2)
        *free_list_entry(heap, victim_indices[1]) = *free_list_entry(heap, --heap->free_list_size);
}

static void remove_entry(pas_simple_large_free_heap* heap,
                         size_t index)
{
    *free_list_entry(heap, index) = *free_list_entry(heap, --heap->free_list_size);
    PAS_ASSERT(free_list_entry(heap, index)->begin);
    PAS_ASSERT(free_list_entry(heap, index)->end > free_list_entry(heap, index)->begin);
}

static pas_generic_large_free_cursor* index_to_cursor(size_t index)
{
    return (pas_generic_large_free_cursor*)(index + 1);
}

static size_t cursor_to_index(pas_generic_large_free_cursor* cursor)
{
    PAS_ASSERT(cursor);
    return (size_t)cursor - 1;
}

PAS_API uint64_t pas_simple_large_free_heap_num_find_first_calls;
PAS_API uint64_t pas_simple_large_free_heap_num_find_first_nodes_considered;

static PAS_ALWAYS_INLINE void simple_find_first(
    pas_generic_large_free_heap* generic_heap,
    size_t min_size,
    bool (*test_candidate)(
        pas_generic_large_free_cursor* cursor,
        pas_large_free candidate,
        void* arg),
    void* arg)
{
    pas_simple_large_free_heap* heap;
    size_t index;

    PAS_UNUSED_PARAM(min_size);
    
    pas_simple_large_free_heap_num_find_first_calls++;
    
    heap = (pas_simple_large_free_heap*)generic_heap;
    
    for (index = 0; index < heap->free_list_size; ++index) {
        pas_large_free candidate;
        
        pas_simple_large_free_heap_num_find_first_nodes_considered++;
        
        candidate = *free_list_entry(heap, index);

        if (verbose)
            pas_log("About to do test_candidate on heap %p.\n", generic_heap);
        
        test_candidate(index_to_cursor(index), candidate, arg);
    }
}

static PAS_ALWAYS_INLINE pas_generic_large_free_cursor* simple_find_by_end(
    pas_generic_large_free_heap* generic_heap,
    uintptr_t end)
{
    pas_simple_large_free_heap* heap;
    size_t index;
    
    heap = (pas_simple_large_free_heap*)generic_heap;
    
    for (index = 0; index < heap->free_list_size; ++index) {
        pas_large_free candidate;
        
        candidate = *free_list_entry(heap, index);
        
        if (candidate.end == end)
            return index_to_cursor(index);
    }
    
    return NULL;
}

static PAS_ALWAYS_INLINE pas_large_free simple_read_cursor(
    pas_generic_large_free_heap* generic_heap,
    pas_generic_large_free_cursor* cursor)
{
    pas_simple_large_free_heap* heap;
    
    heap = (pas_simple_large_free_heap*)generic_heap;
    
    return *free_list_entry(heap, cursor_to_index(cursor));
}

static PAS_ALWAYS_INLINE void simple_write_cursor(
    pas_generic_large_free_heap* generic_heap,
    pas_generic_large_free_cursor* cursor,
    pas_large_free value)
{
    pas_simple_large_free_heap* heap;
    
    heap = (pas_simple_large_free_heap*)generic_heap;

    if (verbose) {
        pas_log("%p: Writing %p...%p (%s)\n",
                heap, (void*)((uintptr_t)value.begin), (void*)((uintptr_t)value.end),
                value.zero_mode ? "zero" : "non-zero");
    }
    
    *free_list_entry(heap, cursor_to_index(cursor)) = value;
}

static PAS_ALWAYS_INLINE void simple_merge(
    pas_generic_large_free_heap* generic_heap,
    pas_large_free new_free,
    pas_large_free_heap_config* config)
{
    pas_simple_large_free_heap* heap;
    
    heap = (pas_simple_large_free_heap*)generic_heap;
    
    merge(heap, new_free, config);
}

static PAS_ALWAYS_INLINE void simple_remove(
    pas_generic_large_free_heap* generic_heap,
    pas_generic_large_free_cursor* cursor)
{
    pas_simple_large_free_heap* heap;
    
    heap = (pas_simple_large_free_heap*)generic_heap;
    
    remove_entry(heap, cursor_to_index(cursor));
}

static PAS_ALWAYS_INLINE void simple_append(
    pas_generic_large_free_heap* generic_heap,
    pas_large_free value)
{
    pas_simple_large_free_heap* heap;
    
    heap = (pas_simple_large_free_heap*)generic_heap;
    
    append(heap, value);
}

static void simple_commit(
    pas_generic_large_free_heap* generic_heap,
    pas_generic_large_free_cursor* cursor,
    pas_large_free allocation)
{
    PAS_UNUSED_PARAM(generic_heap);
    PAS_UNUSED_PARAM(cursor);
    PAS_UNUSED_PARAM(allocation);
}

static void simple_dump(
    pas_generic_large_free_heap* generic_heap)
{
    pas_simple_large_free_heap* heap;
    
    heap = (pas_simple_large_free_heap*)generic_heap;
    
    dump_free_list(heap);
}

static PAS_ALWAYS_INLINE void simple_add_mapped_bytes(
    pas_generic_large_free_heap* generic_heap,
    size_t num_bytes)
{
    pas_simple_large_free_heap* heap;
    
    heap = (pas_simple_large_free_heap*)generic_heap;
    
    heap->num_mapped_bytes += num_bytes;
}

static PAS_ALWAYS_INLINE void initialize_generic_heap_config(
    pas_generic_large_free_heap_config* generic_heap_config)
{
    generic_heap_config->find_first = simple_find_first;
    generic_heap_config->find_by_end = simple_find_by_end;
    generic_heap_config->read_cursor = simple_read_cursor;
    generic_heap_config->write_cursor = simple_write_cursor;
    generic_heap_config->merge = simple_merge;
    generic_heap_config->remove = simple_remove;
    generic_heap_config->append = simple_append;
    generic_heap_config->commit = simple_commit;
    generic_heap_config->dump = simple_dump;
    generic_heap_config->add_mapped_bytes = simple_add_mapped_bytes;
}

static void merge_physical(pas_simple_large_free_heap* heap,
                           uintptr_t begin,
                           uintptr_t end,
                           uintptr_t offset_in_type,
                           pas_zero_mode zero_mode,
                           pas_large_free_heap_config* config)
{
    pas_generic_large_free_heap_config generic_heap_config;
    initialize_generic_heap_config(&generic_heap_config);
    pas_generic_large_free_heap_merge_physical(
        (pas_generic_large_free_heap*)heap,
        begin, end, offset_in_type, zero_mode, config,
        &generic_heap_config);
}

static pas_allocation_result try_allocate_without_fixing(pas_simple_large_free_heap* heap,
                                                         size_t size,
                                                         pas_alignment alignment,
                                                         pas_large_free_heap_config* config)
{
    pas_generic_large_free_heap_config generic_heap_config;
    initialize_generic_heap_config(&generic_heap_config);
    return pas_generic_large_free_heap_try_allocate(
        (pas_generic_large_free_heap*)heap,
        size, alignment, config,
        &generic_heap_config);
}

static void fix_free_list_if_necessary(pas_simple_large_free_heap* heap,
                                       pas_large_free_heap_config* config)
{
    size_t new_capacity;
    pas_large_free* new_free_list;
    size_t index;
    pas_large_free* old_free_list;
    size_t old_capacity;
    
    if (heap != &pas_bootstrap_free_heap)
        return;
    
    PAS_ASSERT(config->type_size == 1);
    
    old_free_list = heap->free_list;
    old_capacity = heap->free_list_capacity;
    
    if (heap->free_list_size <= old_capacity)
        return;
    
    new_capacity = PAS_MAX(heap->free_list_size << 1, PAS_BOOTSTRAP_FREE_LIST_MINIMUM_SIZE);
    
    if (verbose && verbosity >= 2)
        printf("Allocating new free list with new_capacity = %zu:\n", new_capacity);
    new_free_list = (void*)try_allocate_without_fixing(
        heap,
        new_capacity * sizeof(pas_large_free),
        pas_alignment_create_traditional(sizeof(void*)),
        config).begin;
    PAS_ASSERT(new_free_list);

    /* If the original free_list_size was 2 or more, then we will get at least two additional slots
       on top of the ones necessary to store the entire free list. If the original free_list_size was
       1, then we will grow it to 4, ensuring that we get 3 additional slots. So, even if the
       try_allocate_without_fixing() call added two new things to the bootstrap free list, we will
       still have room for them. */
    PAS_ASSERT(heap->free_list_size <= new_capacity);
    
    for (index = 0; index < heap->free_list_size; ++index) {
        new_free_list[index] = *free_list_entry(heap, index);
        PAS_ASSERT(new_free_list[index].begin);
        PAS_ASSERT(new_free_list[index].end > new_free_list[index].begin);
    }
    
    heap->free_list = new_free_list;
    heap->free_list_capacity = new_capacity;

    if (old_free_list) {
        merge_physical(
            heap,
            (uintptr_t)old_free_list,
            (uintptr_t)old_free_list + old_capacity * sizeof(pas_large_free),
            0,
            pas_zero_mode_may_have_non_zero,
            config);
    }

    if (verbose && verbosity >= 2) {
        printf("Fixed:\n");
        dump_free_list(heap);
    }
}

pas_allocation_result pas_simple_large_free_heap_try_allocate(pas_simple_large_free_heap* heap,
                                                              size_t size,
                                                              pas_alignment alignment,
                                                              pas_large_free_heap_config* config)
{
    pas_allocation_result result;
    
    result = try_allocate_without_fixing(heap, size, alignment, config);
    
    fix_free_list_if_necessary(heap, config);
    
    if (verbose) {
        printf("After allocating %p for size %zu:\n", (void*)result.begin, size);
        dump_free_list(heap);
    }

    return result;
}

void pas_simple_large_free_heap_deallocate(pas_simple_large_free_heap* heap,
                                           uintptr_t begin,
                                           uintptr_t end,
                                           pas_zero_mode zero_mode,
                                           pas_large_free_heap_config* config)
{
    PAS_ASSERT(end > begin);
    PAS_ASSERT(begin);
    pas_heap_lock_assert_held();
    merge_physical(heap, begin, end, 0, zero_mode, config);
    fix_free_list_if_necessary(heap, config);

    if (verbose) {
        printf("After deallocating %p for size %zu:\n", (void*)begin, end - begin);
        dump_free_list(heap);
    }
}

void pas_simple_large_free_heap_for_each_free(pas_simple_large_free_heap* heap,
                                              pas_large_free_visitor visitor,
                                              void* arg)
{
    size_t i;
    
    for (i = heap->free_list_size; i--;) {
        if (!visitor(*free_list_entry(heap, i), arg))
            return;
    }
}

size_t pas_simple_large_free_heap_get_num_free_bytes(pas_simple_large_free_heap* heap)
{
    size_t i;
    size_t result;
    
    result = 0;
    for (i = heap->free_list_size; i--;)
        result += pas_large_free_size(*free_list_entry(heap, i));
    
    return result;
}

void pas_simple_large_free_heap_dump_to_printf(pas_simple_large_free_heap* heap)
{
    dump_free_list(heap);
}

#endif /* LIBPAS_ENABLED */
