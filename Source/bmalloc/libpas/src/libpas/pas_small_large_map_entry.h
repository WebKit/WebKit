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

#ifndef PAS_SMALL_LARGE_MAP_ENTRY_H
#define PAS_SMALL_LARGE_MAP_ENTRY_H

#include "pas_large_map_entry.h"
#include "pas_utils.h"

PAS_BEGIN_EXTERN_C;

struct pas_large_heap;
struct pas_small_large_map_entry;

typedef struct pas_large_heap pas_large_heap;
typedef struct pas_small_large_map_entry pas_small_large_map_entry;

struct pas_small_large_map_entry {
    unsigned encoded_begin;
    unsigned encoded_size;
    unsigned encoded_heap;
};

static inline pas_small_large_map_entry pas_small_large_map_entry_create_empty(void)
{
    pas_small_large_map_entry result;
    result.encoded_begin = 0;
    result.encoded_size = 0;
    result.encoded_heap = 0;
    return result;
}

static inline pas_small_large_map_entry pas_small_large_map_entry_create_deleted(void)
{
    pas_small_large_map_entry result;
    result.encoded_begin = 1;
    result.encoded_size = 0;
    result.encoded_heap = 0;
    return result;
}

static inline bool pas_small_large_map_entry_is_empty_or_deleted(pas_small_large_map_entry entry)
{
    return !entry.encoded_size;
}

static inline bool pas_small_large_map_entry_is_empty(pas_small_large_map_entry entry)
{
    return !entry.encoded_begin;
}

static inline bool pas_small_large_map_entry_is_deleted(pas_small_large_map_entry entry)
{
    return entry.encoded_begin == 1;
}

static inline pas_small_large_map_entry pas_small_large_map_entry_create(pas_large_map_entry entry)
{
    pas_small_large_map_entry result;
    result.encoded_begin = (unsigned)(entry.begin >> PAS_MIN_ALIGN_SHIFT);
    result.encoded_size = (unsigned)((entry.end - entry.begin) >> PAS_MIN_ALIGN_SHIFT);
    result.encoded_heap = (unsigned)((uintptr_t)entry.heap / PAS_INTERNAL_MIN_ALIGN);
    return result;
}

static inline uintptr_t pas_small_large_map_entry_begin(pas_small_large_map_entry entry)
{
    return (uintptr_t)entry.encoded_begin << PAS_MIN_ALIGN_SHIFT;
}

static inline uintptr_t pas_small_large_map_entry_get_key(pas_small_large_map_entry entry)
{
    return pas_small_large_map_entry_begin(entry);
}

static inline uintptr_t pas_small_large_map_entry_end(pas_small_large_map_entry entry)
{
    return pas_small_large_map_entry_begin(entry)
        + ((uintptr_t)entry.encoded_size << PAS_MIN_ALIGN_SHIFT);
}

static inline pas_large_heap* pas_small_large_map_entry_heap(pas_small_large_map_entry entry)
{
    return (pas_large_heap*)((uintptr_t)entry.encoded_heap * PAS_INTERNAL_MIN_ALIGN);
}

static inline pas_large_map_entry pas_small_large_map_entry_get_entry(
    pas_small_large_map_entry entry)
{
    pas_large_map_entry result;
    result.begin = pas_small_large_map_entry_begin(entry);
    result.end = pas_small_large_map_entry_end(entry);
    result.heap = pas_small_large_map_entry_heap(entry);
    return result;
}

static inline bool pas_small_large_map_entry_can_create(pas_large_map_entry entry)
{
    pas_small_large_map_entry result;
    result = pas_small_large_map_entry_create(entry);
    return entry.begin == pas_small_large_map_entry_begin(result)
        && entry.end == pas_small_large_map_entry_end(result)
        && entry.heap == pas_small_large_map_entry_heap(result);
}

PAS_END_EXTERN_C;

#endif /* PAS_SMALL_LARGE_MAP_ENTRY_H */

