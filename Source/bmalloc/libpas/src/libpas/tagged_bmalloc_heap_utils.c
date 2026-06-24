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

#include "tagged_bmalloc_heap.h"

#if PAS_ENABLE_BMALLOC

#include "tagged_bmalloc_heap_config.h"
#include "tagged_bmalloc_heap_innards.h"
#include "pas_ensure_heap_forced_into_reserved_memory.h"
#include "pas_get_allocation_size.h"
#include "pas_get_heap.h"
#include "pas_try_allocate_intrinsic.h"

PAS_BEGIN_EXTERN_C;

const bmalloc_type tagged_bmalloc_common_primitive_type = BMALLOC_TYPE_INITIALIZER(1, 1, "Tagged Common Primitive");

pas_intrinsic_heap_support tagged_bmalloc_common_primitive_heap_support =
    PAS_INTRINSIC_HEAP_SUPPORT_INITIALIZER;

pas_heap tagged_bmalloc_common_primitive_heap =
    PAS_INTRINSIC_HEAP_INITIALIZER(
        &tagged_bmalloc_common_primitive_heap,
        &tagged_bmalloc_common_primitive_type,
        tagged_bmalloc_common_primitive_heap_support,
        TAGGED_BMALLOC_HEAP_CONFIG,
        &tagged_bmalloc_intrinsic_runtime_config.base);

pas_allocator_counts tagged_bmalloc_allocator_counts;

size_t tagged_bmalloc_heap_ref_get_type_size(pas_heap_ref* heap_ref)
{
    return TAGGED_BMALLOC_HEAP_CONFIG.get_type_size(heap_ref->type);
}

size_t tagged_bmalloc_get_allocation_size(void* ptr)
{
    return pas_get_allocation_size(ptr, TAGGED_BMALLOC_HEAP_CONFIG);
}

pas_heap* tagged_bmalloc_get_heap(void* ptr)
{
    return pas_get_heap(ptr, TAGGED_BMALLOC_HEAP_CONFIG);
}

PAS_END_EXTERN_C;

#endif /* PAS_ENABLE_BMALLOC */

#endif /* LIBPAS_ENABLED */
