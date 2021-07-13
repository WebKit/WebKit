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

#ifndef PAS_TRY_ALLOCATE_PRIMITIVE_H
#define PAS_TRY_ALLOCATE_PRIMITIVE_H

#include "pas_allocator_counts.h"
#include "pas_heap.h"
#include "pas_heap_config.h"
#include "pas_heap_lock.h"
#include "pas_intrinsic_allocation_result.h"
#include "pas_local_allocator_inlines.h"
#include "pas_physical_memory_transaction.h"
#include "pas_primitive_heap_ref.h"

PAS_BEGIN_EXTERN_C;

static PAS_ALWAYS_INLINE pas_intrinsic_allocation_result
pas_try_allocate_primitive_impl(pas_primitive_heap_ref* heap_ref,
                                size_t size,
                                size_t alignment,
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
        return pas_intrinsic_allocation_result_create_empty();
    
    aligned_size = pas_round_up_to_power_of_2(size, alignment);
    
    index = pas_segregated_heap_index_for_primitive_count(aligned_size, config);
    
    /* Have a fast path for when you allocate some size all the time or
       most of the time. This saves both time and space. The space savings are
       the more interesting part, since */
    
    if (index == heap_ref->cached_index) {
        if (verbose)
            printf("Found cached index.\n");
        allocator_index = heap_ref->base.allocator_index;
    } else {
        pas_heap* heap;
        pas_segregated_heap* segregated_heap;
        
        if (verbose)
            printf("Using full lookup.\n");

        heap = pas_ensure_heap(&heap_ref->base, pas_primitive_heap_ref_kind, config.config_ptr,
                               runtime_config);
        segregated_heap = &heap->segregated_heap;
        
        allocator_index = pas_segregated_heap_allocator_index_for_index(segregated_heap,
                                                                        index,
                                                                        pas_lock_is_not_held);
    }
    
    if (verbose)
        printf("allocator_index = %u\n", allocator_index);
    
    allocator = pas_thread_local_cache_get_local_allocator_if_can_set_cache(
        allocator_index, config.config_ptr);
    
    /* This should be specialized out in the non-alignment case because of ALWAYS_INLINE and
       alignment being the constant 1. */
    if (alignment != 1 && allocator.did_succeed
        && alignment > pas_local_allocator_alignment(allocator.allocator))
        allocator.did_succeed = false;

    return try_allocate_common(&heap_ref->base, aligned_size, aligned_size, alignment, allocator);
}

#define PAS_CREATE_TRY_ALLOCATE_PRIMITIVE(name, heap_config, runtime_config, allocator_counts, result_filter) \
    PAS_CREATE_TRY_ALLOCATE_COMMON( \
        name ## _impl, \
        pas_primitive_heap_ref_kind, \
        (heap_config), \
        (runtime_config), \
        (allocator_counts), \
        pas_avoid_count_lookup, \
        (result_filter)); \
    \
    static PAS_ALWAYS_INLINE pas_intrinsic_allocation_result name( \
        pas_primitive_heap_ref* heap_ref, \
        size_t size, \
        size_t alignment) \
    { \
        return pas_try_allocate_primitive_impl( \
            heap_ref, size, alignment, (heap_config), (runtime_config), name ## _impl); \
    } \
    \
    static PAS_UNUSED PAS_NEVER_INLINE pas_intrinsic_allocation_result name ## _for_realloc( \
        pas_primitive_heap_ref* heap_ref, size_t size) \
    { \
        return name(heap_ref, size, 1); \
    } \
    \
    struct pas_dummy

typedef pas_intrinsic_allocation_result (*pas_try_allocate_primitive)(
    pas_primitive_heap_ref* heap_ref, size_t size, size_t alignment);

typedef pas_intrinsic_allocation_result (*pas_try_allocate_primitive_for_realloc)(
    pas_primitive_heap_ref* heap_ref, size_t size);

PAS_END_EXTERN_C;

#endif /* PAS_TRY_ALLOCATE_PRIMITIVE_H */

