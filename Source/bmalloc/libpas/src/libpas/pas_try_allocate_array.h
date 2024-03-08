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

PAS_BEGIN_EXTERN_C;

/* This is for heaps that hold typed objects. These objects have a certain size. We may allocate
   arrays of these objects. The type may tell us an alignment requirement in addition to a size.
   The alignment requirement is taken together with the minalign argument (the allocator uses
   whichever is bigger), but it's a bit more optimal to convey alignment using the alignment part
   of the type than by passing it to minalign.

   The entrypoints in this file are for the most general case of allocating in the typed heap. You
   can pass any count that is >= 1 and any alignment that is >= 1. Note that these entrypoints have
   optimizations for alignment == 1 and you get the most benefit from those optimizations if you
   really pass the constant "1" for alignment (this causes the special memalign logic to be statically
   killed off by the compiler). If you know that count == 1 and alignment == 1, then you're better off
   using the pas_try_allocate.h entrypoints. */

static PAS_ALWAYS_INLINE pas_allocation_result
pas_try_allocate_array_impl_casual_case_with_heap(pas_heap_ref* heap_ref,
                                                  pas_heap* heap,
                                                  size_t size,
                                                  size_t alignment,
                                                  pas_allocation_mode allocation_mode,
                                                  pas_heap_config config,
                                                  pas_try_allocate_common try_allocate_common)
{
    size_t aligned_size;
    unsigned allocator_index;
    pas_local_allocator_result allocator;

    if (!pas_is_power_of_2(alignment))
        return pas_allocation_result_create_failure();

    aligned_size = pas_try_allocate_compute_aligned_size(size, alignment);
    
    allocator_index = pas_segregated_heap_allocator_index_for_size(
        &heap->segregated_heap, aligned_size, config);
    allocator = pas_thread_local_cache_get_local_allocator_if_can_set_cache_for_possibly_uninitialized_index(
        allocator_index, config.config_ptr);

    if (alignment != 1 && allocator.did_succeed
        && alignment > pas_local_allocator_alignment((pas_local_allocator*)allocator.allocator))
        allocator.did_succeed = false;
    
    return try_allocate_common(heap_ref, aligned_size, alignment, allocation_mode, allocator);
}

static PAS_ALWAYS_INLINE pas_allocation_result
pas_try_allocate_array_impl_inline_only_with_heap(
    pas_heap* heap,
    size_t size,
    size_t alignment,
    pas_allocation_mode allocation_mode,
    pas_heap_config config,
    pas_try_allocate_common_fast_inline_only try_allocate_common_fast_inline_only)
{
    size_t aligned_size;
    unsigned allocator_index;
    pas_local_allocator_result allocator;
    pas_thread_local_cache* cache;

    if (PAS_UNLIKELY(!pas_is_power_of_2(alignment)))
        return pas_allocation_result_create_failure();

    aligned_size = pas_try_allocate_compute_aligned_size(size, alignment);
    
    allocator_index = pas_segregated_heap_allocator_index_for_size_inline_only(
        &heap->segregated_heap, aligned_size, config);

    cache = pas_thread_local_cache_try_get();
    if (PAS_UNLIKELY(!cache))
        return pas_allocation_result_create_failure();
    
    allocator =
        pas_thread_local_cache_try_get_local_allocator_or_unselected_for_uninitialized_index(
            cache, allocator_index);
    if (PAS_UNLIKELY(!allocator.did_succeed))
        return pas_allocation_result_create_failure();

    if (PAS_UNLIKELY(
            alignment != 1
            && alignment > pas_local_allocator_alignment((pas_local_allocator*)allocator.allocator)))
        return pas_allocation_result_create_failure();
    
    return try_allocate_common_fast_inline_only((pas_local_allocator*)allocator.allocator, allocation_mode);
}

static PAS_ALWAYS_INLINE pas_allocation_result
pas_try_allocate_array_impl_inline_only(
    pas_heap_ref* heap_ref,
    size_t size,
    size_t alignment,
    pas_allocation_mode allocation_mode,
    pas_heap_config config,
    pas_try_allocate_common_fast_inline_only try_allocate_common_fast_inline_only)
{
    pas_heap* heap;

    heap = heap_ref->heap;
    if (PAS_UNLIKELY(!heap))
        return pas_allocation_result_create_failure();

    return pas_try_allocate_array_impl_inline_only_with_heap(
        heap, size, alignment, allocation_mode, config, try_allocate_common_fast_inline_only);
}

#define PAS_CREATE_TRY_ALLOCATE_ARRAY(name, heap_config, runtime_config, allocator_counts, result_filter) \
    PAS_CREATE_TRY_ALLOCATE_COMMON( \
        name ## _impl, \
        pas_normal_heap_ref_kind, \
        (heap_config), \
        (runtime_config), \
        (allocator_counts), \
        pas_force_size_lookup, \
        (result_filter)); \
    \
    static PAS_NEVER_INLINE pas_allocation_result name ## _casual_case_with_heap( \
        pas_heap_ref* heap_ref, pas_heap* heap, size_t size, size_t alignment, pas_allocation_mode allocation_mode) \
    { \
        return pas_try_allocate_array_impl_casual_case_with_heap( \
            heap_ref, heap, size, alignment, allocation_mode, (heap_config), name ## _impl); \
    } \
    \
    static PAS_NEVER_INLINE pas_allocation_result name ## _casual_case( \
        pas_heap_ref* heap_ref, size_t size, size_t alignment, pas_allocation_mode allocation_mode) \
    { \
        return name ## _casual_case_with_heap( \
            heap_ref, \
            pas_ensure_heap( \
                heap_ref, pas_normal_heap_ref_kind, (heap_config).config_ptr, (runtime_config)), \
            size, alignment, allocation_mode); \
    } \
    \
    static PAS_ALWAYS_INLINE pas_allocation_result name ## _inline_only_with_heap( \
        pas_heap* heap, size_t size, size_t alignment, pas_allocation_mode allocation_mode) \
    { \
        return pas_try_allocate_array_impl_inline_only_with_heap( \
            heap, size, alignment, allocation_mode, (heap_config), name ## _impl_fast_inline_only); \
    } \
    \
    static PAS_ALWAYS_INLINE pas_allocation_result name ## _inline_only( \
        pas_heap_ref* heap_ref, size_t size, size_t alignment, pas_allocation_mode allocation_mode) \
    { \
        return pas_try_allocate_array_impl_inline_only( \
            heap_ref, size, alignment, allocation_mode, (heap_config), name ## _impl_fast_inline_only); \
    } \
    \
    static PAS_ALWAYS_INLINE pas_allocation_result name ## _with_heap( \
        pas_heap_ref* heap_ref, pas_heap* heap, size_t size, size_t alignment, pas_allocation_mode allocation_mode) \
    { \
        pas_allocation_result result; \
        result = name ## _inline_only_with_heap(heap, size, alignment, allocation_mode); \
        if (PAS_LIKELY(result.did_succeed)) \
            return result; \
        return name ## _casual_case_with_heap(heap_ref, heap, size, alignment, allocation_mode); \
    } \
    \
    static PAS_ALWAYS_INLINE pas_allocation_result name ## _by_size( \
        pas_heap_ref* heap_ref, size_t size, size_t alignment, pas_allocation_mode allocation_mode) \
    { \
        pas_allocation_result result; \
        result = name ## _inline_only(heap_ref, size, alignment, allocation_mode); \
        if (PAS_LIKELY(result.did_succeed)) \
            return result; \
        return name ## _casual_case(heap_ref, size, alignment, allocation_mode); \
    } \
    \
    static PAS_ALWAYS_INLINE pas_allocation_result name ## _by_count( \
        pas_heap_ref* heap_ref, size_t count, size_t alignment, pas_allocation_mode allocation_mode) \
    { \
        const pas_heap_type* type; \
        size_t type_size; \
        size_t size; \
        \
        type = heap_ref->type; \
        type_size = (heap_config).get_type_size(type); \
        \
        if (__builtin_mul_overflow(count, type_size, &size)) \
            return pas_allocation_result_create_failure(); \
        \
        return name ## _by_size(heap_ref, size, alignment, allocation_mode); \
    } \
    \
    static PAS_UNUSED PAS_NEVER_INLINE pas_allocation_result name ## _for_realloc( \
        pas_heap_ref* heap_ref, pas_heap* heap, size_t size, pas_allocation_mode allocation_mode) \
    { \
        return name ## _with_heap(heap_ref, heap, size, 1, allocation_mode); \
    } \
    \
    struct pas_dummy
    
typedef pas_allocation_result (*pas_try_allocate_array_with_heap)(
    pas_heap_ref* heap_ref, pas_heap* heap, size_t size, size_t alignment, pas_allocation_mode allocation_mode);

typedef pas_allocation_result (*pas_try_allocate_array_by_size)(
    pas_heap_ref* heap_ref, size_t size, size_t alignment, pas_allocation_mode allocation_mode);

typedef pas_allocation_result (*pas_try_allocate_array_by_count)(
    pas_heap_ref* heap_ref, size_t count, size_t alignment, pas_allocation_mode allocation_mode);

typedef pas_allocation_result (*pas_try_allocate_array_for_realloc)(
    pas_heap_ref* heap_ref, pas_heap* heap, size_t size, pas_allocation_mode allocation_mode);

PAS_END_EXTERN_C;

#endif /* PAS_TRY_ALLOCATE_ARRAY_H */

