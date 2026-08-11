/*
 * Copyright (c) 2018-2019 Apple Inc. All rights reserved.
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

#ifndef THINGY_HEAP_H
#define THINGY_HEAP_H

#include "pas_config.h"

#if PAS_ENABLE_THINGY

#include "pas_allocator_counts.h"
#include "pas_heap_ref.h"
#include "pas_intrinsic_heap_support.h"
#include "pas_allocation_mode.h"

#include "thingy_heap_prefix.h"

PAS_BEGIN_EXTERN_C;

#define thingy_try_allocate_primitive __thingy_try_allocate_primitive
#define thingy_try_allocate_primitive_zeroed __thingy_try_allocate_primitive_zeroed
#define thingy_try_allocate_primitive_with_alignment __thingy_try_allocate_primitive_with_alignment

#define thingy_try_reallocate_primitive __thingy_try_reallocate_primitive

#define thingy_try_allocate __thingy_try_allocate
#define thingy_try_allocate_zeroed __thingy_try_allocate_zeroed
#define thingy_try_allocate_array __thingy_try_allocate_array
#define thingy_try_allocate_zeroed_array __thingy_try_allocate_zeroed_array

#define thingy_try_reallocate_array __thingy_try_reallocate_array

#define thingy_deallocate __thingy_deallocate

extern PAS_API pas_heap thingy_primitive_heap;
extern PAS_API pas_heap thingy_utility_heap;
extern PAS_API pas_intrinsic_heap_support thingy_primitive_heap_support;
extern PAS_API pas_intrinsic_heap_support thingy_utility_heap_support;
extern PAS_API pas_allocator_counts thingy_allocator_counts;

PAS_API size_t thingy_get_allocation_size(void*);

PAS_API pas_heap* thingy_heap_ref_get_heap(pas_heap_ref* heap_ref);

PAS_API void* thingy_utility_heap_allocate(size_t size, pas_allocation_mode allocation_mode);

PAS_END_EXTERN_C;

#endif /* PAS_ENABLE_THINGY */

#endif /* THINGY_HEAP_H */

