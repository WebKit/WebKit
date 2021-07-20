/*
 * Copyright (c) 2018-2020 Apple Inc. All rights reserved.
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

#include "pas_segregated_heap.h"
#include "pas_segregated_global_size_directory.h"

PAS_BEGIN_EXTERN_C;

static PAS_ALWAYS_INLINE pas_segregated_global_size_directory*
pas_segregated_heap_size_directory_for_index(
    pas_segregated_heap* heap,
    size_t index,
    unsigned* cached_index)
{
    pas_compact_atomic_segregated_global_size_directory_ptr* index_to_size_directory;
    
    if (index == (cached_index ? *cached_index : 1)) {
        pas_segregated_global_size_directory* result;
        result = pas_compact_atomic_segregated_global_size_directory_ptr_load(
            &heap->basic_size_directory_and_head);
        if (result && result->base.is_basic_size_directory)
            return result;
    }
    
    if (index >= (size_t)heap->small_index_upper_bound) {
        return pas_segregated_heap_medium_size_directory_for_index(
            heap, index,
            pas_segregated_heap_medium_size_directory_search_within_size_class_progression,
            pas_lock_is_held);
    }
    
    index_to_size_directory = heap->index_to_small_size_directory;
    if (!index_to_size_directory) {
        /* Code that holds no locks may see this since we have no ordering between when the
           upper_bound is set and when this is set. */
        return NULL;
    }
    
    return pas_compact_atomic_segregated_global_size_directory_ptr_load(index_to_size_directory + index);
}

static PAS_ALWAYS_INLINE pas_segregated_global_size_directory*
pas_segregated_heap_size_directory_for_count(
    pas_segregated_heap* heap,
    size_t count,
    pas_heap_config config,
    unsigned* cached_index)
{
    size_t index;
    
    index = pas_segregated_heap_index_for_count(heap, count, config);
    
    return pas_segregated_heap_size_directory_for_index(heap, index, cached_index);
}

PAS_END_EXTERN_C;

#endif /* PAS_SEGREGATED_HEAP_INLINES_H */
