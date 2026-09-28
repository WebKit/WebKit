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

#include "js_marked_block_heap.h"

#if PAS_ENABLE_JS_MARKED_BLOCK

#include "js_marked_block_heap_inlines.h"
#include "pas_ensure_heap_forced_into_reserved_memory.h"

PAS_BEGIN_EXTERN_C;

pas_primitive_heap_ref js_marked_block_primitive_heap_ref = {
    .base = {
        .type = (const pas_heap_type*)PAS_SIMPLE_TYPE_CREATE(1, 1),
        .heap = NULL,
        .allocator_index = 0
    },
    .cached_index = UINT_MAX
};

pas_allocator_counts js_marked_block_allocator_counts;

void* js_marked_block_heap_try_allocate(void)
{
    return js_marked_block_heap_try_allocate_inline();
}

void* js_marked_block_heap_try_allocate_from(pas_primitive_heap_ref* heap_ref)
{
    return js_marked_block_heap_try_allocate_from_inline(heap_ref);
}

void js_marked_block_heap_deallocate(void* ptr)
{
    js_marked_block_heap_deallocate_inline(ptr);
}

pas_heap* js_marked_block_heap_force_into_reserved_memory(pas_primitive_heap_ref* heap_ref,
                                                          uintptr_t begin,
                                                          uintptr_t end)
{
    return pas_ensure_heap_forced_into_reserved_memory(
        &heap_ref->base, pas_primitive_heap_ref_kind, &js_marked_block_heap_config,
        &js_marked_block_primitive_runtime_config.base, begin, end);
}

pas_heap* js_marked_block_heap_get_heap(void* ptr)
{
    return pas_get_heap(ptr, JS_MARKED_BLOCK_HEAP_CONFIG);
}

size_t js_marked_block_heap_get_allocation_size(void* ptr)
{
    return pas_get_allocation_size(ptr, JS_MARKED_BLOCK_HEAP_CONFIG);
}

PAS_END_EXTERN_C;

#endif /* PAS_ENABLE_JS_MARKED_BLOCK */

#endif /* LIBPAS_ENABLED */
