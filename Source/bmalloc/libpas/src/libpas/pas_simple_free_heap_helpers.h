/*
 * Copyright (c) 2018-2020 Apple Inc. All rights reserved.
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

#ifndef PAS_SIMPLE_FREE_HEAP_HELPERS_H
#define PAS_SIMPLE_FREE_HEAP_HELPERS_H

#include "pas_alignment.h"
#include "pas_allocation_kind.h"
#include "pas_allocation_result.h"
#include "pas_heap_kind.h"

PAS_BEGIN_EXTERN_C;

struct pas_simple_large_free_heap;
struct pas_large_free_heap_config;
typedef struct pas_simple_large_free_heap pas_simple_large_free_heap;
typedef struct pas_large_free_heap_config pas_large_free_heap_config;

PAS_API pas_allocation_result
pas_simple_free_heap_helpers_try_allocate_with_manual_alignment(
    pas_simple_large_free_heap* free_heap,
    void (*initialize_config)(pas_large_free_heap_config* config),
    pas_heap_kind heap_kind,
    size_t size,
    pas_alignment alignment,
    const char* name,
    pas_allocation_kind allocation_kind,
    size_t* num_allocated_object_bytes,
    size_t* num_allocated_object_bytes_peak);

PAS_API void pas_simple_free_heap_helpers_deallocate(
    pas_simple_large_free_heap* free_heap,
    void (*initialize_config)(pas_large_free_heap_config* config),
    pas_heap_kind heap_kind,
    void* ptr,
    size_t size,
    pas_allocation_kind allocation_kind,
    size_t* num_allocated_object_bytes);

PAS_END_EXTERN_C;

#endif /* PAS_SIMPLE_FREE_HEAP_HELPERS_H */

