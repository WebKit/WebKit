/*
 * Copyright (c) 2018-2021 Apple Inc. All rights reserved.
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

#ifndef PAS_LARGE_HEAP_H
#define PAS_LARGE_HEAP_H

#include "pas_fast_large_free_heap.h"
#include "pas_heap_summary.h"
#include "pas_heap_table_state.h"
#include "pas_physical_memory_transaction.h"
#include "pas_utils.h"

PAS_BEGIN_EXTERN_C;

struct pas_heap_config;
struct pas_large_heap;
typedef struct pas_heap_config pas_heap_config;
typedef struct pas_large_heap pas_large_heap;

struct pas_large_heap {
    pas_fast_large_free_heap free_heap;
    uint16_t index;
    pas_heap_table_state table_state : 8;
};

/* Note that all of these functions have to be called with the heap lock held. */

/* NOTE: it's only valid to construct a large heap that is a member of a pas_heap. */
PAS_API void pas_large_heap_construct(pas_large_heap* heap);

PAS_API pas_allocation_result
pas_large_heap_try_allocate(pas_large_heap* heap,
                            size_t size, size_t alignment,
                            pas_heap_config* config,
                            pas_physical_memory_transaction* transaction);

/* Returns true if an object was found and deallocated. */
PAS_API bool pas_large_heap_try_deallocate(uintptr_t base,
                                           pas_heap_config* config);

/* Returns true if an object was found and shrunk. */
PAS_API bool pas_large_heap_try_shrink(uintptr_t base,
                                       size_t new_size,
                                       pas_heap_config* config);

/* This is a super crazy function that lets you shove memory into the allocator. There is
   one user (the large region) and it only does it to one heap (the primitive heap). It's
   not something you probably ever want to do. */
PAS_API void pas_large_heap_shove_into_free(pas_large_heap* heap, uintptr_t begin, uintptr_t end,
                                            pas_zero_mode zero_mode,
                                            pas_heap_config* config);

typedef bool (*pas_large_heap_for_each_live_object_callback)(pas_large_heap* heap,
                                                             uintptr_t begin,
                                                             uintptr_t end,
                                                             void *arg);

PAS_API bool pas_large_heap_for_each_live_object(
    pas_large_heap* heap,
    pas_large_heap_for_each_live_object_callback callback,
    void *arg);

PAS_API pas_large_heap* pas_large_heap_for_object(uintptr_t begin);

PAS_API size_t pas_large_heap_get_num_free_bytes(pas_large_heap* heap);

PAS_API pas_heap_summary pas_large_heap_compute_summary(pas_large_heap* heap);

PAS_END_EXTERN_C;

#endif /* PAS_LARGE_HEAP_H */

