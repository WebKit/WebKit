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

#include "pas_config.h"

#if LIBPAS_ENABLED

#include "pas_large_map.h"

#include "pas_large_heap.h"
#include "pas_large_utility_free_heap.h"

pas_large_map_hashtable pas_large_map_hashtable_instance = PAS_HASHTABLE_INITIALIZER;
pas_large_map_hashtable_in_flux_stash pas_large_map_hashtable_instance_in_flux_stash;
pas_small_large_map_hashtable pas_small_large_map_hashtable_instance = PAS_HASHTABLE_INITIALIZER;
pas_small_large_map_hashtable_in_flux_stash pas_small_large_map_hashtable_instance_in_flux_stash;
pas_tiny_large_map_hashtable pas_tiny_large_map_hashtable_instance = PAS_HASHTABLE_INITIALIZER;
pas_tiny_large_map_hashtable_in_flux_stash pas_tiny_large_map_hashtable_instance_in_flux_stash;
pas_tiny_large_map_second_level_hashtable_in_flux_stash pas_tiny_large_map_second_level_hashtable_in_flux_stash_instance;

pas_large_map_entry pas_large_map_find(uintptr_t begin)
{
    PAS_PROFILE(LARGE_MAP_FIND, begin);

    uintptr_t tiny_base;
    pas_first_level_tiny_large_map_entry* first_level_tiny_entry;
    pas_small_large_map_entry* small_entry;

    pas_heap_lock_assert_held();

    tiny_base = pas_tiny_large_map_entry_base(begin);
    first_level_tiny_entry = pas_tiny_large_map_hashtable_find(
        &pas_tiny_large_map_hashtable_instance, tiny_base);
    if (first_level_tiny_entry) {
        pas_tiny_large_map_entry* tiny_entry;
        PAS_ASSERT(first_level_tiny_entry->base == tiny_base);
        tiny_entry = pas_tiny_large_map_second_level_hashtable_find(
            first_level_tiny_entry->hashtable, begin - tiny_base);
        if (tiny_entry) {
            PAS_ASSERT(pas_tiny_large_map_entry_begin(*tiny_entry, tiny_base) == begin);
            return pas_tiny_large_map_entry_get_entry(*tiny_entry, tiny_base);
        }
    }

    small_entry = pas_small_large_map_hashtable_find(
        &pas_small_large_map_hashtable_instance, begin);
    if (small_entry)
        return pas_small_large_map_entry_get_entry(*small_entry);

    return pas_large_map_hashtable_get(&pas_large_map_hashtable_instance, begin);
}

void pas_large_map_add(pas_large_map_entry entry)
{
    static const bool verbose = PAS_SHOULD_LOG(PAS_LOG_LARGE_HEAPS);
    
    pas_heap_lock_assert_held();

    PAS_PROFILE(LARGE_MAP_ADD, entry.begin, entry.end);

    if (verbose)
        pas_log("large map adding %p...%p, heap = %p.\n", (void*)entry.begin, (void*)entry.end, entry.heap);

    if (pas_tiny_large_map_entry_can_create(entry)) {
        pas_tiny_large_map_hashtable_add_result add_result;
        uintptr_t tiny_base;
        pas_tiny_large_map_entry tiny_entry;

        if (verbose)
            pas_log("large map can be tiny.\n");

        tiny_base = pas_tiny_large_map_entry_base(entry.begin);

        tiny_entry = pas_tiny_large_map_entry_create(entry);

        if (verbose)
            pas_log("large map adding base = %p.\n", (void*)tiny_base);

        add_result = pas_tiny_large_map_hashtable_add(
            &pas_tiny_large_map_hashtable_instance, tiny_base,
            &pas_tiny_large_map_hashtable_instance_in_flux_stash,
            &pas_large_utility_free_heap_allocation_config);

        if (add_result.is_new_entry) {
            if (verbose)
                pas_log("large map allocating table.\n");
            pas_tiny_large_map_hashtable_instance_in_flux_stash.in_flux_entry = add_result.entry;
            pas_compiler_fence();
            add_result.entry->base = tiny_base;
            add_result.entry->hashtable =
                pas_utility_heap_allocate(sizeof(pas_tiny_large_map_second_level_hashtable),
                                          "pas_tiny_large_map_second_level_hashtable");
            pas_tiny_large_map_second_level_hashtable_construct(add_result.entry->hashtable);
            pas_compiler_fence();
            pas_tiny_large_map_hashtable_instance_in_flux_stash.in_flux_entry = NULL;
        }

        pas_tiny_large_map_second_level_hashtable_add_new(
            add_result.entry->hashtable, tiny_entry,
            &pas_tiny_large_map_second_level_hashtable_in_flux_stash_instance,
            &pas_large_utility_free_heap_allocation_config);
        return;
    }

    if (pas_small_large_map_entry_can_create(entry)) {
        pas_small_large_map_entry small_entry;

        if (verbose)
            pas_log("large map can be small.\n");

        small_entry = pas_small_large_map_entry_create(entry);
        
        pas_small_large_map_hashtable_add_new(
            &pas_small_large_map_hashtable_instance, small_entry,
            &pas_small_large_map_hashtable_instance_in_flux_stash,
            &pas_large_utility_free_heap_allocation_config);
        return;
    }

    if (verbose)
        pas_log("large map gotta be large.\n");

    pas_large_map_hashtable_add_new(
        &pas_large_map_hashtable_instance, entry,
        &pas_large_map_hashtable_instance_in_flux_stash,
        &pas_large_utility_free_heap_allocation_config);
}

pas_large_map_entry pas_large_map_take(uintptr_t begin)
{
    static const bool verbose = PAS_SHOULD_LOG(PAS_LOG_LARGE_HEAPS);
    
    uintptr_t tiny_base;
    pas_first_level_tiny_large_map_entry* first_level_tiny_entry;
    pas_small_large_map_entry small_entry;

    PAS_PROFILE(LARGE_MAP_TAKE, begin);

    pas_heap_lock_assert_held();

    if (verbose)
        pas_log("large map taking begin = %p.\n", (void*)begin);

    tiny_base = pas_tiny_large_map_entry_base(begin);
    if (verbose)
        pas_log("large map tiny_base = %p.\n", (void*)tiny_base);
    first_level_tiny_entry = pas_tiny_large_map_hashtable_find(
        &pas_tiny_large_map_hashtable_instance, tiny_base);
    if (first_level_tiny_entry) {
        pas_tiny_large_map_second_level_hashtable* second_hashtable;
        pas_tiny_large_map_entry tiny_entry;
        if (verbose)
            pas_log("large map found first level tiny.\n");
        PAS_ASSERT(first_level_tiny_entry->base == tiny_base);
        second_hashtable = first_level_tiny_entry->hashtable;
        if (pas_tiny_large_map_second_level_hashtable_take_and_return_if_taken(
                second_hashtable, begin - tiny_base, &tiny_entry,
                &pas_tiny_large_map_second_level_hashtable_in_flux_stash_instance,
                &pas_large_utility_free_heap_allocation_config)) {
            if (verbose)
                pas_log("large map found second level tiny.\n");
            PAS_ASSERT(pas_tiny_large_map_entry_begin(tiny_entry, tiny_base) == begin);
            if (!pas_tiny_large_map_second_level_hashtable_size(second_hashtable)) {
                pas_tiny_large_map_hashtable_delete(
                    &pas_tiny_large_map_hashtable_instance, tiny_base,
                    &pas_tiny_large_map_hashtable_instance_in_flux_stash,
                    &pas_large_utility_free_heap_allocation_config);
                pas_tiny_large_map_second_level_hashtable_destruct(
                    second_hashtable, &pas_large_utility_free_heap_allocation_config);
                pas_utility_heap_deallocate(second_hashtable);
            }
            return pas_tiny_large_map_entry_get_entry(tiny_entry, tiny_base);
        }
    }

    if (pas_small_large_map_hashtable_take_and_return_if_taken(
            &pas_small_large_map_hashtable_instance, begin, &small_entry,
            &pas_small_large_map_hashtable_instance_in_flux_stash,
            &pas_large_utility_free_heap_allocation_config))
        return pas_small_large_map_entry_get_entry(small_entry);

    return pas_large_map_hashtable_take(
        &pas_large_map_hashtable_instance, begin,
        &pas_large_map_hashtable_instance_in_flux_stash,
        &pas_large_utility_free_heap_allocation_config);
}

bool pas_large_map_for_each_entry(pas_large_map_for_each_entry_callback callback,
                                  void* arg)
{
    size_t index;

    for (index = pas_large_map_hashtable_entry_index_end(&pas_large_map_hashtable_instance);
         index--;) {
        pas_large_map_entry entry;
        entry = *pas_large_map_hashtable_entry_at_index(&pas_large_map_hashtable_instance,
                                                        index);
        if (pas_large_map_entry_is_empty_or_deleted(entry))
            continue;
        if (!callback(entry, arg))
            return false;
    }

    for (index = pas_small_large_map_hashtable_entry_index_end(
             &pas_small_large_map_hashtable_instance);
         index--;) {
        pas_small_large_map_entry entry;
        entry = *pas_small_large_map_hashtable_entry_at_index(
            &pas_small_large_map_hashtable_instance,
            index);
        if (pas_small_large_map_entry_is_empty_or_deleted(entry))
            continue;
        if (!callback(pas_small_large_map_entry_get_entry(entry), arg))
            return false;
    }

    for (index = pas_tiny_large_map_hashtable_entry_index_end(
             &pas_tiny_large_map_hashtable_instance);
         index--;) {
        pas_first_level_tiny_large_map_entry entry;
        size_t second_index;
        entry = *pas_tiny_large_map_hashtable_entry_at_index(&pas_tiny_large_map_hashtable_instance,
                                                             index);
        if (pas_first_level_tiny_large_map_entry_is_empty_or_deleted(entry))
            continue;
        for (second_index = pas_tiny_large_map_second_level_hashtable_entry_index_end(
                 entry.hashtable);
             second_index--;) {
            pas_tiny_large_map_entry second_entry;
            second_entry = *pas_tiny_large_map_second_level_hashtable_entry_at_index(entry.hashtable,
                                                                                     second_index);
            if (pas_tiny_large_map_entry_is_empty_or_deleted(second_entry))
                continue;
            if (!callback(pas_tiny_large_map_entry_get_entry(second_entry, entry.base), arg))
                return false;
        }
    }

    return true;
}

#endif /* LIBPAS_ENABLED */
