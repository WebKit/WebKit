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

#ifndef PAS_SEGREGATED_HEAP_INLINES_H
#define PAS_SEGREGATED_HEAP_INLINES_H

#include "pas_compact_expendable_memory.h"
#include "pas_large_expendable_memory.h"
#include "pas_segregated_heap.h"
#include "pas_segregated_size_directory.h"

PAS_BEGIN_EXTERN_C;

PAS_API pas_segregated_size_directory* pas_segregated_heap_size_directory_for_index_slow(
    pas_segregated_heap* heap,
    size_t index,
    unsigned* cached_index,
    pas_heap_config* config);

static PAS_ALWAYS_INLINE pas_segregated_size_directory*
pas_segregated_heap_size_directory_for_index(
    pas_segregated_heap* heap,
    size_t index,
    unsigned* cached_index,
    pas_heap_config* config)
{
    pas_compact_atomic_segregated_size_directory_ptr* index_to_size_directory;
    pas_segregated_size_directory* result;
    
    if (index >= (size_t)heap->small_index_upper_bound)
        goto slow;
    
    index_to_size_directory = heap->index_to_small_size_directory;
    if (!index_to_size_directory) {
        /* Code that holds no locks may see this since we have no ordering between when the
           upper_bound is set and when this is set. */
        goto slow;
    }
    
    result = pas_compact_atomic_segregated_size_directory_ptr_load(index_to_size_directory + index);
    if (result)
        return result;

    /* It's possible for basic_size_directory to be set, the size directory lookup table to be created, and
       for the entry corresponding to the basic_size_directory to be unset in the lookup table! */

slow:
    return pas_segregated_heap_size_directory_for_index_slow(heap, index, cached_index, config);
}

static PAS_ALWAYS_INLINE pas_segregated_size_directory*
pas_segregated_heap_size_directory_for_size(
    pas_segregated_heap* heap,
    size_t size,
    pas_heap_config config,
    unsigned* cached_index)
{
    size_t index;
    
    index = pas_segregated_heap_index_for_size(size, config);
    
    return pas_segregated_heap_size_directory_for_index(heap, index, cached_index, config.config_ptr);
}

static PAS_ALWAYS_INLINE bool pas_segregated_heap_touch_lookup_tables(
    pas_segregated_heap* heap, pas_expendable_memory_touch_kind kind)
{
    pas_segregated_heap_rare_data* data;
    bool result;

    result = false;
    
    if (!heap->runtime_config->statically_allocated) {
        unsigned small_index_upper_bound;

        small_index_upper_bound = heap->small_index_upper_bound;
        if (small_index_upper_bound) {
            pas_allocator_index* index_to_small_allocator_index;
            pas_compact_atomic_segregated_size_directory_ptr* index_to_small_size_directory;

            index_to_small_allocator_index = heap->index_to_small_allocator_index;
            if (index_to_small_allocator_index) {
                result |= pas_large_expendable_memory_touch(
                    index_to_small_allocator_index,
                    sizeof(pas_allocator_index) * small_index_upper_bound,
                    kind);
            }

            index_to_small_size_directory = heap->index_to_small_size_directory;
            if (index_to_small_size_directory) {
                result |= pas_large_expendable_memory_touch(
                    index_to_small_size_directory,
                    sizeof(pas_compact_atomic_segregated_size_directory_ptr) * small_index_upper_bound,
                    kind);
            }
        }
    }

    data = pas_segregated_heap_rare_data_ptr_load(&heap->rare_data);
    if (data) {
        unsigned num_medium_directories;
        pas_segregated_heap_medium_directory_tuple* tuples;

        num_medium_directories = data->num_medium_directories;
        tuples = pas_segregated_heap_medium_directory_tuple_ptr_load(
            &data[pas_depend(num_medium_directories)].medium_directories);

        if (num_medium_directories) {
            PAS_ASSERT(tuples);
            result |= pas_compact_expendable_memory_touch(
                tuples,
                num_medium_directories * sizeof(pas_segregated_heap_medium_directory_tuple),
                kind);
        }
    }

    return result;
}

PAS_END_EXTERN_C;

#endif /* PAS_SEGREGATED_HEAP_INLINES_H */
