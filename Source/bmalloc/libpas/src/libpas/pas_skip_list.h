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

#ifndef PAS_SKIP_LIST_H
#define PAS_SKIP_LIST_H

#include "pas_compact_skip_list_node_ptr.h"
#include "pas_compact_skip_list_node_ptr_ptr.h"
#include "pas_utils.h"

PAS_BEGIN_EXTERN_C;

struct pas_skip_list;
struct pas_skip_list_config;
struct pas_skip_list_level;
struct pas_skip_list_node;
typedef struct pas_skip_list pas_skip_list;
typedef struct pas_skip_list_config pas_skip_list_config;
typedef struct pas_skip_list_level pas_skip_list_level;
typedef struct pas_skip_list_node pas_skip_list_node;

#define PAS_SKIP_LIST_MAX_HEIGHT 32

struct pas_skip_list_level {
    pas_compact_skip_list_node_ptr prev;
    pas_compact_skip_list_node_ptr next;
};

/* You must put the skip_list_node at the _end_ of your struct. */
struct pas_skip_list_node {
    uint8_t height;
    pas_skip_list_level level[1];
};

struct pas_skip_list {
    uint8_t height;
    uint8_t height_capacity;
    pas_compact_skip_list_node_ptr_ptr head;
};

typedef int (*pas_skip_list_node_compare_callback)(pas_skip_list_node* a, pas_skip_list_node* b);
typedef int (*pas_skip_list_node_key_compare_callback)(pas_skip_list_node* node, void* key);

#define PAS_SKIP_LIST_INITIALIZER ((pas_skip_list){ \
        .height = 0, \
        .height_capacity = 0, \
        .head = PAS_COMPACT_PTR_INITIALIZER \
    })

PAS_API void pas_skip_list_construct(pas_skip_list* list);

PAS_API void* pas_skip_list_node_allocate_with_height(size_t offset_of_skip_list_node,
                                                      unsigned height);

/* Allocation and deallocation requires holding the heap lock. */
PAS_API void* pas_skip_list_node_allocate(size_t offset_of_skip_list_node);
PAS_API void pas_skip_list_node_deallocate(void* ptr);

static inline pas_skip_list_node* pas_skip_list_node_prev(pas_skip_list_node* node)
{
    return pas_compact_skip_list_node_ptr_load(&node->level[0].prev);
}

static inline pas_skip_list_node* pas_skip_list_node_next(pas_skip_list_node* node)
{
    return pas_compact_skip_list_node_ptr_load(&node->level[0].next);
}

static inline pas_skip_list_node* pas_skip_list_head(pas_skip_list* list)
{
    pas_compact_skip_list_node_ptr* head;
    head = pas_compact_skip_list_node_ptr_ptr_load(&list->head);
    if (!head)
        return NULL;
    return pas_compact_skip_list_node_ptr_load(head);
}

PAS_API void pas_skip_list_remove(pas_skip_list* list,
                                  pas_skip_list_node* node);

static inline bool pas_skip_list_is_empty(pas_skip_list* list)
{
    return !list->height;
}

/* This is an O(N) operation. It's available for testing. */
PAS_API size_t pas_skip_list_size(pas_skip_list* list);

PAS_API void pas_skip_list_validate(
    pas_skip_list* list, pas_skip_list_node_compare_callback compare);

PAS_END_EXTERN_C;

#endif /* PAS_SKIP_LIST_H */

