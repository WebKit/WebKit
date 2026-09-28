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

#ifndef JS_MARKED_BLOCK_HEAP_INLINES_H
#define JS_MARKED_BLOCK_HEAP_INLINES_H

#include "pas_config.h"

#if PAS_ENABLE_JS_MARKED_BLOCK

#include "js_marked_block_heap.h"
#include "js_marked_block_heap_config.h"
#include "js_marked_block_heap_innards.h"
#include "pas_deallocate.h"
#include "pas_get_allocation_size.h"
#include "pas_get_heap.h"
#include "pas_try_allocate_primitive.h"

PAS_BEGIN_EXTERN_C;

PAS_CREATE_TRY_ALLOCATE_PRIMITIVE(
    js_marked_block_heap_try_allocate_block,
    JS_MARKED_BLOCK_HEAP_CONFIG,
    &js_marked_block_primitive_runtime_config.base,
    &js_marked_block_allocator_counts,
    pas_allocation_result_identity);

static PAS_ALWAYS_INLINE void* js_marked_block_heap_try_allocate_from_inline(
    pas_primitive_heap_ref* heap_ref)
{
    /* Compact because JavaScriptCore's blocks are reachable through compact pointers, which is also
       why this heap must never be MTE-tagged. */
    return (void*)js_marked_block_heap_try_allocate_block(
        heap_ref, JS_MARKED_BLOCK_SIZE, JS_MARKED_BLOCK_SIZE,
        pas_always_compact_allocation_mode).begin;
}

static PAS_ALWAYS_INLINE void* js_marked_block_heap_try_allocate_inline(void)
{
    return js_marked_block_heap_try_allocate_from_inline(&js_marked_block_primitive_heap_ref);
}

static PAS_ALWAYS_INLINE void js_marked_block_heap_deallocate_inline(void* ptr)
{
    pas_deallocate(ptr, JS_MARKED_BLOCK_HEAP_CONFIG);
}

PAS_END_EXTERN_C;

#endif /* PAS_ENABLE_JS_MARKED_BLOCK */

#endif /* JS_MARKED_BLOCK_HEAP_INLINES_H */
