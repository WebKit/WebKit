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

#ifndef PAS_TRY_ALLOCATE_COMMON_H
#define PAS_TRY_ALLOCATE_COMMON_H

#include "pas_debug_heap.h"
#include "pas_heap_inlines.h"
#include "pas_intrinsic_allocation_result.h"
#include "pas_local_allocator_inlines.h"
#include "pas_primitive_heap_ref.h"
#include "pas_segregated_heap_inlines.h"
#include "pas_utils.h"

PAS_BEGIN_EXTERN_C;

static PAS_ALWAYS_INLINE bool
pas_try_allocate_common_can_go_fast(pas_local_allocator_result allocator_result)
{
    return allocator_result.did_succeed;
}

static PAS_ALWAYS_INLINE pas_intrinsic_allocation_result
pas_try_allocate_common_impl_fast(
    pas_heap_config config,
    pas_allocator_counts* allocator_counts,
    pas_intrinsic_allocation_result (*result_filter)(pas_intrinsic_allocation_result result),
    pas_local_allocator* allocator,
    pas_size_thunk size_thunk,
    void* size_thunk_arg,
    size_t alignment)
{
    return result_filter(pas_intrinsic_allocation_result_create(
        pas_local_allocator_try_allocate(allocator,
                                         size_thunk,
                                         size_thunk_arg,
                                         alignment,
                                         config,
                                         allocator_counts)));
}

static PAS_ALWAYS_INLINE pas_intrinsic_allocation_result
pas_try_allocate_common_impl_slow(
    pas_heap_ref* heap_ref,
    pas_heap_ref_kind heap_ref_kind,
    size_t aligned_count, /* Must be = round_up(count * type_size, alignment) / type_size */
    size_t size, /* Must be = round_up(size, alignment) */
    size_t alignment,
    pas_heap_config config,
    pas_heap_runtime_config* runtime_config,
    pas_allocator_counts* allocator_counts,
    pas_count_lookup_mode count_lookup_mode)
{
    static const bool verbose = false;
    
    pas_baseline_allocator_result baseline_allocator_result;
    pas_allocation_result result;
    pas_heap* heap;
    pas_heap_type* type;
    pas_segregated_global_size_directory* directory;
    unsigned* cached_index;

    if (verbose)
        pas_log("Allocating in the slow path, size = %zu\n", size);

    result = pas_allocation_result_create_failure();

    type = heap_ref->type;
    alignment = PAS_MAX(alignment, config.get_type_alignment(type));
    
    if (PAS_UNLIKELY(pas_debug_heap_is_enabled())) {
        void* raw_result;
        
        if (alignment != 1) {
            PAS_ASSERT(alignment > 1);
            raw_result = pas_debug_heap_memalign(alignment, size);
        } else
            raw_result = pas_debug_heap_malloc(size);

        if (raw_result) {
            result.did_succeed = true;
            result.begin = (uintptr_t)raw_result;
        }

        return pas_intrinsic_allocation_result_create(result);
    }

    heap = pas_ensure_heap(heap_ref, heap_ref_kind, config.config_ptr, runtime_config);
    
    switch (heap_ref_kind) {
    case pas_normal_heap_ref_kind:
    case pas_fake_heap_ref_kind:
        cached_index = NULL;
        break;
    case pas_primitive_heap_ref_kind:
        cached_index = &((pas_primitive_heap_ref*)heap_ref)->cached_index;
        break;
    }
    
    if (verbose)
        pas_log("Asking heap for a directory.\n");
    
    directory = pas_heap_ensure_size_directory_for_count(
        heap, aligned_count, alignment, count_lookup_mode, config, cached_index,
        allocator_counts);
    
    if (!directory) {
        /* This means we're really doing a large allocation! */
        
        pas_physical_memory_transaction transaction;
        size_t my_size; /* Just for asserting. */
        bool did_overflow;
        
        if (verbose)
            pas_log("Did not get a directory.\n");

        did_overflow = __builtin_umull_overflow(aligned_count, config.get_type_size(type), &my_size);
        PAS_ASSERT(!did_overflow);
        my_size = pas_round_up_to_power_of_2(my_size, alignment);
        PAS_ASSERT(my_size == size);
        
        pas_physical_memory_transaction_construct(&transaction);
        do {
            PAS_ASSERT(!result.did_succeed);
            
            pas_physical_memory_transaction_begin(&transaction);
            pas_heap_lock_lock();
            
            result = pas_large_heap_try_allocate(
                &heap->large_heap, size, alignment, config.config_ptr, &transaction);
            
            pas_heap_lock_unlock();
        } while (!pas_physical_memory_transaction_end(&transaction));
        
        pas_scavenger_notify_eligibility_if_needed();
        
        return pas_intrinsic_allocation_result_create(result);
    }
    
    if (verbose)
        pas_log("Got a directory.\n");
    
    PAS_ASSERT(directory);
    
    baseline_allocator_result = pas_segregated_global_size_directory_select_allocator(
        directory, aligned_count, count_lookup_mode, config.config_ptr, cached_index);
    
    result = pas_local_allocator_try_allocate(baseline_allocator_result.allocator,
                                              pas_trivial_size_thunk,
                                              (void*)size,
                                              alignment,
                                              config,
                                              allocator_counts);
    
    pas_compiler_fence();
    if (baseline_allocator_result.lock)
        pas_lock_unlock(baseline_allocator_result.lock);

    return pas_intrinsic_allocation_result_create(result);
}

static PAS_ALWAYS_INLINE pas_intrinsic_allocation_result
pas_try_allocate_common_impl(
    pas_heap_ref* heap_ref,
    size_t aligned_count, /* Must be = round_up(count * type_size, alignment) / type_size */
    size_t size,
    size_t alignment,
    pas_heap_config config,
    pas_allocator_counts* allocator_counts,
    pas_intrinsic_allocation_result (*result_filter)(pas_intrinsic_allocation_result result),
    pas_intrinsic_allocation_result (*slow)(
        pas_heap_ref* heap_ref, size_t aligned_count, size_t size, size_t alignment),
    pas_local_allocator_result allocator_result)
{
    static const bool verbose = false;
    
    if (verbose) {
        pas_log("heap_ref = %p allocating aligned_count = %zu, alignment = %zu.\n",
                heap_ref, aligned_count, alignment);
    }

    if (PAS_LIKELY(pas_try_allocate_common_can_go_fast(allocator_result))) {
        return pas_try_allocate_common_impl_fast(
            config, allocator_counts, result_filter, allocator_result.allocator,
            pas_trivial_size_thunk, (void*)size, alignment);
    }

    return slow(heap_ref, aligned_count, size, alignment);
}

#define PAS_CREATE_TRY_ALLOCATE_COMMON(name, heap_ref_kind, heap_config, runtime_config, allocator_counts, count_lookup_mode, result_filter) \
    static PAS_UNUSED PAS_ALWAYS_INLINE pas_intrinsic_allocation_result \
    name ## _fast(pas_local_allocator* allocator, pas_size_thunk size_thunk, void* size_thunk_arg, \
                  size_t alignment) \
    { \
        return pas_try_allocate_common_impl_fast( \
            (heap_config), (allocator_counts), (result_filter), allocator, \
            size_thunk, size_thunk_arg, alignment); \
    } \
    \
    static PAS_NEVER_INLINE pas_intrinsic_allocation_result \
    name ## _slow(pas_heap_ref* heap_ref, size_t aligned_count, size_t size, size_t alignment) \
    { \
        return (result_filter)( \
            (heap_config).specialized_try_allocate_common_impl_slow( \
                heap_ref, (heap_ref_kind), aligned_count, size, alignment, (runtime_config), \
                (allocator_counts), (count_lookup_mode))); \
    } \
    \
    static PAS_UNUSED PAS_ALWAYS_INLINE pas_intrinsic_allocation_result \
    name(pas_heap_ref* heap_ref, size_t aligned_count, size_t size, size_t alignment, \
         pas_local_allocator_result allocator_result) \
    { \
        return pas_try_allocate_common_impl( \
            heap_ref, aligned_count, size, alignment, (heap_config), \
            (allocator_counts), (result_filter), name ## _slow, allocator_result); \
    } \
    \
    struct pas_dummy

typedef pas_intrinsic_allocation_result (*pas_try_allocate_common_fast)(
    pas_local_allocator* allocator, pas_size_thunk size_thunk, void* size_thunk_arg, size_t alignment);

typedef pas_intrinsic_allocation_result (*pas_try_allocate_common_slow)(
    pas_heap_ref* heap_ref, size_t aligned_count, size_t size, size_t alignment);

typedef pas_intrinsic_allocation_result (*pas_try_allocate_common)(
    pas_heap_ref* heap_ref, size_t aligned_count, size_t size, size_t alignment,
    pas_local_allocator_result allocator_result);

PAS_END_EXTERN_C;

#endif /* PAS_TRY_ALLOCATE_COMMON_H */

