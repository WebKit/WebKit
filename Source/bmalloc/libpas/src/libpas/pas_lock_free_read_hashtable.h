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

#ifndef PAS_LOCK_FREE_READ_HASHTABLE_H
#define PAS_LOCK_FREE_READ_HASHTABLE_H

#include "pas_hashtable.h"

PAS_BEGIN_EXTERN_C;

/* This creates a hashtable type. You can construct it by just initializing to zero. */

#define PAS_LOCK_FREE_READ_HASHTABLE_INITIALIZER { \
        .table = NULL, \
        .table_size = 0, \
        .table_mask = 0, \
        .key_count = 0, \
        .deleted_count = 0 \
    }

#define PAS_CREATE_LOCK_FREE_READ_HASHTABLE(name, entry_type, key_type) \
    struct name; \
    struct name##_table; \
    struct name##_add_result; \
    typedef struct name name; \
    typedef struct name##_table name##_table; \
    typedef struct name##_add_result name##_add_result; \
    \
    struct name { \
        name##_table* table; \
    }; \
    \
    struct PAS_ALIGNED(sizeof(entry_type)) name##_table { \
        name##_table* previous; /* Just to make it not look like a leak. */ \
        unsigned table_size; \
        unsigned table_mask; \
        unsigned key_count; \
        unsigned deleted_count; \
        entry_type table[1]; \
    }; \
    \
    struct name##_add_result { \
        entry_type* entry; \
        bool is_new_entry; \
    }; \
    \
    PAS_UNUSED static inline void name##_construct(name* table) \
    { \
        pas_zero_memory(table, sizeof(name)); \
    } \
    \
    PAS_UNUSED static inline void name##_rehash(name* hashtable, unsigned new_size) \
    { \
        static const bool verbose = false; \
        \
        size_t new_byte_size; \
        name##_table* table;
        name##_table* new_table; \
        unsigned index; \
        unsigned old_index; \
        unsigned new_table_mask; \
        \
        pas_heap_lock_assert_held(); \
        \
        /* This is a table of large allocations, so the sizes are not going to be large enough */ \
        /* to induce overflow. */ \
        \
        PAS_ASSERT(pas_is_power_of_2(new_size)); \
        \
        table = hashtable->table; \
        \
        new_table_mask = new_size - 1; \
        new_byte_size = PAS_OFFSETOF(name##_table, table) + (size_t)new_size * sizeof(entry_type); \
        if (verbose) \
            pas_log("Allocating a new table with new_size = %zu, new_byte_size = %zu.\n", new_size, new_byte_size); \
        new_table = (name##_table*)(void*)pas_bootstrap_free_heap_allocate_with_alignment( \
            new_byte_size, \
            pas_alignment_create_traditional(sizeof(entry_type)), \
            #name "/table",
            pas_object_allocation).begin;
        \
        for (index = new_size; index--;) \
            new_table->table[index] = entry_type##_create_empty(); \
        \
        for (old_index = 0; old_index < table->table_size; ++old_index) { \
            entry_type* old_entry; \
            unsigned hash; \
            \
            old_entry = table->table + old_index; \
            if (entry_type##_is_empty_or_deleted(*old_entry)) \
                continue; \
            \
            for (hash = key_type##_get_hash(entry_type##_get_key(*old_entry)); ; ++hash) { \
                unsigned new_index; \
                entry_type* new_entry; \
                \
                new_index = hash & new_table_mask; \
                new_entry = new_table->table + new_index; \
                if (entry_type##_is_empty_or_deleted(*new_entry)) { \
                    *new_entry = *old_entry; \
                    break; \
                } \
            } \
        } \
        \
        new_table->table_size = new_size; \
        new_table->table_mask = 0; \
        new_table->deleted_count = 0; \
        new_table->key_count = table->key_count; \
        \
        pas_fence(); \
        \
        hashtable->table = table; \
    } \
    \
    PAS_UNUSED static inline void name##_expand(name* hashtable) \
    { \
        name##_table* table; \
        unsigned new_size; \
        \
        pas_heap_lock_assert_held(); \
        \
        table = hashtable->table; \
        \
        if (!table->table_size) \
            new_size = PAS_HASHTABLE_MIN_SIZE; \
        else if (table->key_count * PAS_HASHTABLE_MIN_LOAD < table->table_size * 2) \
            new_size = table->table_size; \
        else \
            new_size = table->table_size * 2; \
        name##_rehash(hashtable, new_size); \
    } \
    \
    PAS_UNUSED static inline void name##_shrink(name* hashtable) \
    { \
        name##_table* table; \
        \
        pas_heap_lock_assert_held(); \
        \
        table = hashtable->table; \
        name##_rehash(hashtable, table->table_size / 2); \
    } \
    \
    PAS_UNUSED static inline entry_type* name##_find(name* hashtable, key_type key) \
    { \
        name##_table* table; \
        \
        table = hashtable->table; \
        \
        if (!table) \
            return NULL; \
        \
        for (unsigned hash = key_type##_get_hash(key); ; ++hash) { \
            unsigned index; \
            entry_type* entry_ptr; \
            entry_type##_loaded_key_before_value loaded_key_before_value; \
            \
            index = hash & table->table_mask; \
            entry_ptr = table->table + index; \
            \
            loaded_key_before_value = entry_type##_load_key_before_value(entry_ptr); \
            \
            if (entry_type##_loaded_key_before_value_is_empty_or_deleted(loaded_key_before_value)) { \
                if (entry_type##_loaded_key_before_value_is_deleted(loaded_key_before_value)) \
                    continue; \
                return NULL; \
            } \
            if (key_type##_is_equal( \
                    entry_type##_loaded_key_before_value_get_key(loaded_key_before_value), key)) \
                return entry_type##_loaded_key_before_value_get_entry(loaded_key_before_value); \
        } \
    } \
    PAS_UNUSED static inline entry_type name##_get(name* hashtable, key_type key) \
    { \
        entry_type* result; \
        result = name##_find(hashtable, key); \
        if (!result) \
            return entry_type##_create_empty(); \
        return *result; \
    } \
    \
    PAS_UNUSED static inline name##_add_result name##_add(name* hashtable, key_type key) \
    { \
        name##_table* table; \
        entry_type* entry; \
        entry_type* deleted_entry; \
        unsigned hash; \
        name##_add_result result; \
        \
        pas_heap_lock_assert_held(); \
        \
        table = hashtable->table; \
        \
        if ((table->key_count + table->deleted_count) * PAS_HASHTABLE_MAX_LOAD \
            >= table->table_size) \
            name##_expand(hashtable); \
        \
        deleted_entry = NULL; \
        \
        for (hash = key_type##_get_hash(key); ; ++hash) { \
            unsigned index; \
            index = hash & table->table_mask; \
            entry = table->table + index; \
            if (entry_type##_is_empty(*entry)) \
                break; \
            if (entry_type##_is_deleted(*entry)) { \
                if (!deleted_entry) \
                    deleted_entry = entry; \
                continue; \
            } \
            if (key_type##_is_equal(entry_type##_get_key(*entry), key)) { \
                result.entry = entry; \
                result.is_new_entry = false; \
                return result; \
            } \
        } \
        \
        if (deleted_entry) { \
            table->deleted_count--; \
            entry = deleted_entry; \
        } \
        table->key_count++; \
        \
        result.entry = entry; \
        result.is_new_entry = true; \
        return result; \
    } \
    \
    PAS_UNUSED static inline void name##_add_new(name* table, entry_type new_entry) \
    { \
        name##_add_result result = name##_add(table, entry_type##_get_key(new_entry)); \
        PAS_ASSERT(result.is_new_entry); \
        entry_type##_store_atomic(result.entry, new_entry); \
    } \
    \
    PAS_UNUSED static inline bool name##_set(name* table, entry_type new_entry) \
    { \
        name##_add_result result = name##_add(table, entry_type##_get_key(new_entry)); \
        entry_type##_store_atomic(result.entry, new_entry); \
        return result.is_new_entry; \
    } \
    \
    PAS_UNUSED static inline bool name##_take_impl(name* table, key_type key, entry_type* result) \
    { \
        entry_type* entry_ptr; \
        entry_type entry; \
        \
        entry_ptr = name##_find(table, key); \
        if (!entry_ptr) { \
            *result = entry_type##_create_empty(); \
            return false; \
        } \
        \
        entry = *entry_ptr; \
        entry_type##_store_atomic(entry_ptr, entry_type##_create_deleted()); \
        table->deleted_count++; \
        table->key_count--; \
        if (table->key_count * PAS_HASHTABLE_MIN_LOAD < table->table_size \
            && table->table_size > PAS_HASHTABLE_MIN_SIZE) \
            name##_shrink(table); \
        \
        *result = entry; \
        return true; \
    } \
    \
    PAS_UNUSED static inline entry_type name##_take(name* table, key_type key) \
    { \
        entry_type result; \
        name##_take_impl(table, key, &result); \
        return result; \
    } \
    \
    PAS_UNUSED static inline bool name##_remove(name* table, key_type key) \
    { \
        entry_type result; \
        return name##_take_impl(table, key, &result); \
    } \
    \
    PAS_UNUSED static inline void name##_delete(name* table, key_type key) \
    { \
        bool result; \
        result = name##_remove(table, key); \
        PAS_ASSERT(result); \
    } \
    \
    typedef bool (*name##_for_each_entry_callback)(entry_type* entry, void* arg); \
    \
    PAS_UNUSED static inline bool name##_for_each_entry(name* table, \
                                                        name##_for_each_entry_callback callback, \
                                                        void* arg) \
    { \
        unsigned index; \
        \
        for (index = 0; index < table->table_size; ++index) { \
            entry_type* entry; \
            \
            entry = table->table + index; \
            if (entry_type##_is_empty_or_deleted(*entry)) \
                continue; \
            \
            if (!callback(entry, arg)) \
                return false; \
        } \
        \
        return true; \
    } \
    \
    PAS_UNUSED static inline size_t name##_size(name* table) \
    { \
        return table->key_count; \
    } \
    \
    PAS_UNUSED static inline size_t name##_entry_index_end(name* table) \
    { \
        return table->table_size; \
    } \
    \
    PAS_UNUSED static inline entry_type* name##_entry_at_index(name* table, \
                                                               size_t index) \
    { \
        return table->table + index; \
    } \
    \
    struct pas_dummy

PAS_END_EXTERN_C;

#endif /* PAS_LOCK_FREE_READ_HASHTABLE_H */

