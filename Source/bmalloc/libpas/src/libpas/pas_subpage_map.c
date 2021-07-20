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

#include "pas_subpage_map.h"

#include "pas_heap_lock.h"
#include "pas_internal_config.h"
#include "pas_large_utility_free_heap.h"

pas_subpage_map_hashtable pas_subpage_map_hashtable_instance;

pas_subpage_map_entry* pas_subpage_map_add(void* base,
                                           size_t bytes,
                                           pas_commit_mode commit_mode,
                                           pas_lock_hold_mode heap_lock_hold_mode)
{
    void* full_base;
    pas_subpage_map_hashtable_add_result add_result;
    pas_subpage_map_entry* entry;

    PAS_ASSERT(base);
    full_base = pas_subpage_map_get_full_base(base, bytes);
    
    pas_heap_lock_lock_conditionally(heap_lock_hold_mode);

    add_result = pas_subpage_map_hashtable_add(
        &pas_subpage_map_hashtable_instance, full_base, NULL,
        &pas_large_utility_free_heap_allocation_config);
    if (add_result.is_new_entry) {
        pas_compact_subpage_map_entry_ptr_store(
            add_result.entry, pas_subpage_map_entry_create(full_base, commit_mode));
    }

    entry = pas_compact_subpage_map_entry_ptr_load_non_null(add_result.entry);
    pas_subpage_map_assert_commit_state(entry, base, bytes, commit_mode);
    
    pas_heap_lock_unlock_conditionally(heap_lock_hold_mode);

    return entry;
}

pas_subpage_map_entry* pas_subpage_map_get(void* base,
                                           size_t bytes,
                                           pas_lock_hold_mode heap_lock_hold_mode)
{
    pas_compact_subpage_map_entry_ptr* entry;
    void* full_base;

    PAS_ASSERT(base);

    full_base = pas_subpage_map_get_full_base(base, bytes);

    pas_heap_lock_lock_conditionally(heap_lock_hold_mode);

    entry = pas_subpage_map_hashtable_find(&pas_subpage_map_hashtable_instance, full_base);

    pas_heap_lock_unlock_conditionally(heap_lock_hold_mode);

    PAS_ASSERT(entry);
    return pas_compact_subpage_map_entry_ptr_load_non_null(entry);
}

void* pas_subpage_map_get_full_base(void* base, size_t bytes)
{
    void* full_base;
    
    PAS_ASSERT(!((uintptr_t)base & (PAS_SUBPAGE_SIZE - 1)));
    PAS_ASSERT(!((uintptr_t)bytes & (PAS_SUBPAGE_SIZE - 1)));

    full_base = (void*)((uintptr_t)base & -pas_page_malloc_alignment());

    PAS_ASSERT((uintptr_t)base + bytes <= (uintptr_t)full_base + pas_page_malloc_alignment());

    return full_base;
}

#endif /* LIBPAS_ENABLED */
