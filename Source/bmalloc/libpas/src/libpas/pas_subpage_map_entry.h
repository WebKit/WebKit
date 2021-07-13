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

#ifndef PAS_SUBPAGE_MAP_ENTRY_H
#define PAS_SUBPAGE_MAP_ENTRY_H

#include "pas_commit_mode.h"
#include "pas_lock.h"
#include "pas_page_malloc.h"

PAS_BEGIN_EXTERN_C;

struct pas_deferred_decommit_log;
struct pas_subpage_map_entry;
typedef struct pas_deferred_decommit_log pas_deferred_decommit_log;
typedef struct pas_subpage_map_entry pas_subpage_map_entry;

/* NOTE: This is 4-byte aligned! */
struct pas_subpage_map_entry {
    pas_lock commit_lock;

    /* This is like a page pointer, except that the low bits that would normally be zero for a page
       pointer (i.e. the page offset bits) are used as a bitmap. This constrains the possible system
       page size as follows:
       
           SYSTEM_PAGE_SIZE/log(SYSTEM_PAGE_SIZE) <= PAS_SUBPAGE_SIZE

       Which can be also written as (if we want to use integers):
    
           SYSTEM_PAGE_SIZE/PAS_SUBPAGE_SIZE <= log(SYSTEM_PAGE_SIZE)

       NOTE: This is not uintptr_t-aligned! We leverage the fact that this is always accessed under
       the commit lock and save some space. We also leverage the fact that the page full base bits
       are immutable, so reading those can be done at any time, without locking. */
    uintptr_t packed_value;
};

PAS_API pas_subpage_map_entry* pas_subpage_map_entry_create(void* full_base,
                                                            pas_commit_mode commit_mode);

static inline void* pas_subpage_map_entry_full_base(pas_subpage_map_entry* entry)
{
    return (void*)(entry->packed_value & -pas_page_malloc_alignment());
}

static inline unsigned pas_subpage_map_entry_bits(pas_subpage_map_entry* entry)
{
    uintptr_t result = entry->packed_value & (pas_page_malloc_alignment() - 1);
    PAS_ASSERT((unsigned)result == result);
    return (unsigned)result;
}

PAS_API void pas_subpage_map_assert_commit_state(pas_subpage_map_entry* entry,
                                                 void* base, size_t bytes,
                                                 pas_commit_mode commit_mode);

/* Have to already hold the commit lock to do this. */
PAS_API void pas_subpage_map_entry_commit(pas_subpage_map_entry* entry,
                                          void* base, size_t bytes);

/* Have to already hold the commit lock to do this. This will release the commit lock. */
PAS_API void pas_subpage_map_entry_decommit(pas_subpage_map_entry* entry,
                                            void* base, size_t bytes,
                                            pas_deferred_decommit_log* log,
                                            pas_lock_hold_mode heap_lock_hold_mode);

PAS_END_EXTERN_C;

#endif /* PAS_SUBPAGE_MAP_ENTRY_H */

