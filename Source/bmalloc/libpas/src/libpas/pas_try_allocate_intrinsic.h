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

#ifndef PAS_TRY_ALLOCATE_INTRINSIC_H
#define PAS_TRY_ALLOCATE_INTRINSIC_H

#include "pas_designated_intrinsic_heap_inlines.h"
#include "pas_heap.h"
#include "pas_intrinsic_heap_support.h"
#include "pas_try_allocate_common.h"

PAS_BEGIN_EXTERN_C;

/* This is for global, singleton, process-wide heaps -- so the "not isoheaped" heap, the thing
   you get from regular malloc.

   It is possible for a heap_config to have multiple intrinsic heaps, but generally speaking, a
   heap_config will have a fixed number of intrinsic heaps and they will all be known to libpas
   ahead of time.

   It is possible for libpas to be configured with multiple heap_configs, so we may also have
   multiple intrinsic heaps that way (each heap_config may have one intrinsic).

   It is also possible for a heap_config to have no intrinsics. From a functionality standpoint,
   primitive heaps can do everything that intrinsic heaps do, though primitives are optimized for
   the possibility that clients of libpas will create any number of them. */

#define PAS_INTRINSIC_SEGREGATED_HEAP_INITIALIZER(parent_heap_ptr, support, passed_runtime_config) { \
        .runtime_config = (passed_runtime_config), \
        .index_to_small_allocator_index = (support).index_to_allocator_index, \
        .index_to_small_size_directory = (support).index_to_size_directory, \
        .basic_size_directory_and_head = PAS_COMPACT_ATOMIC_PTR_INITIALIZER, \
        .rare_data = PAS_COMPACT_ATOMIC_PTR_INITIALIZER, \
        .small_index_upper_bound = PAS_NUM_INTRINSIC_SIZE_CLASSES, \
    }

#define PAS_INTRINSIC_HEAP_SEGREGATED_HEAP_FIELDS(heap_ptr, intrinsic_support, runtime_config) \
        .segregated_heap = PAS_INTRINSIC_SEGREGATED_HEAP_INITIALIZER( \
            heap_ptr, intrinsic_support, runtime_config), \

#define PAS_INTRINSIC_HEAP_INITIALIZER(heap_ptr, primitive_type, intrinsic_support, passed_config, runtime_config) { \
        PAS_INTRINSIC_HEAP_SEGREGATED_HEAP_FIELDS(heap_ptr, intrinsic_support, runtime_config) \
        .large_heap = { \
            .free_heap = PAS_FAST_LARGE_FREE_HEAP_INITIALIZER, \
            .index = 0, \
            .table_state = pas_heap_table_state_uninitialized, \
        }, \
        .type = (const pas_heap_type*)(primitive_type), \
        .heap_ref = NULL, \
        .next_heap = PAS_COMPACT_PTR_INITIALIZER, \
        .config_kind = (passed_config).kind, \
    }

static PAS_ALWAYS_INLINE pas_allocation_result
pas_try_allocate_intrinsic_impl_casual_case(
    pas_heap* heap,
    size_t size,
    size_t alignment,
    pas_intrinsic_heap_support* intrinsic_support,
    pas_heap_config config,
    pas_try_allocate_common_fast try_allocate_common_fast,
    pas_try_allocate_common_slow try_allocate_common_slow,
    pas_intrinsic_heap_designation_mode designation_mode)
{
    static const bool verbose = false;
    
    size_t aligned_size;
    size_t index;
    pas_heap_ref fake_heap_ref;
    pas_designated_index_result designated_index;
    pas_thread_local_cache* cache;

    PAS_ASSERT(alignment == 1 || !designation_mode);

    if (verbose)
        pas_log("in impl_casual_case for %s\n", pas_heap_config_kind_get_string(config.kind));

    if (!pas_is_power_of_2(alignment))
        return pas_allocation_result_create_failure();

    if (PAS_UNLIKELY(pas_debug_heap_is_enabled(config.kind)))
        return pas_debug_heap_allocate(size, alignment);

    if (verbose)
        pas_log("not doing debug heap in impl_casual_case for %s\n", pas_heap_config_kind_get_string(config.kind));

    aligned_size = pas_try_allocate_compute_aligned_size(size, alignment);
    
    index = pas_segregated_heap_index_for_size(aligned_size, config);

    if (verbose) {
        pas_log("aligned_size = %zu, index = %zu, alignment = %zu.\n",
                aligned_size, index, alignment);
    }

    cache = pas_thread_local_cache_try_get();
    if (PAS_LIKELY(cache)) {
        unsigned allocator_index;
        pas_local_allocator_result allocator_result;
        pas_local_allocator* allocator;
        
        designated_index = pas_designated_intrinsic_heap_designated_index(
            index, designation_mode, config);
        if (PAS_LIKELY(designated_index.did_succeed)) {
            allocator_result = pas_local_allocator_result_create_success(
                pas_thread_local_cache_get_local_allocator_direct(
                    cache,
                    pas_designated_index_result_get_allocator_index(designated_index, config)));
        } else {
            if (PAS_UNLIKELY(index >= PAS_NUM_INTRINSIC_SIZE_CLASSES)) {
                allocator_index =
                    pas_segregated_heap_medium_allocator_index_for_index(
                        &heap->segregated_heap,
                        index,
                        pas_segregated_heap_medium_size_directory_search_within_size_class_progression,
                        pas_lock_is_not_held);
            } else
                allocator_index = intrinsic_support->index_to_allocator_index[index];
        
            if (verbose)
                pas_log("allocator_index = %u.\n", allocator_index);
        
            allocator_result = pas_thread_local_cache_get_local_allocator_for_possibly_uninitialized_index(
                cache, allocator_index, pas_lock_is_not_held);
        }

        allocator = (pas_local_allocator*)allocator_result.allocator;

        PAS_TESTING_ASSERT(!allocator_result.did_succeed || allocator->object_size >= aligned_size
                           || allocator->scavenger_data.kind == pas_local_allocator_decommitted_kind);
    
        /* This should be specialized out in the non-alignment case because of ALWAYS_INLINE and
           alignment being the constant 1. */
        if (alignment != 1 && allocator_result.did_succeed
            && alignment > pas_local_allocator_alignment(allocator)) {
            if (verbose) {
                pas_log("Discarding good allocator result because alignment is inadequate "
                        "(%zu, needed %zu).\n",
                        pas_local_allocator_alignment(allocator), alignment);
            }
            allocator_result.did_succeed = false;
        }

        if (PAS_LIKELY(allocator_result.did_succeed))
            return try_allocate_common_fast(allocator, aligned_size, alignment);
    }

    fake_heap_ref.type = heap->type;
    fake_heap_ref.heap = heap;
    fake_heap_ref.allocator_index = 0;

    return try_allocate_common_slow(&fake_heap_ref, aligned_size, alignment);
}

static PAS_ALWAYS_INLINE pas_allocation_result
pas_try_allocate_intrinsic_impl_inline_only(
    size_t size,
    size_t alignment,
    pas_intrinsic_heap_support* intrinsic_support,
    pas_heap_config config,
    pas_try_allocate_common_fast_inline_only try_allocate_common_fast_inline_only,
    pas_intrinsic_heap_designation_mode designation_mode)
{
    static const bool verbose = false;
    
    size_t aligned_size;
    size_t index;
    pas_designated_index_result designated_index;
    pas_thread_local_cache* cache;
    unsigned allocator_index;
    pas_local_allocator_result allocator_result;
    pas_local_allocator* allocator;

    PAS_ASSERT(alignment == 1 || !designation_mode);

    if (verbose)
        pas_log("in impl_inline_only for %s\n", pas_heap_config_kind_get_string(config.kind));

    if (!pas_is_power_of_2(alignment))
        return pas_allocation_result_create_failure();

    aligned_size = pas_try_allocate_compute_aligned_size(size, alignment);
    
    index = pas_segregated_heap_index_for_size(aligned_size, config);

    if (verbose) {
        pas_log("aligned_size = %zu, index = %zu, alignment = %zu.\n",
                aligned_size, index, alignment);
    }

    cache = pas_thread_local_cache_try_get();
    if (PAS_UNLIKELY(!cache))
        return pas_allocation_result_create_failure();
    
    designated_index = pas_designated_intrinsic_heap_designated_index(
        index, designation_mode, config);
    if (PAS_LIKELY(designated_index.did_succeed)) {
        allocator_result = pas_local_allocator_result_create_success(
            pas_thread_local_cache_get_local_allocator_direct_unchecked(
                cache,
                pas_designated_index_result_get_allocator_index(designated_index, config)));
    } else {
        if (PAS_UNLIKELY(index >= PAS_NUM_INTRINSIC_SIZE_CLASSES))
            return pas_allocation_result_create_failure();
        
        allocator_index = intrinsic_support->index_to_allocator_index[index];
        
        if (verbose)
            pas_log("allocator_index = %u.\n", allocator_index);
        
        allocator_result =
            pas_thread_local_cache_try_get_local_allocator_or_unselected_for_uninitialized_index(
                cache, allocator_index);

        if (PAS_UNLIKELY(!allocator_result.did_succeed))
            return pas_allocation_result_create_failure();
    }

    PAS_TESTING_ASSERT(allocator_result.did_succeed);
    
    allocator = (pas_local_allocator*)allocator_result.allocator;
    
    PAS_TESTING_ASSERT(
        !allocator_result.did_succeed
        || allocator->object_size >= aligned_size
        || allocator->config_kind == pas_local_allocator_config_kind_unselected
        || allocator->scavenger_data.kind == pas_local_allocator_decommitted_kind);
    
    /* This should be specialized out in the non-alignment case because of ALWAYS_INLINE and
       alignment being the constant 1. */
    if (alignment != 1 && alignment > pas_local_allocator_alignment(allocator)) {
        if (verbose) {
            pas_log("Discarding good allocator result because alignment is inadequate "
                    "(%zu, needed %zu).\n",
                    pas_local_allocator_alignment(allocator), alignment);
        }
        return pas_allocation_result_create_failure();
    }
    
    return try_allocate_common_fast_inline_only(allocator);
}

#define PAS_CREATE_TRY_ALLOCATE_INTRINSIC(name, heap_config, runtime_config, allocator_counts, result_filter, heap, heap_support, designation_mode) \
    PAS_CREATE_TRY_ALLOCATE_COMMON( \
        name ## _impl, \
        pas_fake_heap_ref_kind, \
        (heap_config), \
        (runtime_config), \
        (allocator_counts), \
        pas_force_size_lookup, \
        (result_filter)); \
    \
    static PAS_NEVER_INLINE pas_allocation_result \
    name ## _casual_case(size_t size, size_t alignment) \
    { \
        return pas_try_allocate_intrinsic_impl_casual_case( \
            (heap), size, alignment, (heap_support), (heap_config), \
            name ## _impl_fast, name ## _impl_slow, (designation_mode)); \
    } \
    \
    static PAS_ALWAYS_INLINE pas_allocation_result name ## _inline_only(size_t size, size_t alignment) \
    { \
        return pas_try_allocate_intrinsic_impl_inline_only( \
            size, alignment, (heap_support), (heap_config), \
            name ## _impl_fast_inline_only, (designation_mode)); \
    } \
    \
    static PAS_ALWAYS_INLINE pas_allocation_result name(size_t size, size_t alignment) \
    { \
        static const bool verbose = false; \
        pas_allocation_result result; \
        result = name ## _inline_only(size, alignment); \
        if (PAS_LIKELY(result.did_succeed)) { \
            if (verbose) \
                pas_log("Returning successful result (begin = %p)\n", (void*)result.begin); \
            return result; \
        } \
        return name ## _casual_case(size, alignment); \
    } \
    \
    static PAS_UNUSED PAS_NEVER_INLINE pas_allocation_result \
    name ## _for_realloc(size_t size) \
    { \
        static const bool verbose = false; \
        pas_allocation_result result = name(size, 1); \
        if (verbose) \
            pas_log("result.begin = %p\n", (void*)result.begin); \
        return result; \
    } \
    \
    struct pas_dummy

typedef pas_allocation_result (*pas_try_allocate_intrinsic)(size_t size,
                                                            size_t alignment);

typedef pas_allocation_result (*pas_try_allocate_intrinsic_for_realloc)(size_t size);

PAS_END_EXTERN_C;

#endif /* PAS_TRY_ALLOCATE_INTRINSIC_H */

