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

#ifndef PAS_GENERIC_LARGE_FREE_HEAP_H
#define PAS_GENERIC_LARGE_FREE_HEAP_H

#include "pas_allocation_result.h"
#include "pas_heap_lock.h"
#include "pas_large_free_heap_config.h"
#include "pas_large_free_inlines.h"
#include "pas_page_malloc.h"
#include <stdio.h>

PAS_BEGIN_EXTERN_C;

#define PAS_GENERIC_LARGE_FREE_HEAP_VERBOSE 0

/* These structs are opaque. */
struct pas_generic_large_free_cursor;
struct pas_generic_large_free_heap;

/* This one defines the API. */
struct pas_generic_large_free_heap_config;

typedef struct pas_generic_large_free_cursor pas_generic_large_free_cursor;
typedef struct pas_generic_large_free_heap pas_generic_large_free_heap;
typedef struct pas_generic_large_free_heap_config pas_generic_large_free_heap_config;

struct pas_generic_large_free_heap_config {
    void (*find_first)(
        pas_generic_large_free_heap* heap,
        size_t min_size, /* Hind that we're only interested in free objects at least this big,
                            but there's no guarantee that the find_first implementation will
                            care. This hint is an optimization for Cartesian trees. */
        bool (*test_candidate)(
            pas_generic_large_free_cursor* cursor,
            pas_large_free candidate,
            void* arg),
        void* arg);
    
    pas_generic_large_free_cursor* (*find_by_end)(
        pas_generic_large_free_heap* heap,
        uintptr_t end);
    
    pas_large_free (*read_cursor)(
        pas_generic_large_free_heap* heap,
        pas_generic_large_free_cursor* cursor);
    
    void (*write_cursor)(
        pas_generic_large_free_heap* heap,
        pas_generic_large_free_cursor* cursor,
        pas_large_free value);
    
    void (*merge)(
        pas_generic_large_free_heap* heap,
        pas_large_free new_free,
        pas_large_free_heap_config* config);
    
    void (*remove)(
        pas_generic_large_free_heap* heap,
        pas_generic_large_free_cursor* cursor);
    
    void (*append)(
        pas_generic_large_free_heap* heap,
        pas_large_free value);
    
    void (*commit)(
        pas_generic_large_free_heap* heap,
        pas_generic_large_free_cursor* cursor,
        pas_large_free allocation);
    
    void (*dump)(
        pas_generic_large_free_heap* heap);
    
    void (*add_mapped_bytes)(
        pas_generic_large_free_heap* heap,
        size_t num_bytes);
};

static PAS_ALWAYS_INLINE void
pas_generic_large_free_heap_merge_physical(
    pas_generic_large_free_heap* heap,
    uintptr_t begin,
    uintptr_t end,
    uintptr_t offset_in_type,
    pas_zero_mode zero_mode,
    pas_large_free_heap_config* config,
    pas_generic_large_free_heap_config* generic_heap_config)
{
    pas_large_free new_free;
    
    PAS_ASSERT(end >= begin);

    if (begin == end)
        return;
    
    PAS_ASSERT(begin);
    pas_zero_mode_validate(zero_mode);
    
    new_free = pas_large_free_create_empty();
    new_free.begin = begin;
    new_free.end = end;
    new_free.offset_in_type = offset_in_type;
    new_free.zero_mode = zero_mode;
    generic_heap_config->merge(heap, new_free, config);
}

typedef struct {
    pas_generic_large_free_heap* heap;
    size_t size;
    pas_alignment alignment;
    pas_large_free_heap_config* config;
    pas_generic_large_free_cursor* best_cursor;
    pas_large_allocation_result best;
} pas_generic_large_free_heap_try_allocate_test_allocation_candidate_data;

static PAS_ALWAYS_INLINE bool
pas_generic_large_free_heap_try_allocate_test_allocation_candidate(
    pas_generic_large_free_cursor* cursor,
    pas_large_free candidate,
    void* arg)
{
    pas_generic_large_free_heap_try_allocate_test_allocation_candidate_data* data;
    pas_large_allocation_result candidate_result;
    
    data = arg;
    
    if (data->best_cursor && candidate.begin > data->best.allocation.begin)
        return false;
    
    if (data->size > pas_large_free_size(candidate))
        return false;
    
    candidate_result = pas_large_free_allocate(
        candidate, data->size, data->alignment, data->config);
    
    if (pas_large_free_is_empty(candidate_result.allocation))
        return false;
    
    PAS_ASSERT(candidate_result.allocation.begin);
    
    if (!data->best_cursor || candidate_result.allocation.begin < data->best.allocation.begin) {
        data->best_cursor = cursor;
        data->best = candidate_result;
        return true;
    }
    
    return false;
}

static PAS_ALWAYS_INLINE pas_allocation_result
pas_generic_large_free_heap_try_allocate(
    pas_generic_large_free_heap* heap,
    size_t size,
    pas_alignment alignment,
    pas_large_free_heap_config* config,
    pas_generic_large_free_heap_config* generic_heap_config)
{
    static const bool verbose = false;
    
    pas_generic_large_free_cursor* best_cursor;
    pas_large_allocation_result best;
    pas_allocation_result result;
    pas_generic_large_free_heap_try_allocate_test_allocation_candidate_data
        test_allocation_candidate_data;
    
    if (!size)
        return pas_allocation_result_create_success_with_zero_mode(0, pas_zero_mode_is_all_zero);
    
    if (PAS_GENERIC_LARGE_FREE_HEAP_VERBOSE >= 2)
        printf("Allocating size = %zu\n", size);
    
    pas_alignment_validate(alignment);
    pas_heap_lock_assert_held();
    result = pas_allocation_result_create_failure();
    
    test_allocation_candidate_data.heap = heap;
    test_allocation_candidate_data.size = size;
    test_allocation_candidate_data.alignment = alignment;
    test_allocation_candidate_data.config = config;
    test_allocation_candidate_data.best_cursor = NULL;
    test_allocation_candidate_data.best = pas_large_allocation_result_create_empty();
    
    generic_heap_config->find_first(
        heap,
        size,
        pas_generic_large_free_heap_try_allocate_test_allocation_candidate,
        &test_allocation_candidate_data);
    
    best_cursor = test_allocation_candidate_data.best_cursor;
    best = test_allocation_candidate_data.best;
    
    if (!best_cursor) {
        pas_aligned_allocation_result page_allocation;
        pas_large_free candidate;
        pas_large_allocation_result candidate_result;
        pas_large_free merged;
        pas_generic_large_free_cursor* candidate_cursor;
        bool found_candidate;
        
        if (PAS_GENERIC_LARGE_FREE_HEAP_VERBOSE >= 2) {
            printf("Could not allocate %zu bytes with alignment %zu/%zu. Free list status:\n",
                   size, alignment.alignment, alignment.alignment_begin);
            generic_heap_config->dump(heap);
        }
        
        page_allocation = config->aligned_allocator(size, alignment, config->aligned_allocator_arg);
        if (!page_allocation.result) {
            if (PAS_GENERIC_LARGE_FREE_HEAP_VERBOSE >= 2)
                printf("Returning failure.\n");
            return pas_allocation_result_create_failure();
        }
        
        PAS_ASSERT(page_allocation.result_size == size);
        pas_zero_mode_validate(page_allocation.zero_mode);
        
        /* Try to see if any existing free object can be merged with what we just allocated. We are
           only interested in merging on the left side of the physical allocation, including its
           padding, if that allows for an allocation at a lower address than the page allocation.
        
           Note that this does occasionally mean missed opportunities when allocating aligned. It's
           possible that the underlying allocator would have given us an address that wasn't aligned
           that would have been adjacent to something that contained an aligned address. For now, we
           assume that this is too unlikely to matter. */
        
        if (PAS_GENERIC_LARGE_FREE_HEAP_VERBOSE >= 2) {
            printf("Just before candidate search after allocating from the source...\n");
            generic_heap_config->dump(heap);
        }
        
        found_candidate = false;

        candidate_cursor = generic_heap_config->find_by_end(
            heap, (uintptr_t)page_allocation.left_padding);

        candidate_result = pas_large_allocation_result_create_empty(); /* Tell the compiler to take
                                                                          a deep breath. */
        
        if (candidate_cursor) {
            pas_large_free new_free;

            candidate = generic_heap_config->read_cursor(heap, candidate_cursor);
            
            PAS_ASSERT(candidate.end == (uintptr_t)page_allocation.left_padding);
            
            new_free = pas_large_free_create_empty();
            new_free.begin = (uintptr_t)page_allocation.left_padding;
            new_free.end =
                (uintptr_t)page_allocation.right_padding + page_allocation.right_padding_size;
            new_free.offset_in_type =
                pas_large_free_offset_in_type_at_end(candidate, config);
            new_free.zero_mode = page_allocation.zero_mode;
            
            merged = pas_large_free_create_merged(candidate, new_free, config);
            if (verbose) {
                pas_log("merged is %p...%p (%s)\n",
                        (void*)merged.begin, (void*)merged.end,
                        merged.zero_mode ? "zero" : "non-zero");
            }
            if (!pas_large_free_is_empty(merged)) {
                candidate_result = pas_large_free_allocate(merged,
                                                           size,
                                                           alignment,
                                                           config);
                
                if (!pas_large_free_is_empty(candidate_result.allocation)
                    && candidate_result.allocation.begin < (uintptr_t)page_allocation.result)
                    found_candidate = true;
            }
        }
        
        if (!found_candidate) {
            result = pas_aligned_allocation_result_as_allocation_result(page_allocation);
            PAS_ASSERT(result.did_succeed);
            PAS_ASSERT(result.begin);
            
            /* result + size could be smaller than right_padding if the system page size is larger
               than our alignment requirement. */
            PAS_ASSERT(result.begin + size <= (uintptr_t)page_allocation.right_padding);
        
            if (config->deallocator) {
                generic_heap_config->add_mapped_bytes(heap, page_allocation.result_size);
                
                config->deallocator(page_allocation.left_padding,
                                    page_allocation.left_padding_size,
                                    config->deallocator_arg);
                config->deallocator(page_allocation.right_padding,
                                    page_allocation.right_padding_size,
                                    config->deallocator_arg);
            } else {
                generic_heap_config->add_mapped_bytes(
                    heap,
                    page_allocation.result_size +
                    page_allocation.left_padding_size +
                    page_allocation.right_padding_size);
                
                /* It would be ridiculous to have to merge left padding into
                   a heap with type_size != 1. We could come up with some
                   crazy algorithm to do a best effort or whatever, but the
                   fact is that because other reasons, the aligned_allocator
                   for typed heaps never returns any left padding. So, we just
                   assert this rather than have to worry about supporting
                   something that is hard and nonsensical. */
                if (config->type_size != 1)
                    PAS_ASSERT(!page_allocation.left_padding_size);
                else {
                    if (verbose)
                        pas_log("Merging left padding in no-candidate case.\n");
                    pas_generic_large_free_heap_merge_physical(
                        heap,
                        (uintptr_t)page_allocation.left_padding,
                        (uintptr_t)page_allocation.left_padding + page_allocation.left_padding_size,
                        0,
                        page_allocation.zero_mode,
                        config, generic_heap_config);
                }

                if (verbose)
                    pas_log("Merging right padding in no-candidate case.\n");
                pas_generic_large_free_heap_merge_physical(
                    heap,
                    result.begin + size,
                    (uintptr_t)page_allocation.right_padding + page_allocation.right_padding_size,
                    size % config->type_size,
                    page_allocation.zero_mode,
                    config, generic_heap_config);
            }
        } else {
            PAS_ASSERT(candidate_result.allocation.begin < candidate.end);
            
            result = pas_large_allocation_result_as_allocation_result(candidate_result);
            PAS_ASSERT(result.did_succeed);
            PAS_ASSERT(result.begin);
            
            generic_heap_config->commit(heap, candidate_cursor, candidate_result.allocation);
            
            if (pas_large_free_is_empty(candidate_result.left_free))
                generic_heap_config->remove(heap, candidate_cursor);
            else {
                generic_heap_config->write_cursor(
                    heap, candidate_cursor, candidate_result.left_free);
            }
            
            if (config->deallocator) {
                generic_heap_config->add_mapped_bytes(
                    heap, candidate_result.allocation.end - candidate.end);
                
                config->deallocator(
                    (void*)candidate_result.right_free.begin, 
                    candidate_result.right_free.end - candidate_result.right_free.begin,
                    config->deallocator_arg);
            } else {
                generic_heap_config->add_mapped_bytes(
                    heap, candidate_result.right_free.end - candidate.end);

                if (verbose)
                    pas_log("Merging right padding in yes-candidate case.\n");
                pas_generic_large_free_heap_merge_physical(
                    heap,
                    candidate_result.right_free.begin,
                    candidate_result.right_free.end,
                    candidate_result.right_free.offset_in_type,
                    candidate_result.right_free.zero_mode,
                    config, generic_heap_config);
            }
        }
    } else {
        static const size_t max_num_additions = 2;
        pas_large_free additions[max_num_additions];
        size_t num_additions = 0;
        
        result = pas_large_allocation_result_as_allocation_result(best);
        PAS_ASSERT(result.did_succeed);
        PAS_ASSERT(result.begin);
        
        PAS_ASSERT(size == pas_large_free_size(best.allocation));
        
        generic_heap_config->commit(heap, best_cursor, best.allocation);
        
        if (!pas_large_free_is_empty(best.left_free))
            additions[num_additions++] = best.left_free;
        if (!pas_large_free_is_empty(best.right_free))
            additions[num_additions++] = best.right_free;
        
        PAS_ASSERT(num_additions <= max_num_additions);
        
        if (!num_additions)
            generic_heap_config->remove(heap, best_cursor);
        else {
            PAS_ASSERT(num_additions == 1 || num_additions == 2);
            
            generic_heap_config->write_cursor(heap, best_cursor, additions[0]);
            
            if (num_additions == 2)
                generic_heap_config->append(heap, additions[1]);
        }
    }
    
    PAS_ASSERT(pas_alignment_is_ptr_aligned(alignment, result.begin));
    PAS_ASSERT(result.begin);
    PAS_ASSERT(result.did_succeed);
    
    if (PAS_GENERIC_LARGE_FREE_HEAP_VERBOSE >= 2) {
        printf("After allocating %p for size %zu:\n", (void*)result.begin, size);
        generic_heap_config->dump(heap);
    }

    return result;
}

PAS_END_EXTERN_C;

#endif /* PAS_GENERIC_LARGE_FREE_HEAP_H */

