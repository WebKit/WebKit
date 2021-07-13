/*
 * Copyright (c) 2020 Apple Inc. All rights reserved.
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

#include "pas_subpage_map_entry.h"

#include "pas_bitvector.h"
#include "pas_deferred_decommit_log.h"
#include "pas_immortal_heap.h"
#include "pas_internal_config.h"
#include <stdalign.h>

static void set_packed_value(pas_subpage_map_entry* entry,
                             void* full_base,
                             unsigned bits)
{
    entry->packed_value = (uintptr_t)full_base | bits;
    PAS_ASSERT(pas_subpage_map_entry_full_base(entry) == full_base);
    PAS_ASSERT(pas_subpage_map_entry_bits(entry) == bits);
}

pas_subpage_map_entry* pas_subpage_map_entry_create(void* full_base,
                                                    pas_commit_mode commit_mode)
{
    pas_subpage_map_entry* entry;
    unsigned bits;

    PAS_ASSERT(!((uintptr_t)full_base & (pas_page_malloc_alignment() - 1)));
    PAS_ASSERT(pas_page_malloc_alignment() / PAS_SUBPAGE_SIZE <= pas_page_malloc_alignment_shift());

    entry = pas_immortal_heap_allocate_with_manual_alignment(
        sizeof(pas_subpage_map_entry),
        alignof(pas_lock),
        "pas_subpage_map_entry",
        pas_object_allocation);

    pas_lock_construct(&entry->commit_lock);

    bits = 0;
    if (commit_mode) {
        uintptr_t index;
        for (index = pas_page_malloc_alignment() / PAS_SUBPAGE_SIZE; index--;)
            pas_bitvector_set_in_one_word(&bits, index, true);
    }

    set_packed_value(entry, full_base, bits);

    pas_fence();

    return entry;
}

static void prepare_indices(pas_subpage_map_entry* entry,
                            void* base, size_t bytes,
                            void** full_base,
                            uintptr_t* begin,
                            uintptr_t* end)
{
    *full_base = pas_subpage_map_entry_full_base(entry);

    *begin = ((uintptr_t)base - (uintptr_t)*full_base) / PAS_SUBPAGE_SIZE;
    *end = *begin + bytes / PAS_SUBPAGE_SIZE;
}

void pas_subpage_map_assert_commit_state(pas_subpage_map_entry* entry,
                                         void* base, size_t bytes,
                                         pas_commit_mode commit_mode)
{
    /* This doesn't bother with locking on the grounds that we don't have out-of-thin-air values.
       We're only asserting commit state when adding another subpage to a full page that we
       previously created, and we want to assert that nobody changed the commit state of those
       pages in the meantime. Even if the commit state of other subpages is changing concurrently,
       the lack of out-of-thin-air values means that the bits we're asserting about will still be
       stuck in the state that we expected them to be in. */

    void* full_base;
    uintptr_t begin;
    uintptr_t end;
    uintptr_t index;
    unsigned bits;

    prepare_indices(entry, base, bytes, &full_base, &begin, &end);

    bits = pas_subpage_map_entry_bits(entry);

    for (index = end; index-- > begin;)
        PAS_ASSERT(pas_bitvector_get_from_one_word(&bits, index) == !!commit_mode);
}

static void set_bits(pas_subpage_map_entry* entry,
                     void* base, size_t bytes,
                     pas_commit_mode commit_mode,
                     pas_deferred_decommit_log* log,
                     pas_lock_hold_mode heap_lock_hold_mode)
{
    void* full_base;
    uintptr_t begin;
    uintptr_t end;
    uintptr_t index;
    unsigned old_bits;
    unsigned bits;

    pas_lock_assert_held(&entry->commit_lock);

    prepare_indices(entry, base, bytes, &full_base, &begin, &end);

    old_bits = pas_subpage_map_entry_bits(entry);
    bits = old_bits;

    for (index = end; index-- > begin;)
        pas_bitvector_set_in_one_word(&bits, index, commit_mode);

    set_packed_value(entry, full_base, bits);

    if (!old_bits && bits) {
        PAS_ASSERT(commit_mode);
        pas_page_malloc_commit(full_base, pas_page_malloc_alignment());
        return;
    }

    if (old_bits && !bits) {
        PAS_ASSERT(!commit_mode);
        pas_deferred_decommit_log_add_maybe_locked(
            log,
            pas_virtual_range_create(
                (uintptr_t)full_base, (uintptr_t)full_base + pas_page_malloc_alignment(),
                &entry->commit_lock),
            pas_range_is_locked,
            heap_lock_hold_mode);
        return;
    }

    if (!commit_mode)
        pas_deferred_decommit_log_unlock_after_aborted_add(log, &entry->commit_lock);
}

void pas_subpage_map_entry_commit(pas_subpage_map_entry* entry,
                                  void* base, size_t bytes)
{
    /* NOTE: pas_lock_is_not_held can be a lie since set_bits does not use that value unless
       we are decommitting. */
    set_bits(entry, base, bytes, pas_committed, NULL, pas_lock_is_not_held);
}

/* Have to already hold the commit lock to do this. */
void pas_subpage_map_entry_decommit(pas_subpage_map_entry* entry,
                                    void* base, size_t bytes,
                                    pas_deferred_decommit_log* log,
                                    pas_lock_hold_mode heap_lock_hold_mode)
{
    set_bits(entry, base, bytes, pas_decommitted, log, heap_lock_hold_mode);
}

#endif /* LIBPAS_ENABLED */
