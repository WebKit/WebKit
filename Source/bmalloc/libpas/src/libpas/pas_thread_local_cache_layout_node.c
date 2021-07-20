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

#include "pas_thread_local_cache_layout_node.h"

#include "pas_redundant_local_allocator_node.h"
#include "pas_segregated_global_size_directory.h"

pas_segregated_global_size_directory*
pas_thread_local_cache_layout_node_get_directory(pas_thread_local_cache_layout_node node)
{
    if (pas_is_wrapped_segregated_global_size_directory(node))
        return pas_unwrap_segregated_global_size_directory(node);

    return pas_compact_segregated_global_size_directory_ptr_load(
        &pas_unwrap_redundant_local_allocator_node(node)->directory);
}

static pas_allocator_index*
allocator_index_ptr(pas_thread_local_cache_layout_node node)
{
    if (pas_is_wrapped_segregated_global_size_directory(node)) {
        return &pas_segregated_global_size_directory_data_ptr_load(
            &pas_unwrap_segregated_global_size_directory(node)->data)->allocator_index;
    }

    return &pas_unwrap_redundant_local_allocator_node(node)->allocator_index;
}

pas_allocator_index
pas_thread_local_cache_layout_node_get_allocator_index(pas_thread_local_cache_layout_node node)
{
    return *allocator_index_ptr(node);
}

void
pas_thread_local_cache_layout_node_set_allocator_index(pas_thread_local_cache_layout_node node,
                                                       pas_allocator_index index)
{
    *allocator_index_ptr(node) = index;
}

static pas_compact_atomic_thread_local_cache_layout_node*
next_ptr(pas_thread_local_cache_layout_node node)
{
    if (pas_is_wrapped_segregated_global_size_directory(node)) {
        return &pas_segregated_global_size_directory_data_ptr_load(
            &pas_unwrap_segregated_global_size_directory(node)->data)->next_for_layout;
    }
    
    return &pas_unwrap_redundant_local_allocator_node(node)->next;
}

pas_thread_local_cache_layout_node
pas_thread_local_cache_layout_node_get_next(pas_thread_local_cache_layout_node node)
{
    return pas_compact_atomic_thread_local_cache_layout_node_load(next_ptr(node));
}

void
pas_thread_local_cache_layout_node_set_next(pas_thread_local_cache_layout_node node,
                                            pas_thread_local_cache_layout_node next_node)
{
    pas_compact_atomic_thread_local_cache_layout_node_store(next_ptr(node), next_node);
}

#endif /* LIBPAS_ENABLED */
