/*
 * Copyright (c) 2019-2020 Apple Inc. All rights reserved.
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

#ifndef ISO_HEAP_REF_H
#define ISO_HEAP_REF_H

#include "pas_heap_ref.h"
#include "pas_simple_type.h"

PAS_BEGIN_EXTERN_C;

#define ISO_HEAP_REF_INITIALIZER_TLC_PART \
    .allocator_index = 0

#define ISO_HEAP_REF_INITIALIZER_WITH_ALIGNMENT(type_size, alignment) \
    ((pas_heap_ref){ \
         .type = (const pas_heap_type*)PAS_SIMPLE_TYPE_CREATE((type_size), (alignment)), \
         .heap = NULL, \
         ISO_HEAP_REF_INITIALIZER_TLC_PART \
     })

#define ISO_HEAP_REF_INITIALIZER(type_size) \
    ISO_HEAP_REF_INITIALIZER_WITH_ALIGNMENT((type_size), 1)

#define ISO_PRIMITIVE_HEAP_REF_INITIALIZER \
    ((pas_primitive_heap_ref){ \
         .base = ISO_HEAP_REF_INITIALIZER(1), \
         .cached_index = UINT_MAX \
     })

PAS_END_EXTERN_C;

#endif /* ISO_HEAP_REF_H */

