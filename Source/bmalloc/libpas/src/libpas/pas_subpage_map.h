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

#ifndef PAS_SUBPAGE_MAP_H
#define PAS_SUBPAGE_MAP_H

#include "pas_compact_subpage_map_entry_ptr.h"
#include "pas_hashtable.h"
#include "pas_subpage_map_entry.h"

PAS_BEGIN_EXTERN_C;

typedef pas_compact_subpage_map_entry_ptr pas_subpage_map_hashtable_entry;
typedef void* pas_subpage_map_hashtable_key;

static inline pas_subpage_map_hashtable_entry pas_subpage_map_hashtable_entry_create_empty(void)
{
    pas_subpage_map_hashtable_entry result = PAS_COMPACT_PTR_INITIALIZER;
    return result;
}

static inline pas_subpage_map_hashtable_entry pas_subpage_map_hashtable_entry_create_deleted(void)
{
    pas_subpage_map_hashtable_entry result = PAS_COMPACT_PTR_INITIALIZER;
    PAS_ASSERT(!"Should never created a deleted subpage map hashtable entry (and it's impossible)");
    return result;
}

static inline bool
pas_subpage_map_hashtable_entry_is_empty_or_deleted(pas_subpage_map_hashtable_entry entry)
{
    return pas_compact_subpage_map_entry_ptr_is_null(&entry);
}

static inline bool
pas_subpage_map_hashtable_entry_is_empty(pas_subpage_map_hashtable_entry entry)
{
    return pas_compact_subpage_map_entry_ptr_is_null(&entry);
}

static inline bool
pas_subpage_map_hashtable_entry_is_deleted(pas_subpage_map_hashtable_entry entry)
{
    PAS_UNUSED_PARAM(entry);
    return false;
}

static inline void* pas_subpage_map_hashtable_entry_get_key(pas_subpage_map_hashtable_entry entry)
{
    return pas_subpage_map_entry_full_base(pas_compact_subpage_map_entry_ptr_load(&entry));
}

static unsigned pas_subpage_map_hashtable_key_get_hash(void* key)
{
    return pas_hash_ptr(key);
}

static bool pas_subpage_map_hashtable_key_is_equal(void* a, void* b)
{
    return a == b;
}

PAS_CREATE_HASHTABLE(pas_subpage_map_hashtable,
                     pas_subpage_map_hashtable_entry,
                     pas_subpage_map_hashtable_key);

extern PAS_API pas_subpage_map_hashtable pas_subpage_map_hashtable_instance;

/* This initializes an entry for a system (aka full) page in the map based on an allocation of
   zero or more subpages. This has some implications:

   - If you allocate memory for a subpage somehow, then you must also allocate the rest of the
     memory in the full page as subpages.

   - The commit state of all subpages within the full page must start the same way. So, if you
     allocate the first subpage in a full page and tell the map that it's committed, then whenever
     you do get around to allocating the other subpages, those must also start committed. It's OK
     if in the meantime the first subpage got decommitted; the point is that whatever the start
     state was (committed in the example) must also be the start state for all other pages. That's
     because if the subpage map is told about a full page for the first time in the form of a
     call to this that initializes some subpage then the map will initialize the commit state of
     the entire full page to whatever you told this.

   - The bytes cannot be so big that this requires more than one full page. */
PAS_API pas_subpage_map_entry* pas_subpage_map_add(void* base,
                                                   size_t bytes,
                                                   pas_commit_mode commit_mode,
                                                   pas_lock_hold_mode heap_lock_hold_mode);

PAS_API pas_subpage_map_entry* pas_subpage_map_get(void* base,
                                                   size_t bytes,
                                                   pas_lock_hold_mode heap_lock_hold_mode);

/* This is a helper that subpage_map and subpage_map_entry share. */
PAS_API void* pas_subpage_map_get_full_base(void* base, size_t bytes);

PAS_END_EXTERN_C;

#endif /* PAS_SUBPAGE_MAP_H */

