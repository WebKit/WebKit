/*
 * Copyright (c) 2024-2026 Apple Inc. All rights reserved.
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

#include "bmalloc_heap.h"

#if PAS_ENABLE_BMALLOC

#include "bmalloc_heap_iso_internal.h"

PAS_BEGIN_EXTERN_C;

PAS_NEVER_INLINE void* bmalloc_try_iso_allocate_casual(pas_heap_ref* heap_ref, pas_allocation_mode allocation_mode)
{
    return (void*)bmalloc_try_iso_allocate_impl_casual_case(heap_ref, allocation_mode).begin;
}

PAS_NEVER_INLINE void* bmalloc_iso_allocate_casual(pas_heap_ref* heap_ref, pas_allocation_mode allocation_mode)
{
    return (void*)bmalloc_iso_allocate_impl_casual_case(heap_ref, allocation_mode).begin;
}

PAS_NEVER_INLINE void* bmalloc_try_allocate_array_by_size_with_alignment_casual(
    pas_heap_ref* heap_ref, size_t size, size_t alignment, pas_allocation_mode allocation_mode)
{
    return (void*)bmalloc_try_iso_allocate_array_impl_casual_case(heap_ref, size, alignment, allocation_mode).begin;
}

PAS_NEVER_INLINE void* bmalloc_allocate_array_by_size_with_alignment_casual(
    pas_heap_ref* heap_ref, size_t size, size_t alignment, pas_allocation_mode allocation_mode)
{
    return (void*)bmalloc_iso_allocate_array_impl_casual_case(heap_ref, size, alignment, allocation_mode).begin;
}

#if !(defined(PAS_BMALLOC_HIDDEN) && PAS_BMALLOC_HIDDEN)

void* bmalloc_try_iso_allocate(pas_heap_ref* heap_ref, pas_allocation_mode allocation_mode)
{
    return bmalloc_try_iso_allocate_inline(heap_ref, allocation_mode);
}

void* bmalloc_iso_allocate(pas_heap_ref* heap_ref, pas_allocation_mode allocation_mode)
{
    return bmalloc_iso_allocate_inline(heap_ref, allocation_mode);
}

void* bmalloc_try_iso_allocate_array_by_size(pas_heap_ref* heap_ref, size_t size, pas_allocation_mode allocation_mode)
{
    return bmalloc_try_iso_allocate_array_by_size_inline(heap_ref, size, allocation_mode);
}

void* bmalloc_iso_allocate_array_by_size(pas_heap_ref* heap_ref, size_t size, pas_allocation_mode allocation_mode)
{
    return bmalloc_iso_allocate_array_by_size_inline(heap_ref, size, allocation_mode);
}

void* bmalloc_try_iso_allocate_zeroed_array_by_size(pas_heap_ref* heap_ref, size_t size, pas_allocation_mode allocation_mode)
{
    return bmalloc_try_iso_allocate_zeroed_array_by_size_inline(heap_ref, size, allocation_mode);
}

void* bmalloc_iso_allocate_zeroed_array_by_size(pas_heap_ref* heap_ref, size_t size, pas_allocation_mode allocation_mode)
{
    return bmalloc_iso_allocate_zeroed_array_by_size_inline(heap_ref, size, allocation_mode);
}

void* bmalloc_try_iso_allocate_array_by_size_with_alignment(
    pas_heap_ref* heap_ref, size_t size, size_t alignment, pas_allocation_mode allocation_mode)
{
    return bmalloc_try_iso_allocate_array_by_size_with_alignment_inline(heap_ref, size, alignment, allocation_mode);
}

void* bmalloc_iso_allocate_array_by_size_with_alignment(
    pas_heap_ref* heap_ref, size_t size, size_t alignment, pas_allocation_mode allocation_mode)
{
    return bmalloc_iso_allocate_array_by_size_with_alignment_inline(heap_ref, size, alignment, allocation_mode);
}

void* bmalloc_try_iso_reallocate_array_by_size(pas_heap_ref* heap_ref, void* ptr, size_t size, pas_allocation_mode allocation_mode)
{
    return bmalloc_try_iso_reallocate_array_by_size_inline(heap_ref, ptr, size, allocation_mode);
}

void* bmalloc_iso_reallocate_array_by_size(pas_heap_ref* heap_ref, void* ptr, size_t size, pas_allocation_mode allocation_mode)
{
    return bmalloc_iso_reallocate_array_by_size_inline(heap_ref, ptr, size, allocation_mode);
}

void* bmalloc_try_iso_allocate_array_by_count(pas_heap_ref* heap_ref, size_t count, pas_allocation_mode allocation_mode)
{
    return bmalloc_try_iso_allocate_array_by_count_inline(heap_ref, count, allocation_mode);
}

void* bmalloc_iso_allocate_array_by_count(pas_heap_ref* heap_ref, size_t count, pas_allocation_mode allocation_mode)
{
    return bmalloc_iso_allocate_array_by_count_inline(heap_ref, count, allocation_mode);
}

void* bmalloc_try_iso_allocate_array_by_count_with_alignment(
    pas_heap_ref* heap_ref, size_t count, size_t alignment, pas_allocation_mode allocation_mode)
{
    return bmalloc_try_iso_allocate_array_by_count_with_alignment_inline(heap_ref, count, alignment, allocation_mode);
}

void* bmalloc_iso_allocate_array_by_count_with_alignment(
    pas_heap_ref* heap_ref, size_t count, size_t alignment, pas_allocation_mode allocation_mode)
{
    return bmalloc_iso_allocate_array_by_count_with_alignment_inline(heap_ref, count, alignment, allocation_mode);
}

void* bmalloc_try_iso_reallocate_array_by_count(pas_heap_ref* heap_ref, void* ptr, size_t count, pas_allocation_mode allocation_mode)
{
    return bmalloc_try_iso_reallocate_array_by_count_inline(heap_ref, ptr, count, allocation_mode);
}

void* bmalloc_iso_reallocate_array_by_count(pas_heap_ref* heap_ref, void* ptr, size_t count, pas_allocation_mode allocation_mode)
{
    return bmalloc_iso_reallocate_array_by_count_inline(heap_ref, ptr, count, allocation_mode);
}

pas_heap* bmalloc_heap_ref_get_heap(pas_heap_ref* heap_ref)
{
    return pas_ensure_heap(heap_ref, pas_normal_heap_ref_kind,
                           &bmalloc_heap_config, &bmalloc_typed_runtime_config.base);
}

#endif /* !PAS_BMALLOC_HIDDEN */

PAS_END_EXTERN_C;

#endif /* PAS_ENABLE_BMALLOC */

#endif /* LIBPAS_ENABLED */
