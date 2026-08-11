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

#include "pas_large_expendable_memory.h"

#include "pas_bootstrap_free_heap.h"
#include "pas_heap_lock.h"

pas_large_expendable_memory* pas_large_expendable_memory_head;

static void allocate_new_large_expendable_memory(void)
{
    pas_large_expendable_memory* new_memory;
    pas_allocation_result new_memory_result;

    pas_heap_lock_assert_held();

    new_memory_result = pas_bootstrap_free_heap_allocate_with_alignment(
        PAS_LARGE_EXPENDABLE_MEMORY_TOTAL_SIZE,
        pas_alignment_create_traditional(PAS_LARGE_EXPENDABLE_MEMORY_ALIGNMENT),
        "pas_large_expendable_memory",
        pas_delegate_allocation);
    PAS_ASSERT(new_memory_result.did_succeed);
    PAS_ASSERT(new_memory_result.begin);
    new_memory = (pas_large_expendable_memory*)new_memory_result.begin;

    new_memory->next = pas_large_expendable_memory_head;
    pas_expendable_memory_construct(&new_memory->header, PAS_LARGE_EXPENDABLE_MEMORY_PAYLOAD_SIZE);

    pas_store_store_fence();

    pas_large_expendable_memory_head = new_memory;
}

void* pas_large_expendable_memory_allocate(size_t size, size_t alignment, const char* name)
{
    void* result;
    
    pas_heap_lock_assert_held();

    if (!pas_large_expendable_memory_head)
        allocate_new_large_expendable_memory();

    result = pas_expendable_memory_try_allocate(
        &pas_large_expendable_memory_head->header,
        pas_large_expendable_memory_payload(pas_large_expendable_memory_head),
        size, alignment, pas_large_expendable_heap_kind, name);
    if (result)
        return result;

    allocate_new_large_expendable_memory();

    return pas_expendable_memory_allocate(
        &pas_large_expendable_memory_head->header,
        pas_large_expendable_memory_payload(pas_large_expendable_memory_head),
        size, alignment, pas_large_expendable_heap_kind, name);
}

bool pas_large_expendable_memory_commit_if_necessary(void* object, size_t size)
{
    pas_large_expendable_memory* header;
    void* payload;

    pas_heap_lock_assert_held();

    header = pas_large_expendable_memory_header_for_object(object);
    payload = pas_large_expendable_memory_payload(header);

    return pas_expendable_memory_commit_if_necessary(&header->header, payload, object, size);
}

bool pas_large_expendable_memory_scavenge(pas_expendable_memory_scavenge_kind kind)
{
    pas_large_expendable_memory* memory;
    bool result;
    
    pas_heap_lock_assert_held();

    result = false;

    for (memory = pas_large_expendable_memory_head; memory; memory = memory->next) {
        result |= pas_expendable_memory_scavenge(
            &memory->header, pas_large_expendable_memory_payload(memory), kind);
    }

    return result;
}

#endif /* LIBPAS_ENABLED */

