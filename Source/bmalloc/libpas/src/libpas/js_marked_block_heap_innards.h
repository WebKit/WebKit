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

#ifndef JS_MARKED_BLOCK_HEAP_INNARDS_H
#define JS_MARKED_BLOCK_HEAP_INNARDS_H

#include "pas_config.h"

#if PAS_ENABLE_JS_MARKED_BLOCK

#include "pas_allocator_counts.h"
#include "pas_primitive_heap_ref.h"

PAS_BEGIN_EXTERN_C;

/* Untyped, because the block size is conveyed as the allocation size rather than as a type: a typed
   heap is barred from using bitfit at all (PAS_TYPED_MAX_BITFIT_OBJECT_SIZE is zero), and the size is
   a constant at every call site anyway. */
PAS_API extern pas_primitive_heap_ref js_marked_block_primitive_heap_ref;
PAS_API extern pas_allocator_counts js_marked_block_allocator_counts;

PAS_END_EXTERN_C;

#endif /* PAS_ENABLE_JS_MARKED_BLOCK */

#endif /* JS_MARKED_BLOCK_HEAP_INNARDS_H */
