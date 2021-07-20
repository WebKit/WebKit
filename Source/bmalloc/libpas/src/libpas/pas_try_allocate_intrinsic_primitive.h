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

#ifndef PAS_TRY_ALLOCATE_INTRINSIC_PRIMITIVE_H
#define PAS_TRY_ALLOCATE_INTRINSIC_PRIMITIVE_H

#include "pas_designated_intrinsic_heap_inlines.h"
#include "pas_heap.h"
#include "pas_intrinsic_heap_support.h"
#include "pas_try_allocate_common.h"

PAS_BEGIN_EXTERN_C;

#define PAS_INTRINSIC_SEGREGATED_HEAP_INITIALIZER(parent_heap_ptr, support, passed_runtime_config) { \
        .runtime_config = (passed_runtime_config), \
        .basic_size_directory_and_head = PAS_COMPACT_ATOMIC_PTR_INITIALIZER, \
        .small_index_upper_bound = PAS_NUM_INTRINSIC_SIZE_CLASSES, \
        .index_to_small_size_directory = (support).index_to_size_directory, \
        .index_to_small_allocator_index = (support).index_to_allocator_index, \
        .rare_data = PAS_COMPACT_ATOMIC_PTR_INITIALIZER, \
    }

#define PAS_INTRINSIC_PRIMITIVE_HEAP_SEGREGATED_HEAP_FIELDS(heap_ptr, intrinsic_support, runtime_config) \
        .segregated_heap = PAS_INTRINSIC_SEGREGATED_HEAP_INITIALIZER( \
            heap_ptr, intrinsic_support, runtime_config), \

#define PAS_INTRINSIC_PRIMITIVE_HEAP_INITIALIZER(heap_ptr, primitive_type, intrinsic_support, passed_config, runtime_config) { \
        .type = (pas_heap_type*)(primitive_type), \
        PAS_INTRINSIC_PRIMITIVE_HEAP_SEGREGATED_HEAP_FIELDS(heap_ptr, intrinsic_support, runtime_config) \
        .large_heap = { \
            .free_heap = PAS_FAST_LARGE_FREE_HEAP_INITIALIZER, \
            .table_state = pas_heap_table_state_uninitialized, \
            .index = 0 \
        }, \
        .heap_ref = NULL, \
        .next_heap = PAS_COMPACT_PTR_INITIALIZER, \
        .config_kind = (passed_config).kind, \
    }

static PAS_ALWAYS_INLINE pas_intrinsic_allocation_result
pas_try_allocate_intrinsic_primitive_impl_medium_slow_case(
    pas_thread_local_cache* cache,
    pas_heap* heap,
    size_t aligned_size,
    size_t alignment,
    pas_heap_config config,
    pas_try_allocate_common_fast try_allocate_common_fast,
    pas_try_allocate_common_slow try_allocate_common_slow)
{
    static const bool verbose = false;
    
    size_t index;
    unsigned allocator_index;
    pas_local_allocator_result allocator;
    pas_heap_ref fake_heap_ref;

    index = pas_segregated_heap_index_for_primitive_count(aligned_size,
                                                          config);
    
    allocator_index =
        pas_segregated_heap_medium_allocator_index_for_index(
            &heap->segregated_heap,
            index,
            pas_segregated_heap_medium_size_directory_search_within_size_class_progression,
            pas_lock_is_not_held);

    allocator = pas_thread_local_cache_get_local_allocator(
        cache, allocator_index, pas_lock_is_not_held);
    
    PAS_TESTING_ASSERT(!allocator.did_succeed || allocator.allocator->object_size >= aligned_size);
    
    /* This should be specialized out in the non-alignment case because of ALWAYS_INLINE and
       alignment being the constant 1. */
    if (alignment != 1 && allocator.did_succeed
        && alignment > pas_local_allocator_alignment(allocator.allocator)) {
        if (verbose) {
            pas_log("Discarding good allocator result because alignment is inadequate "
                    "(%zu, needed %zu).\n",
                    pas_local_allocator_alignment(allocator.allocator), alignment);
        }
        allocator.did_succeed = false;
    }
    
    if (PAS_LIKELY(allocator.did_succeed)) {
        return try_allocate_common_fast(
            allocator.allocator, pas_trivial_size_thunk, (void*)aligned_size, alignment);
    }

    fake_heap_ref.type = heap->type;
    fake_heap_ref.heap = heap;
    fake_heap_ref.allocator_index = UINT_MAX;

    return try_allocate_common_slow(&fake_heap_ref, aligned_size, aligned_size, alignment);
    
}

static PAS_ALWAYS_INLINE pas_intrinsic_allocation_result
pas_try_allocate_intrinsic_primitive_impl(
    pas_heap* heap,
    size_t size,
    size_t alignment,
    pas_intrinsic_heap_support* intrinsic_support,
    pas_heap_config config,
    pas_try_allocate_common_fast try_allocate_common_fast,
    pas_try_allocate_common_slow try_allocate_common_slow,
    pas_intrinsic_allocation_result (*medium_slow_case)(pas_thread_local_cache* thread_local_cache,
                                                        size_t aligned_size,
                                                        size_t alignment),
    pas_intrinsic_heap_designation_mode designation_mode)
{
    static const bool verbose = false;
    
    size_t aligned_size;
    size_t index;
    pas_heap_ref fake_heap_ref;
    pas_designated_index_result designated_index;
    pas_thread_local_cache* cache;

    PAS_ASSERT(alignment == 1 || !designation_mode);

    if (!pas_is_power_of_2(alignment))
        return pas_intrinsic_allocation_result_create_empty();

    /* In the non-memalign case, we can happily handle zero-sized allocations with aligned_size
       being 0. This works because the heap's 0 size class is just a copy of the minalign size
       class. But we cannot do this properly if the 0 size class has to have some alignment. That's
       because we expect that for a size class to satisfy some alignment, that size must itself
       be aligned to that alignment. */
    if (alignment == 1)
        aligned_size = size;
    else if (size < alignment)
        aligned_size = alignment;
    else
        aligned_size = pas_round_up_to_power_of_2(size, alignment);
    
    index = pas_segregated_heap_index_for_primitive_count(aligned_size,
                                                          config);

    if (verbose) {
        pas_log("aligned_size = %zu, index = %zu, alignment = %zu.\n",
                aligned_size, index, alignment);
    }

    cache = pas_thread_local_cache_try_get();
    if (PAS_LIKELY(cache)) {
        unsigned allocator_index;
        pas_local_allocator_result allocator;
        
        designated_index = pas_designated_intrinsic_heap_designated_index(
            index, designation_mode, config);
        if (PAS_LIKELY(designated_index.did_succeed)) {
            allocator = pas_local_allocator_result_create_success(
                pas_thread_local_cache_get_local_allocator_impl(
                    cache,
                    designated_index.index * pas_designated_intrinsic_heap_num_allocator_indices(config)));
        } else {
            if (PAS_UNLIKELY(index >= PAS_NUM_INTRINSIC_SIZE_CLASSES))
                return medium_slow_case(cache, aligned_size, alignment);
            
            allocator_index = intrinsic_support->index_to_allocator_index[index];
        
            if (verbose)
                pas_log("allocator_index = %u.\n", allocator_index);
        
            allocator = pas_thread_local_cache_try_get_local_allocator(cache, allocator_index);
        }

        PAS_TESTING_ASSERT(!allocator.did_succeed || allocator.allocator->object_size >= size);
    
        /* This should be specialized out in the non-alignment case because of ALWAYS_INLINE and
           alignment being the constant 1. */
        if (alignment != 1 && allocator.did_succeed
            && alignment > pas_local_allocator_alignment(allocator.allocator)) {
            if (verbose) {
                pas_log("Discarding good allocator result because alignment is inadequate "
                        "(%zu, needed %zu).\n",
                        pas_local_allocator_alignment(allocator.allocator), alignment);
            }
            allocator.did_succeed = false;
        }

        if (PAS_LIKELY(allocator.did_succeed)) {
            return try_allocate_common_fast(
                allocator.allocator, pas_trivial_size_thunk, (void*)aligned_size, alignment);
        }
    }

    fake_heap_ref.type = heap->type;
    fake_heap_ref.heap = heap;
    fake_heap_ref.allocator_index = UINT_MAX;

    return try_allocate_common_slow(&fake_heap_ref, aligned_size, aligned_size, alignment);
}

#define PAS_CREATE_TRY_ALLOCATE_INTRINSIC_PRIMITIVE(name, heap_config, runtime_config, allocator_counts, result_filter, heap, heap_support, designation_mode) \
    PAS_CREATE_TRY_ALLOCATE_COMMON( \
        name ## _impl, \
        pas_fake_heap_ref_kind, \
        (heap_config), \
        (runtime_config), \
        (allocator_counts), \
        pas_force_count_lookup, \
        (result_filter)); \
    \
    static PAS_NEVER_INLINE pas_intrinsic_allocation_result \
    name ## _medium_slow_case(pas_thread_local_cache* cache, size_t aligned_size, size_t alignment) \
    { \
        return pas_try_allocate_intrinsic_primitive_impl_medium_slow_case( \
            cache, (heap), aligned_size, alignment, (heap_config), \
            name ## _impl_fast, name ## _impl_slow); \
    } \
    \
    static PAS_ALWAYS_INLINE pas_intrinsic_allocation_result name(size_t size, size_t alignment) \
    { \
        return pas_try_allocate_intrinsic_primitive_impl( \
            (heap), size, alignment, (heap_support), (heap_config), \
            name ## _impl_fast, name ## _impl_slow, name ## _medium_slow_case, \
            (designation_mode)); \
    } \
    \
    static PAS_UNUSED PAS_NEVER_INLINE pas_intrinsic_allocation_result \
    name ## _for_realloc(size_t size) \
    { \
        return name(size, 1); \
    } \
    \
    struct pas_dummy

typedef pas_intrinsic_allocation_result (*pas_try_allocate_intrinsic_primitive)(size_t size,
                                                                                size_t alignment);

typedef pas_intrinsic_allocation_result
(*pas_try_allocate_intrinsic_primitive_for_realloc)(size_t size);       

PAS_END_EXTERN_C;

#endif /* PAS_TRY_ALLOCATE_INTRINSIC_PRIMITIVE_H */

