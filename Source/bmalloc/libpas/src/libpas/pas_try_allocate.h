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

#ifndef PAS_TRY_ALLOCATE_H
#define PAS_TRY_ALLOCATE_H

#include "pas_allocator_counts.h"
#include "pas_cares_about_size_mode.h"
#include "pas_heap.h"
#include "pas_heap_config.h"
#include "pas_heap_ref.h"
#include "pas_local_allocator_inlines.h"
#include "pas_physical_memory_transaction.h"
#include "pas_segregated_heap.h"
#include "pas_thread_local_cache.h"
#include "pas_try_allocate_common.h"

PAS_BEGIN_EXTERN_C;

/* This is for heaps that hold typed objects. These objects have a certain size. We may allocate
   arrays of these objects, but the entrypoints in this header are for the case where we are allocating
   just a single instance. We my allocate these objects with any alignment, but the entrypoints in this
   header are for the case where we are allocating with the alignment that was specified in the
   heap_ref's type.

   If you want to allocate with nontrivial count (i.e. an array) or nontrivial alignment (i.e. memalign)
   then use the pas_try_allocate_array.h entrypoints. */

static PAS_ALWAYS_INLINE pas_allocation_result
pas_try_allocate_impl_casual_case(pas_heap_ref* heap_ref,
                                  pas_allocation_mode allocation_mode,
                                  pas_heap_config config,
                                  pas_try_allocate_common try_allocate_common)
{
    const pas_heap_type* type;
    size_t type_size;
    pas_local_allocator_result allocator;
    unsigned allocator_index;

    type = heap_ref->type;
    type_size = config.get_type_size(type);

    allocator_index = heap_ref->allocator_index;
    allocator = pas_thread_local_cache_get_local_allocator_if_can_set_cache_for_possibly_uninitialized_index(
        allocator_index, config.config_ptr);

    return try_allocate_common(heap_ref, type_size, 1, allocation_mode, allocator);
}

static PAS_ALWAYS_INLINE pas_allocation_result
pas_try_allocate_impl_inline_only(
    pas_heap_ref* heap_ref,
    pas_allocation_mode allocation_mode,
    pas_heap_config config,
    pas_try_allocate_common_fast_inline_only try_allocate_common_fast_inline_only)
{
    static const bool verbose = PAS_SHOULD_LOG(PAS_LOG_OTHER);
    
    pas_local_allocator_result allocator;
    unsigned allocator_index;
    pas_thread_local_cache* cache;
    pas_allocation_result result;

    allocator_index = heap_ref->allocator_index;
    cache = pas_thread_local_cache_try_get();
    if (PAS_UNLIKELY(!cache))
        return pas_allocation_result_create_failure();

    allocator = pas_thread_local_cache_try_get_local_allocator_or_unselected_for_uninitialized_index(
        cache, allocator_index);
    
    if (verbose)
        pas_log("Got an allocator.\n");
    
    if (PAS_UNLIKELY(!allocator.did_succeed)) {
        if (verbose) {
            pas_log("Failed to get allocator in try_allocate_impl_inline_only for type_size = %zu, "
                    "allocator_index = %u\n",
                    config.get_type_size(heap_ref->type), allocator_index);
        }
        return pas_allocation_result_create_failure();
    }
    
    result = try_allocate_common_fast_inline_only((pas_local_allocator*)allocator.allocator, allocation_mode);

    if (verbose) {
        pas_log("Returning from try_allocate_impl_inline_only for type_size = %zu, did_succeed = %s\n",
                config.get_type_size(heap_ref->type), result.did_succeed ? "yes" : "no");
    }
    
    return result;
}

#define PAS_CREATE_TRY_ALLOCATE(name, heap_config, runtime_config, allocator_counts, result_filter) \
    PAS_CREATE_TRY_ALLOCATE_COMMON( \
        name ## _impl, \
        pas_normal_heap_ref_kind, \
        (heap_config), \
        (runtime_config), \
        (allocator_counts), \
        pas_avoid_size_lookup, \
        (result_filter)); \
    \
    static PAS_NEVER_INLINE pas_allocation_result name ## _casual_case(pas_heap_ref* heap_ref, pas_allocation_mode allocation_mode) \
    { \
        return pas_try_allocate_impl_casual_case( \
            heap_ref, allocation_mode, (heap_config), name ## _impl); \
    } \
    \
    static PAS_ALWAYS_INLINE pas_allocation_result name ## _inline_only(pas_heap_ref* heap_ref, pas_allocation_mode allocation_mode) \
    { \
        return pas_try_allocate_impl_inline_only(heap_ref, allocation_mode, (heap_config), name ## _impl_fast_inline_only); \
    } \
    \
    static PAS_ALWAYS_INLINE pas_allocation_result name(pas_heap_ref* heap_ref, pas_allocation_mode allocation_mode) \
    { \
        pas_allocation_result result; \
        result = name ## _inline_only(heap_ref, allocation_mode); \
        if (PAS_LIKELY(result.did_succeed)) \
            return result; \
        return name ## _casual_case(heap_ref, allocation_mode); \
    } \
    \
    static PAS_UNUSED PAS_NEVER_INLINE pas_allocation_result \
    name ## _for_realloc(pas_heap_ref* heap_ref, pas_allocation_mode allocation_mode) \
    { \
        return name(heap_ref, allocation_mode); \
    } \
    \
    struct pas_dummy

typedef pas_allocation_result (*pas_try_allocate)(pas_heap_ref* heap_ref, pas_allocation_mode allocation_mode);

PAS_END_EXTERN_C;

#endif /* PAS_TRY_ALLOCATE_H */

