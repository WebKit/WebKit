/*
 * Copyright (c) 2021 Apple Inc. All rights reserved.
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

#ifndef PAS_THREAD_LOCAL_CACHE_LAYOUT_ENTRY_H
#define PAS_THREAD_LOCAL_CACHE_LAYOUT_ENTRY_H

#include "pas_allocator_index.h"
#include "pas_compact_thread_local_cache_layout_node.h"

PAS_BEGIN_EXTERN_C;

typedef pas_compact_thread_local_cache_layout_node pas_thread_local_cache_layout_entry;
typedef pas_allocator_index pas_thread_local_cache_layout_key;

static inline pas_thread_local_cache_layout_entry pas_thread_local_cache_layout_entry_create_empty(void)
{
    pas_compact_thread_local_cache_layout_node result;
    pas_compact_thread_local_cache_layout_node_store(&result, NULL);
    return result;
}

static inline pas_thread_local_cache_layout_entry pas_thread_local_cache_layout_entry_create_deleted(void)
{
    pas_compact_thread_local_cache_layout_node result;
    pas_compact_thread_local_cache_layout_node_store(&result, (pas_thread_local_cache_layout_node)(uintptr_t)1);
    return result;
}

static inline bool pas_thread_local_cache_layout_entry_is_empty_or_deleted(
    pas_thread_local_cache_layout_entry entry)
{
    return (uintptr_t)pas_compact_thread_local_cache_layout_node_load(&entry) <= (uintptr_t)1;
}

static inline bool pas_thread_local_cache_layout_entry_is_empty(pas_thread_local_cache_layout_entry entry)
{
    return pas_compact_thread_local_cache_layout_node_is_null(&entry);
}

static inline bool pas_thread_local_cache_layout_entry_is_deleted(pas_thread_local_cache_layout_entry entry)
{
    return (uintptr_t)pas_compact_thread_local_cache_layout_node_load(&entry) == (uintptr_t)1;
}

static inline pas_allocator_index pas_thread_local_cache_layout_entry_get_key(
    pas_thread_local_cache_layout_entry entry)
{
    return pas_thread_local_cache_layout_node_get_allocator_index_generic(
        pas_compact_thread_local_cache_layout_node_load_non_null(&entry));
}

static inline unsigned pas_thread_local_cache_layout_key_get_hash(pas_thread_local_cache_layout_key key)
{
    return pas_hash32(key);
}

static inline bool pas_thread_local_cache_layout_key_is_equal(pas_thread_local_cache_layout_key a,
                                                              pas_thread_local_cache_layout_key b)
{
    return a == b;
}

PAS_END_EXTERN_C;

#endif /* PAS_THREAD_LOCAL_CACHE_LAYOUT_ENTRY_H */

