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

#include "pas_compact_expendable_memory.h"

#include "pas_compact_bootstrap_free_heap.h"
#include "pas_heap_lock.h"

pas_compact_expendable_memory pas_compact_expendable_memory_header;
void* pas_compact_expendable_memory_payload;

void* pas_compact_expendable_memory_allocate(size_t size,
                                             size_t alignment,
                                             const char* name)
{
    pas_heap_lock_assert_held();
    
    PAS_ASSERT(!!pas_compact_expendable_memory_header.header.size == !!pas_compact_expendable_memory_payload);

    if (!pas_compact_expendable_memory_payload) {
        pas_allocation_result new_memory_result;

        new_memory_result = pas_compact_bootstrap_free_heap_allocate_with_alignment(
            PAS_COMPACT_EXPENDABLE_MEMORY_PAYLOAD_SIZE,
            pas_alignment_create_traditional(PAS_EXPENDABLE_MEMORY_PAGE_SIZE),
            "pas_large_expendable_memory",
            pas_delegate_allocation);
        PAS_ASSERT(new_memory_result.did_succeed);
        PAS_ASSERT(new_memory_result.begin);
        
        pas_compact_expendable_memory_payload = (void*)new_memory_result.begin;
        pas_expendable_memory_construct(&pas_compact_expendable_memory_header.header,
                                        PAS_COMPACT_EXPENDABLE_MEMORY_PAYLOAD_SIZE);
    }

    PAS_ASSERT(pas_compact_expendable_memory_header.header.size);
    PAS_ASSERT(pas_compact_expendable_memory_payload);

    return pas_expendable_memory_allocate(&pas_compact_expendable_memory_header.header,
                                          pas_compact_expendable_memory_payload,
                                          size, alignment, pas_compact_expendable_heap_kind, name);
}

bool pas_compact_expendable_memory_commit_if_necessary(void* object, size_t size)
{
    pas_heap_lock_assert_held();
    
    PAS_ASSERT(pas_compact_expendable_memory_header.header.size);
    PAS_ASSERT(pas_compact_expendable_memory_payload);

    return pas_expendable_memory_commit_if_necessary(&pas_compact_expendable_memory_header.header,
                                                     pas_compact_expendable_memory_payload,
                                                     object, size);
}

bool pas_compact_expendable_memory_scavenge(pas_expendable_memory_scavenge_kind kind)
{
    pas_heap_lock_assert_held();

    PAS_ASSERT(!!pas_compact_expendable_memory_header.header.size == !!pas_compact_expendable_memory_payload);

    if (!pas_compact_expendable_memory_payload)
        return false;

    return pas_expendable_memory_scavenge(&pas_compact_expendable_memory_header.header,
                                          pas_compact_expendable_memory_payload,
                                          kind);
}

#endif /* LIBPAS_ENABLED */
