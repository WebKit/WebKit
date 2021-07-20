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

#ifndef PAS_FIRST_LEVEL_TINY_LARGE_MAP_ENTRY_H
#define PAS_FIRST_LEVEL_TINY_LARGE_MAP_ENTRY_H

#include "pas_hashtable.h"
#include "pas_internal_config.h"
#include "pas_tiny_large_map_entry.h"
#include "pas_utils.h"

PAS_BEGIN_EXTERN_C;

PAS_CREATE_HASHTABLE(pas_tiny_large_map_second_level_hashtable,
                     pas_tiny_large_map_entry,
                     pas_large_map_key);

typedef uintptr_t pas_first_level_tiny_large_map_key;

struct pas_first_level_tiny_large_map_entry;
typedef struct pas_first_level_tiny_large_map_entry pas_first_level_tiny_large_map_entry;

struct pas_first_level_tiny_large_map_entry {
    uintptr_t base;
    pas_tiny_large_map_second_level_hashtable* hashtable;
};

static inline pas_first_level_tiny_large_map_entry
pas_first_level_tiny_large_map_entry_create_empty(void)
{
    pas_first_level_tiny_large_map_entry result;
    result.base = 0;
    result.hashtable = NULL;
    return result;
}

static inline pas_first_level_tiny_large_map_entry
pas_first_level_tiny_large_map_entry_create_deleted(void)
{
    pas_first_level_tiny_large_map_entry result;
    result.base = 1;
    result.hashtable = NULL;
    return result;
}

static inline bool
pas_first_level_tiny_large_map_entry_is_empty_or_deleted(
    pas_first_level_tiny_large_map_entry entry)
{
    return !entry.hashtable;
}

static inline bool
pas_first_level_tiny_large_map_entry_is_empty(
    pas_first_level_tiny_large_map_entry entry)
{
    return !entry.base;
}

static inline bool
pas_first_level_tiny_large_map_entry_is_deleted(
    pas_first_level_tiny_large_map_entry entry)
{
    return entry.base == 1;
}

static inline uintptr_t pas_first_level_tiny_large_map_entry_get_key(
    pas_first_level_tiny_large_map_entry entry)
{
    return entry.base;
}

static inline unsigned pas_first_level_tiny_large_map_key_get_hash(uintptr_t key)
{
    return pas_large_object_hash(key);
}

static inline bool pas_first_level_tiny_large_map_key_is_equal(uintptr_t a, uintptr_t b)
{
    return a == b;
}

PAS_END_EXTERN_C;

#endif /* PAS_FIRST_LEVEL_TINY_LARGE_MAP_ENTRY_H */

