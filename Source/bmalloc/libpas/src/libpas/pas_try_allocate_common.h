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
#include "pas_allocation_result.h"
#include "pas_local_allocator_inlines.h"
#include "pas_malloc_stack_logging.h"
#include "pas_primitive_heap_ref.h"
#include "pas_segregated_heap_inlines.h"
#include "pas_utils.h"

PAS_BEGIN_EXTERN_C;

static PAS_ALWAYS_INLINE size_t pas_try_allocate_compute_aligned_size(size_t size,
                                                                      size_t alignment)
{
    /* In the non-memalign case, we can happily handle zero-sized allocations with aligned_size
       being 0. This works because the heap's 0 size class is just a copy of the minalign size
       class. But we cannot do this properly if the 0 size class has to have some alignment. That's
       because we expect that for a size class to satisfy some alignment, that size must itself
       be aligned to that alignment. */
    if (alignment == 1)
        return size;
    if (size < alignment)
        return alignment;
    return pas_round_up_to_power_of_2(size, alignment);
}

static PAS_ALWAYS_INLINE bool
pas_try_allocate_common_can_go_fast(pas_local_allocator_result allocator_result)
{
    return allocator_result.did_succeed;
}

static PAS_ALWAYS_INLINE pas_allocation_result
pas_try_allocate_common_impl_fast_inline_only(
    pas_heap_config config,
    pas_allocation_result_filter result_filter,
    pas_local_allocator* allocator)
{
    return pas_local_allocator_try_allocate_inline_only(allocator,
                                                        config,
                                                        result_filter);
}

static PAS_ALWAYS_INLINE pas_allocation_result
pas_try_allocate_common_impl_fast(
    pas_heap_config config,
    pas_allocator_counts* allocator_counts,
    pas_allocation_result_filter result_filter,
    pas_local_allocator* allocator,
    size_t size,
    size_t alignment)
{
    static const bool verbose = false;
    
    pas_allocation_result result;
    result = pas_local_allocator_try_allocate(allocator,
                                              size,
                                              alignment,
                                              config,
                                              allocator_counts,
                                              result_filter);
    if (verbose)
        pas_log("in common - result.begin = %p\n", (void*)result.begin);
    return result;
}

static PAS_ALWAYS_INLINE pas_allocation_result
pas_try_allocate_common_impl_slow(
    pas_heap_ref* heap_ref,
    pas_heap_ref_kind heap_ref_kind,
    size_t size, /* Must be = round_up(size, alignment) */
    size_t alignment,
    pas_heap_config config,
    pas_heap_runtime_config* runtime_config,
    pas_allocator_counts* allocator_counts,
    pas_size_lookup_mode size_lookup_mode)
{
    static const bool verbose = false;
    
    pas_baseline_allocator_result baseline_allocator_result;
    pas_allocation_result result;
    pas_heap* heap;
    const pas_heap_type* type;
    pas_segregated_size_directory* directory;
    unsigned* cached_index;

    if (verbose)
        pas_log("Allocating in the slow path, size = %zu\n", size);

    result = pas_allocation_result_create_failure();

    type = heap_ref->type;
    alignment = PAS_MAX(alignment, config.get_type_alignment(type));
    
    if (PAS_UNLIKELY(pas_debug_heap_is_enabled(config.kind))) {
        if (verbose)
            pas_log("Debug heap enabled, asking debug heap.\n");
        result = pas_debug_heap_allocate(size, alignment);
        if (verbose)
            pas_log("Got result.ptr = %p, did_succeed = %d\n", (void*)result.begin, result.did_succeed);
        return result;
    }

    if (verbose)
        pas_log("Not using debug heap.\n");

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
    
    directory = pas_heap_ensure_size_directory_for_size(
        heap, size, alignment, size_lookup_mode, config, cached_index,
        allocator_counts);
    
    if (!directory) {
        /* This means we're really doing a large allocation! */
        
        pas_physical_memory_transaction transaction;
        
        if (verbose)
            pas_log("Did not get a directory.\n");

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
        
        return pas_msl_malloc_logging(size, result);
    }
    
    if (verbose)
        pas_log("Got a directory.\n");
    
    PAS_ASSERT(directory);
    
    baseline_allocator_result = pas_segregated_size_directory_select_allocator(
        directory, size, size_lookup_mode, config.config_ptr, cached_index);
    
    result = pas_local_allocator_try_allocate(baseline_allocator_result.allocator,
                                              size,
                                              alignment,
                                              config,
                                              allocator_counts,
                                              pas_allocation_result_identity);
    
    pas_compiler_fence();
    if (baseline_allocator_result.lock)
        pas_lock_unlock(baseline_allocator_result.lock);

    return pas_msl_malloc_logging(size, result);
}

static PAS_ALWAYS_INLINE pas_allocation_result
pas_try_allocate_common_impl(
    pas_heap_ref* heap_ref,
    size_t size,
    size_t alignment,
    pas_heap_config config,
    pas_allocator_counts* allocator_counts,
    pas_allocation_result (*result_filter)(pas_allocation_result result),
    pas_allocation_result (*slow)(pas_heap_ref* heap_ref, size_t size, size_t alignment),
    pas_local_allocator_result allocator_result)
{
    static const bool verbose = false;
    
    if (verbose) {
        pas_log("heap_ref = %p allocating size = %zu, alignment = %zu.\n",
                heap_ref, size, alignment);
    }

    if (PAS_LIKELY(pas_try_allocate_common_can_go_fast(allocator_result))) {
        return pas_try_allocate_common_impl_fast(
            config, allocator_counts, result_filter, (pas_local_allocator*)allocator_result.allocator,
            size, alignment);
    }

    return slow(heap_ref, size, alignment);
}

#define PAS_CREATE_TRY_ALLOCATE_COMMON(name, heap_ref_kind, heap_config, runtime_config, allocator_counts, size_lookup_mode, result_filter) \
    static PAS_UNUSED PAS_ALWAYS_INLINE pas_allocation_result \
    name ## _fast_inline_only(pas_local_allocator* allocator) \
    { \
        return pas_try_allocate_common_impl_fast_inline_only( \
            (heap_config), (result_filter), allocator); \
    } \
    \
    static PAS_UNUSED PAS_ALWAYS_INLINE pas_allocation_result \
    name ## _fast(pas_local_allocator* allocator, size_t size, size_t alignment) \
    { \
        return pas_try_allocate_common_impl_fast( \
            (heap_config), (allocator_counts), (result_filter), allocator, \
            size, alignment); \
    } \
    \
    static PAS_NEVER_INLINE pas_allocation_result \
    name ## _slow(pas_heap_ref* heap_ref, size_t size, size_t alignment) \
    { \
        return (result_filter)( \
            (heap_config).specialized_try_allocate_common_impl_slow( \
                heap_ref, (heap_ref_kind), size, alignment, (runtime_config), \
                (allocator_counts), (size_lookup_mode))); \
    } \
    \
    static PAS_UNUSED PAS_ALWAYS_INLINE pas_allocation_result \
    name(pas_heap_ref* heap_ref, size_t size, size_t alignment, \
         pas_local_allocator_result allocator_result) \
    { \
        return pas_try_allocate_common_impl( \
            heap_ref, size, alignment, (heap_config), \
            (allocator_counts), (result_filter), name ## _slow, allocator_result); \
    } \
    \
    struct pas_dummy

typedef pas_allocation_result (*pas_try_allocate_common_fast_inline_only)(
    pas_local_allocator* allocator);

typedef pas_allocation_result (*pas_try_allocate_common_fast)(
    pas_local_allocator* allocator, size_t size, size_t alignment);

typedef pas_allocation_result (*pas_try_allocate_common_slow)(
    pas_heap_ref* heap_ref, size_t size, size_t alignment);

typedef pas_allocation_result (*pas_try_allocate_common)(
    pas_heap_ref* heap_ref, size_t size, size_t alignment,
    pas_local_allocator_result allocator_result);

PAS_END_EXTERN_C;

#endif /* PAS_TRY_ALLOCATE_COMMON_H */

