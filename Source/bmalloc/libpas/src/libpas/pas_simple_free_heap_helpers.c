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

#include "pas_config.h"

#if LIBPAS_ENABLED

#include "pas_simple_free_heap_helpers.h"

#include "pas_allocation_callbacks.h"
#include "pas_config.h"
#include "pas_heap_lock.h"
#include "pas_large_free_heap_config.h"
#include "pas_simple_large_free_heap.h"
#include <stdio.h>

static const bool verbose = false;

pas_allocation_result
pas_simple_free_heap_helpers_try_allocate_with_manual_alignment(
    pas_simple_large_free_heap* free_heap,
    void (*initialize_config)(pas_large_free_heap_config* config),
    pas_heap_kind heap_kind,
    size_t size,
    pas_alignment alignment,
    const char* name,
    pas_allocation_kind allocation_kind,
    size_t* num_allocated_object_bytes,
    size_t* num_allocated_object_bytes_peak)
{
    static const bool exaggerate_cost = false;
    
    pas_large_free_heap_config config;
    pas_allocation_result result;
    
    pas_heap_lock_assert_held();

    if (verbose) {
        printf("%s: Doing simple free heap allocation with size = %zu, alignment = %zu/%zu.\n",
               pas_heap_kind_get_string(heap_kind), size, alignment.alignment,
               alignment.alignment_begin);
    }

    /* NOTE: This cannot align the size. That's because it cannot change the size. It has to
       use the size that the user passed. Anything else would result in us forever forgetting
       about that that alignment slop, since the caller will pass their original size when
       freeing the object later. */
    
    initialize_config(&config);
    result = pas_simple_large_free_heap_try_allocate(free_heap,
                                                     size, alignment,
                                                     &config);
    if (verbose)
        printf("Simple allocated %p with size %zu\n", (void*)result.begin, size);

    if (exaggerate_cost && result.did_succeed) {
        pas_simple_large_free_heap_deallocate(free_heap,
                                              result.begin, result.begin + size,
                                              result.zero_mode,
                                              &config);

        result = pas_simple_large_free_heap_try_allocate(free_heap,
                                                         size, alignment,
                                                         &config);
    }
    
    pas_did_allocate(
        (void*)result.begin, size, heap_kind, name, allocation_kind);
    
    if (result.did_succeed && allocation_kind == pas_object_allocation) {
        (*num_allocated_object_bytes) += size;
        *num_allocated_object_bytes_peak = PAS_MAX(
            *num_allocated_object_bytes,
            *num_allocated_object_bytes_peak);
        if (verbose) {
            pas_log("Allocated %zu simple bytes for %s at %p.\n",
                    size, name, (void*)result.begin);
        }
    }

    return result;
}

void pas_simple_free_heap_helpers_deallocate(
    pas_simple_large_free_heap* free_heap,
    void (*initialize_config)(pas_large_free_heap_config* config),
    pas_heap_kind heap_kind,
    void* ptr,
    size_t size,
    pas_allocation_kind allocation_kind,
    size_t* num_allocated_object_bytes)
{
    static const bool verbose = false;
    
    pas_large_free_heap_config config;
    if (!size)
        return;
    if (verbose) {
        printf("%s: Simple freeing %p with size %zu\n",
               pas_heap_kind_get_string(heap_kind), ptr, size);
    }

    pas_will_deallocate(ptr, size, heap_kind, allocation_kind);
    
    initialize_config(&config);
    pas_simple_large_free_heap_deallocate(free_heap,
                                          (uintptr_t)ptr, (uintptr_t)ptr + size,
                                          pas_zero_mode_may_have_non_zero,
                                          &config);

    if (allocation_kind == pas_object_allocation) {
        (*num_allocated_object_bytes) -= size;
        if (verbose)
            pas_log("Deallocated %zu simple bytes at %p.\n", size, ptr);
    }
}

#endif /* LIBPAS_ENABLED */
