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

#ifndef MARKED_BLOCK_HEAP_INLINES_H
#define MARKED_BLOCK_HEAP_INLINES_H

#include "pas_config.h"

#if PAS_ENABLE_MARKED_BLOCK

#include "marked_block_heap.h"
#include "marked_block_heap_config.h"
#include "pas_deallocate.h"
#include "pas_get_allocation_size.h"
#include "pas_get_heap.h"
#include "pas_try_allocate_primitive.h"

PAS_BEGIN_EXTERN_C;

PAS_CREATE_TRY_ALLOCATE_PRIMITIVE(
    marked_block_heap_try_allocate_block,
    MARKED_BLOCK_HEAP_CONFIG,
    &marked_block_primitive_runtime_config.base,
    &marked_block_allocator_counts,
    pas_allocation_result_identity);

static PAS_ALWAYS_INLINE void* marked_block_heap_try_allocate_from_inline(
    pas_primitive_heap_ref* heap_ref)
{
    /* JavaScriptCore's blocks are reachable through compact pointers, which is why this heap must
       never be MTE-tagged (see the memory_flavor in MARKED_BLOCK_HEAP_CONFIG). */
    return (void*)marked_block_heap_try_allocate_block(
        heap_ref, MARKED_BLOCK_SIZE, MARKED_BLOCK_SIZE).begin;
}

static PAS_ALWAYS_INLINE void* marked_block_heap_try_allocate_inline(void)
{
    return marked_block_heap_try_allocate_from_inline(&marked_block_primitive_heap_ref);
}

static PAS_ALWAYS_INLINE void marked_block_heap_deallocate_inline(void* ptr)
{
    pas_deallocate(ptr, MARKED_BLOCK_HEAP_CONFIG);
}

PAS_END_EXTERN_C;

#endif /* PAS_ENABLE_MARKED_BLOCK */

#endif /* MARKED_BLOCK_HEAP_INLINES_H */
