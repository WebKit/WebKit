/*
 * Copyright (c) 2021 Apple Inc. All rights reserved.
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

#ifndef JIT_HEAP_H
#define JIT_HEAP_H

#include "pas_config.h"

#if PAS_ENABLE_JIT

#include "pas_allocator_counts.h"
#include "pas_heap_ref.h"
#include "pas_intrinsic_heap_support.h"
#include "pas_range.h"

PAS_BEGIN_EXTERN_C;

PAS_API extern pas_heap jit_common_primitive_heap;
PAS_API extern pas_intrinsic_heap_support jit_common_primitive_heap_support;
PAS_API extern pas_allocator_counts jit_allocator_counts;

/* We expect the given memory to be committed and clean, but it may have weird permissions. */
PAS_API void jit_heap_add_fresh_memory(pas_range range);

PAS_API void* jit_heap_try_allocate(size_t size);
PAS_API void jit_heap_shrink(void* object, size_t new_size);
PAS_API size_t jit_heap_get_size(void* object);
PAS_API void jit_heap_deallocate(void* object);

PAS_END_EXTERN_C;

#endif /* PAS_ENABLE_JIT */

#endif /* JIT_HEAP_H */

