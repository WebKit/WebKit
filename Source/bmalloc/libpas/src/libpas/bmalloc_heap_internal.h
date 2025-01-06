/*
 * Copyright (c) 2024 Apple Inc. All rights reserved.
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

#ifndef BMALLOC_HEAP_INTERNAL_H
#define BMALLOC_HEAP_INTERNAL_H

#include "pas_platform.h"

#include "bmalloc_heap.h"

#if PAS_ENABLE_BMALLOC

PAS_BEGIN_EXTERN_C;

PAS_CREATE_TRY_ALLOCATE_ARRAY(
    bmalloc_try_iso_allocate_array_impl,
    BMALLOC_HEAP_CONFIG,
    &bmalloc_typed_runtime_config.base,
    &bmalloc_allocator_counts,
    pas_allocation_result_identity);

PAS_CREATE_TRY_ALLOCATE_ARRAY(
    bmalloc_iso_allocate_array_impl,
    BMALLOC_HEAP_CONFIG,
    &bmalloc_typed_runtime_config.base,
    &bmalloc_allocator_counts,
    pas_allocation_result_crash_on_error);

PAS_API void* bmalloc_try_allocate_array_by_size_with_alignment_casual(
    pas_heap_ref* heap_ref, size_t size, size_t alignment, pas_allocation_mode allocation_mode);
PAS_API void* bmalloc_allocate_array_by_size_with_alignment_casual(
    pas_heap_ref* heap_ref, size_t size, size_t alignment, pas_allocation_mode allocation_mode);

static PAS_ALWAYS_INLINE void* bmalloc_try_iso_allocate_inline(pas_heap_ref* heap_ref, pas_allocation_mode allocation_mode)
{
    pas_allocation_result result;
    result = bmalloc_try_iso_allocate_impl_inline_only(heap_ref, allocation_mode);
    if (PAS_LIKELY(result.did_succeed))
        return (void*)result.begin;
    return bmalloc_try_iso_allocate_casual(heap_ref, allocation_mode);
}

static PAS_ALWAYS_INLINE void* bmalloc_iso_allocate_inline(pas_heap_ref* heap_ref, pas_allocation_mode allocation_mode)
{
    pas_allocation_result result;
    result = bmalloc_iso_allocate_impl_inline_only(heap_ref, allocation_mode);
    if (PAS_LIKELY(result.did_succeed))
        return (void*)result.begin;
    return bmalloc_iso_allocate_casual(heap_ref, allocation_mode);
}

static PAS_ALWAYS_INLINE void* bmalloc_try_iso_allocate_array_by_size_inline(
    pas_heap_ref* heap_ref, size_t size, pas_allocation_mode allocation_mode)
{
    pas_allocation_result result;
    result = bmalloc_try_iso_allocate_array_impl_inline_only(heap_ref, size, 1, allocation_mode);
    if (PAS_LIKELY(result.did_succeed))
        return (void*)result.begin;
    return bmalloc_try_allocate_array_by_size_with_alignment_casual(heap_ref, size, 1, allocation_mode);
}

static PAS_ALWAYS_INLINE void* bmalloc_try_iso_allocate_zeroed_array_by_size_inline(
    pas_heap_ref* heap_ref, size_t size, pas_allocation_mode allocation_mode)
{
    return (void*)pas_allocation_result_zero(
        bmalloc_try_iso_allocate_array_impl_by_size(heap_ref, size, 1, allocation_mode),
        size).begin;
}

static PAS_ALWAYS_INLINE void* bmalloc_try_iso_allocate_array_by_size_with_alignment_inline(
    pas_heap_ref* heap_ref, size_t size, size_t alignment, pas_allocation_mode allocation_mode)
{
    pas_allocation_result result;
    result = bmalloc_try_iso_allocate_array_impl_inline_only(heap_ref, size, alignment, allocation_mode);
    if (PAS_LIKELY(result.did_succeed))
        return (void*)result.begin;
    return bmalloc_try_allocate_array_by_size_with_alignment_casual(heap_ref, size, alignment, allocation_mode);
}

static PAS_ALWAYS_INLINE void* bmalloc_try_iso_allocate_array_by_count_inline(
    pas_heap_ref* heap_ref, size_t count, pas_allocation_mode allocation_mode)
{
    return (void*)bmalloc_try_iso_allocate_array_impl_by_count(heap_ref, count, 1, allocation_mode).begin;
}

static PAS_ALWAYS_INLINE void* bmalloc_try_iso_allocate_array_by_count_with_alignment_inline(
    pas_heap_ref* heap_ref, size_t count, size_t alignment, pas_allocation_mode allocation_mode)
{
    return (void*)bmalloc_try_iso_allocate_array_impl_by_count(heap_ref, count, alignment, allocation_mode).begin;
}

static PAS_ALWAYS_INLINE void* bmalloc_iso_allocate_array_by_size_inline(
    pas_heap_ref* heap_ref, size_t size, pas_allocation_mode allocation_mode)
{
    pas_allocation_result result;
    result = bmalloc_iso_allocate_array_impl_inline_only(heap_ref, size, 1, allocation_mode);
    if (PAS_LIKELY(result.did_succeed))
        return (void*)result.begin;
    return bmalloc_allocate_array_by_size_with_alignment_casual(heap_ref, size, 1, allocation_mode);
}

static PAS_ALWAYS_INLINE void* bmalloc_iso_allocate_zeroed_array_by_size_inline(
    pas_heap_ref* heap_ref, size_t size, pas_allocation_mode allocation_mode)
{
    return (void*)pas_allocation_result_zero(
        bmalloc_iso_allocate_array_impl_by_size(heap_ref, size, 1, allocation_mode),
        size).begin;
}

static PAS_ALWAYS_INLINE void* bmalloc_iso_allocate_array_by_size_with_alignment_inline(
    pas_heap_ref* heap_ref, size_t size, size_t alignment, pas_allocation_mode allocation_mode)
{
    pas_allocation_result result;
    result = bmalloc_iso_allocate_array_impl_inline_only(heap_ref, size, alignment, allocation_mode);
    if (PAS_LIKELY(result.did_succeed))
        return (void*)result.begin;
    return bmalloc_allocate_array_by_size_with_alignment_casual(heap_ref, size, alignment, allocation_mode);
}

static PAS_ALWAYS_INLINE void* bmalloc_iso_allocate_array_by_count_inline(
    pas_heap_ref* heap_ref, size_t count, pas_allocation_mode allocation_mode)
{
    return (void*)bmalloc_iso_allocate_array_impl_by_count(heap_ref, count, 1, allocation_mode).begin;
}

static PAS_ALWAYS_INLINE void* bmalloc_iso_allocate_array_by_count_with_alignment_inline(
    pas_heap_ref* heap_ref, size_t count, size_t alignment, pas_allocation_mode allocation_mode)
{
    return (void*)bmalloc_iso_allocate_array_impl_by_count(heap_ref, count, alignment, allocation_mode).begin;
}


static PAS_ALWAYS_INLINE void* bmalloc_try_iso_reallocate_array_by_size_inline(
    pas_heap_ref* heap_ref, void* ptr, size_t size, pas_allocation_mode allocation_mode)
{
    return (void*)pas_try_reallocate_array_by_size(
        ptr,
        heap_ref,
        size,
        allocation_mode,
        BMALLOC_HEAP_CONFIG,
        bmalloc_try_iso_allocate_array_impl_for_realloc,
        &bmalloc_typed_runtime_config.base,
        pas_reallocate_disallow_heap_teleport,
        pas_reallocate_free_if_successful).begin;
}

static PAS_ALWAYS_INLINE void* bmalloc_iso_reallocate_array_by_size_inline(
    pas_heap_ref* heap_ref, void* ptr, size_t size, pas_allocation_mode allocation_mode)
{
    return (void*)pas_try_reallocate_array_by_size(
        ptr,
        heap_ref,
        size,
        allocation_mode,
        BMALLOC_HEAP_CONFIG,
        bmalloc_iso_allocate_array_impl_for_realloc,
        &bmalloc_typed_runtime_config.base,
        pas_reallocate_disallow_heap_teleport,
        pas_reallocate_free_if_successful).begin;
}

static PAS_ALWAYS_INLINE void* bmalloc_try_iso_reallocate_array_by_count_inline(
    pas_heap_ref* heap_ref, void* ptr, size_t count, pas_allocation_mode allocation_mode)
{
    return (void*)pas_try_reallocate_array_by_count(
        ptr,
        heap_ref,
        count,
        allocation_mode,
        BMALLOC_HEAP_CONFIG,
        bmalloc_try_iso_allocate_array_impl_for_realloc,
        &bmalloc_typed_runtime_config.base,
        pas_reallocate_disallow_heap_teleport,
        pas_reallocate_free_if_successful).begin;
}

static PAS_ALWAYS_INLINE void* bmalloc_iso_reallocate_array_by_count_inline(
    pas_heap_ref* heap_ref, void* ptr, size_t count, pas_allocation_mode allocation_mode)
{
    return (void*)pas_try_reallocate_array_by_count(
        ptr,
        heap_ref,
        count,
        allocation_mode,
        BMALLOC_HEAP_CONFIG,
        bmalloc_iso_allocate_array_impl_for_realloc,
        &bmalloc_typed_runtime_config.base,
        pas_reallocate_disallow_heap_teleport,
        pas_reallocate_free_if_successful).begin;
}

PAS_CREATE_TRY_ALLOCATE_PRIMITIVE(
    bmalloc_try_allocate_flex_impl,
    BMALLOC_HEAP_CONFIG,
    &bmalloc_flex_runtime_config.base,
    &bmalloc_allocator_counts,
    pas_allocation_result_identity);

PAS_CREATE_TRY_ALLOCATE_PRIMITIVE(
    bmalloc_allocate_flex_impl,
    BMALLOC_HEAP_CONFIG,
    &bmalloc_flex_runtime_config.base,
    &bmalloc_allocator_counts,
    pas_allocation_result_crash_on_error);

PAS_API void* bmalloc_try_allocate_flex_with_alignment_casual(
    pas_primitive_heap_ref* heap_ref, size_t size, size_t alignment, pas_allocation_mode allocation_mode);
PAS_API void* bmalloc_allocate_flex_with_alignment_casual(
    pas_primitive_heap_ref* heap_ref, size_t size, size_t alignment, pas_allocation_mode allocation_mode);

static PAS_ALWAYS_INLINE void* bmalloc_try_allocate_flex_inline(
    pas_primitive_heap_ref* heap_ref, size_t size, pas_allocation_mode allocation_mode)
{
    pas_allocation_result result;
    result = bmalloc_try_allocate_flex_impl_inline_only(heap_ref, size, 1, allocation_mode);
    if (PAS_LIKELY(result.did_succeed))
        return (void*)result.begin;
    return bmalloc_try_allocate_flex_with_alignment_casual(heap_ref, size, 1, allocation_mode);
}

static PAS_ALWAYS_INLINE void* bmalloc_allocate_flex_inline(
    pas_primitive_heap_ref* heap_ref, size_t size, pas_allocation_mode allocation_mode)
{
    pas_allocation_result result;
    result = bmalloc_allocate_flex_impl_inline_only(heap_ref, size, 1, allocation_mode);
    if (PAS_LIKELY(result.did_succeed))
        return (void*)result.begin;
    return bmalloc_allocate_flex_with_alignment_casual(heap_ref, size, 1, allocation_mode);
}

static PAS_ALWAYS_INLINE void* bmalloc_try_allocate_zeroed_flex_inline(
    pas_primitive_heap_ref* heap_ref, size_t size, pas_allocation_mode allocation_mode)
{
    return (void*)pas_allocation_result_zero(
        bmalloc_try_allocate_flex_impl(heap_ref, size, 1, allocation_mode),
        size).begin;
}

static PAS_ALWAYS_INLINE void* bmalloc_allocate_zeroed_flex_inline(
    pas_primitive_heap_ref* heap_ref, size_t size, pas_allocation_mode allocation_mode)
{
    return (void*)pas_allocation_result_zero(
        bmalloc_allocate_flex_impl(heap_ref, size, 1, allocation_mode),
        size).begin;
}

static PAS_ALWAYS_INLINE void* bmalloc_try_allocate_flex_with_alignment_inline(
    pas_primitive_heap_ref* heap_ref, size_t size, size_t alignment, pas_allocation_mode allocation_mode)
{
    pas_allocation_result result;
    result = bmalloc_try_allocate_flex_impl_inline_only(heap_ref, size, alignment, allocation_mode);
    if (PAS_LIKELY(result.did_succeed))
        return (void*)result.begin;
    return bmalloc_try_allocate_flex_with_alignment_casual(heap_ref, size, alignment, allocation_mode);
}

static PAS_ALWAYS_INLINE void* bmalloc_allocate_flex_with_alignment_inline(
    pas_primitive_heap_ref* heap_ref, size_t size, size_t alignment, pas_allocation_mode allocation_mode)
{
    pas_allocation_result result;
    result = bmalloc_allocate_flex_impl_inline_only(heap_ref, size, alignment, allocation_mode);
    if (PAS_LIKELY(result.did_succeed))
        return (void*)result.begin;
    return bmalloc_allocate_flex_with_alignment_casual(heap_ref, size, alignment, allocation_mode);
}

static PAS_ALWAYS_INLINE void* bmalloc_try_reallocate_flex_inline(
    pas_primitive_heap_ref* heap_ref, void* old_ptr, size_t new_size, pas_allocation_mode allocation_mode)
{
    return (void*)pas_try_reallocate_primitive(
        old_ptr,
        heap_ref,
        new_size,
        allocation_mode,
        BMALLOC_HEAP_CONFIG,
        bmalloc_try_allocate_flex_impl_for_realloc,
        &bmalloc_flex_runtime_config.base,
        pas_reallocate_disallow_heap_teleport,
        pas_reallocate_free_if_successful).begin;
}

static PAS_ALWAYS_INLINE void* bmalloc_reallocate_flex_inline(
    pas_primitive_heap_ref* heap_ref, void* old_ptr, size_t new_size, pas_allocation_mode allocation_mode)
{
    return (void*)pas_try_reallocate_primitive(
        old_ptr,
        heap_ref,
        new_size,
        allocation_mode,
        BMALLOC_HEAP_CONFIG,
        bmalloc_allocate_flex_impl_for_realloc,
        &bmalloc_flex_runtime_config.base,
        pas_reallocate_disallow_heap_teleport,
        pas_reallocate_free_if_successful).begin;
}

PAS_END_EXTERN_C;

#endif /* PAS_ENABLE_BMALLOC */

#endif /* BMALLOC_HEAP_INTERNAL_H */
