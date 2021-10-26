/*
 * Copyright (c) 2020-2021 Apple Inc. All rights reserved.
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

#include "pas_enumerate_large_heaps.h"

#include "pas_enumerator_internal.h"
#include "pas_large_map.h"
#include "pas_range_begin_min_heap.h"
#include "pas_root.h"

static bool range_list_iterate_add_large_payload_callback(pas_enumerator* enumerator,
                                                          pas_range range,
                                                          void* arg)
{
    pas_range_begin_min_heap* payloads;

    payloads = arg;

    PAS_ASSERT(!pas_range_is_empty(range));

    pas_range_begin_min_heap_add(payloads, range, &enumerator->allocation_config);
    return true;
}

static void record_span(pas_enumerator* enumerator,
                        pas_range range)
{
    static const bool verbose = false;
    if (verbose)
        pas_log("record_span: %p...%p\n", (void*)range.begin, (void*)range.end);
    pas_enumerator_record(
        enumerator, (void*)range.begin, pas_range_size(range), pas_enumerator_payload_record);
}

static bool large_map_hashtable_entry_callback(
    pas_enumerator* enumerator, pas_large_map_entry* entry, void* arg)
{
    PAS_ASSERT(!arg);
    
    pas_enumerator_record(
        enumerator, (void*)entry->begin, entry->end - entry->begin, pas_enumerator_object_record);
    
    return true;
}

static bool small_large_map_hashtable_entry_callback(
    pas_enumerator* enumerator, pas_small_large_map_entry* entry, void* arg)
{
    uintptr_t begin;
    uintptr_t end;
    
    PAS_ASSERT(!arg);

    begin = pas_small_large_map_entry_begin(*entry);
    end = pas_small_large_map_entry_end(*entry);

    pas_enumerator_record(enumerator, (void*)begin, end - begin, pas_enumerator_object_record);

    return true;
}

static bool tiny_large_map_second_level_hashtable_entry_callback(
    pas_enumerator* enumerator, pas_tiny_large_map_entry* second_entry, void* arg)
{
    uintptr_t first_entry_base;
    uintptr_t begin;
    uintptr_t end;

    first_entry_base = (uintptr_t)arg;
    begin = pas_tiny_large_map_entry_begin(*second_entry, first_entry_base);
    end = pas_tiny_large_map_entry_end(*second_entry, first_entry_base);

    pas_enumerator_record(enumerator, (void*)begin, end - begin, pas_enumerator_object_record);

    return true;
}

static bool tiny_large_map_hashtable_entry_callback(
    pas_enumerator* enumerator, pas_first_level_tiny_large_map_entry* entry, void* arg)
{
    PAS_ASSERT(!arg);
    
    return pas_tiny_large_map_second_level_hashtable_for_each_entry_remote(
        enumerator, entry->hashtable,
        enumerator->root->tiny_large_map_second_level_hashtable_in_flux_stash_instance,
        tiny_large_map_second_level_hashtable_entry_callback, (void*)entry->base);
}

bool pas_enumerate_large_heaps(pas_enumerator* enumerator)
{
    static const bool verbose = false;
    
    pas_range_begin_min_heap payloads;
    pas_range span;
    pas_range range;

    pas_range_begin_min_heap_construct(&payloads);

    if (!pas_enumerable_range_list_iterate_remote(
            enumerator->root->large_heap_physical_page_sharing_cache_page_list,
            enumerator,
            range_list_iterate_add_large_payload_callback,
            &payloads))
        return false;

    if (verbose)
        pas_log("Payloads size = %zu\n", payloads.size);

    span = pas_range_create_empty();
    while (!pas_range_is_empty(range = pas_range_begin_min_heap_take_min(&payloads))) {
        uintptr_t page;

        for (page = range.begin; page < range.end; page += enumerator->root->page_malloc_alignment) {
            PAS_ASSERT(page);

            if (verbose)
                pas_log("Looking at page %p\n", (void*)page);
            
            if (!pas_enumerator_exclude_accounted_page(enumerator, (void*)page))
                continue;

            if (verbose)
                pas_log("    Recording as payload.\n");

            if (page != span.end) {
                record_span(enumerator, span);
                span.begin = page;
            }
            span.end = page + enumerator->root->page_malloc_alignment;
        }
    }
    record_span(enumerator, span);

    if (enumerator->record_object) {
        if (!pas_large_map_hashtable_for_each_entry_remote(
                enumerator,
                enumerator->root->large_map_hashtable_instance,
                enumerator->root->large_map_hashtable_instance_in_flux_stash,
                large_map_hashtable_entry_callback,
                NULL))
            return false;
        
        if (!pas_small_large_map_hashtable_for_each_entry_remote(
                enumerator,
                enumerator->root->small_large_map_hashtable_instance,
                enumerator->root->small_large_map_hashtable_instance_in_flux_stash,
                small_large_map_hashtable_entry_callback,
                NULL))
            return false;
        
        if (!pas_tiny_large_map_hashtable_for_each_entry_remote(
                enumerator,
                enumerator->root->tiny_large_map_hashtable_instance,
                enumerator->root->tiny_large_map_hashtable_instance_in_flux_stash,
                tiny_large_map_hashtable_entry_callback,
                NULL))
            return false;
    }

    return true;
}

#endif /* LIBPAS_ENABLED */
