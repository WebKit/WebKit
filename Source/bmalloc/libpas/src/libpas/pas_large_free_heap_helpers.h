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

#ifndef PAS_LARGE_FREE_HEAP_HELPERS_H
#define PAS_LARGE_FREE_HEAP_HELPERS_H

#include "pas_alignment.h"
#include "pas_allocation_kind.h"
#include "pas_allocation_result.h"
#include "pas_heap_summary.h"
#include "pas_utils.h"

PAS_BEGIN_EXTERN_C;

struct pas_fast_large_free_heap;
typedef struct pas_fast_large_free_heap pas_fast_large_free_heap;

PAS_API extern bool pas_large_utility_free_heap_talks_to_large_sharing_pool;

typedef pas_allocation_result (*pas_large_free_heap_helpers_memory_source)(
    size_t size,
    pas_alignment alignment,
    const char* name,
    pas_allocation_kind allocation_kind);

PAS_API void* pas_large_free_heap_helpers_try_allocate_with_alignment(
    pas_fast_large_free_heap* heap,
    pas_large_free_heap_helpers_memory_source memory_source,
    size_t* num_allocated_object_bytes_ptr,
    size_t* num_allocated_object_bytes_peak_ptr,
    size_t size,
    pas_alignment alignment,
    const char* name);

PAS_API void pas_large_free_heap_helpers_deallocate(
    pas_fast_large_free_heap* heap,
    pas_large_free_heap_helpers_memory_source memory_source,
    size_t* num_allocated_object_bytes_ptr,
    void* ptr,
    size_t size);

PAS_API pas_heap_summary pas_large_free_heap_helpers_compute_summary(
    pas_fast_large_free_heap* heap,
    size_t* num_allocated_object_bytes_ptr);

PAS_END_EXTERN_C;

#endif /* PAS_LARGE_FREE_HEAP_HELPERS_H */

