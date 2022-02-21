/*
 * Copyright (c) 2018-2021 Apple Inc. All rights reserved.
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

#ifndef PAS_THREAD_LOCAL_CACHE_LAYOUT_H
#define PAS_THREAD_LOCAL_CACHE_LAYOUT_H

#include "pas_allocator_index.h"
#include "pas_compact_atomic_thread_local_cache_layout_node.h"
#include "pas_config.h"
#include "pas_hashtable.h"
#include "pas_lock.h"
#include "pas_thread_local_cache_layout_entry.h"
#include "pas_thread_local_cache_layout_node.h"
#include "pas_utils.h"

PAS_BEGIN_EXTERN_C;

#define PAS_THREAD_LOCAL_CACHE_LAYOUT_SEGMENT_SIZE 257
struct pas_thread_local_cache_layout_segment {
    pas_compact_atomic_thread_local_cache_layout_node nodes[PAS_THREAD_LOCAL_CACHE_LAYOUT_SEGMENT_SIZE];
    pas_compact_atomic_thread_local_cache_layout_node sentinel;
    struct pas_thread_local_cache_layout_segment* next;
};
typedef struct pas_thread_local_cache_layout_segment pas_thread_local_cache_layout_segment;

PAS_API extern pas_thread_local_cache_layout_segment* pas_thread_local_cache_layout_first_segment;

PAS_CREATE_HASHTABLE(pas_thread_local_cache_layout_hashtable,
                     pas_thread_local_cache_layout_entry,
                     pas_thread_local_cache_layout_key);

/* Lock used internally for accessing the hashtable_instance. You don't have to use this lock unless you
   access the hashtable_instance directly. */
PAS_DECLARE_LOCK(pas_thread_local_cache_layout_hashtable);

PAS_API extern pas_thread_local_cache_layout_hashtable pas_thread_local_cache_layout_hashtable_instance;

/* Clients can use this to force the next call to add to go to this index. */
PAS_API extern pas_allocator_index pas_thread_local_cache_layout_next_allocator_index;

PAS_API pas_allocator_index pas_thread_local_cache_layout_add_node(
    pas_thread_local_cache_layout_node node);

PAS_API pas_allocator_index pas_thread_local_cache_layout_add(
    pas_segregated_size_directory* directory);

PAS_API pas_allocator_index pas_thread_local_cache_layout_duplicate(
    pas_segregated_size_directory* directory);

PAS_API pas_allocator_index pas_thread_local_cache_layout_add_view_cache(
    pas_segregated_size_directory* directory);

/* You don't need to hold any locks to use this because this uses its own lock behind the scenes. */
PAS_API pas_thread_local_cache_layout_node pas_thread_local_cache_layout_get_node_for_index(
    pas_allocator_index index);

PAS_API pas_thread_local_cache_layout_node pas_thread_local_cache_layout_get_last_node(void);

static PAS_ALWAYS_INLINE pas_thread_local_cache_layout_node pas_thread_local_cache_layout_segment_get_node(pas_thread_local_cache_layout_segment* segment, uintptr_t index)
{
    if (!segment)
        return NULL;
    return pas_compact_atomic_thread_local_cache_layout_node_load(&segment->nodes[index]);
}

static PAS_ALWAYS_INLINE pas_thread_local_cache_layout_node pas_thread_local_cache_layout_segment_next_node(pas_thread_local_cache_layout_segment** segment, uintptr_t* index)
{
    PAS_TESTING_ASSERT(segment);
    PAS_TESTING_ASSERT(*segment);
    uintptr_t next_index = *index + 1;
    pas_thread_local_cache_layout_node next = pas_thread_local_cache_layout_segment_get_node(*segment, next_index);
    if (next) {
        *index = next_index;
        return next;
    }
    *segment = (*segment)->next;
    *index = 0;
    return pas_thread_local_cache_layout_segment_get_node(*segment, 0);
}

#define PAS_THREAD_LOCAL_CACHE_LAYOUT_EACH_ALLOCATOR(node) \
    uintptr_t internal_node_index = 0, internal_segment = (uintptr_t)pas_thread_local_cache_layout_first_segment, internal_node = (uintptr_t)pas_thread_local_cache_layout_segment_get_node((pas_thread_local_cache_layout_segment*)internal_segment, internal_node_index); \
    (node = ((pas_thread_local_cache_layout_node)internal_node)); \
    internal_node = (uintptr_t)pas_thread_local_cache_layout_segment_next_node((pas_thread_local_cache_layout_segment**)&internal_segment, &internal_node_index)

#define PAS_THREAD_LOCAL_CACHE_LAYOUT_EACH_ALLOCATOR_WITH_SEGMENT_AND_INDEX(node, segment, index) \
    node = pas_thread_local_cache_layout_segment_get_node(segment, index); \
    node; \
    node = pas_thread_local_cache_layout_segment_next_node(&segment, &index)

PAS_END_EXTERN_C;

#endif /* PAS_THREAD_LOCAL_CACHE_LAYOUT_H */

