/*
 * Copyright (c) 2019-2021 Apple Inc. All rights reserved.
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

#ifndef PAS_PAGE_SHARING_POOL_H
#define PAS_PAGE_SHARING_POOL_H

#include "pas_bootstrap_free_heap.h"
#include "pas_config.h"
#include "pas_lock.h"
#include "pas_min_heap.h"
#include "pas_page_sharing_participant.h"
#include "pas_page_sharing_pool_take_result.h"
#include "pas_segmented_vector.h"
#include "pas_utils.h"
#include "pas_versioned_field.h"

PAS_BEGIN_EXTERN_C;

struct pas_heap_config;
struct pas_page_base;
struct pas_page_base_config;
struct pas_page_sharing_pool;
typedef struct pas_heap_config pas_heap_config;
typedef struct pas_page_base pas_page_base;
typedef struct pas_page_base_config pas_page_base_config;
typedef struct pas_page_sharing_pool pas_page_sharing_pool;

PAS_DECLARE_SEGMENTED_VECTOR(pas_page_sharing_pool_segmented_delta_bitvector,
                             unsigned,
                             4);
PAS_DECLARE_SEGMENTED_VECTOR(pas_page_sharing_pool_segmented_participant_vector,
                             pas_page_sharing_participant,
                             4);

static inline int pas_page_sharing_participant_compare(pas_page_sharing_participant* left_ptr,
                                                       pas_page_sharing_participant* right_ptr)
{
    pas_page_sharing_participant_payload* left_payload;
    pas_page_sharing_participant_payload* right_payload;
    
    left_payload = pas_page_sharing_participant_get_payload(*left_ptr);
    right_payload = pas_page_sharing_participant_get_payload(*right_ptr);
    
    if (left_payload->use_epoch_for_min_heap < right_payload->use_epoch_for_min_heap)
        return -1;
    if (left_payload->use_epoch_for_min_heap == right_payload->use_epoch_for_min_heap)
        return 0;
    return 1;
}

static inline size_t pas_page_sharing_participant_get_index(pas_page_sharing_participant* ptr)
{
    return pas_page_sharing_participant_get_payload(*ptr)->index_in_sharing_pool_min_heap;
}

static inline void pas_page_sharing_participant_set_index(pas_page_sharing_participant* ptr,
                                                          size_t index)
{
    PAS_ASSERT((unsigned)index == index);
    pas_page_sharing_participant_get_payload(*ptr)->index_in_sharing_pool_min_heap = (unsigned)index;
}

PAS_CREATE_MIN_HEAP(
    pas_page_sharing_pool_min_heap,
    pas_page_sharing_participant,
    1,
    .compare = pas_page_sharing_participant_compare,
    .get_index = pas_page_sharing_participant_get_index,
    .set_index = pas_page_sharing_participant_set_index);

struct PAS_ALIGNED(sizeof(pas_versioned_field)) pas_page_sharing_pool {
    pas_versioned_field first_delta;
    
    pas_page_sharing_pool_segmented_delta_bitvector delta;
    pas_page_sharing_pool_segmented_participant_vector participants;
    pas_page_sharing_pool_min_heap participant_by_epoch;
    pas_page_sharing_participant current_participant;
};

PAS_API extern pas_page_sharing_pool pas_physical_page_sharing_pool;

/* Positive numbers indicate that we have extra physical pages that can be taken.
   
   Negative numbers indicate that we did not succeed at taking some number of bytes last time,
   so we should try this time. */
PAS_API extern intptr_t pas_physical_page_sharing_pool_balance;

PAS_API extern bool pas_physical_page_sharing_pool_balancing_enabled;
PAS_API extern bool pas_physical_page_sharing_pool_balancing_enabled_for_utility;

PAS_API void pas_page_sharing_pool_construct(pas_page_sharing_pool* pool);

PAS_API void pas_page_sharing_pool_add_at_index(pas_page_sharing_pool* pool,
                                                pas_page_sharing_participant participant,
                                                size_t index);

PAS_API void pas_page_sharing_pool_add(pas_page_sharing_pool* pool,
                                       pas_page_sharing_participant participant);

/* This is the low-level interface for taking things from the sharing pool. Usually you want to call
   pas_physical_page_sharing_pool_take, pas_physical_page_sharing_pool_scavenge,
   pas_physical_page_sharing_pool_take_for_page_config, or pas_bias_page_sharing_pool_take.
   
   This just takes the least recently used piece of memory (could be a page, a span of multiple pages,
   or even a set of disjoint pages). The amount of memory returned is going to be whatever is
   algorithmically convenient. So, usually you want to call this in a loop until you take the amount of
   memory you want.
   
   The max_epoch parameter is used as follows:
   
   - If max_epoch == 0, then this tries to take whatever is truly the least recently used piece of
     memory.
   
   - If max_epoch > 0, then this takes may take any piece of memory that is not newer than max_epoch.
     This may be the least recently used piece of memory or it may be something younger, if that's
     algorithmically convenient. */
PAS_API pas_page_sharing_pool_take_result
pas_page_sharing_pool_take_least_recently_used(
    pas_page_sharing_pool* pool,
    pas_deferred_decommit_log* decommit_log,
    pas_lock_hold_mode heap_lock_hold_mode,
    uint64_t max_epoch);

PAS_API void pas_physical_page_sharing_pool_take(
    size_t bytes,
    pas_lock_hold_mode heap_lock_hold_mode,
    pas_lock** locks_already_held,
    size_t num_locks_already_held);

/* This returns the excuse for why it stopped scavenging. */
PAS_API pas_page_sharing_pool_take_result
pas_physical_page_sharing_pool_scavenge(uint64_t max_epoch);

PAS_API void pas_physical_page_sharing_pool_take_later(size_t bytes);
PAS_API void pas_physical_page_sharing_pool_give_back(size_t bytes);

PAS_API void pas_physical_page_sharing_pool_take_for_page_config(
    size_t bytes,
    pas_page_base_config* page_config,
    pas_lock_hold_mode heap_lock_hold_mode,
    pas_lock** locks_already_held,
    size_t num_locks_already_held);

PAS_API bool pas_bias_page_sharing_pool_take(pas_page_sharing_pool* pool);

PAS_API void pas_page_sharing_pool_did_create_delta(pas_page_sharing_pool* pool,
                                                    pas_page_sharing_participant participant);

/* Some functions for white-box testing. */
PAS_API void pas_page_sharing_pool_verify(pas_page_sharing_pool* pool, 
                                          pas_lock_hold_mode heap_lock_hold_mode);
PAS_API bool pas_page_sharing_pool_has_delta(pas_page_sharing_pool* pool);
PAS_API bool pas_page_sharing_pool_has_current_participant(pas_page_sharing_pool* pool);

static inline size_t pas_page_sharing_pool_num_participants(pas_page_sharing_pool* pool)
{
    if (!pool)
        return 0;
    return pool->participants.size;
}

static inline pas_page_sharing_participant pas_page_sharing_pool_get_participant(
    pas_page_sharing_pool* pool, size_t index)
{
    size_t size;

    if (!pool)
        return NULL;
    
    size = pool->participants.size;
    if (index >= size)
        return NULL;

    return *pas_page_sharing_pool_segmented_participant_vector_get_ptr(
        &pool[pas_depend(size)].participants, index);
}

PAS_END_EXTERN_C;

#endif /* PAS_PAGE_SHARING_POOL_H */

