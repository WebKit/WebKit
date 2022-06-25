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

#ifndef PAS_THREAD_LOCAL_CACHE_LAYOUT_NODE_H
#define PAS_THREAD_LOCAL_CACHE_LAYOUT_NODE_H

#include "pas_allocator_index.h"
#include "pas_lock.h"
#include "pas_range.h"
#include "pas_thread_local_cache_layout_node_kind.h"
#include "pas_utils.h"

PAS_BEGIN_EXTERN_C;

struct pas_redundant_local_allocator_node;
struct pas_segregated_size_directory;
struct pas_thread_local_cache;
struct pas_thread_local_cache_layout_node_opaque;
typedef struct pas_redundant_local_allocator_node pas_redundant_local_allocator_node;
typedef struct pas_segregated_size_directory pas_segregated_size_directory;
typedef struct pas_thread_local_cache pas_thread_local_cache;
typedef struct pas_thread_local_cache_layout_node_opaque* pas_thread_local_cache_layout_node;

#define PAS_THREAD_LOCAL_CACHE_LAYOUT_NODE_KIND_MASK 3lu

static inline void* pas_thread_local_cache_layout_node_get_ptr(pas_thread_local_cache_layout_node node)
{
    return (void*)((uintptr_t)node & ~PAS_THREAD_LOCAL_CACHE_LAYOUT_NODE_KIND_MASK);
}

static inline pas_thread_local_cache_layout_node_kind
pas_thread_local_cache_layout_node_get_kind(pas_thread_local_cache_layout_node node)
{
    return (pas_thread_local_cache_layout_node_kind)(
        (uintptr_t)node & PAS_THREAD_LOCAL_CACHE_LAYOUT_NODE_KIND_MASK);
}

static inline pas_thread_local_cache_layout_node
pas_thread_local_cache_layout_node_create(pas_thread_local_cache_layout_node_kind kind, void* ptr)
{
    pas_thread_local_cache_layout_node result;
    
    PAS_ASSERT(ptr);

    result = (pas_thread_local_cache_layout_node)((uintptr_t)ptr | (uintptr_t)kind);
    PAS_ASSERT(pas_thread_local_cache_layout_node_get_ptr(result) == ptr);
    PAS_ASSERT(pas_thread_local_cache_layout_node_get_kind(result) == kind);

    return result;
}

static inline pas_thread_local_cache_layout_node
pas_wrap_segregated_size_directory(pas_segregated_size_directory* directory)
{
    return pas_thread_local_cache_layout_node_create(
        pas_thread_local_cache_layout_segregated_size_directory_node_kind, directory);
}

static inline pas_thread_local_cache_layout_node
pas_wrap_redundant_local_allocator_node(pas_redundant_local_allocator_node* node)
{
    return pas_thread_local_cache_layout_node_create(
        pas_thread_local_cache_layout_redundant_local_allocator_node_kind, node);
}

static inline pas_thread_local_cache_layout_node
pas_wrap_local_view_cache_node(pas_segregated_size_directory* directory)
{
    return pas_thread_local_cache_layout_node_create(
        pas_thread_local_cache_layout_local_view_cache_node_kind, directory);
}

static inline bool
pas_is_wrapped_segregated_size_directory(pas_thread_local_cache_layout_node node)
{
    return pas_thread_local_cache_layout_node_get_kind(node)
        == pas_thread_local_cache_layout_segregated_size_directory_node_kind;
}

static inline bool
pas_is_wrapped_redundant_local_allocator_node(pas_thread_local_cache_layout_node node)
{
    return pas_thread_local_cache_layout_node_get_kind(node)
        == pas_thread_local_cache_layout_redundant_local_allocator_node_kind;
}

static inline bool
pas_is_wrapped_local_view_cache_node(pas_thread_local_cache_layout_node node)
{
    return pas_thread_local_cache_layout_node_get_kind(node)
        == pas_thread_local_cache_layout_local_view_cache_node_kind;
}

static inline pas_segregated_size_directory*
pas_unwrap_segregated_size_directory(pas_thread_local_cache_layout_node node)
{
    PAS_ASSERT(pas_is_wrapped_segregated_size_directory(node));
    return (pas_segregated_size_directory*)pas_thread_local_cache_layout_node_get_ptr(node);
}

static inline pas_redundant_local_allocator_node*
pas_unwrap_redundant_local_allocator_node(pas_thread_local_cache_layout_node node)
{
    PAS_ASSERT(pas_is_wrapped_redundant_local_allocator_node(node));
    return (pas_redundant_local_allocator_node*)pas_thread_local_cache_layout_node_get_ptr(node);
}

static inline pas_segregated_size_directory*
pas_unwrap_local_view_cache_node(pas_thread_local_cache_layout_node node)
{
    PAS_ASSERT(pas_is_wrapped_local_view_cache_node(node));
    return (pas_segregated_size_directory*)pas_thread_local_cache_layout_node_get_ptr(node);
}

PAS_API pas_segregated_size_directory*
pas_thread_local_cache_layout_node_get_directory(pas_thread_local_cache_layout_node node);

PAS_API pas_allocator_index
pas_thread_local_cache_layout_node_num_allocator_indices(pas_thread_local_cache_layout_node node);

static inline bool
pas_thread_local_cache_layout_node_represents_allocator(pas_thread_local_cache_layout_node node)
{
    return pas_is_wrapped_segregated_size_directory(node)
        || pas_is_wrapped_redundant_local_allocator_node(node);
}

static inline bool
pas_thread_local_cache_layout_node_represents_view_cache(pas_thread_local_cache_layout_node node)
{
    return pas_is_wrapped_local_view_cache_node(node);
}

PAS_API pas_allocator_index
pas_thread_local_cache_layout_node_get_allocator_index_generic(pas_thread_local_cache_layout_node node);

PAS_API pas_allocator_index
pas_thread_local_cache_layout_node_get_allocator_index_for_allocator(
    pas_thread_local_cache_layout_node node);

PAS_API pas_allocator_index
pas_thread_local_cache_layout_node_get_allocator_index_for_view_cache(
    pas_thread_local_cache_layout_node node);

PAS_API void
pas_thread_local_cache_layout_node_set_allocator_index(pas_thread_local_cache_layout_node node,
                                                       pas_allocator_index index);

PAS_API void pas_thread_local_cache_layout_node_commit_and_construct(pas_thread_local_cache_layout_node node,
                                                                     pas_thread_local_cache* cache);

PAS_API void pas_thread_local_cache_layout_node_move(pas_thread_local_cache_layout_node node,
                                                     pas_thread_local_cache* to_cache,
                                                     pas_thread_local_cache* from_cache);

PAS_API void pas_thread_local_cache_layout_node_stop(pas_thread_local_cache_layout_node node,
                                                     pas_thread_local_cache* cache,
                                                     pas_lock_lock_mode page_lock_mode,
                                                     pas_lock_hold_mode heap_lock_hold_mode);

PAS_API bool pas_thread_local_cache_layout_node_is_committed(pas_thread_local_cache_layout_node node,
                                                             pas_thread_local_cache* cache);
PAS_API void pas_thread_local_cache_layout_node_ensure_committed(pas_thread_local_cache_layout_node node,
                                                                 pas_thread_local_cache* cache);
PAS_API void pas_thread_local_cache_layout_node_prepare_to_decommit(pas_thread_local_cache_layout_node node,
                                                                    pas_thread_local_cache* cache,
                                                                    pas_range decommit_range);

PAS_END_EXTERN_C;

#endif /* PAS_THREAD_LOCAL_CACHE_LAYOUT_NODE_H */

