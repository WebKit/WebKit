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

#include "pas_config.h"

#if LIBPAS_ENABLED

#include "pas_lock_free_read_ptr_ptr_hashtable.h"

#include "pas_bootstrap_free_heap.h"
#include "pas_hashtable.h"
#include "pas_heap_lock.h"

void pas_lock_free_read_ptr_ptr_hashtable_set(
    pas_lock_free_read_ptr_ptr_hashtable* hashtable,
    unsigned (*hash_key)(const void* key, void* arg),
    void* hash_arg,
    const void* key,
    const void* value,
    pas_lock_free_read_ptr_ptr_hashtable_set_mode set_mode)
{
    pas_lock_free_read_ptr_ptr_hashtable_table* table;
    unsigned hash;

    PAS_ASSERT(key);
    pas_heap_lock_assert_held();

    table = hashtable->table;

    if (!table || table->key_count * PAS_HASHTABLE_MAX_LOAD >= table->table_size) {
        unsigned new_size;
        size_t new_byte_size;
        pas_lock_free_read_ptr_ptr_hashtable_table* new_table;
        unsigned old_index;
        unsigned new_table_mask;
        
        if (table)
            new_size = table->table_size * 2;
        else
            new_size = PAS_HASHTABLE_MIN_SIZE;

        PAS_ASSERT(pas_is_power_of_2(new_size));

        new_table_mask = new_size - 1;
        new_byte_size =
            PAS_OFFSETOF(pas_lock_free_read_ptr_ptr_hashtable_table, array) +
            sizeof(pas_pair) * new_size;
        new_table = (void*)pas_bootstrap_free_heap_allocate_with_alignment(
            new_byte_size,
            pas_alignment_create_traditional(sizeof(pas_pair)),
            "pas_lock_free_read_ptr_ptr_hashtable/table",
            pas_object_allocation).begin;

        memset(new_table, -1, new_byte_size);

        new_table->previous = table;

        if (table) {
            for (old_index = 0; old_index < table->table_size; ++old_index) {
                pas_pair* old_entry;
                unsigned hash;
                
                old_entry = table->array + old_index;
                if (old_entry->low == UINTPTR_MAX)
                    continue;
                
                for (hash = hash_key((const void*)old_entry->low, hash_arg); ; ++hash) {
                    unsigned new_index;
                    pas_pair* new_entry;
                    
                    new_index = hash & new_table_mask;
                    new_entry = new_table->array + new_index;
                    
                    if (new_entry->low == UINTPTR_MAX) {
                        /* Can do this without atomics because the old table is frozen, the old_entry
                           is frozen if non-null even if the old table wasn't, and the new table is
                           still private to this thread. */
                        *new_entry = *old_entry;
                        break;
                    }
                }
            }
        }
        
        new_table->table_size = new_size;
        new_table->table_mask = new_table_mask;
        new_table->key_count = table ? table->key_count : 0;

        pas_fence();
        
        hashtable->table = new_table;
        table = new_table;
    }

    for (hash = hash_key(key, hash_arg); ; ++hash) {
        unsigned index;
        pas_pair* entry;

        index = hash & table->table_mask;
        entry = table->array + index;

        if (entry->low == UINTPTR_MAX) {
            pas_atomic_store_pair(entry, pas_pair_create((uintptr_t)key,
                                                         (uintptr_t)value));
            table->key_count++;
            break;
        }

        if (entry->low == (uintptr_t)key) {
            PAS_ASSERT(set_mode == pas_lock_free_read_ptr_ptr_hashtable_set_maybe_existing);
            entry->high = (uintptr_t)value;
            break;
        }

        PAS_ASSERT((const void*)entry->low != key);
    }
}

#endif /* LIBPAS_ENABLED */
