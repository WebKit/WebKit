/*
 * Copyright (c) 2019 Apple Inc. All rights reserved.
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

#ifndef PAS_IMMORTAL_HEAP_H
#define PAS_IMMORTAL_HEAP_H

#include "pas_allocation_kind.h"
#include "pas_lock.h"
#include "pas_utils.h"

PAS_BEGIN_EXTERN_C;

extern PAS_API uintptr_t pas_immortal_heap_current;
extern PAS_API uintptr_t pas_immortal_heap_end;
extern PAS_API size_t pas_immortal_heap_allocated_external;
extern PAS_API size_t pas_immortal_heap_allocated_internal;
extern PAS_API size_t pas_immortal_heap_allocation_granule;

PAS_API void* pas_immortal_heap_allocate_with_manual_alignment(size_t size,
                                                               size_t alignment,
                                                               const char* name,
                                                               pas_allocation_kind allocation_kind);

PAS_API void* pas_immortal_heap_allocate_with_alignment(size_t size,
                                                        size_t alignment,
                                                        const char* name,
                                                        pas_allocation_kind allocation_kind);

PAS_API void* pas_immortal_heap_allocate(size_t size,
                                         const char* name,
                                         pas_allocation_kind allocation_kind);

PAS_API void* pas_immortal_heap_hold_lock_and_allocate(size_t size,
                                                       const char* name,
                                                       pas_allocation_kind allocation_kind);

PAS_API void* pas_immortal_heap_allocate_with_heap_lock_hold_mode(
    size_t size,
    const char* name,
    pas_allocation_kind allocation_kind,
    pas_lock_hold_mode heap_lock_hold_mode);

PAS_API void* pas_immortal_heap_allocate_with_alignment_and_heap_lock_hold_mode(
    size_t size,
    size_t alignment,
    const char* name,
    pas_allocation_kind allocation_kind,
    pas_lock_hold_mode heap_lock_hold_mode);

PAS_END_EXTERN_C;

#endif /* PAS_IMMORTAL_HEAP_H */

