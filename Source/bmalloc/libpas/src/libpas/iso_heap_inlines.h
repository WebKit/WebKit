/*
 * Copyright (c) 2021 Apple Inc. All rights reserved.
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

#ifndef ISO_HEAP_INLINES_H
#define ISO_HEAP_INLINES_H

#include "iso_heap.h"
#include "iso_heap_config.h"
#include "iso_heap_innards.h"
#include "pas_deallocate.h"
#include "pas_get_allocation_size.h"
#include "pas_get_heap.h"
#include "pas_has_object.h"
#include "pas_try_allocate.h"
#include "pas_try_allocate_array.h"
#include "pas_try_allocate_intrinsic_primitive.h"
#include "pas_try_allocate_primitive.h"
#include "pas_try_reallocate.h"

#if PAS_ENABLE_ISO

PAS_BEGIN_EXTERN_C;

PAS_CREATE_TRY_ALLOCATE_INTRINSIC_PRIMITIVE(
    iso_try_allocate_common_primitive_impl,
    ISO_HEAP_CONFIG,
    &iso_intrinsic_primitive_runtime_config.base,
    &iso_allocator_counts,
    pas_intrinsic_allocation_result_set_errno,
    &iso_common_primitive_heap,
    &iso_common_primitive_heap_support,
    pas_intrinsic_heap_is_not_designated);

/* Need to create a different set of allocation functions if we want to pass nontrivial alignment,
   since in that case we do not want to use the fancy lookup path. */
PAS_CREATE_TRY_ALLOCATE_INTRINSIC_PRIMITIVE(
    iso_try_allocate_common_primitive_with_alignment_impl,
    ISO_HEAP_CONFIG,
    &iso_intrinsic_primitive_runtime_config.base,
    &iso_allocator_counts,
    pas_intrinsic_allocation_result_set_errno,
    &iso_common_primitive_heap,
    &iso_common_primitive_heap_support,
    pas_intrinsic_heap_is_not_designated);

PAS_CREATE_TRY_ALLOCATE_INTRINSIC_PRIMITIVE(
    iso_allocate_common_primitive_impl,
    ISO_HEAP_CONFIG,
    &iso_intrinsic_primitive_runtime_config.base,
    &iso_allocator_counts,
    pas_intrinsic_allocation_result_crash_on_error,
    &iso_common_primitive_heap,
    &iso_common_primitive_heap_support,
    pas_intrinsic_heap_is_not_designated);

PAS_CREATE_TRY_ALLOCATE_INTRINSIC_PRIMITIVE(
    iso_allocate_common_primitive_with_alignment_impl,
    ISO_HEAP_CONFIG,
    &iso_intrinsic_primitive_runtime_config.base,
    &iso_allocator_counts,
    pas_intrinsic_allocation_result_crash_on_error,
    &iso_common_primitive_heap,
    &iso_common_primitive_heap_support,
    pas_intrinsic_heap_is_not_designated);

static PAS_ALWAYS_INLINE void* iso_try_allocate_common_primitive_inline(size_t size)
{
    return iso_try_allocate_common_primitive_impl(size, 1).ptr;
}

static PAS_ALWAYS_INLINE void*
iso_try_allocate_common_primitive_with_alignment_inline(size_t size, size_t alignment)
{
    return iso_try_allocate_common_primitive_with_alignment_impl(size, alignment).ptr;
}

static PAS_ALWAYS_INLINE void* iso_try_allocate_common_primitive_zeroed_inline(size_t size)
{
    return pas_intrinsic_allocation_result_zero(
        iso_try_allocate_common_primitive_impl(size, 1),
        size).ptr;
}

static PAS_ALWAYS_INLINE void* iso_allocate_common_primitive_inline(size_t size)
{
    return iso_allocate_common_primitive_impl(size, 1).ptr;
}

static PAS_ALWAYS_INLINE void*
iso_allocate_common_primitive_with_alignment_inline(size_t size, size_t alignment)
{
    return iso_allocate_common_primitive_with_alignment_impl(size, alignment).ptr;
}

static PAS_ALWAYS_INLINE void* iso_allocate_common_primitive_zeroed_inline(size_t size)
{
    return pas_intrinsic_allocation_result_zero(
        iso_allocate_common_primitive_impl(size, 1),
        size).ptr;
}

static PAS_ALWAYS_INLINE void*
iso_try_reallocate_common_primitive_inline(void* old_ptr, size_t new_size,
                                           pas_reallocate_free_mode free_mode)
{
    return pas_try_reallocate_intrinsic_primitive(
        old_ptr,
        &iso_common_primitive_heap,
        new_size,
        ISO_HEAP_CONFIG,
        iso_try_allocate_common_primitive_impl_for_realloc,
        pas_reallocate_allow_heap_teleport,
        free_mode).ptr;
}

static PAS_ALWAYS_INLINE void*
iso_reallocate_common_primitive_inline(void* old_ptr, size_t new_size,
                                       pas_reallocate_free_mode free_mode)
{
    return pas_try_reallocate_intrinsic_primitive(
        old_ptr,
        &iso_common_primitive_heap,
        new_size,
        ISO_HEAP_CONFIG,
        iso_allocate_common_primitive_impl_for_realloc,
        pas_reallocate_allow_heap_teleport,
        free_mode).ptr;
}

PAS_CREATE_TRY_ALLOCATE(
    iso_try_allocate_impl,
    ISO_HEAP_CONFIG,
    &iso_typed_runtime_config.base,
    &iso_allocator_counts,
    pas_intrinsic_allocation_result_set_errno);

static PAS_ALWAYS_INLINE void* iso_try_allocate_inline(pas_heap_ref* heap_ref)
{
    return iso_try_allocate_impl(heap_ref).ptr;
}

PAS_CREATE_TRY_ALLOCATE(
    iso_allocate_impl,
    ISO_HEAP_CONFIG,
    &iso_typed_runtime_config.base,
    &iso_allocator_counts,
    pas_intrinsic_allocation_result_crash_on_error);

static PAS_ALWAYS_INLINE void* iso_allocate_inline(pas_heap_ref* heap_ref)
{
    return iso_allocate_impl(heap_ref).ptr;
}

PAS_CREATE_TRY_ALLOCATE_ARRAY(
    iso_try_allocate_array_impl,
    ISO_HEAP_CONFIG,
    &iso_typed_runtime_config.base,
    &iso_allocator_counts,
    pas_intrinsic_allocation_result_set_errno);

static PAS_ALWAYS_INLINE void*
iso_try_allocate_array_inline(pas_heap_ref* heap_ref, size_t count, size_t alignment)
{
    return iso_try_allocate_array_impl(heap_ref, count, alignment).ptr;
}

PAS_CREATE_TRY_ALLOCATE_ARRAY(
    iso_allocate_array_impl,
    ISO_HEAP_CONFIG,
    &iso_typed_runtime_config.base,
    &iso_allocator_counts,
    pas_intrinsic_allocation_result_crash_on_error);

static PAS_ALWAYS_INLINE void*
iso_allocate_array_inline(pas_heap_ref* heap_ref, size_t count, size_t alignment)
{
    return iso_allocate_array_impl(heap_ref, count, alignment).ptr;
}

static PAS_ALWAYS_INLINE void* iso_try_allocate_array_zeroed_inline(
    pas_heap_ref* heap_ref, size_t count, size_t alignment)
{
    return pas_typed_allocation_result_zero(
        iso_try_allocate_array_impl(heap_ref, count, alignment)).ptr;
}

static PAS_ALWAYS_INLINE void* iso_allocate_array_zeroed_inline(
    pas_heap_ref* heap_ref, size_t count, size_t alignment)
{
    return pas_typed_allocation_result_zero(iso_allocate_array_impl(heap_ref, count, alignment)).ptr;
}

static PAS_ALWAYS_INLINE void* iso_try_reallocate_array_inline(void* old_ptr, pas_heap_ref* heap_ref,
                                                               size_t new_count,
                                                               pas_reallocate_free_mode free_mode)
{
    return pas_try_reallocate_array(
        old_ptr,
        heap_ref,
        new_count,
        ISO_HEAP_CONFIG,
        iso_try_allocate_array_impl_for_realloc,
        &iso_typed_runtime_config.base,
        pas_reallocate_allow_heap_teleport,
        free_mode).ptr;
}

static PAS_ALWAYS_INLINE void* iso_reallocate_array_inline(void* old_ptr, pas_heap_ref* heap_ref,
                                                           size_t new_count,
                                                           pas_reallocate_free_mode free_mode)
{
    return pas_try_reallocate_array(
        old_ptr,
        heap_ref,
        new_count,
        ISO_HEAP_CONFIG,
        iso_allocate_array_impl_for_realloc,
        &iso_typed_runtime_config.base,
        pas_reallocate_allow_heap_teleport,
        free_mode).ptr;
}

PAS_CREATE_TRY_ALLOCATE_PRIMITIVE(
    iso_try_allocate_primitive_impl,
    ISO_HEAP_CONFIG,
    &iso_primitive_runtime_config.base,
    &iso_allocator_counts,
    pas_intrinsic_allocation_result_set_errno);

static PAS_ALWAYS_INLINE void* iso_try_allocate_primitive_inline(pas_primitive_heap_ref* heap_ref,
                                                                 size_t size)
{
    return iso_try_allocate_primitive_impl(heap_ref, size, 1).ptr;
}

PAS_CREATE_TRY_ALLOCATE_PRIMITIVE(
    iso_allocate_primitive_impl,
    ISO_HEAP_CONFIG,
    &iso_primitive_runtime_config.base,
    &iso_allocator_counts,
    pas_intrinsic_allocation_result_crash_on_error);

static PAS_ALWAYS_INLINE void* iso_allocate_primitive_inline(pas_primitive_heap_ref* heap_ref,
                                                             size_t size)
{
    return iso_allocate_primitive_impl(heap_ref, size, 1).ptr;
}

static PAS_ALWAYS_INLINE void* iso_try_allocate_primitive_zeroed_inline(pas_primitive_heap_ref* heap_ref,
                                                                        size_t size)
{
    return pas_intrinsic_allocation_result_zero(
        iso_try_allocate_primitive_impl(heap_ref, size, 1),
        size).ptr;
}

static PAS_ALWAYS_INLINE void* iso_allocate_primitive_zeroed_inline(pas_primitive_heap_ref* heap_ref,
                                                                    size_t size)
{
    return pas_intrinsic_allocation_result_zero(
        iso_allocate_primitive_impl(heap_ref, size, 1),
        size).ptr;
}

static PAS_ALWAYS_INLINE void*
iso_try_allocate_primitive_with_alignment_inline(pas_primitive_heap_ref* heap_ref,
                                                 size_t size,
                                                 size_t alignment)
{
    return iso_try_allocate_primitive_impl(heap_ref, size, alignment).ptr;
}

static PAS_ALWAYS_INLINE void*
iso_allocate_primitive_with_alignment_inline(pas_primitive_heap_ref* heap_ref,
                                             size_t size,
                                             size_t alignment)
{
    return iso_allocate_primitive_impl(heap_ref, size, alignment).ptr;
}

static PAS_ALWAYS_INLINE void* iso_try_reallocate_primitive_inline(void* old_ptr,
                                                                   pas_primitive_heap_ref* heap_ref,
                                                                   size_t new_size,
                                                                   pas_reallocate_free_mode free_mode)
{
    return pas_try_reallocate_primitive(
        old_ptr,
        heap_ref,
        new_size,
        ISO_HEAP_CONFIG,
        iso_try_allocate_primitive_impl_for_realloc,
        &iso_primitive_runtime_config.base,
        pas_reallocate_allow_heap_teleport,
        free_mode).ptr;
}

static PAS_ALWAYS_INLINE void* iso_reallocate_primitive_inline(void* old_ptr,
                                                               pas_primitive_heap_ref* heap_ref,
                                                               size_t new_size,
                                                               pas_reallocate_free_mode free_mode)
{
    return pas_try_reallocate_primitive(
        old_ptr,
        heap_ref,
        new_size,
        ISO_HEAP_CONFIG,
        iso_allocate_primitive_impl_for_realloc,
        &iso_primitive_runtime_config.base,
        pas_reallocate_allow_heap_teleport,
        free_mode).ptr;
}

PAS_CREATE_TRY_ALLOCATE_PRIMITIVE(
    iso_try_allocate_for_objc_impl,
    ISO_HEAP_CONFIG,
    &iso_objc_runtime_config.base,
    &iso_allocator_counts,
    pas_intrinsic_allocation_result_set_errno);

static PAS_ALWAYS_INLINE void* iso_try_allocate_for_objc_inline(const void* cls, size_t size)
{
    return pas_intrinsic_allocation_result_zero(
        iso_try_allocate_for_objc_impl(
            pas_dynamic_primitive_heap_map_find(
                &iso_objc_dynamic_heap_map, cls, size),
            size, 1),
        size).ptr;
}

static PAS_ALWAYS_INLINE bool iso_has_object_inline(void* ptr)
{
    return pas_has_object(ptr, ISO_HEAP_CONFIG);
}

static PAS_ALWAYS_INLINE size_t iso_get_allocation_size_inline(void* ptr)
{
    return pas_get_allocation_size(ptr, ISO_HEAP_CONFIG);
}

static PAS_ALWAYS_INLINE pas_heap* iso_get_heap_inline(void* ptr)
{
    return pas_get_heap(ptr, ISO_HEAP_CONFIG);
}

static PAS_ALWAYS_INLINE void iso_deallocate_inline(void* ptr)
{
    pas_deallocate(ptr, ISO_HEAP_CONFIG);
}

PAS_END_EXTERN_C;

#endif /* PAS_ENABLE_ISO */

#endif /* ISO_HEAP_INLINES_H */

