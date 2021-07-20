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

#ifndef PAS_TINY_LARGE_MAP_ENTRY_H
#define PAS_TINY_LARGE_MAP_ENTRY_H

#include "pas_heap_table.h"
#include "pas_large_map_entry.h"
#include "pas_utils.h"

PAS_BEGIN_EXTERN_C;

struct pas_large_heap;
struct pas_tiny_large_map_entry;
typedef struct pas_large_heap pas_large_heap;
typedef struct pas_tiny_large_map_entry pas_tiny_large_map_entry;

struct pas_tiny_large_map_entry {
    /* We want:
       
       - 12 bits for begin
       - 12 bits for size
       - 16 bits for heap. */
    
    uint8_t bytes[5];
};

static inline uintptr_t pas_tiny_large_map_entry_base(uintptr_t begin)
{
    return begin >> PAS_MIN_ALIGN_SHIFT >> 12 << PAS_MIN_ALIGN_SHIFT << 12;
}

static inline pas_tiny_large_map_entry pas_tiny_large_map_entry_create(pas_large_map_entry entry)
{
    pas_tiny_large_map_entry result;
    uint16_t heap_index;
    uintptr_t size;
    size = entry.end - entry.begin;
    heap_index = pas_heap_table_get_index(entry.heap);
    result.bytes[0] = (uint8_t)(entry.begin >> PAS_MIN_ALIGN_SHIFT);
    result.bytes[1] = (uint8_t)(
        ((entry.begin >> PAS_MIN_ALIGN_SHIFT >> 8) & 0xf) |
        (((size >> PAS_MIN_ALIGN_SHIFT) & 0xf) << 4));
    result.bytes[2] = (uint8_t)(size >> PAS_MIN_ALIGN_SHIFT >> 4);
    result.bytes[3] = (uint8_t)heap_index;
    result.bytes[4] = (uint8_t)(heap_index >> 8);
    return result;
}

static inline uintptr_t pas_tiny_large_map_entry_begin(pas_tiny_large_map_entry entry,
                                                       uintptr_t base)
{
    return (((uintptr_t)entry.bytes[0] | (((uintptr_t)entry.bytes[1] & 0xf) << 8))
            << PAS_MIN_ALIGN_SHIFT)
        + base;
}

static inline uintptr_t pas_tiny_large_map_entry_end(pas_tiny_large_map_entry entry,
                                                     uintptr_t base)
{
    return (((((uintptr_t)entry.bytes[1] >> 4) & 0xf) | ((uintptr_t)entry.bytes[2] << 4))
            << PAS_MIN_ALIGN_SHIFT)
        + pas_tiny_large_map_entry_begin(entry, base);
}

static inline pas_large_heap* pas_tiny_large_map_entry_heap(pas_tiny_large_map_entry entry)
{
    return pas_heap_table[(uintptr_t)entry.bytes[3] | ((uintptr_t)entry.bytes[4] << 8)];
}

static inline pas_large_map_entry pas_tiny_large_map_entry_get_entry(pas_tiny_large_map_entry entry,
                                                                     uintptr_t base)
{
    pas_large_map_entry result;
    result.begin = pas_tiny_large_map_entry_begin(entry, base);
    result.end = pas_tiny_large_map_entry_end(entry, base);
    result.heap = pas_tiny_large_map_entry_heap(entry);
    return result;
}

static inline bool pas_tiny_large_map_entry_can_create(pas_large_map_entry entry)
{
    static const bool verbose = false;
    pas_tiny_large_map_entry result;
    uintptr_t base;
    if (!pas_heap_table_has_index(entry.heap))
        return false;
    if (verbose) {
        pas_log("going to create tiny for %p...%p, heap = %p.\n",
                (void*)entry.begin, (void*)entry.end, entry.heap);
    }
    base = pas_tiny_large_map_entry_base(entry.begin);
    if (verbose)
        pas_log("base = %p.\n", base);
    result = pas_tiny_large_map_entry_create(entry);
    if (verbose) {
        pas_log("encoded: %u, %u, %u, %u, %u\n",
                (unsigned)result.bytes[0],
                (unsigned)result.bytes[1],
                (unsigned)result.bytes[2],
                (unsigned)result.bytes[3],
                (unsigned)result.bytes[4]);
        pas_log("round tripped: %p...%p, heap = %p.\n",
                pas_tiny_large_map_entry_begin(result, base),
                pas_tiny_large_map_entry_end(result, base),
                pas_tiny_large_map_entry_heap(result));
    }
    return entry.begin == pas_tiny_large_map_entry_begin(result, base)
        && entry.end == pas_tiny_large_map_entry_end(result, base)
        && entry.heap == pas_tiny_large_map_entry_heap(result);
}

static inline pas_tiny_large_map_entry pas_tiny_large_map_entry_create_empty(void)
{
    pas_tiny_large_map_entry result;
    pas_zero_memory(&result, sizeof(result));
    return result;
}

static inline pas_tiny_large_map_entry pas_tiny_large_map_entry_create_deleted(void)
{
    pas_tiny_large_map_entry result;
    pas_zero_memory(&result, sizeof(result));
    result.bytes[0] = 1;
    return result;
}

static inline bool pas_tiny_large_map_entry_is_empty_or_deleted(pas_tiny_large_map_entry entry)
{
    size_t index;
    for (index = 5; index-- > 1;) {
        if (entry.bytes[index])
            return false;
    }
    return entry.bytes[0] <= 1;
}

static inline bool pas_tiny_large_map_entry_is_empty(pas_tiny_large_map_entry entry)
{
    size_t index;
    for (index = 5; index--;) {
        if (entry.bytes[index])
            return false;
    }
    return true;
}

static inline bool pas_tiny_large_map_entry_is_deleted(pas_tiny_large_map_entry entry)
{
    size_t index;
    for (index = 5; index-- > 1;) {
        if (entry.bytes[index])
            return false;
    }
    return entry.bytes[0] == 1;
}

static inline uintptr_t pas_tiny_large_map_entry_get_key(pas_tiny_large_map_entry entry)
{
    return pas_tiny_large_map_entry_begin(entry, 0);
}

static inline unsigned pas_tiny_large_map_key_get_hash(uintptr_t key)
{
    return pas_hash32((unsigned)key);
}

static inline bool pas_tiny_large_map_key_is_equal(uintptr_t a, uintptr_t b)
{
    return a == b;
}

PAS_END_EXTERN_C;

#endif /* PAS_TINY_LARGE_MAP_ENTRY_H */

