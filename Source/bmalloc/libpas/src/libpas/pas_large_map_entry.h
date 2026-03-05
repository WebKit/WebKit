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

#ifndef PAS_LARGE_MAP_ENTRY_H
#define PAS_LARGE_MAP_ENTRY_H

#include "pas_utils.h"

PAS_BEGIN_EXTERN_C;

struct pas_large_heap;
struct pas_large_map_entry;

typedef struct pas_large_heap pas_large_heap;
typedef struct pas_large_map_entry pas_large_map_entry;

typedef uintptr_t pas_large_map_key;

struct pas_large_map_entry {
    uintptr_t begin;
    uintptr_t end;
    pas_large_heap* heap;
    bool delegated_to_system_malloc;
};

static inline pas_large_map_entry pas_large_map_entry_create_empty(void)
{
    pas_large_map_entry result;
    result.begin = 0;
    result.end = 0;
    result.heap = NULL;
    result.delegated_to_system_malloc = false;
    return result;
}

static inline pas_large_map_entry pas_large_map_entry_create_deleted(void)
{
    pas_large_map_entry result;
    result.begin = 1;
    result.end = 0;
    result.heap = NULL;
    result.delegated_to_system_malloc = false;
    return result;
}

static inline bool pas_large_map_entry_is_empty_or_deleted(pas_large_map_entry entry)
{
    return !entry.end;
}

static inline bool pas_large_map_entry_is_empty(pas_large_map_entry entry)
{
    return !entry.begin;
}

static inline bool pas_large_map_entry_is_deleted(pas_large_map_entry entry)
{
    return entry.begin == 1;
}

static inline uintptr_t pas_large_map_entry_get_key(pas_large_map_entry entry)
{
    return entry.begin;
}

static inline unsigned pas_large_map_key_get_hash(uintptr_t key)
{
    return pas_large_object_hash(key);
}

static inline bool pas_large_map_key_is_equal(uintptr_t a, uintptr_t b)
{
    return a == b;
}

PAS_END_EXTERN_C;

#endif /* PAS_LARGE_MAP_ENTRY_H */

