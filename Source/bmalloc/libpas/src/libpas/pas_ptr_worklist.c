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

#include "pas_ptr_worklist.h"

void pas_ptr_worklist_construct(pas_ptr_worklist* worklist)
{
    pas_ptr_hash_set_construct(&worklist->seen);
    worklist->worklist = NULL;
    worklist->worklist_size = 0;
    worklist->worklist_capacity = 0;
}

void pas_ptr_worklist_destruct(pas_ptr_worklist* worklist,
                               const pas_allocation_config* allocation_config)
{
    pas_ptr_hash_set_destruct(&worklist->seen, allocation_config);
    if (worklist->worklist) {
        allocation_config->deallocate(worklist->worklist,
                                      sizeof(void*) * worklist->worklist_capacity,
                                      pas_object_allocation,
                                      allocation_config->arg);
    }
}

bool pas_ptr_worklist_push(pas_ptr_worklist* worklist,
                           void* ptr,
                           const pas_allocation_config* allocation_config)
{
    pas_ptr_hash_set_add_result add_result;

    if (!ptr)
        return false;

    add_result = pas_ptr_hash_set_add(&worklist->seen, ptr, NULL, allocation_config);
    if (!add_result.is_new_entry)
        return false;

    *add_result.entry = ptr;

    if (worklist->worklist_size >= worklist->worklist_capacity) {
        void* new_worklist;
        size_t new_worklist_capacity;
        
        PAS_ASSERT(worklist->worklist_size == worklist->worklist_capacity);

        new_worklist_capacity = (worklist->worklist_capacity + 1) << 1;
        new_worklist = allocation_config->allocate(sizeof(void*) * new_worklist_capacity,
                                                   "pas_ptr_worklist/worklist",
                                                   pas_object_allocation,
                                                   allocation_config->arg);
        memcpy(new_worklist, worklist->worklist, sizeof(void*) * worklist->worklist_size);
        allocation_config->deallocate(worklist->worklist,
                                      sizeof(void*) * worklist->worklist_capacity,
                                      pas_object_allocation,
                                      allocation_config->arg);
        worklist->worklist = new_worklist;
        worklist->worklist_capacity = new_worklist_capacity;

        PAS_ASSERT(worklist->worklist_size < worklist->worklist_capacity);
    }

    worklist->worklist[worklist->worklist_size++] = ptr;
    return true;
}

void* pas_ptr_worklist_pop(pas_ptr_worklist* worklist)
{
    if (!worklist->worklist_size)
        return NULL;

    return worklist->worklist[--worklist->worklist_size];
}

#endif /* LIBPAS_ENABLED */
