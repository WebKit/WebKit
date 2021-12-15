/*
 * Copyright (c) 2019-2021 Apple Inc. All rights reserved.
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

#ifndef PAS_LOCK_FREE_READ_PTR_PTR_HASHTABLE_H
#define PAS_LOCK_FREE_READ_PTR_PTR_HASHTABLE_H

#include "pas_log.h"
#include "pas_utils.h"
#include <unistd.h>

PAS_BEGIN_EXTERN_C;

struct pas_lock_free_read_ptr_ptr_hashtable;
struct pas_lock_free_read_ptr_ptr_hashtable_table;
typedef struct pas_lock_free_read_ptr_ptr_hashtable pas_lock_free_read_ptr_ptr_hashtable;
typedef struct pas_lock_free_read_ptr_ptr_hashtable_table pas_lock_free_read_ptr_ptr_hashtable_table;

struct pas_lock_free_read_ptr_ptr_hashtable {
    pas_lock_free_read_ptr_ptr_hashtable_table* table;
};

struct PAS_ALIGNED(sizeof(pas_pair)) pas_lock_free_read_ptr_ptr_hashtable_table {
    pas_lock_free_read_ptr_ptr_hashtable_table* previous;
    unsigned table_size;
    unsigned table_mask;
    unsigned key_count;
    pas_pair array[1];
};

#define PAS_LOCK_FREE_READ_PTR_PTR_HASHTABLE_INITIALIZER \
    ((pas_lock_free_read_ptr_ptr_hashtable){ \
         .table = NULL \
     })

#define PAS_LOCK_FREE_READ_PTR_PTR_HASHTABLE_ENABLE_COLLISION_COUNT 0

#if PAS_LOCK_FREE_READ_PTR_PTR_HASHTABLE_ENABLE_COLLISION_COUNT
PAS_API extern uint64_t pas_lock_free_read_ptr_ptr_hashtable_collision_count;
#endif /* PAS_LOCK_FREE_READ_PTR_PTR_HASHTABLE_ENABLE_COLLISION_COUNT */

static PAS_ALWAYS_INLINE void* pas_lock_free_read_ptr_ptr_hashtable_find(
    pas_lock_free_read_ptr_ptr_hashtable* hashtable,
    unsigned (*hash_key)(const void* key, void* arg),
    void* hash_arg,
    const void* key)
{
    pas_lock_free_read_ptr_ptr_hashtable_table* table;

    table = hashtable->table;
    if (!table)
        return NULL;
    
    for (unsigned hash = hash_key(key, hash_arg); ; ++hash) {
        unsigned index;
        pas_pair* entry;
        uintptr_t loaded_key;

        index = hash & table->table_mask;

        entry = table->array + index;

        /* It's crazy, but we *can* load the two words separately. They do have to happen in the
           right order, though. Otherwise it's possible to get a NULL value even though the key
           was already set.
        
           NOTE: Perf would be better if we did an atomic pair read on Apple Silicon. Then we'd
           avoid the synthetic pointer chase. */
        loaded_key = pas_pair_low(*entry);
        if (pas_compare_ptr_opaque(loaded_key, (uintptr_t)key))
            return (void*)pas_pair_high(entry[pas_depend(loaded_key)]);

        if (loaded_key == UINTPTR_MAX)
            return NULL;

#if PAS_LOCK_FREE_READ_PTR_PTR_HASHTABLE_ENABLE_COLLISION_COUNT
        for (;;) {
            uint64_t old_collision_count;
            uint64_t new_collision_count;

            old_collision_count = pas_lock_free_read_ptr_ptr_hashtable_collision_count;
            new_collision_count = old_collision_count + 1;

            if (pas_compare_and_swap_uint64_weak(
                    &pas_lock_free_read_ptr_ptr_hashtable_collision_count,
                    old_collision_count, new_collision_count)) {
                if (!(new_collision_count % 10000))
                    pas_log("%d: Saw %llu collisions.\n", getpid(), new_collision_count);
                break;
            }
        }
#endif /* PAS_LOCK_FREE_READ_PTR_PTR_HASHTABLE_ENABLE_COLLISION_COUNT */
    }
}

enum pas_lock_free_read_ptr_ptr_hashtable_set_mode {
    pas_lock_free_read_ptr_ptr_hashtable_add_new,
    pas_lock_free_read_ptr_ptr_hashtable_set_maybe_existing
};

typedef enum pas_lock_free_read_ptr_ptr_hashtable_set_mode pas_lock_free_read_ptr_ptr_hashtable_set_mode;

PAS_API void pas_lock_free_read_ptr_ptr_hashtable_set(
    pas_lock_free_read_ptr_ptr_hashtable* hashtable,
    unsigned (*hash_key)(const void* key, void* arg),
    void* hash_arg,
    const void* key,
    const void* value,
    pas_lock_free_read_ptr_ptr_hashtable_set_mode set_mode);

static inline unsigned pas_lock_free_read_ptr_ptr_hashtable_size(
    pas_lock_free_read_ptr_ptr_hashtable* hashtable)
{
    return hashtable->table->key_count;
}

PAS_END_EXTERN_C;

#endif /* PAS_LOCK_FREE_READ_PTR_PTR_HASHTABLE_H */

