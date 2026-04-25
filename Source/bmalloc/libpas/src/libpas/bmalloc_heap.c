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

PAS_BEGIN_EXTERN_C;

PAS_NEVER_INLINE void* bmalloc_try_allocate_casual(size_t size, pas_allocation_mode allocation_mode)
{
    if (allocation_mode == pas_always_compact_allocation_mode)
        return (void*)bmalloc_try_allocate_auxiliary(&bmalloc_compact_primitive_heap_ref, size, allocation_mode);
    return (void*)bmalloc_try_allocate_impl_casual_case(size, 1, allocation_mode).begin;
}

PAS_NEVER_INLINE void* bmalloc_allocate_casual(size_t size, pas_allocation_mode allocation_mode)
{
    if (allocation_mode == pas_always_compact_allocation_mode)
        return (void*)bmalloc_allocate_auxiliary(&bmalloc_compact_primitive_heap_ref, size, allocation_mode);
    return (void*)bmalloc_allocate_impl_casual_case(size, 1, allocation_mode).begin;
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

#if !(defined(PAS_BMALLOC_HIDDEN) && PAS_BMALLOC_HIDDEN)

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

void* bmalloc_try_allocate_zeroed_with_alignment(size_t size, size_t alignment, pas_allocation_mode allocation_mode)
{
    return bmalloc_try_allocate_zeroed_with_alignment_inline(size, alignment, allocation_mode);
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

void* bmalloc_allocate_zeroed_with_alignment(size_t size, size_t alignment, pas_allocation_mode allocation_mode)
{
    return bmalloc_allocate_zeroed_with_alignment_inline(size, alignment, allocation_mode);
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

void* bmalloc_try_allocate_auxiliary_zeroed_with_alignment(pas_primitive_heap_ref* heap_ref,
                                                           size_t size,
                                                           size_t alignment,
                                                           pas_allocation_mode allocation_mode)
{
    return bmalloc_try_allocate_auxiliary_zeroed_with_alignment_inline(heap_ref, size, alignment, allocation_mode);
}

void* bmalloc_allocate_auxiliary_zeroed_with_alignment(pas_primitive_heap_ref* heap_ref,
                                                       size_t size,
                                                       size_t alignment,
                                                       pas_allocation_mode allocation_mode)
{
    return bmalloc_allocate_auxiliary_zeroed_with_alignment_inline(heap_ref, size, alignment, allocation_mode);
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

void bmalloc_deallocate(void* ptr)
{
    bmalloc_deallocate_inline(ptr);
}

#endif /* !PAS_BMALLOC_HIDDEN */

PAS_END_EXTERN_C;

#endif /* PAS_ENABLE_BMALLOC */

#endif /* LIBPAS_ENABLED */
