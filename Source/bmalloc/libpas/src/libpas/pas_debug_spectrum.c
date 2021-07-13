/*
 * Copyright (c) 2021 Apple Inc. All rights reserved.
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

#include "pas_debug_spectrum.h"

#include "pas_heap_lock.h"
#include "pas_immortal_heap.h"
#include "pas_large_utility_free_heap.h"
#include "pas_stream.h"

pas_ptr_hash_map pas_debug_spectrum = PAS_HASHTABLE_INITIALIZER;

void pas_debug_spectrum_add(
    void* key, pas_debug_spectrum_dump_key dump, uint64_t count)
{
    pas_ptr_hash_map_add_result add_result;
    pas_debug_spectrum_entry* entry;
    
    pas_heap_lock_assert_held();

    add_result = pas_ptr_hash_map_add(
        &pas_debug_spectrum, key, NULL, &pas_large_utility_free_heap_allocation_config);

    if (add_result.is_new_entry) {
        entry = pas_immortal_heap_allocate(
            sizeof(pas_debug_spectrum_entry),
            "pas_debug_spectrum_entry",
            pas_object_allocation);
        entry->dump = dump;
        entry->count = count;
        add_result.entry->key = key;
        add_result.entry->value = entry;
        return;
    }

    entry = add_result.entry->value;

    PAS_ASSERT(entry->dump == dump);
    entry->count += count;
}

void pas_debug_spectrum_dump(pas_stream* stream)
{
    unsigned index;

    pas_heap_lock_assert_held();

    for (index = 0; index < pas_debug_spectrum.table_size; ++index) {
        pas_ptr_hash_map_entry hash_entry;
        pas_debug_spectrum_entry* entry;

        hash_entry = pas_debug_spectrum.table[index];
        if (pas_ptr_hash_map_entry_is_empty_or_deleted(hash_entry))
            continue;

        entry = hash_entry.value;

        if (!entry->count)
            continue;

        entry->dump(stream, hash_entry.key);
        pas_stream_printf(stream, ": %llu\n", entry->count);
    }
}

void pas_debug_spectrum_reset(void)
{
    unsigned index;

    pas_heap_lock_assert_held();

    for (index = 0; index < pas_debug_spectrum.table_size; ++index) {
        pas_ptr_hash_map_entry hash_entry;
        pas_debug_spectrum_entry* entry;

        hash_entry = pas_debug_spectrum.table[index];
        if (pas_ptr_hash_map_entry_is_empty_or_deleted(hash_entry))
            continue;

        entry = hash_entry.value;
        entry->count = 0;
    }
}

#endif /* LIBPAS_ENABLED */
