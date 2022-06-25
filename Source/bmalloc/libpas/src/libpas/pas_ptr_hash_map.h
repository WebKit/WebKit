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

#ifndef PAS_PTR_HASH_MAP_H
#define PAS_PTR_HASH_MAP_H

#include "pas_hashtable.h"

PAS_BEGIN_EXTERN_C;

struct pas_ptr_hash_map_entry;
typedef struct pas_ptr_hash_map_entry pas_ptr_hash_map_entry;

struct pas_ptr_hash_map_entry {
    void* key;
    void* value;
};

typedef void* pas_ptr_hash_map_key;

static inline pas_ptr_hash_map_entry pas_ptr_hash_map_entry_create_empty(void)
{
    pas_ptr_hash_map_entry result;
    result.key = (void*)UINTPTR_MAX;
    result.value = NULL;
    return result;
}

static inline pas_ptr_hash_map_entry pas_ptr_hash_map_entry_create_deleted(void)
{
    pas_ptr_hash_map_entry result;
    result.key = (void*)UINTPTR_MAX;
    result.value = (void*)(uintptr_t)1;
    return result;
}

static inline bool pas_ptr_hash_map_entry_is_empty_or_deleted(pas_ptr_hash_map_entry entry)
{
    if (entry.key == (void*)UINTPTR_MAX) {
        PAS_TESTING_ASSERT(entry.value <= (void*)(uintptr_t)1);
        return true;
    }
    return false;
}

static inline bool pas_ptr_hash_map_entry_is_empty(pas_ptr_hash_map_entry entry)
{
    return entry.key == (void*)UINTPTR_MAX
        && !entry.value;
}

static inline bool pas_ptr_hash_map_entry_is_deleted(pas_ptr_hash_map_entry entry)
{
    return entry.key == (void*)UINTPTR_MAX
        && entry.value == (void*)(uintptr_t)1;
}

static inline void* pas_ptr_hash_map_entry_get_key(pas_ptr_hash_map_entry entry)
{
    return entry.key;
}

static inline unsigned pas_ptr_hash_map_key_get_hash(pas_ptr_hash_map_key key)
{
    return pas_hash_ptr(key);
}

static inline bool pas_ptr_hash_map_key_is_equal(pas_ptr_hash_map_key a,
                                                 pas_ptr_hash_map_key b)
{
    return a == b;
}

PAS_CREATE_HASHTABLE(pas_ptr_hash_map,
                     pas_ptr_hash_map_entry,
                     pas_ptr_hash_map_key);

PAS_END_EXTERN_C;

#endif /* PAS_PTR_HASH_MAP_H */

