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

#include "pas_config.h"

#if LIBPAS_ENABLED

#include "js_heap.h"

#if PAS_ENABLE_BMALLOC

#include "js_heap_inlines.h"

PAS_BEGIN_EXTERN_C;

PAS_NEVER_INLINE void* js_try_allocate_with_alignment_casual(
    pas_primitive_heap_ref* heap_ref, size_t size, size_t alignment, pas_allocation_mode allocation_mode)
{
    return (void*)js_try_allocate_impl_casual_case(heap_ref, size, alignment, allocation_mode).begin;
}

PAS_NEVER_INLINE void* js_allocate_with_alignment_casual(
    pas_primitive_heap_ref* heap_ref, size_t size, size_t alignment, pas_allocation_mode allocation_mode)
{
    return (void*)js_allocate_impl_casual_case(heap_ref, size, alignment, allocation_mode).begin;
}

void* js_try_allocate(pas_primitive_heap_ref* heap_ref,
                      size_t size,
                      pas_allocation_mode allocation_mode)
{
    return js_try_allocate_inline(heap_ref, size, allocation_mode);
}

void* js_allocate(pas_primitive_heap_ref* heap_ref,
                  size_t size,
                  pas_allocation_mode allocation_mode)
{
    return js_allocate_inline(heap_ref, size, allocation_mode);
}

void* js_try_allocate_zeroed(pas_primitive_heap_ref* heap_ref,
                             size_t size,
                             pas_allocation_mode allocation_mode)
{
    return js_try_allocate_zeroed_inline(heap_ref, size, allocation_mode);
}

void* js_allocate_zeroed(pas_primitive_heap_ref* heap_ref,
                         size_t size,
                         pas_allocation_mode allocation_mode)
{
    return js_allocate_zeroed_inline(heap_ref, size, allocation_mode);
}

void* js_try_allocate_with_alignment(pas_primitive_heap_ref* heap_ref,
                                     size_t size,
                                     size_t alignment,
                                     pas_allocation_mode allocation_mode)
{
    return js_try_allocate_with_alignment_inline(heap_ref, size, alignment, allocation_mode);
}

void* js_allocate_with_alignment(pas_primitive_heap_ref* heap_ref,
                                 size_t size,
                                 size_t alignment,
                                 pas_allocation_mode allocation_mode)
{
    return js_allocate_with_alignment_inline(heap_ref, size, alignment, allocation_mode);
}

void* js_try_allocate_zeroed_with_alignment(pas_primitive_heap_ref* heap_ref,
                                            size_t size,
                                            size_t alignment,
                                            pas_allocation_mode allocation_mode)
{
    return js_try_allocate_zeroed_with_alignment_inline(heap_ref, size, alignment, allocation_mode);
}

void* js_allocate_zeroed_with_alignment(pas_primitive_heap_ref* heap_ref,
                                        size_t size,
                                        size_t alignment,
                                        pas_allocation_mode allocation_mode)
{
    return js_allocate_zeroed_with_alignment_inline(heap_ref, size, alignment, allocation_mode);
}

void* js_try_reallocate(void* old_ptr,
                        pas_primitive_heap_ref* heap_ref,
                        size_t new_size,
                        pas_allocation_mode allocation_mode,
                        pas_reallocate_free_mode free_mode)
{
    return js_try_reallocate_inline(old_ptr, heap_ref, new_size, allocation_mode, free_mode);
}

void* js_reallocate(void* old_ptr,
                    pas_primitive_heap_ref* heap_ref,
                    size_t new_size,
                    pas_allocation_mode allocation_mode,
                    pas_reallocate_free_mode free_mode)
{
    return js_reallocate_inline(old_ptr, heap_ref, new_size, allocation_mode, free_mode);
}

void js_deallocate(void* ptr)
{
    js_deallocate_inline(ptr);
}

PAS_END_EXTERN_C;

#endif /* PAS_ENABLE_BMALLOC */

#endif /* LIBPAS_ENABLED */
