/*
 * Copyright (c) 2023 Apple Inc. All rights reserved.
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

#ifndef IR_HEAP_INLINES_H
#define IR_HEAP_INLINES_H

#include "pas_platform.h"

PAS_IGNORE_WARNINGS_BEGIN("missing-field-initializers")

#include "ir_heap.h"
#include "ir_heap_config.h"
#include "pas_deallocate.h"
#include "pas_try_allocate.h"
#include "pas_try_allocate_array.h"
#include "pas_try_allocate_intrinsic.h"
#include "pas_try_allocate_primitive.h"
#include "pas_try_reallocate.h"

#if PAS_ENABLE_JIT

PAS_BEGIN_EXTERN_C;

PAS_CREATE_TRY_ALLOCATE_INTRINSIC(
    ir_try_allocate_impl,
    IR_HEAP_CONFIG,
    &ir_heap_runtime_config,
    &ir_allocator_counts,
    pas_allocation_result_identity,
    &ir_common_primitive_heap,
    &ir_common_primitive_heap_support,
    pas_intrinsic_heap_is_designated);

/* Need to create a different set of allocation functions if we want to pass nontrivial alignment,
   since in that case we do not want to use the fancy lookup path. */
PAS_CREATE_TRY_ALLOCATE_INTRINSIC(
    ir_try_allocate_with_alignment_impl,
    IR_HEAP_CONFIG,
    &ir_heap_runtime_config,
    &ir_allocator_counts,
    pas_allocation_result_identity,
    &ir_common_primitive_heap,
    &ir_common_primitive_heap_support,
    pas_intrinsic_heap_is_not_designated);

PAS_CREATE_TRY_ALLOCATE_INTRINSIC(
    ir_allocate_impl,
    IR_HEAP_CONFIG,
    &ir_heap_runtime_config,
    &ir_allocator_counts,
    pas_allocation_result_crash_on_error,
    &ir_common_primitive_heap,
    &ir_common_primitive_heap_support,
    pas_intrinsic_heap_is_designated);

PAS_CREATE_TRY_ALLOCATE_INTRINSIC(
    ir_allocate_with_alignment_impl,
    IR_HEAP_CONFIG,
    &ir_heap_runtime_config,
    &ir_allocator_counts,
    pas_allocation_result_crash_on_error,
    &ir_common_primitive_heap,
    &ir_common_primitive_heap_support,
    pas_intrinsic_heap_is_not_designated);

static PAS_ALWAYS_INLINE void* ir_try_allocate_inline(size_t size)
{
    pas_allocation_result result;
    result = ir_try_allocate_impl_inline_only(size, 1);
    if (PAS_LIKELY(result.did_succeed))
        return (void*)result.begin;
    result = ir_try_allocate_impl(size, 1);
    if (PAS_LIKELY(result.did_succeed))
        return (void*)result.begin;
    return nullptr;
}

static PAS_ALWAYS_INLINE void*
ir_try_allocate_with_alignment_inline(size_t size, size_t alignment)
{
    return (void*)ir_try_allocate_with_alignment_impl(size, alignment).begin;
}

static PAS_ALWAYS_INLINE void* ir_try_allocate_zeroed_inline(size_t size)
{
    return (void*)pas_allocation_result_zero(
        ir_try_allocate_impl(size, 1),
        size).begin;
}

static PAS_ALWAYS_INLINE void* ir_allocate_inline(size_t size)
{
    pas_allocation_result result;
    result = ir_allocate_impl_inline_only(size, 1);
    if (PAS_LIKELY(result.did_succeed))
        return (void*)result.begin;
    result = ir_allocate_impl(size, 1);
    pas_allocation_result_crash_on_error(result);
    return (void*)result.begin;
}

static PAS_ALWAYS_INLINE void*
ir_allocate_with_alignment_inline(size_t size, size_t alignment)
{
    return (void*)ir_allocate_with_alignment_impl(size, alignment).begin;
}

static PAS_ALWAYS_INLINE void* ir_allocate_zeroed_inline(size_t size)
{
    return (void*)pas_allocation_result_zero(
        ir_allocate_impl(size, 1),
        size).begin;
}

static PAS_ALWAYS_INLINE void*
ir_try_reallocate_inline(void* old_ptr, size_t new_size,
                              pas_reallocate_free_mode free_mode)
{
    return (void*)pas_try_reallocate_intrinsic(
        old_ptr,
        &ir_common_primitive_heap,
        new_size,
        IR_HEAP_CONFIG,
        ir_try_allocate_impl_for_realloc,
        pas_reallocate_allow_heap_teleport,
        free_mode).begin;
}

static PAS_ALWAYS_INLINE void*
ir_reallocate_inline(void* old_ptr, size_t new_size,
                          pas_reallocate_free_mode free_mode)
{
    return (void*)pas_try_reallocate_intrinsic(
        old_ptr,
        &ir_common_primitive_heap,
        new_size,
        IR_HEAP_CONFIG,
        ir_allocate_impl_for_realloc,
        pas_reallocate_allow_heap_teleport,
        free_mode).begin;
}

static PAS_ALWAYS_INLINE void ir_deallocate_inline(void* ptr)
{
    pas_deallocate(ptr, IR_HEAP_CONFIG);
}

PAS_END_EXTERN_C;

#endif /* PAS_ENABLE_BMALLOC */

PAS_IGNORE_WARNINGS_END

#endif /* BMALLOC_HEAP_INLINES_H */

