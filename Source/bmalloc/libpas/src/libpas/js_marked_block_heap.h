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

#include "pas_utils.h"
#include <stddef.h>
#include <stdint.h>

PAS_BEGIN_EXTERN_C;

struct pas_primitive_heap_ref;
typedef struct pas_primitive_heap_ref pas_primitive_heap_ref;

/* A libpas heap dedicated to MarkedBlock-shaped allocations (16 KB size, 16 KB
   alignment). It shares js_heap_config with the general-purpose js_heap but
   provides a thin entry path that exploits the fixed size/alignment:

   - No pas_thread_local_cache lookup. 16 KB allocations always miss the
     segregated fast path and end up in the large-heap path; consulting the
     TLC just adds a cache line touch and lazily spins up a ~16 KB TLC page
     that is never otherwise used for this heap.
   - No pas_segregated_heap_index_for_size or size-class lookup.
   - No pas_try_allocate_compute_aligned_size rounding (size == alignment).
   - Direct call into pas_large_heap_try_allocate_user_allocation /
     pas_large_heap_try_deallocate, which keeps libpas's deferred decommit
     (via pas_large_sharing_pool) intact.

   js_heap is still available and unchanged for future arbitrary-sized uses. */

#define JS_MARKED_BLOCK_HEAP_BLOCK_SIZE ((size_t)16384)

PAS_API extern pas_primitive_heap_ref js_marked_block_heap_ref;

PAS_API void js_marked_block_heap_force_into_reserved_memory(uintptr_t begin, uintptr_t end);

PAS_API void* js_marked_block_heap_try_allocate(void);

PAS_API void js_marked_block_heap_deallocate(void* ptr);

PAS_END_EXTERN_C;

#endif /* JS_MARKED_BLOCK_HEAP_H */
