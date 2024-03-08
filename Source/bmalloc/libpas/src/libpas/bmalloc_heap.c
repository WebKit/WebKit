/*
 * Copyright (c) 2019-2022 Apple Inc. All rights reserved.
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

#include "bmalloc_heap_inlines.h"
#include "pas_deallocate.h"
#include "pas_ensure_heap_forced_into_reserved_memory.h"
#include "pas_get_allocation_size.h"
#include "pas_get_heap.h"

PAS_BEGIN_EXTERN_C;

const bmalloc_type bmalloc_common_primitive_type = BMALLOC_TYPE_INITIALIZER(1, 1, "Common Primitive");

pas_intrinsic_heap_support bmalloc_common_primitive_heap_support =
    PAS_INTRINSIC_HEAP_SUPPORT_INITIALIZER;

pas_heap bmalloc_common_primitive_heap =
    PAS_INTRINSIC_HEAP_INITIALIZER(
        &bmalloc_common_primitive_heap,
        &bmalloc_common_primitive_type,
        bmalloc_common_primitive_heap_support,
        BMALLOC_HEAP_CONFIG,
        &bmalloc_intrinsic_runtime_config.base);

pas_allocator_counts bmalloc_allocator_counts;

PAS_NEVER_INLINE void* bmalloc_try_allocate_casual(size_t size, pas_allocation_mode allocation_mode)
{
    return (void*)bmalloc_try_allocate_impl_casual_case(size, 1, allocation_mode).begin;
}

PAS_NEVER_INLINE void* bmalloc_allocate_casual(size_t size, pas_allocation_mode allocation_mode)
{
    return (void*)bmalloc_allocate_impl_casual_case(size, 1, allocation_mode).begin;
}

void* bmalloc_try_allocate(size_t size, pas_allocation_mode allocation_mode)
{
    return bmalloc_try_allocate_inline(size, allocation_mode);
}

void* bmalloc_try_allocate_with_alignment(size_t size, size_t alignment, pas_allocation_mode allocation_mode)
{
    return bmalloc_try_allocate_with_alignment_inline(size, alignment, allocation_mode);
}

void* bmalloc_try_allocate_zeroed(size_t size, pas_allocation_mode allocation_mode)
{
    return bmalloc_try_allocate_zeroed_inline(size, allocation_mode);
}

void* bmalloc_allocate(size_t size, pas_allocation_mode allocation_mode)
{
    return bmalloc_allocate_inline(size, allocation_mode);
}

void* bmalloc_allocate_with_alignment(size_t size, size_t alignment, pas_allocation_mode allocation_mode)
{
    return bmalloc_allocate_with_alignment_inline(size, alignment, allocation_mode);
}

void* bmalloc_allocate_zeroed(size_t size, pas_allocation_mode allocation_mode)
{
    return bmalloc_allocate_zeroed_inline(size, allocation_mode);
}

void* bmalloc_try_reallocate(void* old_ptr, size_t new_size,
                             pas_allocation_mode allocation_mode,
                             pas_reallocate_free_mode free_mode)
{
    return bmalloc_try_reallocate_inline(old_ptr, new_size, allocation_mode, free_mode);
}

void* bmalloc_reallocate(void* old_ptr, size_t new_size,
                         pas_allocation_mode allocation_mode,
                         pas_reallocate_free_mode free_mode)
{
    return bmalloc_reallocate_inline(old_ptr, new_size, allocation_mode, free_mode);
}

PAS_NEVER_INLINE void* bmalloc_try_iso_allocate_casual(pas_heap_ref* heap_ref, pas_allocation_mode allocation_mode)
{
    return (void*)bmalloc_try_iso_allocate_impl_casual_case(heap_ref, allocation_mode).begin;
}

PAS_NEVER_INLINE void* bmalloc_iso_allocate_casual(pas_heap_ref* heap_ref, pas_allocation_mode allocation_mode)
{
    return (void*)bmalloc_iso_allocate_impl_casual_case(heap_ref, allocation_mode).begin;
}

void* bmalloc_try_iso_allocate(pas_heap_ref* heap_ref, pas_allocation_mode allocation_mode)
{
    return bmalloc_try_iso_allocate_inline(heap_ref, allocation_mode);
}

void* bmalloc_iso_allocate(pas_heap_ref* heap_ref, pas_allocation_mode allocation_mode)
{
    return bmalloc_iso_allocate_inline(heap_ref, allocation_mode);
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

void* bmalloc_try_allocate_flex_with_alignment_casual(
    pas_primitive_heap_ref* heap_ref, size_t size, size_t alignment, pas_allocation_mode allocation_mode)
{
    return (void*)bmalloc_try_allocate_flex_impl_casual_case(heap_ref, size, alignment, allocation_mode).begin;
}

void* bmalloc_allocate_flex_with_alignment_casual(
    pas_primitive_heap_ref* heap_ref, size_t size, size_t alignment, pas_allocation_mode allocation_mode)
{
    return (void*)bmalloc_allocate_flex_impl_casual_case(heap_ref, size, alignment, allocation_mode).begin;
}

void* bmalloc_try_allocate_flex(pas_primitive_heap_ref* heap_ref, size_t size, pas_allocation_mode allocation_mode)
{
    return bmalloc_try_allocate_flex_inline(heap_ref, size, allocation_mode);
}

void* bmalloc_allocate_flex(pas_primitive_heap_ref* heap_ref, size_t size, pas_allocation_mode allocation_mode)
{
    return bmalloc_allocate_flex_inline(heap_ref, size, allocation_mode);
}

void* bmalloc_try_allocate_zeroed_flex(pas_primitive_heap_ref* heap_ref, size_t size, pas_allocation_mode allocation_mode)
{
    return bmalloc_try_allocate_zeroed_flex_inline(heap_ref, size, allocation_mode);
}

void* bmalloc_allocate_zeroed_flex(pas_primitive_heap_ref* heap_ref, size_t size, pas_allocation_mode allocation_mode)
{
    return bmalloc_allocate_zeroed_flex_inline(heap_ref, size, allocation_mode);
}

void* bmalloc_try_allocate_flex_with_alignment(
    pas_primitive_heap_ref* heap_ref, size_t size, size_t alignment, pas_allocation_mode allocation_mode)
{
    return bmalloc_try_allocate_flex_with_alignment_inline(heap_ref, size, alignment, allocation_mode);
}

void* bmalloc_allocate_flex_with_alignment(
    pas_primitive_heap_ref* heap_ref, size_t size, size_t alignment, pas_allocation_mode allocation_mode)
{
    return bmalloc_allocate_flex_with_alignment_inline(heap_ref, size, alignment, allocation_mode);
}

void* bmalloc_try_reallocate_flex(pas_primitive_heap_ref* heap_ref, void* old_ptr, size_t new_size, pas_allocation_mode allocation_mode)
{
    return bmalloc_try_reallocate_flex_inline(heap_ref, old_ptr, new_size, allocation_mode);
}

void* bmalloc_reallocate_flex(pas_primitive_heap_ref* heap_ref, void* old_ptr, size_t new_size, pas_allocation_mode allocation_mode)
{
    return bmalloc_reallocate_flex_inline(heap_ref, old_ptr, new_size, allocation_mode);
}

pas_heap* bmalloc_flex_heap_ref_get_heap(pas_primitive_heap_ref* heap_ref)
{
    return pas_ensure_heap(&heap_ref->base, pas_primitive_heap_ref_kind,
                           &bmalloc_heap_config, &bmalloc_flex_runtime_config.base);
}

PAS_NEVER_INLINE void* bmalloc_try_allocate_auxiliary_with_alignment_casual(
    pas_primitive_heap_ref* heap_ref, size_t size, size_t alignment, pas_allocation_mode allocation_mode)
{
    return (void*)bmalloc_try_allocate_auxiliary_impl_casual_case(heap_ref, size, alignment, allocation_mode).begin;
}

PAS_NEVER_INLINE void* bmalloc_allocate_auxiliary_with_alignment_casual(
    pas_primitive_heap_ref* heap_ref, size_t size, size_t alignment, pas_allocation_mode allocation_mode)
{
    return (void*)bmalloc_allocate_auxiliary_impl_casual_case(heap_ref, size, alignment, allocation_mode).begin;
}

void* bmalloc_try_allocate_auxiliary(pas_primitive_heap_ref* heap_ref,
                                     size_t size,
                                     pas_allocation_mode allocation_mode)
{
    return bmalloc_try_allocate_auxiliary_inline(heap_ref, size, allocation_mode);
}

void* bmalloc_allocate_auxiliary(pas_primitive_heap_ref* heap_ref,
                                 size_t size,
                                 pas_allocation_mode allocation_mode)
{
    return bmalloc_allocate_auxiliary_inline(heap_ref, size, allocation_mode);
}

void* bmalloc_try_allocate_auxiliary_zeroed(pas_primitive_heap_ref* heap_ref,
                                            size_t size,
                                            pas_allocation_mode allocation_mode)
{
    return bmalloc_try_allocate_auxiliary_zeroed_inline(heap_ref, size, allocation_mode);
}

void* bmalloc_allocate_auxiliary_zeroed(pas_primitive_heap_ref* heap_ref,
                                        size_t size,
                                        pas_allocation_mode allocation_mode)
{
    return bmalloc_allocate_auxiliary_zeroed_inline(heap_ref, size, allocation_mode);
}

void* bmalloc_try_allocate_auxiliary_with_alignment(pas_primitive_heap_ref* heap_ref,
                                                    size_t size,
                                                    size_t alignment,
                                                    pas_allocation_mode allocation_mode)
{
    return bmalloc_try_allocate_auxiliary_with_alignment_inline(heap_ref, size, alignment, allocation_mode);
}

void* bmalloc_allocate_auxiliary_with_alignment(pas_primitive_heap_ref* heap_ref,
                                                size_t size,
                                                size_t alignment,
                                                pas_allocation_mode allocation_mode)
{
    return bmalloc_allocate_auxiliary_with_alignment_inline(heap_ref, size, alignment, allocation_mode);
}

void* bmalloc_try_reallocate_auxiliary(void* old_ptr,
                                       pas_primitive_heap_ref* heap_ref,
                                       size_t new_size,
                                       pas_allocation_mode allocation_mode,
                                       pas_reallocate_free_mode free_mode)
{
    return bmalloc_try_reallocate_auxiliary_inline(old_ptr, heap_ref, new_size, allocation_mode, free_mode);
}

void* bmalloc_reallocate_auxiliary(void* old_ptr,
                                   pas_primitive_heap_ref* heap_ref,
                                   size_t new_size,
                                   pas_allocation_mode allocation_mode,
                                   pas_reallocate_free_mode free_mode)
{
    return bmalloc_reallocate_auxiliary_inline(old_ptr, heap_ref, new_size, allocation_mode, free_mode);
}

pas_heap* bmalloc_auxiliary_heap_ref_get_heap(pas_primitive_heap_ref* heap_ref)
{
    return pas_ensure_heap(&heap_ref->base, pas_primitive_heap_ref_kind,
                           &bmalloc_heap_config, &bmalloc_primitive_runtime_config.base);
}

void bmalloc_deallocate(void* ptr)
{
    bmalloc_deallocate_inline(ptr);
}

pas_heap* bmalloc_force_auxiliary_heap_into_reserved_memory(pas_primitive_heap_ref* heap_ref,
                                                            uintptr_t begin,
                                                            uintptr_t end)
{
    return pas_ensure_heap_forced_into_reserved_memory(
        &heap_ref->base, pas_primitive_heap_ref_kind, &bmalloc_heap_config,
        &bmalloc_primitive_runtime_config.base, begin, end);
}

size_t bmalloc_heap_ref_get_type_size(pas_heap_ref* heap_ref)
{
    return BMALLOC_HEAP_CONFIG.get_type_size(heap_ref->type);
}

size_t bmalloc_get_allocation_size(void* ptr)
{
    return pas_get_allocation_size(ptr, BMALLOC_HEAP_CONFIG);
}

pas_heap* bmalloc_get_heap(void* ptr)
{
    return pas_get_heap(ptr, BMALLOC_HEAP_CONFIG);
}

PAS_END_EXTERN_C;

#endif /* PAS_ENABLE_BMALLOC */

#endif /* LIBPAS_ENABLED */
