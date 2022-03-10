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

#ifndef PAS_HASHTABLE_H
#define PAS_HASHTABLE_H

#include "pas_allocation_config.h"
#include "pas_allocation_kind.h"
#include "pas_enumerator.h"
#include "pas_log.h"
#include "pas_utils.h"

PAS_BEGIN_EXTERN_C;

#define PAS_HASHTABLE_MIN_SIZE 16
#define PAS_HASHTABLE_MAX_LOAD 2
#define PAS_HASHTABLE_MIN_LOAD 6

/* This creates a hashtable type. You can construct it by just initializing to zero. */

#define PAS_HASHTABLE_INITIALIZER { \
        .table = NULL, \
        .table_size = 0, \
        .table_mask = 0, \
        .key_count = 0, \
        .deleted_count = 0 \
    }

#define PAS_CREATE_HASHTABLE(name, entry_type, key_type) \
    struct name; \
    struct name##_add_result; \
    struct name##_in_flux_stash; \
    typedef struct name name; \
    typedef struct name##_add_result name##_add_result; \
    typedef struct name##_in_flux_stash name##_in_flux_stash; \
    \
    struct name { \
        entry_type* table; \
        unsigned table_size; \
        unsigned table_mask; \
        unsigned key_count; \
        unsigned deleted_count; \
    }; \
    \
    struct name##_add_result { \
        entry_type* entry; \
        bool is_new_entry; \
    }; \
    \
    struct name##_in_flux_stash { \
        name* hashtable_being_resized; /* Not NULL if there is a hashtable being resized. */ \
        entry_type* table_before_resize; \
        size_t table_size_before_resize; \
        \
        entry_type* in_flux_entry; /* NOTE: If you use _add(), then you have to manage this yourself. */ \
    }; \
    \
    PAS_UNUSED static inline void name##_construct(name* table) \
    { \
        pas_zero_memory(table, sizeof(name)); \
    } \
    \
    PAS_UNUSED static inline void name##_destruct( \
        name* table, const pas_allocation_config* allocation_config) \
    { \
        allocation_config->deallocate( \
            table->table, table->table_size * sizeof(entry_type), pas_object_allocation, \
            allocation_config->arg); \
    } \
    \
    PAS_UNUSED static inline void name##_rehash( \
        name* table, unsigned new_size, name##_in_flux_stash* in_flux_stash, \
        const pas_allocation_config* allocation_config) \
    { \
        static const bool verbose = false; \
        \
        size_t new_byte_size; \
        size_t old_size; \
        entry_type* old_table; \
        entry_type* new_table; \
        unsigned index; \
        unsigned old_index; \
        unsigned new_table_mask; \
        \
        /* This is a table of large allocations, so the sizes are not going to be large enough */ \
        /* to induce overflow. */ \
        \
        PAS_ASSERT(pas_is_power_of_2(new_size)); \
        \
        new_table_mask = new_size - 1; \
        new_byte_size = (size_t)new_size * sizeof(entry_type); \
        if (verbose) \
            pas_log("Allocating a new table with new_size = %u, new_byte_size = %zu.\n", new_size, new_byte_size); \
        new_table = (entry_type*)allocation_config->allocate( \
            new_byte_size, #name "/table", pas_object_allocation, allocation_config->arg); \
        \
        for (index = new_size; index--;) \
            new_table[index] = entry_type##_create_empty(); \
        \
        old_table = table->table; \
        old_size = table->table_size; \
        \
        for (old_index = 0; old_index < old_size; ++old_index) { \
            entry_type* old_entry; \
            unsigned hash; \
            \
            old_entry = old_table + old_index; \
            if (entry_type##_is_empty_or_deleted(*old_entry)) \
                continue; \
            \
            for (hash = key_type##_get_hash(entry_type##_get_key(*old_entry)); ; ++hash) { \
                unsigned new_index; \
                entry_type* new_entry; \
                \
                new_index = hash & new_table_mask; \
                new_entry = new_table + new_index; \
                if (entry_type##_is_empty_or_deleted(*new_entry)) { \
                    *new_entry = *old_entry; \
                    break; \
                } \
            } \
        } \
        \
        if (in_flux_stash) { \
            PAS_TESTING_ASSERT(!in_flux_stash->table_before_resize); \
            PAS_TESTING_ASSERT(!in_flux_stash->table_size_before_resize); \
            PAS_TESTING_ASSERT(!in_flux_stash->hashtable_being_resized); \
            in_flux_stash->table_before_resize = old_table; \
            in_flux_stash->table_size_before_resize = old_size; \
            pas_compiler_fence(); /* When hashtable_being_resized is pointing at table, table_before_resize and table_size_before_resize need to be right values. */ \
            in_flux_stash->hashtable_being_resized = table; \
        } \
        pas_compiler_fence(); \
        /* We do not need to ensure the ordering of the following stores since in-flux-stash is effective while running this code. */ \
        table->table = new_table; \
        table->table_size = new_size; \
        table->table_mask = new_table_mask; \
        table->deleted_count = 0; \
        \
        pas_compiler_fence(); \
        if (in_flux_stash) { \
            in_flux_stash->hashtable_being_resized = NULL; \
            pas_compiler_fence(); /* We should clear hashtable_being_resized first to tell memory enumerator that in_flux_stash is no longer effective. */ \
            in_flux_stash->table_before_resize = NULL; \
            in_flux_stash->table_size_before_resize = 0; \
        } \
        \
        allocation_config->deallocate( \
            old_table, \
            old_size * sizeof(entry_type), \
            pas_object_allocation, \
            allocation_config->arg); \
    } \
    \
    PAS_UNUSED static inline void name##_expand( \
        name* table, name##_in_flux_stash* in_flux_stash, const pas_allocation_config* allocation_config) \
    { \
        unsigned new_size; \
        if (!table->table_size) \
            new_size = PAS_HASHTABLE_MIN_SIZE; \
        else if (table->key_count * PAS_HASHTABLE_MIN_LOAD < table->table_size * 2) \
            new_size = table->table_size; \
        else \
            new_size = table->table_size * 2; \
        name##_rehash(table, new_size, in_flux_stash, allocation_config); \
    } \
    \
    PAS_UNUSED static inline void name##_shrink( \
        name* table, name##_in_flux_stash* in_flux_stash, const pas_allocation_config* allocation_config) \
    { \
        name##_rehash(table, table->table_size / 2, in_flux_stash, allocation_config); \
    } \
    \
    PAS_UNUSED static inline entry_type* name##_find(name* hashtable, key_type key) \
    { \
        entry_type* table = hashtable->table; \
        if (!table) \
            return NULL; \
        \
        for (unsigned hash = key_type##_get_hash(key); ; ++hash) { \
            unsigned index; \
            entry_type* entry; \
            \
            index = hash & hashtable->table_mask; \
            entry = table + index; \
            \
            if (entry_type##_is_empty_or_deleted(*entry)) { \
                if (entry_type##_is_deleted(*entry)) \
                    continue; \
                return NULL; \
            } \
            if (key_type##_is_equal(entry_type##_get_key(*entry), key)) \
                return entry; \
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
    PAS_UNUSED static inline name##_add_result name##_add( \
        name* table, key_type key, name##_in_flux_stash* in_flux_stash, \
        const pas_allocation_config* allocation_config) \
    { \
        entry_type* entry; \
        entry_type* deleted_entry; \
        unsigned hash; \
        \
        name##_add_result result; \
        \
        if ((table->key_count + table->deleted_count) * PAS_HASHTABLE_MAX_LOAD \
            >= table->table_size) \
            name##_expand(table, in_flux_stash, allocation_config); \
        \
        deleted_entry = NULL; \
        \
        for (hash = key_type##_get_hash(key); ; ++hash) { \
            unsigned index = hash & table->table_mask; \
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
    PAS_UNUSED static inline void name##_add_new( \
        name* table, entry_type new_entry, name##_in_flux_stash* in_flux_stash, \
        const pas_allocation_config* allocation_config) \
    { \
        name##_add_result result = name##_add( \
            table, entry_type##_get_key(new_entry), in_flux_stash, allocation_config); \
        PAS_ASSERT(result.is_new_entry); \
        if (in_flux_stash) { \
            PAS_TESTING_ASSERT(!in_flux_stash->in_flux_entry); \
            in_flux_stash->in_flux_entry = result.entry; \
        } \
        pas_compiler_fence(); \
        *result.entry = new_entry; \
        pas_compiler_fence(); \
        if (in_flux_stash) \
            in_flux_stash->in_flux_entry = NULL; \
    } \
    \
    PAS_UNUSED static inline bool name##_set( \
        name* table, entry_type new_entry, name##_in_flux_stash* in_flux_stash, \
        const pas_allocation_config* allocation_config) \
    { \
        name##_add_result result = name##_add( \
            table, entry_type##_get_key(new_entry), in_flux_stash, allocation_config); \
        if (in_flux_stash) { \
            PAS_TESTING_ASSERT(!in_flux_stash->in_flux_entry); \
            in_flux_stash->in_flux_entry = result.entry; \
        } \
        pas_compiler_fence(); \
        *result.entry = new_entry; \
        pas_compiler_fence(); \
        if (in_flux_stash) \
            in_flux_stash->in_flux_entry = NULL; \
        return result.is_new_entry; \
    } \
    \
    PAS_UNUSED static inline bool name##_take_and_return_if_taken( \
        name* table, key_type key, entry_type* result, name##_in_flux_stash* in_flux_stash, \
        const pas_allocation_config* allocation_config) \
    { \
        entry_type* entry_ptr; \
        entry_type entry; \
        \
        entry_ptr = name##_find(table, key); \
        if (!entry_ptr) { \
            if (result) \
                *result = entry_type##_create_empty(); \
            return false; \
        } \
        \
        entry = *entry_ptr; \
        \
        *entry_ptr = entry_type##_create_deleted(); \
        \
        pas_compiler_fence(); \
        if (in_flux_stash) \
            in_flux_stash->in_flux_entry = NULL; \
        \
        table->deleted_count++; \
        table->key_count--; \
        if (table->key_count * PAS_HASHTABLE_MIN_LOAD < table->table_size \
            && table->table_size > PAS_HASHTABLE_MIN_SIZE) \
            name##_shrink(table, in_flux_stash, allocation_config); \
        \
        if (result) \
            *result = entry; \
        return true; \
    } \
    \
    PAS_UNUSED static inline entry_type name##_take( \
        name* table, key_type key, name##_in_flux_stash* in_flux_stash, \
        const pas_allocation_config* allocation_config) \
    { \
        entry_type result; \
        name##_take_and_return_if_taken(table, key, &result, in_flux_stash, allocation_config); \
        return result; \
    } \
    \
    PAS_UNUSED static inline bool name##_remove( \
        name* table, key_type key, name##_in_flux_stash* in_flux_stash, \
        const pas_allocation_config* allocation_config) \
    { \
        return name##_take_and_return_if_taken(table, key, NULL, in_flux_stash, allocation_config); \
    } \
    \
    PAS_UNUSED static inline void name##_delete( \
        name* table, key_type key, name##_in_flux_stash* in_flux_stash, \
        const pas_allocation_config* allocation_config) \
    { \
        bool result; \
        result = name##_remove(table, key, in_flux_stash, allocation_config); \
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
    typedef bool (*name##_for_each_entry_remote_callback)( \
        pas_enumerator* enumerator, entry_type* entry, void* arg); \
    \
    PAS_UNUSED static inline bool name##_for_each_entry_remote( \
        pas_enumerator* enumerator, name* remote_table, name##_in_flux_stash* remote_in_flux_stash, \
        name##_for_each_entry_remote_callback callback, void* arg) \
    { \
        name##_in_flux_stash* in_flux_stash; \
        entry_type* table_table; \
        size_t table_size; \
        size_t index; \
        \
        in_flux_stash = (name##_in_flux_stash*)pas_enumerator_read( \
            enumerator, remote_in_flux_stash, sizeof(name##_in_flux_stash)); \
        if (!in_flux_stash) \
            return false; \
        \
        if (in_flux_stash->hashtable_being_resized == remote_table) { \
            table_table = in_flux_stash->table_before_resize; \
            table_size = in_flux_stash->table_size_before_resize; \
        } else { \
            name* table; \
            \
            table = (name*)pas_enumerator_read(enumerator, remote_table, sizeof(name)); \
            if (!table) \
                return false; \
            \
            table_table = table->table; \
            table_size = table->table_size; \
        } \
        \
        if (!table_size) { \
            PAS_ASSERT(!table_table); \
            return true; \
        } \
        \
        table_table = (entry_type*)pas_enumerator_read( \
            enumerator, table_table, sizeof(entry_type) * table_size); \
        if (!table_table) \
            return false; \
        \
        for (index = table_size; index--;) { \
            entry_type* entry; \
            \
            entry = table_table + index; \
            if (entry == in_flux_stash->in_flux_entry) \
                continue; \
            if (entry_type##_is_empty_or_deleted(*entry)) \
                continue; \
            \
            if (!callback(enumerator, entry, arg)) \
                return false; \
        } \
        \
        return true; \
    } \
    \
    struct pas_dummy

PAS_END_EXTERN_C;

#endif /* PAS_HASHTABLE_H */

