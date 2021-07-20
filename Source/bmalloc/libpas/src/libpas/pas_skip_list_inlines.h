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

#ifndef PAS_SKIP_LIST_INLINES_H
#define PAS_SKIP_LIST_INLINES_H

#include "pas_list_direction.h"
#include "pas_log.h"
#include "pas_skip_list.h"
#include "pas_utility_heap.h"

PAS_BEGIN_EXTERN_C;

typedef struct {
    bool did_find_exact;
    pas_skip_list_node* next;
    pas_skip_list_node* prev;
} pas_skip_list_find_result;

static inline pas_skip_list_find_result pas_skip_list_find_result_create_exact(
    pas_skip_list_node* node)
{
    pas_skip_list_find_result result;
    result.did_find_exact = true;
    result.next = node;
    result.prev = node;
    return result;
}

static inline pas_skip_list_find_result pas_skip_list_find_result_create_inexact(
    pas_skip_list_node* prev, pas_skip_list_node* next)
{
    pas_skip_list_find_result result;
    result.did_find_exact = false;
    result.next = next;
    result.prev = prev;
    return result;
}

typedef void (*pas_skip_list_note_head_attachment_callback)(pas_compact_skip_list_node_ptr* head,
                                                            unsigned index,
                                                            pas_skip_list_node* next_node,
                                                            void* arg);

typedef void (*pas_skip_list_note_pole_attachment_callback)(pas_skip_list_node* node,
                                                            unsigned index,
                                                            pas_skip_list_node* next_node,
                                                            void* arg);

static PAS_ALWAYS_INLINE pas_skip_list_node*
pas_skip_list_level_get_direction(pas_skip_list_level* level,
                                  pas_list_direction direction)
{
    switch (direction) {
    case pas_list_direction_prev:
        return pas_compact_skip_list_node_ptr_load(&level->prev);
    case pas_list_direction_next:
        return pas_compact_skip_list_node_ptr_load(&level->next);
    }
    PAS_ASSERT(!"Should not be reached");
    return NULL;
}

static PAS_ALWAYS_INLINE pas_skip_list_find_result
pas_skip_list_find_impl(pas_skip_list* list,
                        void* key,
                        pas_skip_list_node_key_compare_callback compare_key,
                        pas_skip_list_note_head_attachment_callback note_head_attachment,
                        pas_skip_list_note_pole_attachment_callback note_pole_attachment,
                        void* arg)
{
    static const bool verbose = false;
    
    int comparison_result;
    unsigned head_height;
    unsigned current_height;
    pas_skip_list_node* node;
    pas_skip_list_node* next_node;
    pas_compact_skip_list_node_ptr* head;
    pas_skip_list_find_result result;

    result = pas_skip_list_find_result_create_inexact(NULL, NULL); /* Compilers need diapers
                                                                      sometimes. */

    if (verbose)
        pas_log("Starting find for list = %p.\n", list);

    head_height = list->height;
    if (!head_height)
        return pas_skip_list_find_result_create_inexact(NULL, NULL);

    head = pas_compact_skip_list_node_ptr_ptr_load(&list->head);
    PAS_TESTING_ASSERT(head);
    
    current_height = head_height;

    for (;;) {
        node = pas_compact_skip_list_node_ptr_load(head + current_height - 1);
        comparison_result = compare_key(node, key);
        
        if (comparison_result < 0)
            break;
        
        if (!comparison_result)
            return pas_skip_list_find_result_create_exact(node);

        note_head_attachment(head, current_height - 1, node, arg);
        
        current_height--;
        
        if (!current_height)
            return pas_skip_list_find_result_create_inexact(NULL, node);
    }

    for (;;) {
        PAS_TESTING_ASSERT(current_height);
        
        for (;;) {
            next_node = pas_compact_skip_list_node_ptr_load(&node->level[current_height - 1].next);

            if (next_node) {
                comparison_result = compare_key(next_node, key);

                if (comparison_result < 0)
                    break;

                if (!comparison_result)
                    return pas_skip_list_find_result_create_exact(next_node);
            }

            note_pole_attachment(node, current_height - 1, next_node, arg);

            current_height--;

            if (!current_height)
                return pas_skip_list_find_result_create_inexact(node, next_node);
        }

        node = next_node;
    }

    PAS_ASSERT(!"Should not be reached");
}

static PAS_ALWAYS_INLINE void
pas_skip_list_find_ignore_head_attachment(pas_compact_skip_list_node_ptr* head,
                                          unsigned index,
                                          pas_skip_list_node* next_node,
                                          void* arg)
{
    PAS_UNUSED_PARAM(head);
    PAS_UNUSED_PARAM(index);
    PAS_UNUSED_PARAM(next_node);
    PAS_UNUSED_PARAM(arg);
}

static PAS_ALWAYS_INLINE void
pas_skip_list_find_ignore_pole_attachment(pas_skip_list_node* node,
                                          unsigned index,
                                          pas_skip_list_node* next_node,
                                          void* arg)
{
    PAS_UNUSED_PARAM(node);
    PAS_UNUSED_PARAM(index);
    PAS_UNUSED_PARAM(next_node);
    PAS_UNUSED_PARAM(arg);
}

static PAS_ALWAYS_INLINE pas_skip_list_find_result
pas_skip_list_find(pas_skip_list* list,
                   void* key,
                   pas_skip_list_node_key_compare_callback compare_key)
{
    return pas_skip_list_find_impl(list, key, compare_key,
                                   pas_skip_list_find_ignore_head_attachment,
                                   pas_skip_list_find_ignore_pole_attachment,
                                   NULL);
}

typedef struct {
    pas_skip_list_node* node;
    unsigned node_height;
} pas_skip_list_insert_after_data;

static PAS_ALWAYS_INLINE void
pas_skip_list_insert_after_note_head_attachment(
    pas_compact_skip_list_node_ptr* head,
    unsigned index,
    pas_skip_list_node* next_node,
    void* arg)
{
    pas_skip_list_insert_after_data* data;
    
    pas_skip_list_node* node_to_insert;
    unsigned node_to_insert_height;

    data = (pas_skip_list_insert_after_data*)arg;
    node_to_insert = data->node;
    node_to_insert_height = data->node_height;

    if (index >= node_to_insert_height)
        return;
    
    PAS_TESTING_ASSERT(pas_compact_skip_list_node_ptr_is_null(&node_to_insert->level[index].prev));
    PAS_TESTING_ASSERT(pas_compact_skip_list_node_ptr_is_null(&node_to_insert->level[index].next));
    
    PAS_TESTING_ASSERT(next_node);
    PAS_TESTING_ASSERT(next_node != node_to_insert);
    
    pas_compact_skip_list_node_ptr_store(
        &node_to_insert->level[index].next,
        next_node);
    pas_compact_skip_list_node_ptr_store(
        &next_node->level[index].prev,
        node_to_insert);
    pas_compact_skip_list_node_ptr_store(
        head + index,
        node_to_insert);
}

static PAS_ALWAYS_INLINE void
pas_skip_list_insert_after_note_pole_attachment(
    pas_skip_list_node* node,
    unsigned index,
    pas_skip_list_node* next_node,
    void* arg)
{
    pas_skip_list_insert_after_data* data;
    pas_skip_list_node* node_to_insert;
    unsigned node_to_insert_height;

    data = (pas_skip_list_insert_after_data*)arg;
    node_to_insert = data->node;
    node_to_insert_height = data->node_height;

    if (index >= node_to_insert_height)
        return;
    
    PAS_TESTING_ASSERT(pas_compact_skip_list_node_ptr_is_null(&node_to_insert->level[index].prev));
    PAS_TESTING_ASSERT(pas_compact_skip_list_node_ptr_is_null(&node_to_insert->level[index].next));
    
    PAS_TESTING_ASSERT(next_node != node);
    PAS_TESTING_ASSERT(next_node != node_to_insert);
    
    pas_compact_skip_list_node_ptr_store(
        &node_to_insert->level[index].prev,
        node);
    pas_compact_skip_list_node_ptr_store(
        &node_to_insert->level[index].next,
        next_node);
    if (next_node) {
        pas_compact_skip_list_node_ptr_store(
            &next_node->level[index].prev,
            node_to_insert);
    }
    pas_compact_skip_list_node_ptr_store(
        &node->level[index].next,
        node_to_insert);
}

static PAS_ALWAYS_INLINE void
pas_skip_list_insert(
    pas_skip_list* list,
    void* key,
    pas_skip_list_node* node,
    pas_skip_list_node_key_compare_callback compare_key)
{
    static const bool verbose = false;
    
    pas_skip_list_insert_after_data data;
    pas_skip_list_find_result find_result;
    unsigned index;
    unsigned node_height;
    unsigned list_height;

    node_height = node->height;

    if (verbose) {
        pas_log("Inserting: list = %p, key = %p, node = %p, node height = %u.\n",
                list,
                key,
                node,
                node_height);
    }

    for (index = node_height; index--;) {
        pas_compact_skip_list_node_ptr_store(&node->level[index].prev, NULL);
        pas_compact_skip_list_node_ptr_store(&node->level[index].next, NULL);
    }

    data.node = node;
    data.node_height = node_height;

    find_result = pas_skip_list_find_impl(
        list, key, compare_key,
        pas_skip_list_insert_after_note_head_attachment,
        pas_skip_list_insert_after_note_pole_attachment,
        &data);

    PAS_ASSERT(!find_result.did_find_exact);

    list_height = list->height;
    if (node_height > list_height) {
        pas_compact_skip_list_node_ptr* head;

        head = pas_compact_skip_list_node_ptr_ptr_load(&list->head);
        
        if (node_height > list->height_capacity) {
            pas_compact_skip_list_node_ptr* new_head;

            new_head = (pas_compact_skip_list_node_ptr*)
                pas_utility_heap_allocate(
                    sizeof(pas_compact_skip_list_node_ptr) * node_height,
                    "pas_skip_ist/head");

            memcpy(new_head, head,
                   sizeof(pas_compact_skip_list_node_ptr) * list_height);

            pas_compact_skip_list_node_ptr_ptr_store(&list->head, new_head);
            list->height_capacity = node_height;
            
            pas_utility_heap_deallocate(head);

            head = new_head;
        }

        for (index = node_height; index-- > list_height;)
            pas_compact_skip_list_node_ptr_store(head + index, node);

        list->height = node_height;
    }
}

PAS_END_EXTERN_C;

#endif /* PAS_SKIP_LIST_INLINES_H */

