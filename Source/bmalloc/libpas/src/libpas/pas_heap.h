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

#ifndef PAS_HEAP_H
#define PAS_HEAP_H

#include "pas_heap_ref.h"
#include "pas_heap_summary.h"
#include "pas_large_heap.h"
#include "pas_segregated_heap.h"

PAS_BEGIN_EXTERN_C;

struct pas_heap;
struct pas_heap_ref;
struct pas_segregated_global_size_directory;
struct pas_segregated_page;
typedef struct pas_heap pas_heap;
typedef struct pas_heap_ref pas_heap_ref;
typedef struct pas_segregated_global_size_directory pas_segregated_global_size_directory;
typedef struct pas_segregated_page pas_segregated_page;

struct pas_heap {
    pas_segregated_heap segregated_heap;
    pas_large_heap large_heap;
    pas_heap_type* type;
    pas_heap_ref* heap_ref;
    pas_compact_heap_ptr next_heap;
    pas_heap_config_kind config_kind : 8;
    pas_heap_ref_kind heap_ref_kind : 2;
};

PAS_API pas_heap* pas_heap_create(pas_heap_ref* heap_ref,
                                  pas_heap_ref_kind heap_ref_kind,
                                  pas_heap_config* config,
                                  pas_heap_runtime_config* runtime_config);

/* Returns 1 for NULL heap. */
PAS_API size_t pas_heap_get_type_size(pas_heap* heap);

/* The large heap belongs to the heap in such a way that given a large heap, we can find the
   heap. */
static inline pas_heap* pas_heap_for_large_heap(pas_large_heap* large_heap)
{
    return (pas_heap*)((uintptr_t)large_heap - PAS_OFFSETOF(pas_heap, large_heap));
}

/* The large heap belongs to the heap in such a way that given a large heap, we can find the
   heap. */
static inline pas_heap* pas_heap_for_segregated_heap(pas_segregated_heap* segregated_heap)
{
    if (!segregated_heap->runtime_config->is_part_of_heap)
        return NULL;
    return (pas_heap*)((uintptr_t)segregated_heap - PAS_OFFSETOF(pas_heap, segregated_heap));
}

PAS_API size_t pas_heap_get_num_free_bytes(pas_heap* heap);

typedef bool (*pas_heap_for_each_live_object_callback)(pas_heap* heap,
                                                       uintptr_t begin,
                                                       size_t size,
                                                       void* arg);

PAS_API bool pas_heap_for_each_live_object(pas_heap* heap,
                                           pas_heap_for_each_live_object_callback callback,
                                           void *arg,
                                           pas_lock_hold_mode heap_lock_hold_mode);

PAS_API pas_heap_summary pas_heap_compute_summary(pas_heap* heap,
                                                  pas_lock_hold_mode heap_lock_hold_mode);

PAS_API void pas_heap_reset_heap_ref(pas_heap* heap);

PAS_END_EXTERN_C;

#endif /* PAS_HEAP_H */
