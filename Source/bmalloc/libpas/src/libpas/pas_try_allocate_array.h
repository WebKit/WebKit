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

#ifndef PAS_TRY_ALLOCATE_ARRAY_H
#define PAS_TRY_ALLOCATE_ARRAY_H

#include "pas_allocator_counts.h"
#include "pas_cares_about_size_mode.h"
#include "pas_heap.h"
#include "pas_heap_config.h"
#include "pas_heap_ref.h"
#include "pas_local_allocator_inlines.h"
#include "pas_physical_memory_transaction.h"
#include "pas_segregated_heap.h"
#include "pas_typed_allocation_result.h"

PAS_BEGIN_EXTERN_C;

static PAS_ALWAYS_INLINE pas_typed_allocation_result
pas_try_allocate_array_impl(pas_heap_ref* heap_ref,
                            pas_heap* heap,
                            size_t count,
                            size_t alignment,
                            pas_heap_config config,
                            pas_try_allocate_common try_allocate_common)
{
    pas_heap_type* type;
    size_t type_size;
    size_t aligned_size;
    size_t aligned_count;
    unsigned allocator_index;
    pas_local_allocator_result allocator;

    type = heap_ref->type;
    type_size = config.get_type_size(type);

    if (__builtin_mul_overflow(count, type_size, &aligned_size))
        return pas_typed_allocation_result_create_empty();

    if (alignment == 1)
        aligned_count = count;
    else {
        if (!pas_is_power_of_2(alignment))
            return pas_typed_allocation_result_create_empty();
        
        /* It's important that we use the right count for the lookup. This gets weird. Consider
           that we might have a type of size 24, count 3, and alignment 64. 24 * 3 is 72, which is
           smaller than 128, which is the size we'd want for alignment 64. The effect of this code
           is to raise the count to 5, so that we try to allocate 120 bytes with alignment 64,
           resulting in a 128 byte allocation.
        
           If we didn't do this correction then we'd:
        
           - Do the lookup with count 3. If we had already created a 64-byte aligned size class at
             count 5, we'd fail to see it.
        
           - Ask for a 64-byte aligned size class to be ensured at count 3. That would be wasteful
             for any future count=3 allocations that did not want this kind of alignment. */

        if (aligned_size < alignment)
            aligned_size = alignment;
        else
            aligned_size = pas_round_up_to_power_of_2(aligned_size, alignment);
     
        /* This is gross. In an ideal world, we wouldn't have to execute divisions. We could have
           avoided this by requiring the size to be the currency used to lookup size classes.
           However, since aligned allocations are rare, this is probably OK. This has the effect of
           making the non-aligned case more efficient. */
        aligned_count = aligned_size / type_size;
    }
    
    allocator_index = pas_segregated_heap_allocator_index_for_count_not_primitive(
        &heap->segregated_heap,
        aligned_count);
    allocator = pas_thread_local_cache_get_local_allocator_if_can_set_cache(
        allocator_index, config.config_ptr);

    if (alignment != 1 && allocator.did_succeed
        && alignment > pas_local_allocator_alignment(allocator.allocator))
        allocator.did_succeed = false;
    
    return pas_typed_allocation_result_create_with_intrinsic_allocation_result(
        try_allocate_common(heap_ref, aligned_count, aligned_size, alignment, allocator),
        type, aligned_size);
}

#define PAS_CREATE_TRY_ALLOCATE_ARRAY(name, heap_config, runtime_config, allocator_counts, result_filter) \
    PAS_CREATE_TRY_ALLOCATE_COMMON( \
        name ## _impl, \
        pas_normal_heap_ref_kind, \
        (heap_config), \
        (runtime_config), \
        (allocator_counts), \
        pas_force_count_lookup, \
        (result_filter)); \
    \
    static PAS_ALWAYS_INLINE pas_typed_allocation_result name ## _with_heap( \
        pas_heap_ref* heap_ref, pas_heap* heap, size_t count, size_t alignment) \
    { \
        return pas_try_allocate_array_impl( \
            heap_ref, heap, count, alignment, (heap_config), name ## _impl); \
    } \
    \
    static PAS_ALWAYS_INLINE pas_typed_allocation_result name( \
        pas_heap_ref* heap_ref, size_t count, size_t alignment) \
    { \
        return name ## _with_heap( \
            heap_ref, \
            pas_ensure_heap( \
                heap_ref, pas_normal_heap_ref_kind, (heap_config).config_ptr, (runtime_config)), \
            count, alignment); \
    } \
    \
    static PAS_UNUSED PAS_NEVER_INLINE pas_typed_allocation_result name ## _for_realloc( \
        pas_heap_ref* heap_ref, pas_heap* heap, size_t count) \
    { \
        return name ## _with_heap(heap_ref, heap, count, 1); \
    } \
    \
    struct pas_dummy
    
typedef pas_typed_allocation_result (*pas_try_allocate_array_with_heap)(
    pas_heap_ref* heap_ref, pas_heap* heap, size_t count, size_t alignment);

typedef pas_typed_allocation_result (*pas_try_allocate_array)(
    pas_heap_ref* heap_ref, size_t count, size_t alignment);

typedef pas_typed_allocation_result (*pas_try_allocate_array_for_realloc)(
    pas_heap_ref* heap_ref, pas_heap* heap, size_t count);

PAS_END_EXTERN_C;

#endif /* PAS_TRY_ALLOCATE_ARRAY_H */

