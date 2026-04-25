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

#ifndef PAS_FAST_LARGE_FREE_HEAP_H
#define PAS_FAST_LARGE_FREE_HEAP_H

#include "pas_config.h"

#include "pas_cartesian_tree.h"
#include "pas_large_free.h"
#include "pas_large_free_visitor.h"

PAS_BEGIN_EXTERN_C;

struct pas_fast_large_free_heap;
struct pas_fast_large_free_heap_node;
typedef struct pas_fast_large_free_heap pas_fast_large_free_heap;
typedef struct pas_fast_large_free_heap_node pas_fast_large_free_heap_node;

typedef uintptr_t pas_fast_large_free_heap_end_hashtable_key;
typedef pas_fast_large_free_heap_node* pas_fast_large_free_heap_end_hashtable_entry;

/* This uses a Cartesian tree with X = address and Y = size. */

struct pas_fast_large_free_heap_node {
    pas_cartesian_tree_node tree_node;
    pas_large_free free;
};

struct pas_fast_large_free_heap {
    pas_cartesian_tree tree;
    size_t num_mapped_bytes;
};

#define PAS_FAST_LARGE_FREE_HEAP_INITIALIZER { \
        .tree = PAS_CARTESIAN_TREE_INITIALIZER, \
        .num_mapped_bytes = 0 \
    }

PAS_API void pas_fast_large_free_heap_construct(pas_fast_large_free_heap* heap);

PAS_API pas_allocation_result
pas_fast_large_free_heap_try_allocate(pas_fast_large_free_heap* heap,
                                      size_t size,
                                      pas_alignment alignment,
                                      pas_large_free_heap_config* config);

PAS_API void pas_fast_large_free_heap_deallocate(pas_fast_large_free_heap* heap,
                                                 uintptr_t begin,
                                                 uintptr_t end,
                                                 pas_zero_mode zero_mode,
                                                 pas_large_free_heap_config* config);

PAS_API void pas_fast_large_free_heap_for_each_free(pas_fast_large_free_heap* heap,
                                                    pas_large_free_visitor visitor,
                                                    void* arg);

PAS_API size_t pas_fast_large_free_heap_get_num_free_bytes(pas_fast_large_free_heap* heap);

static inline size_t pas_fast_large_free_heap_get_num_mapped_bytes(
    pas_fast_large_free_heap* heap)
{
    return heap->num_mapped_bytes;
}

PAS_API void pas_fast_large_free_heap_validate(pas_fast_large_free_heap* heap);
PAS_API void pas_fast_large_free_heap_dump_to_printf(pas_fast_large_free_heap* heap);

PAS_END_EXTERN_C;

#endif /* PAS_FAST_LARGE_FREE_HEAP_H */

