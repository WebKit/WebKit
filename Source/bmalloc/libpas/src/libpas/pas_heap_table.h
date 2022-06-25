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

#ifndef PAS_HEAP_TABLE_H
#define PAS_HEAP_TABLE_H

#include "pas_large_heap.h"
#include "pas_log.h"
#include "pas_utils.h"

PAS_BEGIN_EXTERN_C;

/* We want the table to hold the first 2^16 heaps. */
#define PAS_HEAP_TABLE_SIZE 65536

PAS_API extern pas_large_heap** pas_heap_table;
PAS_API extern unsigned pas_heap_table_bump_index;

/* Call with heap lock held. */
PAS_API void pas_heap_table_try_allocate_index(pas_large_heap* heap);

static inline bool pas_heap_table_has_index(pas_large_heap* heap)
{
    static const bool verbose = false;
    if (heap->table_state == pas_heap_table_state_uninitialized)
        pas_heap_table_try_allocate_index(heap);
    switch (heap->table_state) {
    case pas_heap_table_state_failed:
        if (verbose)
            pas_log("failed to get index for heap %p.\n", heap);
        return false;
    case pas_heap_table_state_has_index:
        if (verbose)
            pas_log("going to get an index for heap %p.\n", heap);
        return true;
    default:
        PAS_ASSERT(!"Should not be reached");
        return false;
    }
}

static inline uint16_t pas_heap_table_get_index(pas_large_heap* heap)
{
    PAS_ASSERT(heap->table_state == pas_heap_table_state_has_index);
    return heap->index;
}

PAS_END_EXTERN_C;

#endif /* PAS_HEAP_TABLE_H */

