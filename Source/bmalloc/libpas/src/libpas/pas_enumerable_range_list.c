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

#include "pas_config.h"

#if LIBPAS_ENABLED

#include "pas_enumerable_range_list.h"

#include "pas_heap_lock.h"
#include "pas_immortal_heap.h"

void pas_enumerable_range_list_append(pas_enumerable_range_list* list,
                                      pas_range range)
{
    pas_enumerable_range_list_chunk* chunk;

    pas_heap_lock_assert_held();

    if (pas_range_is_empty(range))
        return;

    chunk = pas_compact_atomic_enumerable_range_list_chunk_ptr_load(&list->head);

    if (!chunk || chunk->num_entries >= PAS_ENUMERABLE_RANGE_LIST_CHUNK_SIZE) {
        pas_enumerable_range_list_chunk* new_chunk;
        
        PAS_ASSERT_WITH_DETAIL(!chunk || chunk->num_entries == PAS_ENUMERABLE_RANGE_LIST_CHUNK_SIZE);

        new_chunk = pas_immortal_heap_allocate(sizeof(pas_enumerable_range_list_chunk),
                                               "pas_enumerable_range_list_chunk",
                                               pas_object_allocation);
        pas_compact_atomic_enumerable_range_list_chunk_ptr_store(&new_chunk->next, chunk);
        new_chunk->num_entries = 0;
        pas_compiler_fence();
        pas_compact_atomic_enumerable_range_list_chunk_ptr_store(&list->head, new_chunk);
        chunk = new_chunk;
    }

    PAS_ASSERT_WITH_DETAIL(chunk->num_entries < PAS_ENUMERABLE_RANGE_LIST_CHUNK_SIZE);

    chunk->entries[chunk->num_entries] = range;
    pas_compiler_fence();
    chunk->num_entries++;
}

bool pas_enumerable_range_list_iterate(
    pas_enumerable_range_list* list,
    pas_enumerable_range_list_iterate_callback callback,
    void* arg)
{
    pas_enumerable_range_list_chunk* chunk;

    for (chunk = pas_compact_atomic_enumerable_range_list_chunk_ptr_load(&list->head);
         chunk;
         chunk = pas_compact_atomic_enumerable_range_list_chunk_ptr_load(&chunk->next)) {
        size_t index;

        PAS_ASSERT_WITH_DETAIL(chunk->num_entries <= PAS_ENUMERABLE_RANGE_LIST_CHUNK_SIZE);

        for (index = chunk->num_entries; index--;) {
            pas_range range;

            range = chunk->entries[index];

            if (!callback(range, arg))
                return false;
        }
    }

    return true;
}

bool pas_enumerable_range_list_iterate_remote(
    pas_enumerable_range_list* remote_list,
    pas_enumerator* enumerator,
    pas_enumerable_range_list_iterate_remote_callback callback,
    void* arg)
{
    pas_enumerable_range_list* list;
    pas_enumerable_range_list_chunk* chunk;

    list = pas_enumerator_read(enumerator, remote_list, sizeof(pas_enumerable_range_list));
    if (!list)
        return false;

    for (chunk = pas_compact_atomic_enumerable_range_list_chunk_ptr_load_remote(enumerator,
                                                                                &list->head);
         chunk;
         chunk = pas_compact_atomic_enumerable_range_list_chunk_ptr_load_remote(enumerator,
                                                                                &chunk->next)) {
        size_t index;

        PAS_ASSERT_WITH_DETAIL(chunk->num_entries <= PAS_ENUMERABLE_RANGE_LIST_CHUNK_SIZE);

        for (index = chunk->num_entries; index--;) {
            pas_range range;

            range = chunk->entries[index];

            if (!callback(enumerator, range, arg))
                return false;
        }
    }

    return true;
}

#endif /* LIBPAS_ENABLED */
