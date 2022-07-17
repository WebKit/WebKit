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

#include "pas_config.h"

#if LIBPAS_ENABLED

#include "pas_page_sharing_pool.h"

#include "pas_bootstrap_free_heap.h"
#include "pas_deferred_decommit_log.h"
#include "pas_epoch.h"
#include "pas_log.h"
#include "pas_page_base.h"
#include "pas_page_base_config_inlines.h"
#include "pas_physical_memory_transaction.h"
#include "pas_scavenger.h"
#include "pas_utility_heap_config.h"

pas_page_sharing_pool pas_physical_page_sharing_pool = {
    .first_delta = PAS_VERSIONED_FIELD_INITIALIZER,
    .delta = PAS_SEGMENTED_VECTOR_INITIALIZER,
    .participants = PAS_SEGMENTED_VECTOR_INITIALIZER,
    .current_participant = NULL,
    .participant_by_epoch = PAS_MIN_HEAP_INITIALIZER(pas_page_sharing_pool_min_heap)
};

intptr_t pas_physical_page_sharing_pool_balance = 0;

bool pas_physical_page_sharing_pool_balancing_enabled = PAS_PAGE_BALANCING_ENABLED;
bool pas_physical_page_sharing_pool_balancing_enabled_for_utility = false;

static void verify_first_delta(pas_page_sharing_pool* pool)
{
    size_t actual_first_delta;
    size_t index;
    
    actual_first_delta = pool->participants.size;
    
    for (index = 0; index < pool->participants.size; ++index) {
        unsigned* word_ptr;
        
        word_ptr = pas_page_sharing_pool_segmented_delta_bitvector_get_ptr(
            &pool->delta, PAS_BITVECTOR_WORD_INDEX(index));
        
        if (pas_bitvector_get_from_word(*word_ptr, index)) {
            actual_first_delta = index;
            break;
        }
    }
    
    PAS_ASSERT(actual_first_delta == pool->first_delta.value);
}

static void verify_participants(pas_page_sharing_pool* pool)
{
    size_t index;
    
    for (index = 0; index < pool->participants.size; ++index) {
        pas_page_sharing_participant participant;
        pas_page_sharing_participant_payload* payload;

        participant = *pas_page_sharing_pool_segmented_participant_vector_get_ptr(
            &pool->participants, index);
        
        PAS_ASSERT(participant);
        
        payload = pas_page_sharing_participant_get_payload(participant);
        
        PAS_ASSERT(pas_page_sharing_participant_get_parent_pool(participant) == pool);

        if (payload->index_in_sharing_pool != index) {
            pas_log("Verifying participants in pool %p.\n", pool);
            pas_log("participant = %p (%s).\n",
                    participant,
                    pas_page_sharing_participant_kind_get_string(
                        pas_page_sharing_participant_get_kind(participant)));
            pas_log("payload = %p.\n", payload);
            pas_log("payload->index_in_sharing_pool = %u, index = %zu.\n",
                    payload->index_in_sharing_pool, index);
            PAS_ASSERT(payload->index_in_sharing_pool == index);
        }
        
        if (payload->index_in_sharing_pool_min_heap) {
            PAS_ASSERT(payload->index_in_sharing_pool_min_heap
                       <= pool->participant_by_epoch.size);
            PAS_ASSERT(*pas_page_sharing_pool_min_heap_get_ptr_by_index(
                           &pool->participant_by_epoch,
                           payload->index_in_sharing_pool_min_heap) == participant);
        }
        
        /* This verifies that if there is no delta bit set for this participant then the
           participant has no deltas. */
        if (payload->delta_has_been_noted) {
            unsigned* word_ptr;
            word_ptr = pas_page_sharing_pool_segmented_delta_bitvector_get_ptr(
                &pool->delta, PAS_BITVECTOR_WORD_INDEX(index));
            PAS_ASSERT(pas_bitvector_get_from_word(*word_ptr, index));
        } else {
            uint64_t epoch;
            uint64_t epoch_for_min_heap;

            epoch = pas_page_sharing_participant_get_use_epoch(participant);
            epoch_for_min_heap = payload->use_epoch_for_min_heap;

            if (epoch < epoch_for_min_heap) {
                pas_log("Bad epoch: epoch = %llu, epoch_for_min_heap = %llu\n",
                        (unsigned long long)epoch, (unsigned long long)epoch_for_min_heap);
                PAS_ASSERT(epoch >= epoch_for_min_heap);
            }
            
            if (pas_page_sharing_participant_is_eligible(participant))
                PAS_ASSERT(payload->index_in_sharing_pool_min_heap);
        }
    }
}

enum verify_min_heap_mode {
    verify_min_heap_indices_only,
    verify_min_heap_indices_and_order,
};

typedef enum verify_min_heap_mode verify_min_heap_mode;

static void verify_min_heap(pas_page_sharing_pool* pool,
                            verify_min_heap_mode mode)
{
    static const bool extreme_verification = false;
    
    size_t index;
    
    for (index = 1; index <= pool->participant_by_epoch.size; ++index) {
        pas_page_sharing_participant participant;
        size_t sub_index;

        participant = *pas_page_sharing_pool_min_heap_get_ptr_by_index(
            &pool->participant_by_epoch, index);
        
        if (extreme_verification) {
            for (sub_index = index + 1; sub_index <= pool->participant_by_epoch.size; ++sub_index) {
                pas_page_sharing_participant sub_participant;
                
                sub_participant = *pas_page_sharing_pool_min_heap_get_ptr_by_index(
                    &pool->participant_by_epoch, sub_index);
                
                PAS_ASSERT(participant != sub_participant);
            }
        }
        
        PAS_ASSERT(pas_page_sharing_participant_get_payload(
                       participant)->index_in_sharing_pool_min_heap == index);
        
        if (mode == verify_min_heap_indices_and_order) {
            PAS_ASSERT(pas_page_sharing_pool_min_heap_is_still_ordered(
                           &pool->participant_by_epoch, participant));
        }
    }
}

static void dump_min_heap(pas_page_sharing_pool* pool)
{
    size_t index;
    
    pas_log("Min heap:\n");
    
    for (index = 1; index <= pool->participant_by_epoch.size; ++index) {
        pas_page_sharing_participant participant;

        participant = *pas_page_sharing_pool_min_heap_get_ptr_by_index(
            &pool->participant_by_epoch, index);
        
        pas_log("    %zu: %p, %llu\n", index, participant,
                (unsigned long long)pas_page_sharing_participant_get_payload(participant)->use_epoch_for_min_heap);
    }
}

void pas_page_sharing_pool_construct(pas_page_sharing_pool* pool)
{
    pas_versioned_field_construct(&pool->first_delta, 0);
    pas_page_sharing_pool_segmented_delta_bitvector_construct(&pool->delta);
    pas_page_sharing_pool_segmented_participant_vector_construct(&pool->participants);
    pas_page_sharing_pool_min_heap_construct(&pool->participant_by_epoch);
    pool->current_participant = NULL;
}

void pas_page_sharing_pool_add_at_index(pas_page_sharing_pool* pool,
                                        pas_page_sharing_participant participant,
                                        size_t index_in_sharing_pool)
{
    static const bool verbose = false;
    
    size_t word_index;
    pas_page_sharing_participant_payload* payload;
    pas_versioned_field first_delta;
    size_t old_size;

    PAS_ASSERT((unsigned)index_in_sharing_pool == index_in_sharing_pool);

    /* Don't need to watch because we are only using this to increase the first delta if it's already
       at the end. Since we are assuing that the heap lock is held, there can only be one thread doing
       this at any time. */
    first_delta = pas_versioned_field_read(&pool->first_delta);
    
    pas_heap_lock_assert_held();
    
    payload = pas_page_sharing_participant_get_payload(participant);

    payload->index_in_sharing_pool = (unsigned)index_in_sharing_pool;
    pas_page_sharing_participant_set_parent_pool(participant, pool);

    if (verbose) {
        pas_log("Adding participant %p to pool %p (%s), index = %zu.\n",
                participant,
                pool,
                pas_page_sharing_participant_kind_get_string(
                    pas_page_sharing_participant_get_kind(participant)),
                index_in_sharing_pool);
    }
    
    word_index = PAS_BITVECTOR_WORD_INDEX(index_in_sharing_pool);
    while (word_index >= pool->delta.size)
        pas_page_sharing_pool_segmented_delta_bitvector_append(&pool->delta, 0, pas_lock_is_held);

    pas_fence();
    
    /* We do this last because we rely on this thing's size field. It's safe to do it last because
       we won't access this other than to get the size field unless the delta bit gets set. */
    old_size = pool->participants.size;
    
    while (index_in_sharing_pool >= pool->participants.size) {
        pas_page_sharing_pool_segmented_participant_vector_append(
            &pool->participants, NULL, pas_lock_is_held);
    }
    
    PAS_ASSERT(
        !*pas_page_sharing_pool_segmented_participant_vector_get_ptr(
            &pool->participants, index_in_sharing_pool));
    PAS_ASSERT(index_in_sharing_pool < pool->participants.size);
    
    *pas_page_sharing_pool_segmented_participant_vector_get_ptr(
        &pool->participants, index_in_sharing_pool) = participant;
    
    if (first_delta.value == old_size && pool->participants.size > old_size)
        pas_versioned_field_try_write(&pool->first_delta, first_delta, pool->participants.size);
}

void pas_page_sharing_pool_add(pas_page_sharing_pool* pool,
                               pas_page_sharing_participant participant)
{
    pas_page_sharing_pool_add_at_index(pool, participant, pool->participants.size);
}

static pas_page_sharing_participant
get_current_participant(pas_page_sharing_pool* pool,
                        pas_lock_hold_mode heap_lock_hold_mode)
{
    static const bool verbose = false;
    static const bool debug_min_heap = false;
    
    pas_page_sharing_participant participant;
    pas_allocation_config allocation_config;
    
    if (verbose) {
        pas_log("In %p: getting current participant\n",
                pool);
    }
    
    participant = NULL;
    
    if (pool->first_delta.value == pool->participants.size) {
        participant = pool->current_participant;
        if (verbose) {
            pas_log("In %p: Got current participant = %p\n",
                    pool,
                    participant);
        }
        if (participant) {
            pas_page_sharing_participant_payload* payload;
            uint64_t use_epoch;
        
            payload = pas_page_sharing_participant_get_payload(participant);
        
            use_epoch = pas_page_sharing_participant_get_use_epoch(participant);
        
            if (verbose) {
                pas_log("pool = %p, participant = %p (%s).\n",
                        pool,
                        participant,
                        pas_page_sharing_participant_kind_get_string(
                            pas_page_sharing_participant_get_kind(participant)));
                pas_log("Use epoch = %llu\n", (unsigned long long)use_epoch);
                pas_log("Use epoch for min heap = %llu\n", (unsigned long long)payload->use_epoch_for_min_heap);
            }
            if (use_epoch == payload->use_epoch_for_min_heap
                && pas_page_sharing_participant_is_eligible(participant)) {
                /* This means that the participant that was the min one hasn't changed its epoch. That
                   means we can keep sucking from that participant. */
                return participant;
            }
        
            pas_page_sharing_pool_did_create_delta(pool, participant);
        } else {
            /* This means that there's no way for a new participants to have anything empty. */
            return NULL;
        }
    }
    
    if (verbose)
        pas_log("%p: Actually looking for the deltas.\n", pool);
    
    /* FIXME: I think that when we are going to get pages from the OS, we will always find
       ourselves holding a delta. */
    
    /* If we have to mess with the minheap then we have to grab locks. But there's a solid chance
       that there will be no deltas. Also, even if we do grab some locks, we have to use lock-free
       algorithms to manage the deltas. Wacky!
    
       Also - we really want to make sure that there aren't _any_ deltas when we leave here. Of
       course it's possible for more deltas to arise right after we return. But if we discover
       more deltas incidentally then we should do something about it. */
    
    pas_heap_lock_lock_conditionally(heap_lock_hold_mode);
    
    PAS_ASSERT(pool->participants.size);
    pas_heap_lock_assert_held();
    
    if (participant) {
        pas_page_sharing_participant_payload* payload;
        
        payload = pas_page_sharing_participant_get_payload(participant);
        
        if (payload->index_in_sharing_pool_min_heap) {
            PAS_ASSERT(*pas_page_sharing_pool_min_heap_get_ptr_by_index(
                           &pool->participant_by_epoch,
                           payload->index_in_sharing_pool_min_heap) == participant);
        }
    }
    
    pas_bootstrap_free_heap_allocation_config_construct(&allocation_config, pas_lock_is_held);
    
    for (;;) {
        pas_versioned_field first_delta;
        size_t index;
        size_t size;

        /* Need to flag this as being watched because of the following race if we don't:
           
             1. Us:    -> Start with first_delta at A, A < size.
             
             2. Them:  -> Notify delta at B, A < B < size. If first_delta isn't watched, that will no
                          do anything.

             3. Us:    -> We finish, and if first_delta wasn't watched, then first_delta wasn't changed
                          by Them in 2. So, we set first_delta to size.

           This is an invalid outcome, since actually, the first_delta should now be B. It should be B
           since we can't guarantee that Us saw B during its scan (Them might have notified after Us
           skipped it).
        
           By watching first_delta, we guarante that Them will bump the version in 2, which guarantees
           that Us will not change first_delta in 3. That will result in first_delta being B at the
           end. */
        first_delta = pas_versioned_field_read_to_watch(&pool->first_delta);
        size = pool->participants.size;

        if (verbose)
            pas_log("Starting with first_delta = %zu, size = %zu\n", (size_t)first_delta.value, size);
        
        /* FIXME: This loop could be so much more efficient. */
        for (index = first_delta.value; index < size; ++index) {
            pas_page_sharing_participant_payload* payload;
            
            if (debug_min_heap)
                verify_min_heap(pool, verify_min_heap_indices_and_order);
            
            if (!pas_bitvector_set_atomic_in_word(
                    pas_page_sharing_pool_segmented_delta_bitvector_get_ptr(
                        &pool->delta, PAS_BITVECTOR_WORD_INDEX(index)),
                    index,
                    false)) {
                if (verbose)
                    pas_log("Skipping index = %zu, bit already clear.\n", index);
                continue;
            }
            
            participant =
                *pas_page_sharing_pool_segmented_participant_vector_get_ptr(
                    &pool->participants, index);
            payload = pas_page_sharing_participant_get_payload(participant);
            
            payload->use_epoch_for_min_heap = pas_page_sharing_participant_get_use_epoch(participant);
            if (verbose) {
                pas_log("for participant %p set use epoch to %llu\n",
                        participant, (unsigned long long)payload->use_epoch_for_min_heap);
            }
            
            /* So: do we just set the delta_has_been_noted to false?
               
               The trouble is that right now, we're in this window where if some participant
               wanted to note that it created a delta, it would think that it doesn't have to
               do anything. The thing we don't want is for some participant to have a delta that
               gets lost forever.
            
               However: if a participant does lose a delta forever then it just means we use a bit
               more memory. It's not even clear that bugs of that sort are something we should
               ever worry about.
            
               But let's think through what will happen:
            
               - Before this point, any new deltas will get lost. But that's fine because we're
                 about to take a good look at this participant.
            
               - After this point, new deltas will get registered.
            
               So, I think that the only potential problems are due to relaxed memory issues,
               which may yet cause a delta to get lost. Big deal. */
            payload->delta_has_been_noted = false;
            if (verbose)
                pas_log("Acknowledged delta on %p\n", participant);
            
            pas_fence();
            
            if (!pas_page_sharing_participant_is_eligible(participant)) {
                if (payload->index_in_sharing_pool_min_heap) {
                    pas_page_sharing_pool_min_heap_remove(&pool->participant_by_epoch, participant);
                    if (debug_min_heap)
                        verify_min_heap(pool, verify_min_heap_indices_and_order);
                }
                PAS_ASSERT(!payload->index_in_sharing_pool_min_heap);
                continue;
            }
            
            if (debug_min_heap) {
                verify_min_heap(pool, verify_min_heap_indices_only);
                
                dump_min_heap(pool);
            }
            
            pas_page_sharing_pool_min_heap_update_order(
                &pool->participant_by_epoch, participant, &allocation_config);

            if (debug_min_heap) {
                dump_min_heap(pool);
                
                verify_min_heap(pool, verify_min_heap_indices_and_order);
            }
            
            PAS_ASSERT(payload->index_in_sharing_pool_min_heap);
            PAS_ASSERT(*pas_page_sharing_pool_min_heap_get_ptr_by_index(
                           &pool->participant_by_epoch,
                           payload->index_in_sharing_pool_min_heap) == participant);
        }

        if (verbose)
            pas_log("%p: trying to set first delta to size (%zu).\n", pool, size);
        if (pas_versioned_field_try_write(&pool->first_delta,
                                          first_delta,
                                          size)) {
            if (verbose)
                pas_log("%p: set first delta to size (%zu)\n", pool, size);
            break;
        }
    }
    
    participant = pas_page_sharing_pool_min_heap_get_min(&pool->participant_by_epoch);
    pool->current_participant = participant;

    if (verbose)
        pas_log("Ended up with participant %p.\n", participant);
    
    if (participant) {
        pas_page_sharing_participant_payload* payload;
        
        payload = pas_page_sharing_participant_get_payload(participant);
        
        PAS_ASSERT(payload->index_in_sharing_pool_min_heap);
    }
    
    pas_heap_lock_unlock_conditionally(heap_lock_hold_mode);
    
    return participant;
}

static pas_page_sharing_pool_take_result
take_from(
    pas_page_sharing_pool* pool,
    pas_page_sharing_participant participant,
    pas_deferred_decommit_log* decommit_log,
    pas_lock_hold_mode heap_lock_hold_mode)
{
    static const bool verbose = false;
    
    pas_page_sharing_pool_take_result result;
    
    PAS_ASSERT(participant);

    if (verbose) {
        pas_log("Taking least recently used from %p (%s)\n",
                participant,
                pas_page_sharing_participant_kind_get_string(
                    pas_page_sharing_participant_get_kind(participant)));
    }
    
    result = pas_page_sharing_participant_take_least_recently_used(
        participant, decommit_log, heap_lock_hold_mode);

    switch (result) {
    case pas_page_sharing_pool_take_none_available:
        pas_page_sharing_participant_get_payload(participant)->delta_has_been_noted = false;
        pas_page_sharing_pool_did_create_delta(pool, participant);
        break;
        
    case pas_page_sharing_pool_take_none_within_max_epoch:
    case pas_page_sharing_pool_take_locks_unavailable:
    case pas_page_sharing_pool_take_success:
        break;
    }
    
    return result;
}

pas_page_sharing_pool_take_result
pas_page_sharing_pool_take_least_recently_used(
    pas_page_sharing_pool* pool,
    pas_deferred_decommit_log* decommit_log,
    pas_lock_hold_mode heap_lock_hold_mode,
    uint64_t max_epoch)
{
    static const bool verbose = false;

    if (verbose) {
        pas_log("%p: taking least recently used.\n",
                pool);
    }
    
    for (;;) {
        pas_page_sharing_pool_take_result result;
        pas_page_sharing_participant participant;
        pas_page_sharing_participant_payload* payload;
        uint64_t epoch;

        participant = NULL;
        epoch = 0;

        /* When we're scavenging, the epochs of our participants are constantly changing. So, if
           we call get_current_participant, we're likely to have to process deltas.
           
           But we're going to take all of the pages up to this max epoch. So, it's fine to keep taking
           from the current participant so long as their epoch matches. We can let
           get_current_participant sort things out only if this fast check fails. */
        if (max_epoch) {
            pas_page_sharing_participant possible_participant;
            possible_participant = pool->current_participant;
            if (possible_participant
                && pas_page_sharing_participant_is_eligible(possible_participant)) {
                uint64_t possible_epoch;
                possible_epoch = pas_page_sharing_participant_get_use_epoch(possible_participant);
                if (possible_epoch <= max_epoch) {
                    participant = possible_participant;
                    epoch = possible_epoch;
                }
            }
        }

        if (!participant) {
            participant = get_current_participant(pool, heap_lock_hold_mode);
            
            if (participant) {
                payload = pas_page_sharing_participant_get_payload(participant);
                epoch = payload->use_epoch_for_min_heap;
            } else {
                payload = NULL;
                epoch = 0;
            }
        }
        
        if (verbose) {
            pas_log("Got participant: %p, %s; use epoch = %llu.\n",
                    participant,
                    pas_page_sharing_participant_kind_get_string(
                        pas_page_sharing_participant_get_kind(participant)),
                    (unsigned long long)epoch);
        }
        
        if (!participant) {
            if (verbose)
                pas_log("None available!\n");
            return pas_page_sharing_pool_take_none_available;
        }
        
        if (max_epoch && epoch > max_epoch) {
            if (verbose)
                pas_log("None within epoch!\n");
            return pas_page_sharing_pool_take_none_within_max_epoch;
        }
        
        result = take_from(pool, participant, decommit_log, heap_lock_hold_mode);
        switch (result) {
        case pas_page_sharing_pool_take_none_available:
            if (verbose)
                pas_log("None available!\n");
            break;
            
        case pas_page_sharing_pool_take_none_within_max_epoch:
            if (verbose)
                pas_log("None withing max epoch!\n");
            break;
            
        case pas_page_sharing_pool_take_locks_unavailable:
            if (verbose)
                pas_log("Locks unavailable!\n");
            return result;
            
        case pas_page_sharing_pool_take_success:
            if (verbose)
                pas_log("Success!\n");
            return result;
        }
    }
}

static void atomic_add_balance(size_t addend)
{
    for (;;) {
        size_t balance;
        size_t new_balance;
        
        balance = (size_t)pas_physical_page_sharing_pool_balance;
        new_balance = balance + addend;
        
        if (pas_compare_and_swap_uintptr_weak(
                (uintptr_t*)&pas_physical_page_sharing_pool_balance, balance, new_balance))
            return;
    }
}

void pas_physical_page_sharing_pool_take(
    size_t bytes,
    pas_lock_hold_mode heap_lock_hold_mode,
    pas_lock** locks_already_held,
    size_t num_locks_already_held)
{
    static const bool verbose = false;
    
    size_t size_remaining;
    pas_deferred_decommit_log decommit_log;
    intptr_t balance_addend;
    pas_page_sharing_pool_take_result last_take_result;
    
    if (!pas_physical_page_sharing_pool_balancing_enabled)
        return;

    if (verbose)
        pas_log("Taking %zu bytes from the physical pool.\n", bytes);
    
    for (;;) {
        intptr_t balance;
        intptr_t new_balance;
        
        balance = pas_physical_page_sharing_pool_balance;

        if (verbose)
            pas_log("Balance = %ld\n", balance);
        
        if (balance < 0) {
            size_remaining = bytes - (size_t)balance;
            new_balance = 0;
        } else if (bytes > (size_t)balance) {
            size_remaining = bytes - (size_t)balance;
            new_balance = 0;
        } else {
            size_remaining = 0;
            new_balance = (intptr_t)((size_t)balance - bytes);
        }
        
        if (pas_compare_and_swap_uintptr_weak(
                (uintptr_t*)&pas_physical_page_sharing_pool_balance,
                (uintptr_t)balance, (uintptr_t)new_balance))
            break;
    }
    
    if (!size_remaining)
        return;
    
    pas_deferred_decommit_log_construct(&decommit_log,
                                        locks_already_held,
                                        num_locks_already_held,
                                        NULL);
    
    /* Calm the compiler down. This always gets overwritten. */
    last_take_result = pas_page_sharing_pool_take_success;
    
    while (decommit_log.total < size_remaining) {
        last_take_result = pas_page_sharing_pool_take_least_recently_used(
            &pas_physical_page_sharing_pool, &decommit_log, heap_lock_hold_mode, 0);
        
        PAS_ASSERT(last_take_result != pas_page_sharing_pool_take_none_within_max_epoch);
        
        if (last_take_result != pas_page_sharing_pool_take_success)
            break;
    }
    
    /* We only want a negative balance_addend if the last_take_result is locks_unavailable. */
    if (decommit_log.total < size_remaining
        && last_take_result != pas_page_sharing_pool_take_locks_unavailable)
        balance_addend = 0;
    else
        balance_addend = (intptr_t)(decommit_log.total - size_remaining);
    
    pas_deferred_decommit_log_decommit_all(&decommit_log);
    pas_deferred_decommit_log_destruct(&decommit_log, heap_lock_hold_mode);

    if (verbose) {
        pas_log("Balance addend: %ld.\n",
                balance_addend);
    }

    atomic_add_balance((size_t)balance_addend);
}

pas_page_sharing_pool_scavenge_result
pas_physical_page_sharing_pool_scavenge(uint64_t max_epoch)
{
    static const bool verbose = false;

    pas_page_sharing_pool_take_result take_result;
    pas_physical_memory_transaction transaction;
    size_t total_bytes;
    
    if (verbose)
        pas_log("Doing scavenge up to %llu\n", (unsigned long long)max_epoch);

    total_bytes = 0;
    
    pas_physical_memory_transaction_construct(&transaction);
    do {
        pas_deferred_decommit_log decommit_log;
        
        if (verbose)
            pas_log("Beginning scavenge transaction.\n");
        
        pas_physical_memory_transaction_begin(&transaction);
        
        pas_deferred_decommit_log_construct(&decommit_log, NULL, 0, &transaction);
        
        do {
            take_result = pas_page_sharing_pool_take_least_recently_used(
                &pas_physical_page_sharing_pool,
                &decommit_log,
                pas_lock_is_not_held,
                max_epoch);
            if (verbose) {
                pas_log("Take result = %s\n",
                        pas_page_sharing_pool_take_result_get_string(take_result));
            }
        } while (take_result == pas_page_sharing_pool_take_success);

        total_bytes += decommit_log.total;
        
        for (;;) {
            intptr_t balance;
            intptr_t new_balance;
            
            balance = pas_physical_page_sharing_pool_balance;
            
            /* If we were previously in the red, forgive that. But this won't ever make us
               positively balanced because that would basically mean that the mutator could
               grow the memory back without incrementally scavenging.

               This sort of means that under the right circumstances, a race during physical
               memory transactions can cause the target heap size to increase a bit. I think
               that's probably fine since the scavenger thread exerts downward pressure on
               the target heap size. */
            new_balance = PAS_MIN(0, (intptr_t)balance + (intptr_t)decommit_log.total);
            
            if (pas_compare_and_swap_uintptr_weak(
                    (uintptr_t*)&pas_physical_page_sharing_pool_balance,
                    (uintptr_t)balance, (uintptr_t)new_balance))
                break;
        }
        
        pas_deferred_decommit_log_decommit_all(&decommit_log);
        pas_deferred_decommit_log_destruct(&decommit_log, pas_lock_is_not_held);
    } while (!pas_physical_memory_transaction_end(&transaction));

    PAS_ASSERT(take_result != pas_page_sharing_pool_take_locks_unavailable);
    PAS_ASSERT(take_result != pas_page_sharing_pool_take_success);
    
    return pas_page_sharing_pool_scavenge_result_create(take_result, total_bytes);
}

void pas_physical_page_sharing_pool_take_later(size_t bytes)
{
    static const bool verbose = false;
    
    if (!pas_physical_page_sharing_pool_balancing_enabled)
        return;

    if (verbose)
        pas_log("Taking %zu bytes later.\n", bytes);
    
    atomic_add_balance(-bytes);
}

void pas_physical_page_sharing_pool_give_back(size_t bytes)
{
    static const bool verbose = false;
    
    if (!pas_physical_page_sharing_pool_balancing_enabled)
        return;

    if (verbose)
        pas_log("Giving back %zu bytes.\n", bytes);
    
    atomic_add_balance(bytes);
}

void pas_physical_page_sharing_pool_take_for_page_config(
    size_t bytes,
    const pas_page_base_config* page_config,
    pas_lock_hold_mode heap_lock_hold_mode,
    pas_lock** locks_already_held,
    size_t num_locks_already_held)
{
    static const bool verbose = false;

    if (verbose)
        pas_log("Taking %zu bytes for page_config = %p\n", bytes, page_config);
    
    if (!pas_physical_page_sharing_pool_balancing_enabled)
        return;
    
    if (pas_page_base_config_is_utility(page_config)) {
        if (!pas_physical_page_sharing_pool_balancing_enabled_for_utility)
            return;
        pas_physical_page_sharing_pool_take_later(bytes);
        return;
    }
    
    pas_physical_page_sharing_pool_take(bytes, heap_lock_hold_mode,
                                        locks_already_held, num_locks_already_held);
}

void pas_page_sharing_pool_did_create_delta(pas_page_sharing_pool* pool,
                                            pas_page_sharing_participant participant)
{
    static const bool verbose = false;
    
    size_t index;
    pas_page_sharing_participant_payload* payload;

    payload = pas_page_sharing_participant_get_payload(participant);
    
    PAS_ASSERT(*pas_page_sharing_pool_segmented_participant_vector_get_ptr(
                   &pool->participants, payload->index_in_sharing_pool) == participant);
    
    if (!payload->delta_has_been_noted) {
        if (verbose) {
            pas_log("%p: Noting delta on %p, %s\n",
                    pool,
                    participant,
                    pas_page_sharing_participant_kind_get_string(
                        pas_page_sharing_participant_get_kind(participant)));
        }
        payload->delta_has_been_noted = true;
        
        pas_fence();
        
        index = payload->index_in_sharing_pool;
        
        if (verbose)
            pas_log("index = %zu\n", index);
        
        pas_bitvector_set_atomic_in_word(
            pas_page_sharing_pool_segmented_delta_bitvector_get_ptr(
                &pool->delta, PAS_BITVECTOR_WORD_INDEX(index)),
            index, true);
        
        for (;;) {
            pas_versioned_field first_delta;
            size_t new_index;

            /* Don't need to watch because we know that we want to make the first delta smaller
               than whatever it is now and do nothing otherwise. */
            first_delta = pas_versioned_field_read(&pool->first_delta);
            
            new_index = PAS_MIN((size_t)first_delta.value, index);

            /* Note - if first_delta is being watched then this will bump the version even if
               new_index is equal to first_delta.value. */
            if (pas_versioned_field_try_write(&pool->first_delta,
                                              first_delta,
                                              new_index))
                break;
        }
    }
    
    pas_scavenger_did_create_eligible();
}

void pas_page_sharing_pool_verify(pas_page_sharing_pool* pool,
                                  pas_lock_hold_mode heap_lock_hold_mode)
{
    pas_heap_lock_lock_conditionally(heap_lock_hold_mode);
    verify_first_delta(pool);
    verify_participants(pool);
    verify_min_heap(pool, verify_min_heap_indices_and_order);
    pas_heap_lock_unlock_conditionally(heap_lock_hold_mode);
}

bool pas_page_sharing_pool_has_delta(pas_page_sharing_pool* pool)
{
    return pool->first_delta.value < pool->participants.size;
}

bool pas_page_sharing_pool_has_current_participant(pas_page_sharing_pool* pool)
{
    return !!pool->current_participant;
}

#endif /* LIBPAS_ENABLED */
