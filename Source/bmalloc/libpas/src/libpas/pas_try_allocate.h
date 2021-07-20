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
#include "pas_typed_allocation_result.h"

PAS_BEGIN_EXTERN_C;

typedef struct {
    pas_heap_ref* heap_ref;
    pas_heap_config config;
} pas_try_allocate_impl_size_thunk_data;

static PAS_ALWAYS_INLINE size_t pas_try_allocate_impl_size_thunk(void* arg)
{
    pas_try_allocate_impl_size_thunk_data* data;

    data = (pas_try_allocate_impl_size_thunk_data*)arg;

    return data->config.get_type_size(data->heap_ref->type);
}

static PAS_ALWAYS_INLINE pas_typed_allocation_result
pas_try_allocate_impl(pas_heap_ref* heap_ref,
                      pas_heap_config config,
                      pas_try_allocate_common_fast try_allocate_common_fast,
                      pas_try_allocate_common_slow try_allocate_common_slow)
{
    pas_heap_type* type;
    size_t type_size;
    pas_local_allocator_result allocator;
    unsigned allocator_index;
    pas_thread_local_cache* cache;

    allocator_index = heap_ref->allocator_index;
    cache = pas_thread_local_cache_try_get();
    if (PAS_LIKELY(cache)) {
        allocator = pas_thread_local_cache_try_get_local_allocator(
            cache,
            allocator_index);
        if (PAS_LIKELY(allocator.did_succeed)) {
            pas_intrinsic_allocation_result result;
            pas_try_allocate_impl_size_thunk_data size_thunk_data;
            
            /* This thing with the size thunk means that isoheaps usually don't have to get the
               heap's type or size, unless we are in a heap config that uses the .type or .size
               properties of typed allocation results. */
            size_thunk_data.config = config;
            size_thunk_data.heap_ref = heap_ref;

            result = try_allocate_common_fast(
                allocator.allocator, pas_try_allocate_impl_size_thunk, &size_thunk_data, 1);

            type = heap_ref->type;
            return pas_typed_allocation_result_create_with_intrinsic_allocation_result(
                result, type, config.get_type_size(type));
        }
    }

    type = heap_ref->type;
    type_size = config.get_type_size(type);

    return pas_typed_allocation_result_create_with_intrinsic_allocation_result(
        try_allocate_common_slow(heap_ref, 1, type_size, 1),
        type, type_size);
}

#define PAS_CREATE_TRY_ALLOCATE(name, heap_config, runtime_config, allocator_counts, result_filter) \
    PAS_CREATE_TRY_ALLOCATE_COMMON( \
        name ## _impl, \
        pas_normal_heap_ref_kind, \
        (heap_config), \
        (runtime_config), \
        (allocator_counts), \
        pas_avoid_count_lookup, \
        (result_filter)); \
    \
    static PAS_ALWAYS_INLINE pas_typed_allocation_result name(pas_heap_ref* heap_ref) \
    { \
        return pas_try_allocate_impl( \
            heap_ref, (heap_config), name ## _impl_fast, name ## _impl_slow); \
    } \
    \
    static PAS_UNUSED PAS_NEVER_INLINE pas_typed_allocation_result \
    name ## _for_realloc(pas_heap_ref* heap_ref) \
    { \
        return name(heap_ref); \
    } \
    \
    struct pas_dummy

typedef pas_typed_allocation_result (*pas_try_allocate)(pas_heap_ref* heap_ref);

PAS_END_EXTERN_C;

#endif /* PAS_TRY_ALLOCATE_H */

