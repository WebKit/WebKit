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

#include "pas_config.h"

#if LIBPAS_ENABLED

#include "thingy_heap.h"

#if PAS_ENABLE_THINGY

#include "pas_deallocate.h"
#include "pas_get_allocation_size.h"
#include "pas_try_allocate.h"
#include "pas_try_allocate_array.h"
#include "pas_try_allocate_intrinsic.h"
#include "pas_try_reallocate.h"
#include "thingy_heap_config.h"

#define THINGY_TYPE_PRIMITIVE PAS_SIMPLE_TYPE_CREATE(1, 1)

pas_intrinsic_heap_support thingy_primitive_heap_support = PAS_INTRINSIC_HEAP_SUPPORT_INITIALIZER;

pas_heap thingy_primitive_heap =
    PAS_INTRINSIC_HEAP_INITIALIZER(
        &thingy_primitive_heap,
        THINGY_TYPE_PRIMITIVE,
        thingy_primitive_heap_support,
        THINGY_HEAP_CONFIG,
        &thingy_intrinsic_runtime_config.base);

pas_intrinsic_heap_support thingy_utility_heap_support = PAS_INTRINSIC_HEAP_SUPPORT_INITIALIZER;

pas_heap thingy_utility_heap =
    PAS_INTRINSIC_HEAP_INITIALIZER(
        &thingy_utility_heap,
        THINGY_TYPE_PRIMITIVE,
        thingy_utility_heap_support,
        THINGY_HEAP_CONFIG,
        &thingy_intrinsic_runtime_config.base);

pas_allocator_counts thingy_allocator_counts;

PAS_CREATE_TRY_ALLOCATE_INTRINSIC(
    try_allocate_primitive,
    THINGY_HEAP_CONFIG,
    &thingy_intrinsic_runtime_config.base,
    &thingy_allocator_counts,
    pas_allocation_result_identity,
    &thingy_primitive_heap,
    &thingy_primitive_heap_support,
    pas_intrinsic_heap_is_not_designated);

void* thingy_try_allocate_primitive(size_t size)
{
    return (void*)try_allocate_primitive(size, 1).begin;
}

void* thingy_try_allocate_primitive_zeroed(size_t size)
{
    return (void*)pas_allocation_result_zero(try_allocate_primitive(size, 1), size).begin;
}

void* thingy_try_allocate_primitive_with_alignment(size_t size, size_t alignment)
{
    return (void*)try_allocate_primitive(size, alignment).begin;
}

void* thingy_try_reallocate_primitive(void* old_ptr, size_t new_size)
{
    return (void*)pas_try_reallocate_intrinsic(
        old_ptr,
        &thingy_primitive_heap,
        new_size,
        THINGY_HEAP_CONFIG,
        try_allocate_primitive_for_realloc,
        pas_reallocate_disallow_heap_teleport,
        pas_reallocate_free_if_successful).begin;
}

PAS_CREATE_TRY_ALLOCATE(
    try_allocate,
    THINGY_HEAP_CONFIG,
    &thingy_typed_runtime_config.base,
    &thingy_allocator_counts,
    pas_allocation_result_identity);

__attribute__((malloc))
void* thingy_try_allocate(pas_heap_ref* heap_ref)
{
    return (void*)try_allocate(heap_ref).begin;
}

void* thingy_try_allocate_zeroed(pas_heap_ref* heap_ref)
{
    return (void*)pas_allocation_result_zero(
        try_allocate(heap_ref), THINGY_HEAP_CONFIG.get_type_size(heap_ref->type)).begin;
}

PAS_CREATE_TRY_ALLOCATE_ARRAY(
    try_allocate_array,
    THINGY_HEAP_CONFIG,
    &thingy_typed_runtime_config.base,
    &thingy_allocator_counts,
    pas_allocation_result_identity);

__attribute__((malloc))
void* thingy_try_allocate_array(pas_heap_ref* heap_ref, size_t count, size_t alignment)
{
    return (void*)try_allocate_array_by_count(heap_ref, count, alignment).begin;
}

void* thingy_try_allocate_zeroed_array(pas_heap_ref* heap_ref, size_t count, size_t alignment)
{
    size_t size;

    if (__builtin_mul_overflow(count, THINGY_HEAP_CONFIG.get_type_size(heap_ref->type), &size))
        return NULL;
    
    return (void*)pas_allocation_result_zero(
        try_allocate_array_by_size(heap_ref, size, alignment), size).begin;
}

size_t thingy_get_allocation_size(void* ptr)
{
    return pas_get_allocation_size(ptr, THINGY_HEAP_CONFIG);
}

void* thingy_try_reallocate_array(
    void* old_ptr, pas_heap_ref* heap_ref, size_t new_count)
{
    return (void*)pas_try_reallocate_array_by_count(old_ptr,
                                                    heap_ref,
                                                    new_count,
                                                    THINGY_HEAP_CONFIG,
                                                    try_allocate_array_for_realloc,
                                                    &thingy_typed_runtime_config.base,
                                                    pas_reallocate_disallow_heap_teleport,
                                                    pas_reallocate_free_if_successful).begin;
}

void thingy_deallocate(void* ptr)
{
    pas_deallocate(ptr, THINGY_HEAP_CONFIG);
}

pas_heap* thingy_heap_ref_get_heap(pas_heap_ref* heap_ref)
{
    return pas_ensure_heap(heap_ref, pas_normal_heap_ref_kind, &thingy_heap_config,
                           &thingy_typed_runtime_config.base);
}

PAS_CREATE_TRY_ALLOCATE_INTRINSIC(
    utility_heap_allocate,
    THINGY_HEAP_CONFIG,
    &thingy_intrinsic_runtime_config.base,
    &thingy_allocator_counts,
    pas_allocation_result_crash_on_error,
    &thingy_utility_heap,
    &thingy_utility_heap_support,
    pas_intrinsic_heap_is_not_designated);

void* thingy_utility_heap_allocate(size_t size)
{
    return (void*)utility_heap_allocate(size, 1).begin;
}

#endif /* PAS_ENABLE_THINGY */

#endif /* LIBPAS_ENABLED */
