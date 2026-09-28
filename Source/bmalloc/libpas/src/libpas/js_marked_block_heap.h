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

#ifndef JS_MARKED_BLOCK_HEAP_H
#define JS_MARKED_BLOCK_HEAP_H

#include "pas_config.h"

#if PAS_ENABLE_JS_MARKED_BLOCK

#include "pas_primitive_heap_ref.h"

PAS_BEGIN_EXTERN_C;

/* A heap that serves exactly one size, JS_MARKED_BLOCK_SIZE, always aligned to that same size. It
   exists for JavaScriptCore's MarkedBlock, which recovers a block's header from any object inside it
   by masking off the low bits of the pointer, and so cannot tolerate any other alignment.

   Serving one size means the page config's minimum alignment can be the block size itself. That is
   what makes this heap worth having: alignment costs nothing to satisfy, a page holds nothing but
   blocks, and one granule holds exactly one block, so committing and decommitting track live blocks
   exactly. See js_marked_block_heap_config.h for the page config that follows from that. */

/* Must match JSC's MarkedBlock::blockSize, which is max(16KB, CeilingOnPageSize). The client asserts
   the equality at compile time, and a build whose blocks would be 64KB does not enable libpas at
   all, so nothing here has to cope with a mismatch. */
#ifndef JS_MARKED_BLOCK_SHIFT
#define JS_MARKED_BLOCK_SHIFT ((size_t)14)
#endif
#define JS_MARKED_BLOCK_SIZE ((size_t)1 << JS_MARKED_BLOCK_SHIFT)

/* One block, JS_MARKED_BLOCK_SIZE bytes and aligned to JS_MARKED_BLOCK_SIZE, or NULL. Free it with
   js_marked_block_heap_deallocate and nothing else. */
PAS_API void* js_marked_block_heap_try_allocate(void);

/* The same, from a heap the caller made with js_marked_block_heap_force_into_reserved_memory. */
PAS_API void* js_marked_block_heap_try_allocate_from(pas_primitive_heap_ref* heap_ref);

PAS_API void js_marked_block_heap_deallocate(void* ptr);

/* Gives a heap whose blocks all come from [begin, end). JavaScriptCore's Structure blocks need this:
   a StructureID is the low bits of a block address, so every one of them has to live inside a range
   reserved up front. Only ever hand a heap_ref that nothing has allocated from yet.

   A process told to allocate through the system heap gets system memory from this heap like from any
   other, which is nowhere near [begin, end), so a caller that depends on the range must ask
   bmalloc::api::isEnabled() first and do its own thing when the answer is no. */
PAS_API pas_heap* js_marked_block_heap_force_into_reserved_memory(pas_primitive_heap_ref* heap_ref,
                                                                 uintptr_t begin,
                                                                 uintptr_t end);

PAS_API pas_heap* js_marked_block_heap_get_heap(void* ptr);
PAS_API size_t js_marked_block_heap_get_allocation_size(void* ptr);

PAS_END_EXTERN_C;

#endif /* PAS_ENABLE_JS_MARKED_BLOCK */

#endif /* JS_MARKED_BLOCK_HEAP_H */
