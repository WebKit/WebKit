/*
 * Copyright (c) 2019-2020 Apple Inc. All rights reserved.
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

#ifndef PAS_DYNAMIC_PRIMITIVE_HEAP_MAP_H
#define PAS_DYNAMIC_PRIMITIVE_HEAP_MAP_H

#include "pas_hashtable.h"
#include "pas_lock_free_read_ptr_ptr_hashtable.h"
#include "pas_log.h"
#include "pas_primitive_heap_ref.h"
#include "pas_simple_type.h"

PAS_BEGIN_EXTERN_C;

typedef size_t pas_dynamic_primitive_heap_map_heaps_for_size_table_key;

struct pas_dynamic_primitive_heap_map_heaps_for_size_table_entry;
typedef struct pas_dynamic_primitive_heap_map_heaps_for_size_table_entry pas_dynamic_primitive_heap_map_heaps_for_size_table_entry;

struct pas_dynamic_primitive_heap_map_heaps_for_size_table_entry {
    size_t size; /* This is just size of the first allocation performed. */
    unsigned num_heaps;
    unsigned capacity;
    pas_primitive_heap_ref** heaps;
};

static inline pas_dynamic_primitive_heap_map_heaps_for_size_table_entry
pas_dynamic_primitive_heap_map_heaps_for_size_table_entry_create_empty(void)
{
    pas_dynamic_primitive_heap_map_heaps_for_size_table_entry result;
    result.size = 0;
    result.num_heaps = UINT_MAX;
    result.capacity = 0;
    result.heaps = NULL;
    return result;
}

static inline pas_dynamic_primitive_heap_map_heaps_for_size_table_entry
pas_dynamic_primitive_heap_map_heaps_for_size_table_entry_create_deleted(void)
{
    pas_dynamic_primitive_heap_map_heaps_for_size_table_entry result;
    result.size = 0;
    result.num_heaps = UINT_MAX;
    result.capacity = UINT_MAX;
    result.heaps = NULL;
    return result;
}

static inline bool
pas_dynamic_primitive_heap_map_heaps_for_size_table_entry_is_empty_or_deleted(
    pas_dynamic_primitive_heap_map_heaps_for_size_table_entry entry)
{
    return entry.num_heaps == UINT_MAX;
}

static inline bool
pas_dynamic_primitive_heap_map_heaps_for_size_table_entry_is_empty(
    pas_dynamic_primitive_heap_map_heaps_for_size_table_entry entry)
{
    return entry.num_heaps == UINT_MAX
        && !entry.capacity;
}

static inline bool
pas_dynamic_primitive_heap_map_heaps_for_size_table_entry_is_deleted(
    pas_dynamic_primitive_heap_map_heaps_for_size_table_entry entry)
{
    return entry.num_heaps == UINT_MAX
        && entry.capacity == UINT_MAX;
}

static inline pas_dynamic_primitive_heap_map_heaps_for_size_table_key
pas_dynamic_primitive_heap_map_heaps_for_size_table_entry_get_key(
    pas_dynamic_primitive_heap_map_heaps_for_size_table_entry entry)
{
    return entry.size;
}

static inline unsigned
pas_dynamic_primitive_heap_map_heaps_for_size_table_key_get_hash(
    pas_dynamic_primitive_heap_map_heaps_for_size_table_key key)
{
    return pas_hash_intptr(key);
}

static inline bool
pas_dynamic_primitive_heap_map_heaps_for_size_table_key_is_equal(
    pas_dynamic_primitive_heap_map_heaps_for_size_table_key a,
    pas_dynamic_primitive_heap_map_heaps_for_size_table_key b)
{
    return a == b;
}

PAS_CREATE_HASHTABLE(pas_dynamic_primitive_heap_map_heaps_for_size_table,
                     pas_dynamic_primitive_heap_map_heaps_for_size_table_entry,
                     pas_dynamic_primitive_heap_map_heaps_for_size_table_key);

static inline unsigned pas_dynamic_primitive_heap_map_hash(const void *key, void* arg)
{
    PAS_TESTING_ASSERT(!arg);
    return (unsigned)(uintptr_t)key;
}

struct pas_dynamic_primitive_heap_map;
typedef struct pas_dynamic_primitive_heap_map pas_dynamic_primitive_heap_map;

struct pas_dynamic_primitive_heap_map {
    pas_primitive_heap_ref** heaps;
    unsigned num_heaps;
    unsigned heaps_capacity;
    pas_dynamic_primitive_heap_map_heaps_for_size_table heaps_for_size;
    pas_lock_free_read_ptr_ptr_hashtable heap_for_key;
    void (*constructor)(pas_primitive_heap_ref* heap,
                        pas_simple_type type);
    unsigned max_heaps_per_size;
    unsigned max_heaps;
};

#define PAS_DYNAMIC_PRIMITIVE_HEAP_MAP_INITIALIZER(passed_constructor) \
    ((pas_dynamic_primitive_heap_map){ \
        .heaps = NULL, \
        .num_heaps = 0, \
        .heaps_capacity = 0, \
        .heaps_for_size = PAS_HASHTABLE_INITIALIZER, \
        .heap_for_key = PAS_LOCK_FREE_READ_PTR_PTR_HASHTABLE_INITIALIZER, \
        .constructor = (passed_constructor), \
        .max_heaps_per_size = 1000, \
        .max_heaps = UINT_MAX \
    })

PAS_API pas_primitive_heap_ref*
pas_dynamic_primitive_heap_map_find_slow(pas_dynamic_primitive_heap_map* map,
                                         const void* key,
                                         size_t size);

/* WARNING: This thing is global but weird things happen if you use it with a mix of heap configs.
   Fortunately, we happen to not do that. */
static PAS_ALWAYS_INLINE pas_primitive_heap_ref*
pas_dynamic_primitive_heap_map_find(pas_dynamic_primitive_heap_map* map,
                                    const void* key,
                                    size_t size)
{
    static const bool verbose = false;
    
    const void* result;

    if (verbose)
        pas_log("Doing dynamic lookup.\n");

    result = pas_lock_free_read_ptr_ptr_hashtable_find(
        &map->heap_for_key,
        pas_dynamic_primitive_heap_map_hash,
        NULL,
        key);
    if (PAS_LIKELY(result))
        return (pas_primitive_heap_ref*)(uintptr_t)result;

    return pas_dynamic_primitive_heap_map_find_slow(map, key, size);
}

PAS_END_EXTERN_C;

#endif /* PAS_DYNAMIC_PRIMITIVE_HEAP_MAP_H */

