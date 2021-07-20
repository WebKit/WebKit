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

#ifndef PAS_THREAD_LOCAL_CACHE_LAYOUT_NODE_H
#define PAS_THREAD_LOCAL_CACHE_LAYOUT_NODE_H

#include "pas_allocator_index.h"
#include "pas_utils.h"

PAS_BEGIN_EXTERN_C;

struct pas_redundant_local_allocator_node;
struct pas_segregated_global_size_directory;
struct pas_thread_local_cache_layout_node_opaque;
typedef struct pas_redundant_local_allocator_node pas_redundant_local_allocator_node;
typedef struct pas_segregated_global_size_directory pas_segregated_global_size_directory;
typedef struct pas_thread_local_cache_layout_node_opaque* pas_thread_local_cache_layout_node;

#define PAS_IS_REDUNDANT_LOCAL_ALLOCATOR_NODE_BIT ((uintptr_t)1)

static inline pas_thread_local_cache_layout_node
pas_wrap_segregated_global_size_directory(pas_segregated_global_size_directory* directory)
{
    PAS_ASSERT(directory);
    return (pas_thread_local_cache_layout_node)directory;
}

static inline pas_thread_local_cache_layout_node
pas_wrap_redundant_local_allocator_node(pas_redundant_local_allocator_node* node)
{
    PAS_ASSERT(node);
    return (pas_thread_local_cache_layout_node)(
        (uintptr_t)node | PAS_IS_REDUNDANT_LOCAL_ALLOCATOR_NODE_BIT);
}

static inline bool
pas_is_wrapped_segregated_global_size_directory(pas_thread_local_cache_layout_node node)
{
    return !((uintptr_t)node & PAS_IS_REDUNDANT_LOCAL_ALLOCATOR_NODE_BIT);
}

static inline bool
pas_is_wrapped_redundant_local_allocator_node(pas_thread_local_cache_layout_node node)
{
    return !pas_is_wrapped_segregated_global_size_directory(node);
}

static inline pas_segregated_global_size_directory*
pas_unwrap_segregated_global_size_directory(pas_thread_local_cache_layout_node node)
{
    PAS_ASSERT(pas_is_wrapped_segregated_global_size_directory(node));
    return (pas_segregated_global_size_directory*)node;
}

static inline pas_redundant_local_allocator_node*
pas_unwrap_redundant_local_allocator_node(pas_thread_local_cache_layout_node node)
{
    PAS_ASSERT(pas_is_wrapped_redundant_local_allocator_node(node));
    return (pas_redundant_local_allocator_node*)(
        (uintptr_t)node & ~PAS_IS_REDUNDANT_LOCAL_ALLOCATOR_NODE_BIT);
}

PAS_API pas_segregated_global_size_directory*
pas_thread_local_cache_layout_node_get_directory(pas_thread_local_cache_layout_node node);

PAS_API pas_allocator_index
pas_thread_local_cache_layout_node_get_allocator_index(pas_thread_local_cache_layout_node node);

PAS_API void
pas_thread_local_cache_layout_node_set_allocator_index(pas_thread_local_cache_layout_node node,
                                                       pas_allocator_index index);

PAS_API pas_thread_local_cache_layout_node
pas_thread_local_cache_layout_node_get_next(pas_thread_local_cache_layout_node node);

PAS_API void
pas_thread_local_cache_layout_node_set_next(pas_thread_local_cache_layout_node node,
                                            pas_thread_local_cache_layout_node next_node);

PAS_END_EXTERN_C;

#endif /* PAS_THREAD_LOCAL_CACHE_LAYOUT_NODE_H */

