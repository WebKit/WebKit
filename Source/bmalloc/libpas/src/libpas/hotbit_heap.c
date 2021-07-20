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

#include "hotbit_heap.h"

#if PAS_ENABLE_HOTBIT

#include "hotbit_heap_inlines.h"
#include "pas_deallocate.h"

pas_intrinsic_heap_support hotbit_common_primitive_heap_support =
    PAS_INTRINSIC_HEAP_SUPPORT_INITIALIZER;

pas_heap hotbit_common_primitive_heap =
    PAS_INTRINSIC_PRIMITIVE_HEAP_INITIALIZER(
        &hotbit_common_primitive_heap,
        PAS_SIMPLE_TYPE_CREATE(1, 1),
        hotbit_common_primitive_heap_support,
        HOTBIT_HEAP_CONFIG,
        &hotbit_intrinsic_primitive_runtime_config.base);

pas_allocator_counts hotbit_allocator_counts;

void* hotbit_try_allocate(size_t size)
{
    return hotbit_try_allocate_inline(size);
}

void* hotbit_try_allocate_with_alignment(size_t size, size_t alignment)
{
    return hotbit_try_allocate_with_alignment_inline(size, alignment);
}

void* hotbit_try_reallocate(void* old_ptr, size_t new_size,
                             pas_reallocate_free_mode free_mode)
{
    return hotbit_try_reallocate_inline(old_ptr, new_size, free_mode);
}

void hotbit_deallocate(void* ptr)
{
    hotbit_deallocate_inline(ptr);
}

#endif /* PAS_ENABLE_HOTBIT */

#endif /* LIBPAS_ENABLED */
