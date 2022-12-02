/*
 * Copyright (c) 2020-2021 Apple Inc. All rights reserved.
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

#ifndef PAS_ROOT_H
#define PAS_ROOT_H

#include "pas_heap.h"
#include "pas_thread_local_cache_layout_node.h"
#include "pas_utils.h"

#if PAS_OS(DARWIN)
#include <malloc/malloc.h>
#endif

PAS_BEGIN_EXTERN_C;

struct pas_baseline_allocator;
struct pas_enumerable_range_list;
struct pas_heap_config;
struct pas_large_map_hashtable;
struct pas_large_map_hashtable_in_flux_stash;
struct pas_red_black_tree;
struct pas_red_black_tree_jettisoned_nodes;
struct pas_root;
struct pas_small_large_map_hashtable;
struct pas_small_large_map_hashtable_in_flux_stash;
struct pas_thread_local_cache_node;
struct pas_tiny_large_map_hashtable;
struct pas_tiny_large_map_hashtable_in_flux_stash;
struct pas_tiny_large_map_second_level_hashtable_in_flux_stash;
typedef struct pas_baseline_allocator pas_baseline_allocator;
typedef struct pas_enumerable_range_list pas_enumerable_range_list;
typedef struct pas_heap_config pas_heap_config;
typedef struct pas_large_map_hashtable pas_large_map_hashtable;
typedef struct pas_large_map_hashtable_in_flux_stash pas_large_map_hashtable_in_flux_stash;
typedef struct pas_red_black_tree pas_red_black_tree;
typedef struct pas_red_black_tree_jettisoned_nodes pas_red_black_tree_jettisoned_nodes;
typedef struct pas_root pas_root;
typedef struct pas_small_large_map_hashtable pas_small_large_map_hashtable;
typedef struct pas_small_large_map_hashtable_in_flux_stash pas_small_large_map_hashtable_in_flux_stash;
typedef struct pas_thread_local_cache_node pas_thread_local_cache_node;
typedef struct pas_thread_local_cache_layout_segment pas_thread_local_cache_layout_segment;
typedef struct pas_tiny_large_map_hashtable pas_tiny_large_map_hashtable;
typedef struct pas_tiny_large_map_hashtable_in_flux_stash pas_tiny_large_map_hashtable_in_flux_stash;
typedef struct pas_tiny_large_map_second_level_hashtable_in_flux_stash pas_tiny_large_map_second_level_hashtable_in_flux_stash;

struct pas_root {
    uintptr_t magic;
    uintptr_t* compact_heap_reservation_base;
    size_t* compact_heap_reservation_size;
    size_t* compact_heap_reservation_guard_size;
    size_t* compact_heap_reservation_available_size;
    size_t* compact_heap_reservation_bump;
    pas_enumerable_range_list* enumerable_page_malloc_page_list;
    pas_enumerable_range_list* large_heap_physical_page_sharing_cache_page_list;
    pas_enumerable_range_list* payload_reservation_page_list;
    pas_thread_local_cache_node** thread_local_cache_node_first;
    pas_thread_local_cache_layout_segment** thread_local_cache_layout_first_segment;
    pas_heap** all_heaps_first_heap;
    pas_heap** static_heaps;
    size_t num_static_heaps;
    pas_large_map_hashtable* large_map_hashtable_instance;
    pas_large_map_hashtable_in_flux_stash* large_map_hashtable_instance_in_flux_stash;
    pas_small_large_map_hashtable* small_large_map_hashtable_instance;
    pas_small_large_map_hashtable_in_flux_stash* small_large_map_hashtable_instance_in_flux_stash;
    pas_tiny_large_map_hashtable* tiny_large_map_hashtable_instance;
    pas_tiny_large_map_hashtable_in_flux_stash* tiny_large_map_hashtable_instance_in_flux_stash;
    pas_tiny_large_map_second_level_hashtable_in_flux_stash* tiny_large_map_second_level_hashtable_in_flux_stash_instance;
    const pas_heap_config** heap_configs;
    unsigned num_heap_configs;
    pas_red_black_tree* large_sharing_tree;
    pas_red_black_tree_jettisoned_nodes* large_sharing_tree_jettisoned_nodes;
    size_t page_malloc_alignment;
    pas_baseline_allocator** baseline_allocator_table;
    size_t num_baseline_allocators;
};

#define PAS_ROOT_MAGIC 0xbeeeeeeeeflu

PAS_API void pas_root_construct(pas_root* root);
PAS_API pas_root* pas_root_create(void);

#if PAS_OS(DARWIN)
PAS_API kern_return_t pas_root_enumerate_for_libmalloc(pas_root* remote_root,
                                                       task_t task,
                                                       void* context,
                                                       unsigned type_mask,
                                                       memory_reader_t reader,
                                                       vm_range_recorder_t recorder);

PAS_API kern_return_t pas_root_enumerate_for_libmalloc_with_root_after_zone(
    task_t task,
    void *context,
    unsigned type_mask,
    vm_address_t zone_address,
    memory_reader_t reader,
    vm_range_recorder_t recorder);

PAS_API extern pas_root* pas_root_for_libmalloc_enumeration;

/* This creates the root used for libmalloc enumeration, if there wasn't one already. Clients of
   libpas can choose not to call this, for example because they want to set up enumeration manually. */
PAS_API pas_root* pas_root_ensure_for_libmalloc_enumeration(void);

#define PAS_MAX_CANDIDATE_POINTERS 256

typedef struct pas_conservative_candidate_pointer_and_location {
    vm_address_t address_of_pointer;
    vm_address_t candidate_pointer;
} pas_conservative_candidate_pointer_and_location;

typedef kern_return_t (*pas_pointer_visitor_t)(void* context, const pas_conservative_candidate_pointer_and_location*, size_t num_candidates);

PAS_API kern_return_t pas_root_visit_conservative_candidate_pointers_in_address_range(task_t task, void* context, vm_address_t zone_address, vm_address_t address, size_t size, memory_reader_t reader, pas_pointer_visitor_t visitor);

#endif

PAS_END_EXTERN_C;

#endif /* PAS_ROOT_H */

