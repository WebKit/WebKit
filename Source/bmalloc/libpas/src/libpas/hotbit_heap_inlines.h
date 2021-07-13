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

#ifndef HOTBIT_HEAP_INLINES_H
#define HOTBIT_HEAP_INLINES_H

#include "hotbit_heap.h"
#include "hotbit_heap_config.h"
#include "hotbit_heap_innards.h"
#include "pas_deallocate.h"
#include "pas_try_allocate_intrinsic_primitive.h"
#include "pas_try_reallocate.h"

#if PAS_ENABLE_HOTBIT

PAS_BEGIN_EXTERN_C;

PAS_CREATE_TRY_ALLOCATE_INTRINSIC_PRIMITIVE(
    hotbit_try_allocate_impl,
    HOTBIT_HEAP_CONFIG,
    &hotbit_intrinsic_primitive_runtime_config.base,
    &hotbit_allocator_counts,
    pas_intrinsic_allocation_result_identity,
    &hotbit_common_primitive_heap,
    &hotbit_common_primitive_heap_support,
    pas_intrinsic_heap_is_designated);

/* Need to create a different set of allocation functions if we want to pass nontrivial alignment,
   since in that case we do not want to use the fancy lookup path. */
PAS_CREATE_TRY_ALLOCATE_INTRINSIC_PRIMITIVE(
    hotbit_try_allocate_with_alignment_impl,
    HOTBIT_HEAP_CONFIG,
    &hotbit_intrinsic_primitive_runtime_config.base,
    &hotbit_allocator_counts,
    pas_intrinsic_allocation_result_identity,
    &hotbit_common_primitive_heap,
    &hotbit_common_primitive_heap_support,
    pas_intrinsic_heap_is_not_designated);

static PAS_ALWAYS_INLINE void* hotbit_try_allocate_inline(size_t size)
{
    return hotbit_try_allocate_impl(size, 1).ptr;
}

static PAS_ALWAYS_INLINE void*
hotbit_try_allocate_with_alignment_inline(size_t size, size_t alignment)
{
    return hotbit_try_allocate_with_alignment_impl(size, alignment).ptr;
}

static PAS_ALWAYS_INLINE void*
hotbit_try_reallocate_inline(void* old_ptr, size_t new_size,
                              pas_reallocate_free_mode free_mode)
{
    return pas_try_reallocate_intrinsic_primitive(
        old_ptr,
        &hotbit_common_primitive_heap,
        new_size,
        HOTBIT_HEAP_CONFIG,
        hotbit_try_allocate_impl_for_realloc,
        pas_reallocate_allow_heap_teleport,
        free_mode).ptr;
}

static PAS_ALWAYS_INLINE void hotbit_deallocate_inline(void* ptr)
{
    pas_deallocate(ptr, HOTBIT_HEAP_CONFIG);
}

PAS_END_EXTERN_C;

#endif /* PAS_ENABLE_HOTBIT */

#endif /* HOTBIT_HEAP_INLINES_H */

