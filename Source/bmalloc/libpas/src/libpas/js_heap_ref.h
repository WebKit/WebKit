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

#ifndef JS_HEAP_REF_H
#define JS_HEAP_REF_H

#include "bmalloc_heap_ref.h"
#include "bmalloc_type.h"
#include "pas_heap_ref.h"

PAS_BEGIN_EXTERN_C;

#define JS_HEAP_REF_INITIALIZER(passed_type, heap_ref_kind) \
    ((pas_heap_ref){ \
         .type = (const pas_heap_type*)(passed_type), \
         .heap = NULL, \
         .allocator_index = 0, \
         .is_non_compact_heap = (heap_ref_kind == pas_bmalloc_heap_ref_kind_non_compact) \
     })

#define JS_PRIMITIVE_HEAP_REF_INITIALIZER(passed_type, heap_ref_kind) \
    ((pas_primitive_heap_ref){ \
         .base = JS_HEAP_REF_INITIALIZER(passed_type, heap_ref_kind), \
         .cached_index = UINT_MAX \
     })

PAS_END_EXTERN_C;

#endif /* JS_HEAP_REF_H */
