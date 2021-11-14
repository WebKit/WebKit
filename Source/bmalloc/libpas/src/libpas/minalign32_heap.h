/*
 * Copyright (c) 2020 Apple Inc. All rights reserved.
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

#ifndef MINALIGN32_HEAP_H
#define MINALIGN32_HEAP_H

#include "pas_config.h"

#if PAS_ENABLE_MINALIGN32

#include "iso_heap.h"
#include "pas_intrinsic_heap_support.h"

PAS_BEGIN_EXTERN_C;

PAS_API extern pas_heap minalign32_common_primitive_heap;
PAS_API extern pas_intrinsic_heap_support minalign32_common_primitive_heap_support;

PAS_API void* minalign32_allocate_common_primitive(size_t size);
PAS_API void* minalign32_allocate(pas_heap_ref* heap_ref);
PAS_API void* minalign32_allocate_array_by_count(pas_heap_ref* heap_ref, size_t count, size_t alignment);
PAS_API void minalign32_deallocate(void* ptr);
PAS_API pas_heap* minalign32_heap_ref_get_heap(pas_heap_ref* heap_ref);

PAS_END_EXTERN_C;

#endif /* PAS_ENABLE_MINALIGN32 */

#endif /* MINALIGN32_HEAP_H */

