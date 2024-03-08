/*
 * Copyright (c) 2019-2021 Apple Inc. All rights reserved.
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

#ifndef PAS_TRY_ALLOCATE_PRIMITIVE_H
#define PAS_TRY_ALLOCATE_PRIMITIVE_H

#include "pas_allocator_counts.h"
#include "pas_heap.h"
#include "pas_heap_config.h"
#include "pas_heap_lock.h"
#include "pas_allocation_result.h"
#include "pas_local_allocator_inlines.h"
#include "pas_physical_memory_transaction.h"
#include "pas_primitive_heap_ref.h"

PAS_BEGIN_EXTERN_C;

/* This header provides entrypoints for both primitive and flex heaps. You select the mode by passing
   either the primitive heap_runtuime_config or the flex heap_runtime_config.
   
   Primitive heaps are for objects that should be kept separate from the main heap but that otherwise
   don't have any interesting type. So, they are like intrinsic heaps, but the tuning has to be such
   that we are efficient for lots of small heaps. But, there's no restriction on whether the data in
   the heap is reused in any particular way -- we run this with a type size of 1.

   These have all the functionality of intrinsic heaps but are tuned to allow for there to be a
   variable number of heaps. While libpas knows about every intrinsic heap that a heap_config may
   have, libpas allows primitive heaps to be created by libpas clients.

   Flex heaps are for flexible array members. Currently we tune them almost exactly like primitive
   heaps except that bitfit is disabled for flex.

   FIXME: Flex heaps should provide stronger isolation in the large heap. That can be achieved while
   still keeping these entrypoints. It's just a different large heap reuse policy. */

static PAS_ALWAYS_INLINE pas_allocation_result
pas_try_allocate_primitive_impl_casual_case(pas_primitive_heap_ref* heap_ref,
                                            size_t size,
                                            size_t alignment,
                                            pas_allocation_mode allocation_mode,
                                            pas_heap_config config,
                                            pas_heap_runtime_config* runtime_config,
                                            pas_try_allocate_common try_allocate_common)
{
    static const bool verbose = false;
    
    size_t aligned_size;
    unsigned allocator_index;
    pas_local_allocator_result allocator;
    size_t index;
    
    if (!pas_is_power_of_2(alignment))
        return pas_allocation_result_create_failure();
    
    aligned_size = pas_try_allocate_compute_aligned_size(size, alignment);
    
    index = pas_segregated_heap_index_for_size(aligned_size, config);
    
    /* Have a fast path for when you allocate some size all the time or
       most of the time. This saves both time and space. The space savings are
       the more interesting part, since */

    if (verbose)
        pas_log("%p: getting allocator index.\n", (void*)pthread_self());
    
    if (index == heap_ref->cached_index) {
        if (verbose)
            pas_log("Found cached index.\n");
        allocator_index = heap_ref->base.allocator_index;
    } else {
        pas_heap* heap;
        pas_segregated_heap* segregated_heap;
        
        if (verbose)
            pas_log("Using full lookup.\n");

        heap = pas_ensure_heap(&heap_ref->base, pas_primitive_heap_ref_kind, config.config_ptr,
                               runtime_config);
        segregated_heap = &heap->segregated_heap;
        
        allocator_index = pas_segregated_heap_allocator_index_for_index(segregated_heap,
                                                                        index,
                                                                        pas_lock_is_not_held);
    }
    
    if (verbose)
        pas_log("allocator_index = %u\n", allocator_index);

    /* Cool fact: there could be a race where the allocator_index we get is 0 (i.e. unselected) even though
       at this point, the directory has already initialized the allocator_index. That's because between when
       we got the allocator_index above and now, some other thread could have already initialized the
       directory's allocator_index. */
    
    allocator = pas_thread_local_cache_get_local_allocator_if_can_set_cache_for_possibly_uninitialized_index(
        allocator_index, config.config_ptr);

    if (verbose && !allocator.did_succeed)
        pas_log("%p: Failed to quickly get the allocator, allocator_index = %u.\n", (void*)pthread_self(), allocator_index);
    
    /* This should be specialized out in the non-alignment case because of ALWAYS_INLINE and
       alignment being the constant 1. */
    if (alignment != 1 && allocator.did_succeed
        && alignment > pas_local_allocator_alignment((pas_local_allocator*)allocator.allocator))
        allocator.did_succeed = false;

    return try_allocate_common(&heap_ref->base, aligned_size, alignment, allocation_mode, allocator);
}

static PAS_ALWAYS_INLINE pas_allocation_result
pas_try_allocate_primitive_impl_inline_only(
    pas_primitive_heap_ref* heap_ref,
    size_t size,
    size_t alignment,
    pas_allocation_mode allocation_mode,
    pas_heap_config config,
    pas_try_allocate_common_fast_inline_only try_allocate_common_fast_inline_only)
{
    static const bool verbose = false;
    
    size_t aligned_size;
    unsigned allocator_index;
    pas_local_allocator_result allocator;
    size_t index;
    pas_thread_local_cache* cache;
    
    if (!pas_is_power_of_2(alignment))
        return pas_allocation_result_create_failure();
    
    aligned_size = pas_try_allocate_compute_aligned_size(size, alignment);
    
    index = pas_segregated_heap_index_for_size(aligned_size, config);
    
    /* Have a fast path for when you allocate some size all the time or
       most of the time. This saves both time and space. The space savings are
       the more interesting part, since */
    
    if (index == heap_ref->cached_index) {
        if (verbose)
            pas_log("Found cached index.\n");
        allocator_index = heap_ref->base.allocator_index;
    } else {
        pas_heap* heap;
        pas_segregated_heap* segregated_heap;
        
        if (verbose)
            pas_log("Using full lookup.\n");

        heap = heap_ref->base.heap;
        if (!heap)
            return pas_allocation_result_create_failure();
        segregated_heap = &heap->segregated_heap;
        
        allocator_index = pas_segregated_heap_allocator_index_for_index_inline_only(segregated_heap,
                                                                                    index);
    }
    
    if (verbose)
        pas_log("allocator_index = %u\n", allocator_index);

    cache = pas_thread_local_cache_try_get();
    if (PAS_UNLIKELY(!cache))
        return pas_allocation_result_create_failure();
    
    allocator =
        pas_thread_local_cache_try_get_local_allocator_or_unselected_for_uninitialized_index(
            cache, allocator_index);
    if (PAS_UNLIKELY(!allocator.did_succeed)) {
        if (verbose)
            pas_log("Could not quickly get the allocator.\n");
        return pas_allocation_result_create_failure();
    }

    /* This should be specialized out in the non-alignment case because of ALWAYS_INLINE and
       alignment being the constant 1. */
    if (PAS_UNLIKELY(
            alignment != 1
            && alignment > pas_local_allocator_alignment((pas_local_allocator*)allocator.allocator)))
        return pas_allocation_result_create_failure();

    return try_allocate_common_fast_inline_only((pas_local_allocator*)allocator.allocator, allocation_mode);
}

#define PAS_CREATE_TRY_ALLOCATE_PRIMITIVE(name, heap_config, runtime_config, allocator_counts, result_filter) \
    PAS_CREATE_TRY_ALLOCATE_COMMON( \
        name ## _impl, \
        pas_primitive_heap_ref_kind, \
        (heap_config), \
        (runtime_config), \
        (allocator_counts), \
        pas_avoid_size_lookup, \
        (result_filter)); \
    \
    static PAS_NEVER_INLINE pas_allocation_result name ## _casual_case( \
        pas_primitive_heap_ref* heap_ref, \
        size_t size, \
        size_t alignment, \
        pas_allocation_mode allocation_mode) \
    { \
        return pas_try_allocate_primitive_impl_casual_case( \
            heap_ref, size, alignment, allocation_mode, (heap_config), (runtime_config), name ## _impl); \
    } \
    \
    static PAS_ALWAYS_INLINE pas_allocation_result name ## _inline_only( \
        pas_primitive_heap_ref* heap_ref, \
        size_t size, \
        size_t alignment, \
        pas_allocation_mode allocation_mode) \
    { \
        return pas_try_allocate_primitive_impl_inline_only( \
            heap_ref, size, alignment, allocation_mode, (heap_config), name ## _impl_fast_inline_only); \
    } \
    \
    static PAS_ALWAYS_INLINE pas_allocation_result name( \
        pas_primitive_heap_ref* heap_ref, \
        size_t size, \
        size_t alignment, \
        pas_allocation_mode allocation_mode) \
    { \
        pas_allocation_result result; \
        result = name ## _inline_only(heap_ref, size, alignment, allocation_mode); \
        if (PAS_LIKELY(result.did_succeed)) \
            return result; \
        return name ## _casual_case(heap_ref, size, alignment, allocation_mode); \
    } \
    \
    static PAS_UNUSED PAS_NEVER_INLINE pas_allocation_result name ## _for_realloc( \
        pas_primitive_heap_ref* heap_ref, size_t size, pas_allocation_mode allocation_mode) \
    { \
        return name(heap_ref, size, 1, allocation_mode); \
    } \
    \
    struct pas_dummy

typedef pas_allocation_result (*pas_try_allocate_primitive)(
    pas_primitive_heap_ref* heap_ref, size_t size, size_t alignment, pas_allocation_mode allocation_mode);

typedef pas_allocation_result (*pas_try_allocate_primitive_for_realloc)(
    pas_primitive_heap_ref* heap_ref, size_t size, pas_allocation_mode allocation_mode);

PAS_END_EXTERN_C;

#endif /* PAS_TRY_ALLOCATE_PRIMITIVE_H */

