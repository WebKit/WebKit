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

#include "pas_config.h"

#if LIBPAS_ENABLED

#include "pas_dynamic_primitive_heap_map.h"

#include "pas_heap_lock.h"
#include "pas_immortal_heap.h"
#include "pas_large_utility_free_heap.h"
#include "pas_log.h"
#include "pas_random.h"

pas_primitive_heap_ref*
pas_dynamic_primitive_heap_map_find_slow(pas_dynamic_primitive_heap_map* map,
                                         const void* key,
                                         size_t size)
{
    static const bool verbose = false;
    
    const void* raw_result;
    pas_primitive_heap_ref* result;

    PAS_ASSERT(key);
    PAS_ASSERT(map->max_heaps_per_size);
    
    pas_heap_lock_lock();

    raw_result = pas_lock_free_read_ptr_ptr_hashtable_find(
        &map->heap_for_key,
        pas_dynamic_primitive_heap_map_hash,
        NULL,
        key);
    if (raw_result)
        result = (pas_primitive_heap_ref*)(uintptr_t)raw_result;
    else {
        pas_dynamic_primitive_heap_map_heaps_for_size_table_add_result add_result;
        pas_dynamic_primitive_heap_map_heaps_for_size_table_entry* heaps_for_size;

        if (verbose)
            pas_log("Looking up size = %zu\n", size);
        
        add_result = pas_dynamic_primitive_heap_map_heaps_for_size_table_add(
            &map->heaps_for_size, size, NULL, &pas_large_utility_free_heap_allocation_config);
        heaps_for_size = add_result.entry;
        if (add_result.is_new_entry) {
            heaps_for_size->size = size;
            heaps_for_size->num_heaps = 0;
            heaps_for_size->capacity = 4;
            heaps_for_size->heaps = pas_large_utility_free_heap_allocate(
                heaps_for_size->capacity * sizeof(pas_primitive_heap_ref*),
                "pas_dynamic_primitive_heap_map_heaps_for_size_table_entry/heaps");
        }

        if (heaps_for_size->num_heaps >= map->max_heaps_per_size) {
            if (verbose)
                pas_log("Returning existing heap for size.\n");

            PAS_ASSERT(heaps_for_size->num_heaps);

            /* It's possible that num_heaps is greater than
               map->max_heaps_per_size, if that configuration parameter was
               being dynamically changed. We try to allow that. */
        
            result = heaps_for_size->heaps[
                pas_get_random(pas_secure_random, heaps_for_size->num_heaps)];
        } else {
            if (map->num_heaps >= map->max_heaps) {
                if (verbose)
                    pas_log("Returning existing heap globally.\n");

                result = map->heaps[pas_get_random(pas_secure_random, map->num_heaps)];
            } else {
                pas_simple_type_with_key_data* key_data;
                
                if (verbose)
                    pas_log("Adding new heap.\n");

                if (heaps_for_size->num_heaps >= heaps_for_size->capacity) {
                    pas_primitive_heap_ref** new_heaps;
                    unsigned new_capacity;
                    
                    PAS_ASSERT(heaps_for_size->num_heaps == heaps_for_size->capacity);
                    PAS_ASSERT(
                        heaps_for_size->num_heaps < map->max_heaps_per_size);
                    
                    new_capacity = PAS_MIN(heaps_for_size->capacity * 2,
                                           map->max_heaps_per_size);
                    
                    new_heaps = pas_large_utility_free_heap_allocate(
                        new_capacity * sizeof(pas_primitive_heap_ref*),
                        "pas_dynamic_primitive_heap_map_heaps_for_size_table_entry/heaps");
                    
                    memcpy(new_heaps, heaps_for_size->heaps,
                           heaps_for_size->num_heaps * sizeof(pas_primitive_heap_ref*));
                    
                    pas_large_utility_free_heap_deallocate(
                        heaps_for_size->heaps,
                        heaps_for_size->capacity * sizeof(pas_primitive_heap_ref*));
                    
                    heaps_for_size->capacity = new_capacity;
                    heaps_for_size->heaps = new_heaps;
                }
                
                PAS_ASSERT(heaps_for_size->num_heaps < heaps_for_size->capacity);

                key_data = pas_immortal_heap_allocate(
                    sizeof(pas_simple_type_with_key_data),
                    "pas_dynamic_primitive_heap_map/type_with_key_data",
                    pas_object_allocation);
                key_data->simple_type = pas_simple_type_create(1, 1);
                key_data->key = key;
                
                result = pas_immortal_heap_allocate(
                    sizeof(pas_primitive_heap_ref), "pas_dnamic_primitive_heap_map/heap",
                    pas_object_allocation);
                map->constructor(result,
                                 pas_simple_type_create_with_key_data(key_data));
                heaps_for_size->heaps[heaps_for_size->num_heaps++] = result;

                if (map->num_heaps >= map->heaps_capacity) {
                    pas_primitive_heap_ref** new_heaps;
                    unsigned new_heaps_capacity;

                    PAS_ASSERT(map->num_heaps == map->heaps_capacity);

                    new_heaps_capacity = (map->heaps_capacity + 1) << 1;
                    new_heaps = pas_large_utility_free_heap_allocate(
                        sizeof(pas_primitive_heap_ref*) * new_heaps_capacity,
                        "pas_dynamic_primitive_heap_map/heaps");

                    memcpy(new_heaps, map->heaps, sizeof(pas_primitive_heap_ref*) * map->num_heaps);

                    pas_large_utility_free_heap_deallocate(
                        map->heaps,
                        sizeof(pas_primitive_heap_ref*) * map->heaps_capacity);

                    map->heaps = new_heaps;
                    map->heaps_capacity = new_heaps_capacity;
                }

                map->heaps[map->num_heaps++] = result;
            }
        }

        if (verbose)
            pas_log("result = %p\n", result);

        pas_lock_free_read_ptr_ptr_hashtable_set(
            &map->heap_for_key,
            pas_dynamic_primitive_heap_map_hash,
            NULL,
            key, result,
            pas_lock_free_read_ptr_ptr_hashtable_add_new);
    }
    
    pas_heap_lock_unlock();
    return result;
}

#endif /* LIBPAS_ENABLED */
