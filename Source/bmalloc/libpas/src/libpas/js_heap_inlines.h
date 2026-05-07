/*
 * Copyright (c) 2026 Apple Inc. All rights reserved.
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

#ifndef JS_HEAP_INLINES_H
#define JS_HEAP_INLINES_H

#include "pas_config.h"

#if LIBPAS_ENABLED

PAS_IGNORE_WARNINGS_BEGIN("missing-field-initializers")

#include "js_heap.h"
#include "js_heap_config.h"
#include "js_heap_innards.h"
#include "pas_deallocate.h"
#include "pas_try_allocate_primitive.h"
#include "pas_try_reallocate.h"

#if PAS_ENABLE_BMALLOC

PAS_BEGIN_EXTERN_C;

PAS_CREATE_TRY_ALLOCATE_PRIMITIVE(
    js_try_allocate_impl,
    JS_HEAP_CONFIG,
    &js_primitive_runtime_config.base,
    &js_allocator_counts,
    pas_allocation_result_identity);

PAS_CREATE_TRY_ALLOCATE_PRIMITIVE(
    js_allocate_impl,
    JS_HEAP_CONFIG,
    &js_primitive_runtime_config.base,
    &js_allocator_counts,
    pas_allocation_result_crash_on_error);

PAS_API void* js_try_allocate_with_alignment_casual(
    pas_primitive_heap_ref* heap_ref, size_t size, size_t alignment, pas_allocation_mode allocation_mode);
PAS_API void* js_allocate_with_alignment_casual(
    pas_primitive_heap_ref* heap_ref, size_t size, size_t alignment, pas_allocation_mode allocation_mode);

static PAS_ALWAYS_INLINE void* js_try_allocate_inline(pas_primitive_heap_ref* heap_ref,
                                                      size_t size,
                                                      pas_allocation_mode allocation_mode)
{
    pas_allocation_result result;
    result = js_try_allocate_impl_inline_only(heap_ref, size, 1, allocation_mode);
    if (PAS_LIKELY(result.did_succeed))
        return (void*)result.begin;
    return js_try_allocate_with_alignment_casual(heap_ref, size, 1, allocation_mode);
}

static PAS_ALWAYS_INLINE void* js_allocate_inline(pas_primitive_heap_ref* heap_ref,
                                                  size_t size,
                                                  pas_allocation_mode allocation_mode)
{
    pas_allocation_result result;
    result = js_allocate_impl_inline_only(heap_ref, size, 1, allocation_mode);
    if (PAS_LIKELY(result.did_succeed))
        return (void*)result.begin;
    return js_allocate_with_alignment_casual(heap_ref, size, 1, allocation_mode);
}

static PAS_ALWAYS_INLINE void* js_try_allocate_zeroed_inline(
    pas_primitive_heap_ref* heap_ref,
    size_t size,
    pas_allocation_mode allocation_mode)
{
    pas_allocation_result result;
    result = js_try_allocate_impl(heap_ref, size, 1, allocation_mode);
    return (void*)pas_allocation_result_zero(result, size).begin;
}

static PAS_ALWAYS_INLINE void* js_allocate_zeroed_inline(pas_primitive_heap_ref* heap_ref,
                                                         size_t size,
                                                         pas_allocation_mode allocation_mode)
{
    pas_allocation_result result;
    result = js_allocate_impl(heap_ref, size, 1, allocation_mode);
    return (void*)pas_allocation_result_zero(result, size).begin;
}

static PAS_ALWAYS_INLINE void*
js_try_allocate_with_alignment_inline(pas_primitive_heap_ref* heap_ref,
                                      size_t size,
                                      size_t alignment,
                                      pas_allocation_mode allocation_mode)
{
    pas_allocation_result result;
    result = js_try_allocate_impl_inline_only(heap_ref, size, alignment, allocation_mode);
    if (PAS_LIKELY(result.did_succeed))
        return (void*)result.begin;
    return js_try_allocate_with_alignment_casual(heap_ref, size, alignment, allocation_mode);
}

static PAS_ALWAYS_INLINE void*
js_allocate_with_alignment_inline(pas_primitive_heap_ref* heap_ref,
                                  size_t size,
                                  size_t alignment,
                                  pas_allocation_mode allocation_mode)
{
    pas_allocation_result result;
    result = js_allocate_impl_inline_only(heap_ref, size, alignment, allocation_mode);
    if (PAS_LIKELY(result.did_succeed))
        return (void*)result.begin;
    return js_allocate_with_alignment_casual(heap_ref, size, alignment, allocation_mode);
}

static PAS_ALWAYS_INLINE void*
js_try_allocate_zeroed_with_alignment_inline(pas_primitive_heap_ref* heap_ref,
                                             size_t size,
                                             size_t alignment,
                                             pas_allocation_mode allocation_mode)
{
    pas_allocation_result result;
    result = js_try_allocate_impl_inline_only(heap_ref, size, alignment, allocation_mode);
    if (PAS_LIKELY(result.did_succeed))
        return (void*)pas_allocation_result_zero(result, size).begin;
    return (void*)pas_allocation_result_zero(
        js_try_allocate_impl_casual_case(heap_ref, size, alignment, allocation_mode),
        size).begin;
}

static PAS_ALWAYS_INLINE void*
js_allocate_zeroed_with_alignment_inline(pas_primitive_heap_ref* heap_ref,
                                         size_t size,
                                         size_t alignment,
                                         pas_allocation_mode allocation_mode)
{
    pas_allocation_result result;
    result = js_allocate_impl_inline_only(heap_ref, size, alignment, allocation_mode);
    if (PAS_LIKELY(result.did_succeed))
        return (void*)(pas_allocation_result_zero(result, size).begin);
    return (void*)pas_allocation_result_zero(
        js_allocate_impl_casual_case(heap_ref, size, alignment, allocation_mode),
        size).begin;
}

static PAS_ALWAYS_INLINE void* js_try_reallocate_inline(
    void* old_ptr,
    pas_primitive_heap_ref* heap_ref,
    size_t new_size,
    pas_allocation_mode allocation_mode,
    pas_reallocate_free_mode free_mode)
{
    return (void*)pas_try_reallocate_primitive(
        old_ptr,
        heap_ref,
        new_size,
        allocation_mode,
        JS_HEAP_CONFIG,
        js_try_allocate_impl_for_realloc,
        &js_primitive_runtime_config.base,
        pas_reallocate_allow_heap_teleport,
        free_mode).begin;
}

static PAS_ALWAYS_INLINE void* js_reallocate_inline(void* old_ptr,
                                                    pas_primitive_heap_ref* heap_ref,
                                                    size_t new_size,
                                                    pas_allocation_mode allocation_mode,
                                                    pas_reallocate_free_mode free_mode)
{
    return (void*)pas_try_reallocate_primitive(
        old_ptr,
        heap_ref,
        new_size,
        allocation_mode,
        JS_HEAP_CONFIG,
        js_allocate_impl_for_realloc,
        &js_primitive_runtime_config.base,
        pas_reallocate_allow_heap_teleport,
        free_mode).begin;
}

static PAS_ALWAYS_INLINE void js_deallocate_inline(void* ptr)
{
    pas_deallocate(ptr, JS_HEAP_CONFIG);
}

PAS_END_EXTERN_C;

#endif /* PAS_ENABLE_BMALLOC */

PAS_IGNORE_WARNINGS_END

#endif /* LIBPAS_ENABLED */
#endif /* JS_HEAP_INLINES_H */
