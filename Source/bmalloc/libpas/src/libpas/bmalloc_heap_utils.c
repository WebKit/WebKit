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

#include "bmalloc_heap.h"

#if PAS_ENABLE_BMALLOC

#include "bmalloc_heap_config.h"
#include "bmalloc_heap_innards.h"
#include "pas_ensure_heap_forced_into_reserved_memory.h"
#include "pas_get_allocation_size.h"
#include "pas_get_heap.h"
#include "pas_try_allocate_intrinsic.h"

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

const bmalloc_type bmalloc_compact_primitive_type = BMALLOC_TYPE_INITIALIZER(1, 1, "Compact Primitive");

pas_primitive_heap_ref bmalloc_compact_primitive_heap_ref =  BMALLOC_AUXILIARY_HEAP_REF_INITIALIZER(&bmalloc_compact_primitive_type, pas_bmalloc_heap_ref_kind_compact);

pas_allocator_counts bmalloc_allocator_counts;

pas_heap* bmalloc_auxiliary_heap_ref_get_heap(pas_primitive_heap_ref* heap_ref)
{
    return pas_ensure_heap(&heap_ref->base, pas_primitive_heap_ref_kind,
                           &bmalloc_heap_config, &bmalloc_primitive_runtime_config.base);
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
