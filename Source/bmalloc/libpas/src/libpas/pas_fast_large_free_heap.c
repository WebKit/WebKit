/*
 * Copyright (c) 2019-2020 Apple Inc. All rights reserved.
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

#include "pas_fast_large_free_heap.h"

#include "pas_generic_large_free_heap.h"
#include "pas_utility_heap.h"

/* It so happens that we can use this for both x (address) and y (size). */
static PAS_ALWAYS_INLINE int key_compare_callback(void* raw_left,
                                                  void* raw_right)
{
    uintptr_t left_value;
    uintptr_t right_value;

    left_value = (uintptr_t)raw_left;
    right_value = (uintptr_t)raw_right;

    if (left_value < right_value)
        return -1;
    if (left_value == right_value)
        return 0;
    return 1;
}

static PAS_ALWAYS_INLINE void* get_x_key_callback(pas_cartesian_tree_node* node)
{
    return (void*)((uintptr_t)((pas_fast_large_free_heap_node*)node)->free.begin);
}

static PAS_ALWAYS_INLINE void* get_y_key_callback(pas_cartesian_tree_node* node)
{
    return (void*)pas_large_free_size(((pas_fast_large_free_heap_node*)node)->free);
}

static PAS_ALWAYS_INLINE void
initialize_cartesian_config(pas_cartesian_tree_config* config)
{
    config->get_x_key = get_x_key_callback;
    config->get_y_key = get_y_key_callback;
    config->x_compare = key_compare_callback;
    config->y_compare = key_compare_callback;
}

void pas_fast_large_free_heap_construct(pas_fast_large_free_heap* heap)
{
    pas_cartesian_tree_construct(&heap->tree);
    heap->num_mapped_bytes = 0;
}

static void insert_node(pas_fast_large_free_heap* heap,
                        pas_large_free new_free)
{
    pas_fast_large_free_heap_node* node;
    pas_cartesian_tree_config cartesian_config;
    
    PAS_ASSERT(new_free.begin);
    PAS_ASSERT(new_free.end > new_free.begin);
    
    node = pas_utility_heap_allocate(sizeof(pas_fast_large_free_heap_node),
                                     "pas_fast_large_free_heap_node");
    node->free = new_free;

    initialize_cartesian_config(&cartesian_config);
    pas_cartesian_tree_insert(
        &heap->tree,
        (void*)((uintptr_t)new_free.begin),
        (void*)pas_large_free_size(new_free),
        &node->tree_node,
        &cartesian_config);
}

static void remove_node(pas_fast_large_free_heap* heap,
                        pas_fast_large_free_heap_node* node)
{
    pas_cartesian_tree_config cartesian_config;
    initialize_cartesian_config(&cartesian_config);
    pas_cartesian_tree_remove(&heap->tree, &node->tree_node, &cartesian_config);
    pas_utility_heap_deallocate(node);
}

static void dump_heap(pas_fast_large_free_heap* heap)
{
    pas_cartesian_tree_node* tree_node;
    
    printf("Fast free list:");
    
    for (tree_node = pas_cartesian_tree_minimum(&heap->tree);
         tree_node;
         tree_node = pas_cartesian_tree_node_successor(tree_node)) {
        pas_fast_large_free_heap_node* node;
        
        node = (pas_fast_large_free_heap_node*)tree_node;
        
        printf(" [%p, %p)", (void*)((uintptr_t)node->free.begin), (void*)((uintptr_t)node->free.end));
    }
    
    printf("\n");
}

static PAS_ALWAYS_INLINE void fast_find_first(
    pas_generic_large_free_heap* generic_heap,
    size_t min_size,
    bool (*test_candidate)(
        pas_generic_large_free_cursor* cursor,
        pas_large_free candidate,
        void* arg),
    void* arg)
{
    pas_fast_large_free_heap* heap;
    pas_cartesian_tree_node* tree_node;
    pas_cartesian_tree_config cartesian_config;
    
    heap = (pas_fast_large_free_heap*)generic_heap;

    initialize_cartesian_config(&cartesian_config);

    for (tree_node = pas_cartesian_tree_minimum_constrained(
             &heap->tree, (void*)min_size, &cartesian_config);
         tree_node;
         tree_node = pas_cartesian_tree_node_successor_constrained(
             tree_node, (void*)min_size, &cartesian_config)) {
        pas_fast_large_free_heap_node* node;
        node = (pas_fast_large_free_heap_node*)tree_node;
        if (test_candidate((pas_generic_large_free_cursor*)node, node->free, arg))
            return;
    }
}

static PAS_ALWAYS_INLINE pas_generic_large_free_cursor* fast_find_by_end(
    pas_generic_large_free_heap* generic_heap,
    uintptr_t end)
{
    pas_fast_large_free_heap* heap;
    pas_cartesian_tree_config cartesian_config;
    pas_fast_large_free_heap_node* node;

    heap = (pas_fast_large_free_heap*)generic_heap;

    initialize_cartesian_config(&cartesian_config);
    
    node = (pas_fast_large_free_heap_node*)
        pas_cartesian_tree_find_greatest_less_than(
            &heap->tree, (void*)end, &cartesian_config);
    
    if (node && node->free.end == end)
        return (pas_generic_large_free_cursor*)node;
    
    return NULL;
}

static PAS_ALWAYS_INLINE pas_large_free fast_read_cursor(
    pas_generic_large_free_heap* generic_heap,
    pas_generic_large_free_cursor* cursor)
{
    PAS_UNUSED_PARAM(generic_heap);
    return ((pas_fast_large_free_heap_node*)cursor)->free;
}

static void fast_write_cursor(
    pas_generic_large_free_heap* generic_heap,
    pas_generic_large_free_cursor* cursor,
    pas_large_free value)
{
    pas_fast_large_free_heap* heap;
    pas_fast_large_free_heap_node* node;
    bool need_to_readd_to_tree;
    pas_cartesian_tree_config cartesian_config;
    
    heap = (pas_fast_large_free_heap*)generic_heap;
    node = (pas_fast_large_free_heap_node*)cursor;
    
    need_to_readd_to_tree = false;

    if (value.begin != node->free.begin) {
        if (value.begin < node->free.begin) {
            pas_fast_large_free_heap_node* prev_node;
            prev_node = (pas_fast_large_free_heap_node*)
                pas_cartesian_tree_node_predecessor(&node->tree_node);
            need_to_readd_to_tree |= prev_node && value.begin <= prev_node->free.begin;
        } else {
            pas_fast_large_free_heap_node* next_node;
            next_node = (pas_fast_large_free_heap_node*)
                pas_cartesian_tree_node_successor(&node->tree_node);
            need_to_readd_to_tree |= next_node && value.begin >= next_node->free.begin;
        }
    }

    if (pas_large_free_size(value) != pas_large_free_size(node->free)) {
        if (pas_large_free_size(value) < pas_large_free_size(node->free)) {
            pas_fast_large_free_heap_node* parent_node;
            parent_node = (pas_fast_large_free_heap_node*)
                pas_compact_cartesian_tree_node_ptr_load(&node->tree_node.parent);
            need_to_readd_to_tree |= parent_node
                && pas_large_free_size(value) < pas_large_free_size(parent_node->free);
        } else {
            pas_fast_large_free_heap_node* left_node;
            pas_fast_large_free_heap_node* right_node;
            left_node = (pas_fast_large_free_heap_node*)
                pas_compact_cartesian_tree_node_ptr_load(&node->tree_node.left);
            right_node = (pas_fast_large_free_heap_node*)
                pas_compact_cartesian_tree_node_ptr_load(&node->tree_node.right);
            need_to_readd_to_tree |=
                (left_node && pas_large_free_size(value) > pas_large_free_size(left_node->free)) ||
                (right_node && pas_large_free_size(value) > pas_large_free_size(right_node->free));
        }
    }

    initialize_cartesian_config(&cartesian_config);
    
    if (need_to_readd_to_tree)
        pas_cartesian_tree_remove(&heap->tree, &node->tree_node, &cartesian_config);
    
    node->free = value;
    
    if (need_to_readd_to_tree) {
        pas_cartesian_tree_insert(
            &heap->tree,
            (void*)((uintptr_t)value.begin),
            (void*)pas_large_free_size(value),
            &node->tree_node,
            &cartesian_config);
    }
}

static void fast_merge(
    pas_generic_large_free_heap* generic_heap,
    pas_large_free new_free,
    pas_large_free_heap_config* config)
{
    pas_fast_large_free_heap* heap;
    pas_fast_large_free_heap_node* left_node;
    pas_fast_large_free_heap_node* right_node;
    pas_large_free merger;
    pas_cartesian_tree_config cartesian_config;
    
    heap = (pas_fast_large_free_heap*)generic_heap;

    initialize_cartesian_config(&cartesian_config);
    
    left_node = (pas_fast_large_free_heap_node*)
        pas_cartesian_tree_find_greatest_less_than(
            &heap->tree, (void*)((uintptr_t)new_free.begin), &cartesian_config);
    if (left_node && (
            left_node->free.end != new_free.begin
            || !pas_large_free_can_merge(left_node->free, new_free, config)))
        left_node = NULL;
    
    right_node = (pas_fast_large_free_heap_node*)
        pas_cartesian_tree_find_exact(
            &heap->tree, (void*)((uintptr_t)new_free.end), &cartesian_config);
    if (right_node && !pas_large_free_can_merge(right_node->free, new_free, config))
        right_node = NULL;
    
    merger = new_free;
    if (left_node)
        merger = pas_large_free_create_merged_for_sure(merger, left_node->free, config);
    if (right_node)
        merger = pas_large_free_create_merged_for_sure(merger, right_node->free, config);
    
    if (left_node) {
        pas_fast_large_free_heap_node* left_left_node;
        pas_fast_large_free_heap_node* left_right_node;
        bool need_to_readd_to_tree;
        
        if (right_node)
            remove_node(heap, right_node);
        
        /* Thanks to this fact, we don't have to change left_node's position in the tree! */
        PAS_ASSERT(left_node->free.begin == merger.begin);
        PAS_ASSERT(left_node->free.end < merger.end);
        
        left_left_node = (pas_fast_large_free_heap_node*)
            pas_compact_cartesian_tree_node_ptr_load(&left_node->tree_node.left);
        left_right_node = (pas_fast_large_free_heap_node*)
            pas_compact_cartesian_tree_node_ptr_load(&left_node->tree_node.right);
        need_to_readd_to_tree =
            (left_left_node
             && pas_large_free_size(merger) > pas_large_free_size(left_left_node->free)) ||
            (left_right_node
             && pas_large_free_size(merger) > pas_large_free_size(left_right_node->free));

        if (need_to_readd_to_tree)
            pas_cartesian_tree_remove(&heap->tree, &left_node->tree_node, &cartesian_config);
        
        left_node->free = merger;

        if (need_to_readd_to_tree) {
            pas_cartesian_tree_insert(
                &heap->tree,
                (void*)((uintptr_t)merger.begin),
                (void*)pas_large_free_size(merger),
                &left_node->tree_node,
                &cartesian_config);
        }
        return;
    }
    
    if (right_node) {
        pas_cartesian_tree_remove(&heap->tree, &right_node->tree_node, &cartesian_config);
        
        PAS_ASSERT(right_node->free.end == merger.end);
        right_node->free = merger;
        
        pas_cartesian_tree_insert(
            &heap->tree,
            (void*)((uintptr_t)merger.begin),
            (void*)pas_large_free_size(merger),
            &right_node->tree_node,
            &cartesian_config);
        return;
    }
    
    insert_node(heap, new_free);
}

static void fast_remove(
    pas_generic_large_free_heap* generic_heap,
    pas_generic_large_free_cursor* cursor)
{
    pas_fast_large_free_heap* heap;
    
    heap = (pas_fast_large_free_heap*)generic_heap;
    
    remove_node(heap, (pas_fast_large_free_heap_node*)cursor);
}

static void fast_append(
    pas_generic_large_free_heap* generic_heap,
    pas_large_free value)
{
    pas_fast_large_free_heap* heap;
    
    heap = (pas_fast_large_free_heap*)generic_heap;
    
    insert_node(heap, value);
}

static void fast_commit(
    pas_generic_large_free_heap* generic_heap,
    pas_generic_large_free_cursor* cursor,
    pas_large_free allocation)
{
    PAS_UNUSED_PARAM(generic_heap);
    PAS_UNUSED_PARAM(cursor);
    PAS_UNUSED_PARAM(allocation);
}

static void fast_dump(
    pas_generic_large_free_heap* generic_heap)
{
    pas_fast_large_free_heap* heap;
    
    heap = (pas_fast_large_free_heap*)generic_heap;
    
    dump_heap(heap);
}

static PAS_ALWAYS_INLINE void fast_add_mapped_bytes(
    pas_generic_large_free_heap* generic_heap,
    size_t num_bytes)
{
    pas_fast_large_free_heap* heap;
    
    heap = (pas_fast_large_free_heap*)generic_heap;
    
    heap->num_mapped_bytes += num_bytes;
}

static PAS_ALWAYS_INLINE void initialize_generic_heap_config(
    pas_generic_large_free_heap_config* generic_heap_config)
{
    pas_zero_memory(generic_heap_config, sizeof(pas_generic_large_free_heap_config));
    generic_heap_config->find_first = fast_find_first;
    generic_heap_config->find_by_end = fast_find_by_end;
    generic_heap_config->read_cursor = fast_read_cursor;
    generic_heap_config->write_cursor = fast_write_cursor;
    generic_heap_config->merge = fast_merge;
    generic_heap_config->remove = fast_remove;
    generic_heap_config->append = fast_append;
    generic_heap_config->commit = fast_commit;
    generic_heap_config->dump = fast_dump;
    generic_heap_config->add_mapped_bytes = fast_add_mapped_bytes;
}

pas_allocation_result pas_fast_large_free_heap_try_allocate(pas_fast_large_free_heap* heap,
                                                            size_t size,
                                                            pas_alignment alignment,
                                                            pas_large_free_heap_config* config)
{
    static const bool verbose = PAS_SHOULD_LOG(PAS_LOG_LARGE_HEAPS);
    pas_generic_large_free_heap_config generic_heap_config;
    if (verbose)
        pas_log("heap %p allocating %zu, alignment %zu\n", heap, size, alignment.alignment);
    initialize_generic_heap_config(&generic_heap_config);
    return pas_generic_large_free_heap_try_allocate(
        (pas_generic_large_free_heap*)heap,
        size, alignment, config,
        &generic_heap_config);
}

void pas_fast_large_free_heap_deallocate(pas_fast_large_free_heap* heap,
                                         uintptr_t begin,
                                         uintptr_t end,
                                         pas_zero_mode zero_mode,
                                         pas_large_free_heap_config* config)
{
    static const bool verbose = PAS_SHOULD_LOG(PAS_LOG_LARGE_HEAPS);
    pas_generic_large_free_heap_config generic_heap_config;
    if (verbose)
        pas_log("heap %p deallocating %p of size %zu\n", heap, (void*)begin, end - begin);
    initialize_generic_heap_config(&generic_heap_config);
    pas_generic_large_free_heap_merge_physical(
        (pas_generic_large_free_heap*)heap,
        begin, end, 0, zero_mode, config,
        &generic_heap_config);
}

void pas_fast_large_free_heap_for_each_free(pas_fast_large_free_heap* heap,
                                            pas_large_free_visitor visitor,
                                            void* arg)
{
    pas_cartesian_tree_node* tree_node;
    
    for (tree_node = pas_cartesian_tree_minimum(&heap->tree);
         tree_node;
         tree_node = pas_cartesian_tree_node_successor(tree_node)) {
        pas_fast_large_free_heap_node* node;
        
        node = (pas_fast_large_free_heap_node*)tree_node;
        
        if (!visitor(node->free, arg))
            return;
    }
}

size_t pas_fast_large_free_heap_get_num_free_bytes(pas_fast_large_free_heap* heap)
{
    pas_cartesian_tree_node* tree_node;
    size_t result;
    
    result = 0;

    for (tree_node = pas_cartesian_tree_minimum(&heap->tree);
         tree_node;
         tree_node = pas_cartesian_tree_node_successor(tree_node)) {
        pas_fast_large_free_heap_node* node;
        
        node = (pas_fast_large_free_heap_node*)tree_node;
        
        result += pas_large_free_size(node->free);
    }

    return result;
}

void pas_fast_large_free_heap_validate(pas_fast_large_free_heap* heap)
{
    pas_cartesian_tree_config cartesian_config;
    initialize_cartesian_config(&cartesian_config);
    pas_cartesian_tree_validate(&heap->tree, &cartesian_config);
}

void pas_fast_large_free_heap_dump_to_printf(pas_fast_large_free_heap* heap)
{
    dump_heap(heap);
}

#endif /* LIBPAS_ENABLED */
