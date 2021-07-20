/*
 * Copyright (c) 2019-2021 Apple Inc. All rights reserved.
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

pas_intrinsic_heap_support bmalloc_common_primitive_heap_support =
    PAS_INTRINSIC_HEAP_SUPPORT_INITIALIZER;

pas_heap bmalloc_common_primitive_heap =
    PAS_INTRINSIC_PRIMITIVE_HEAP_INITIALIZER(
        &bmalloc_common_primitive_heap,
        PAS_SIMPLE_TYPE_CREATE(1, 1),
        bmalloc_common_primitive_heap_support,
        BMALLOC_HEAP_CONFIG,
        &bmalloc_intrinsic_primitive_runtime_config.base);

pas_allocator_counts bmalloc_allocator_counts;

void* bmalloc_try_allocate(size_t size)
{
    return bmalloc_try_allocate_inline(size);
}

void* bmalloc_try_allocate_with_alignment(size_t size, size_t alignment)
{
    return bmalloc_try_allocate_with_alignment_inline(size, alignment);
}

void* bmalloc_try_allocate_zeroed(size_t size)
{
    return bmalloc_try_allocate_zeroed_inline(size);
}

void* bmalloc_allocate(size_t size)
{
    return bmalloc_allocate_inline(size);
}

void* bmalloc_allocate_with_alignment(size_t size, size_t alignment)
{
    return bmalloc_allocate_with_alignment_inline(size, alignment);
}

void* bmalloc_allocate_zeroed(size_t size)
{
    return bmalloc_allocate_zeroed_inline(size);
}

void* bmalloc_try_reallocate(void* old_ptr, size_t new_size,
                             pas_reallocate_free_mode free_mode)
{
    return bmalloc_try_reallocate_inline(old_ptr, new_size, free_mode);
}

void* bmalloc_reallocate(void* old_ptr, size_t new_size,
                         pas_reallocate_free_mode free_mode)
{
    return bmalloc_reallocate_inline(old_ptr, new_size, free_mode);
}

void* bmalloc_try_iso_allocate(pas_heap_ref* heap_ref)
{
    return bmalloc_try_iso_allocate_inline(heap_ref);
}

void* bmalloc_iso_allocate(pas_heap_ref* heap_ref)
{
    return bmalloc_iso_allocate_inline(heap_ref);
}

pas_heap* bmalloc_heap_ref_get_heap(pas_heap_ref* heap_ref)
{
    return pas_ensure_heap(heap_ref, pas_normal_heap_ref_kind,
                           &bmalloc_heap_config, &bmalloc_typed_runtime_config.base);
}

void* bmalloc_try_allocate_auxiliary(pas_primitive_heap_ref* heap_ref,
                                     size_t size)
{
    return bmalloc_try_allocate_auxiliary_inline(heap_ref, size);
}

void* bmalloc_allocate_auxiliary(pas_primitive_heap_ref* heap_ref,
                                 size_t size)
{
    return bmalloc_allocate_auxiliary_inline(heap_ref, size);
}

void* bmalloc_try_allocate_auxiliary_zeroed(pas_primitive_heap_ref* heap_ref,
                                            size_t size)
{
    return bmalloc_try_allocate_auxiliary_zeroed_inline(heap_ref, size);
}

void* bmalloc_allocate_auxiliary_zeroed(pas_primitive_heap_ref* heap_ref,
                                        size_t size)
{
    return bmalloc_allocate_auxiliary_zeroed_inline(heap_ref, size);
}

void* bmalloc_try_allocate_auxiliary_with_alignment(pas_primitive_heap_ref* heap_ref,
                                                    size_t size,
                                                    size_t alignment)
{
    return bmalloc_try_allocate_auxiliary_with_alignment_inline(heap_ref, size, alignment);
}

void* bmalloc_allocate_auxiliary_with_alignment(pas_primitive_heap_ref* heap_ref,
                                                size_t size,
                                                size_t alignment)
{
    return bmalloc_allocate_auxiliary_with_alignment_inline(heap_ref, size, alignment);
}

void* bmalloc_try_reallocate_auxiliary(void* old_ptr,
                                       pas_primitive_heap_ref* heap_ref,
                                       size_t new_size,
                                       pas_reallocate_free_mode free_mode)
{
    return bmalloc_try_reallocate_auxiliary_inline(old_ptr, heap_ref, new_size, free_mode);
}

void* bmalloc_reallocate_auxiliary(void* old_ptr,
                                   pas_primitive_heap_ref* heap_ref,
                                   size_t new_size,
                                   pas_reallocate_free_mode free_mode)
{
    return bmalloc_reallocate_auxiliary_inline(old_ptr, heap_ref, new_size, free_mode);
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

#endif /* PAS_ENABLE_BMALLOC */

#endif /* LIBPAS_ENABLED */
