/*
 * Copyright (c) 2020 Apple Inc. All rights reserved.
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

#include "pas_skip_list.h"

#include "pas_random.h"
#include "pas_skip_list_inlines.h"
#include "pas_utility_heap.h"

void pas_skip_list_construct(pas_skip_list* list)
{
    *list = PAS_SKIP_LIST_INITIALIZER;
}

void* pas_skip_list_node_allocate_with_height(size_t offset_of_skip_list_node,
                                              unsigned height)
{
    size_t total_size;
    void* result;
    pas_skip_list_node* node;

    PAS_TESTING_ASSERT(height);
    PAS_TESTING_ASSERT(!(offset_of_skip_list_node % PAS_INTERNAL_MIN_ALIGN));

    total_size =
        offset_of_skip_list_node +
        PAS_OFFSETOF(pas_skip_list_node, level) +
        sizeof(pas_skip_list_level) * height;

    result = pas_utility_heap_allocate(total_size, "pas_skip_list_node");

    node = (pas_skip_list_node*)((char*)result + offset_of_skip_list_node);

    node->height = height;

    /* NOTE: The actual levels are initialized in insert_after. That makes reinserting after
       removing but without reallocating just work. */

    return result;
}

void* pas_skip_list_node_allocate(size_t offset_of_skip_list_node)
{
    unsigned height;
    unsigned random;

    random = pas_get_random(pas_cheesy_random, 0);
    if (!random)
        height = 33;
    else
        height = __builtin_clz(random) + 1;

    return pas_skip_list_node_allocate_with_height(offset_of_skip_list_node, height);
}

void pas_skip_list_node_deallocate(void* ptr)
{
    pas_utility_heap_deallocate(ptr);
}

void pas_skip_list_remove(pas_skip_list* list,
                          pas_skip_list_node* node)
{
    static const bool verbose = false;
    
    unsigned index;
    pas_compact_skip_list_node_ptr* head;
    unsigned node_height;
    unsigned head_height;
    bool has_seen_non_null_next;

    if (verbose)
        pas_log("Removing node %p from list %p.\n", node, list);

    head = pas_compact_skip_list_node_ptr_ptr_load(&list->head);
    PAS_TESTING_ASSERT(head);

    node_height = node->height;
    head_height = list->height;
    has_seen_non_null_next = node_height < head_height;
    
    for (index = node_height; index--;) {
        pas_skip_list_node* prev_node;
        pas_skip_list_node* next_node;

        prev_node = pas_compact_skip_list_node_ptr_load(&node->level[index].prev);
        next_node = pas_compact_skip_list_node_ptr_load(&node->level[index].next);

        if (next_node) {
            pas_compact_skip_list_node_ptr_store(&next_node->level[index].prev, prev_node);
            has_seen_non_null_next = true;
        }

        if (prev_node)
            pas_compact_skip_list_node_ptr_store(&prev_node->level[index].next, next_node);
        else {
            pas_compact_skip_list_node_ptr_store(head + index, next_node);
            if (!has_seen_non_null_next)
                head_height = index;
        }
    }

    list->height = head_height;
}

size_t pas_skip_list_size(pas_skip_list* list)
{
    size_t result;
    pas_skip_list_node* node;

    result = 0;

    for (node = pas_skip_list_head(list); node; node = pas_skip_list_node_next(node))
        result++;

    return result;
}

static void validate_other_node(pas_skip_list* list,
                                pas_skip_list_node* node,
                                size_t index,
                                pas_list_direction direction,
                                pas_skip_list_node_compare_callback compare)
{
    pas_skip_list_node* other_node;
    pas_skip_list_node* reflect_node;
    pas_skip_list_node* iter_node;
    int compare_result;

    other_node = pas_skip_list_level_get_direction(node->level + index, direction);
    if (!other_node) {
        for (iter_node = pas_skip_list_level_get_direction(node->level, direction);
             iter_node;
             iter_node = pas_skip_list_level_get_direction(iter_node->level, direction))
            PAS_ASSERT(iter_node->height <= index);
        if (direction == pas_list_direction_prev) {
            PAS_ASSERT(pas_compact_skip_list_node_ptr_load(
                           pas_compact_skip_list_node_ptr_ptr_load(&list->head) + index)
                       == node);
        }
        return;
    }

    PAS_ASSERT(other_node != node);

    compare_result = compare(node, other_node);
    switch (direction) {
    case pas_list_direction_prev:
        PAS_ASSERT(compare_result >= 0);
        break;
    case pas_list_direction_next:
        PAS_ASSERT(compare_result <= 0);
        break;
    }
    
    PAS_ASSERT(other_node->height > index);

    reflect_node = pas_skip_list_level_get_direction(other_node->level + index,
                                                     pas_list_direction_invert(direction));
    PAS_ASSERT(reflect_node == node);

    for (iter_node = pas_skip_list_level_get_direction(node->level, direction);
         iter_node != other_node;
         iter_node = pas_skip_list_level_get_direction(iter_node->level, direction)) {
        PAS_ASSERT(iter_node);
        PAS_ASSERT(iter_node->height <= index);
    }
}

void pas_skip_list_validate(pas_skip_list* list, pas_skip_list_node_compare_callback compare)
{
    pas_skip_list_node* node;
    unsigned index;

    for (node = pas_skip_list_head(list); node; node = pas_skip_list_node_next(node)) {
        size_t index;

        for (index = node->height; index--;) {
            validate_other_node(list, node, index, pas_list_direction_prev, compare);
            validate_other_node(list, node, index, pas_list_direction_next, compare);
        }
    }

    for (index = list->height; index--;) {
        PAS_ASSERT(pas_compact_skip_list_node_ptr_load(
                       pas_compact_skip_list_node_ptr_ptr_load(&list->head) + index));
    }
}

#endif /* LIBPAS_ENABLED */
