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

#include "js_heap_config.h"
#include "js_heap_innards.h"
#include "js_heap_ref.h"
#include "pas_ensure_heap_forced_into_reserved_memory.h"
#include "pas_get_allocation_size.h"
#include "pas_get_heap.h"

PAS_BEGIN_EXTERN_C;

pas_allocator_counts js_allocator_counts;

pas_heap* js_heap_ref_get_heap(pas_primitive_heap_ref* heap_ref)
{
    return pas_ensure_heap(&heap_ref->base, pas_primitive_heap_ref_kind,
                           &js_heap_config, &js_primitive_runtime_config.base);
}

pas_heap* js_force_heap_into_reserved_memory(pas_primitive_heap_ref* heap_ref,
                                             uintptr_t begin,
                                             uintptr_t end)
{
    return pas_ensure_heap_forced_into_reserved_memory(
        &heap_ref->base, pas_primitive_heap_ref_kind, &js_heap_config,
        &js_primitive_runtime_config.base, begin, end);
}

size_t js_heap_ref_get_type_size(pas_heap_ref* heap_ref)
{
    return JS_HEAP_CONFIG.get_type_size(heap_ref->type);
}

size_t js_get_allocation_size(void* ptr)
{
    return pas_get_allocation_size(ptr, JS_HEAP_CONFIG);
}

pas_heap* js_get_heap(void* ptr)
{
    return pas_get_heap(ptr, JS_HEAP_CONFIG);
}

PAS_END_EXTERN_C;

#endif /* PAS_ENABLE_BMALLOC */

#endif /* LIBPAS_ENABLED */
