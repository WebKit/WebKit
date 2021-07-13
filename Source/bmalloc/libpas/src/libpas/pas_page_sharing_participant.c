/*
 * Copyright (c) 2019-2020 Apple Inc. All rights reserved.
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

#include "pas_page_sharing_participant.h"

#include "pas_biasing_directory.h"
#include "pas_bitfit_global_directory.h"
#include "pas_epoch.h"
#include "pas_heap_lock.h"
#include "pas_large_sharing_pool.h"
#include "pas_page_sharing_pool.h"
#include "pas_segregated_global_size_directory.h"
#include "pas_segregated_shared_page_directory.h"
#include "pas_segregated_size_directory.h"

pas_page_sharing_participant
pas_page_sharing_participant_create(void* ptr,
                                    pas_page_sharing_participant_kind kind)
{
    pas_page_sharing_participant result;
    
    result = (pas_page_sharing_participant)((uintptr_t)ptr | (uintptr_t)kind);
    
    PAS_ASSERT(pas_page_sharing_participant_get_ptr(result) == ptr);
    PAS_ASSERT(pas_page_sharing_participant_get_kind(result) == kind);
    
    return result;
}

pas_page_sharing_participant_payload*
pas_page_sharing_participant_get_payload(pas_page_sharing_participant participant)
{
    void* ptr = pas_page_sharing_participant_get_ptr(participant);
    switch (pas_page_sharing_participant_get_kind(participant)) {
    case pas_page_sharing_participant_null:
        PAS_ASSERT(!"Null participant has no payload.");
        return NULL;
    case pas_page_sharing_participant_segregated_shared_page_directory:
    case pas_page_sharing_participant_segregated_size_directory:
        return pas_segregated_directory_data_try_get_sharing_payload(
            pas_segregated_directory_data_ptr_load(&((pas_segregated_directory*)ptr)->data));
    case pas_page_sharing_participant_bitfit_directory:
        return &((pas_bitfit_global_directory*)ptr)->physical_sharing_payload;
    case pas_page_sharing_participant_biasing_directory:
        return &((pas_biasing_directory*)ptr)->bias_sharing_payload.base;
    case pas_page_sharing_participant_large_sharing_pool:
        return &pas_large_sharing_participant_payload.base;
    }
    PAS_ASSERT(!"Bad participant kind");
    return NULL;
}

void pas_page_sharing_participant_payload_construct(pas_page_sharing_participant_payload* payload)
{
    *payload = PAS_PAGE_SHARING_PARTICIPANT_PAYLOAD_INITIALIZER;
}

void pas_page_sharing_participant_payload_with_use_epoch_construct(
    pas_page_sharing_participant_payload_with_use_epoch* payload)
{
    *payload = PAS_PAGE_SHARING_PARTICIPANT_PAYLOAD_WITH_USE_EPOCH_INITIALIZER;
}

uint64_t pas_page_sharing_participant_get_use_epoch(pas_page_sharing_participant participant)
{
    void* ptr = pas_page_sharing_participant_get_ptr(participant);
    switch (pas_page_sharing_participant_get_kind(participant)) {
    case pas_page_sharing_participant_null:
        PAS_ASSERT(!"Null participant has no use epoch.");
        return 0;
    case pas_page_sharing_participant_segregated_shared_page_directory:
    case pas_page_sharing_participant_segregated_size_directory:
        return pas_segregated_directory_get_use_epoch(ptr);
    case pas_page_sharing_participant_bitfit_directory:
        return pas_bitfit_global_directory_get_use_epoch(ptr);
    case pas_page_sharing_participant_biasing_directory:
        return ((pas_biasing_directory*)ptr)->bias_sharing_payload.use_epoch;
    case pas_page_sharing_participant_large_sharing_pool:
        return pas_large_sharing_participant_payload.use_epoch;
    }
    PAS_ASSERT(!"Bad participant kind");
    return 0;
}

void pas_page_sharing_participant_set_parent_pool(pas_page_sharing_participant participant,
                                                  pas_page_sharing_pool* pool)
{
    PAS_ASSERT(pas_page_sharing_participant_get_parent_pool(participant) == pool);
}

pas_page_sharing_pool*
pas_page_sharing_participant_get_parent_pool(pas_page_sharing_participant participant)
{
    void* ptr;

    ptr = pas_page_sharing_participant_get_ptr(participant);

    switch (pas_page_sharing_participant_get_kind(participant)) {
    case pas_page_sharing_participant_null:
        PAS_ASSERT(!"Cannot get null participant's parent.");
        return NULL;
    case pas_page_sharing_participant_segregated_shared_page_directory:
    case pas_page_sharing_participant_segregated_size_directory:
    case pas_page_sharing_participant_bitfit_directory:
        return &pas_physical_page_sharing_pool;
    case pas_page_sharing_participant_biasing_directory:
        return pas_biasing_directory_get_sharing_pool(ptr);
    case pas_page_sharing_participant_large_sharing_pool:
        return &pas_physical_page_sharing_pool;
    }
    PAS_ASSERT(!"Bad participant kind");
    return NULL;
}

bool pas_page_sharing_participant_is_eligible(pas_page_sharing_participant participant)
{
    void* ptr;

    ptr = pas_page_sharing_participant_get_ptr(participant);

    switch (pas_page_sharing_participant_get_kind(participant)) {
    case pas_page_sharing_participant_null:
        PAS_ASSERT(!"Cannot check if null participant is eligible.");
        return false;
    case pas_page_sharing_participant_segregated_shared_page_directory:
    case pas_page_sharing_participant_segregated_size_directory:
        return !!pas_segregated_directory_get_last_empty_plus_one(ptr).value;
    case pas_page_sharing_participant_bitfit_directory:
        return !!((pas_bitfit_global_directory*)ptr)->last_empty_plus_one.value;
    case pas_page_sharing_participant_biasing_directory:
        return !!pas_biasing_directory_unused_span_size(ptr);
    case pas_page_sharing_participant_large_sharing_pool:
        return !!pas_large_sharing_min_heap_instance.size;
    }
    PAS_ASSERT(!"Bad participant kind");
    return false;
}

pas_page_sharing_pool_take_result
pas_page_sharing_participant_take_least_recently_used(
    pas_page_sharing_participant participant,
    pas_deferred_decommit_log* decommit_log,
    pas_lock_hold_mode heap_lock_hold_mode)
{
    void* ptr;
    pas_page_sharing_pool_take_result result;

    ptr = pas_page_sharing_participant_get_ptr(participant);

    switch (pas_page_sharing_participant_get_kind(participant)) {
    case pas_page_sharing_participant_null:
        PAS_ASSERT(!"Cannot take from null participant.");
        return false;
        
    case pas_page_sharing_participant_segregated_size_directory:
        return pas_segregated_global_size_directory_take_last_empty(
            ptr, decommit_log, heap_lock_hold_mode);

    case pas_page_sharing_participant_segregated_shared_page_directory:
        PAS_ASSERT(decommit_log);
        return pas_segregated_shared_page_directory_take_last_empty(
            ptr, decommit_log, heap_lock_hold_mode);

    case pas_page_sharing_participant_bitfit_directory:
        PAS_ASSERT(decommit_log);
        return pas_bitfit_global_directory_take_last_empty(ptr, decommit_log, heap_lock_hold_mode);
        
    case pas_page_sharing_participant_biasing_directory:
        PAS_ASSERT(!decommit_log);
        PAS_ASSERT(heap_lock_hold_mode == pas_lock_is_not_held);
        return pas_biasing_directory_take_last_unused(ptr);
        
    case pas_page_sharing_participant_large_sharing_pool:
        PAS_ASSERT(decommit_log);
        pas_heap_lock_lock_conditionally(heap_lock_hold_mode);
        result = pas_large_sharing_pool_decommit_least_recently_used(decommit_log);
        pas_heap_lock_unlock_conditionally(heap_lock_hold_mode);
        return result;
    }
    PAS_ASSERT(!"Bad participant kind");
    return pas_page_sharing_pool_take_none_available;
}

#endif /* LIBPAS_ENABLED */
