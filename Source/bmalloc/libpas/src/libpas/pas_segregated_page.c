/*
 * Copyright (c) 2018-2022 Apple Inc. All rights reserved.
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

#include "pas_segregated_page.h"

#include <math.h>
#include "pas_commit_span.h"
#include "pas_debug_spectrum.h"
#include "pas_deferred_decommit_log.h"
#include "pas_epoch.h"
#include "pas_free_granules.h"
#include "pas_full_alloc_bits_inlines.h"
#include "pas_get_page_base_and_kind_for_small_other_in_fast_megapage.h"
#include "pas_heap_lock.h"
#include "pas_log.h"
#include "pas_page_malloc.h"
#include "pas_page_sharing_pool.h"
#include "pas_range.h"
#include "pas_segregated_page_inlines.h"
#include "pas_segregated_size_directory.h"
#include "pas_utility_heap_config.h"

double pas_segregated_page_extra_wasteage_handicap_for_config_variant[
    PAS_NUM_SEGREGATED_PAGE_CONFIG_VARIANTS] = {
    [0 ... PAS_NUM_SEGREGATED_PAGE_CONFIG_VARIANTS - 1] = 1.
};

PAS_API bool pas_segregated_page_lock_with_unbias_impl(
    pas_segregated_page* page,
    pas_lock** held_lock,
    pas_lock* lock_ptr)
{
    pas_lock_lock(lock_ptr);
    
    if (lock_ptr == page->lock_ptr) {
        pas_segregated_view owner;

        owner = page->owner;
        if (pas_segregated_view_is_some_exclusive(owner)) {
            pas_segregated_exclusive_view* exclusive;
            pas_lock* fallback_lock;
            
            exclusive = pas_segregated_view_get_exclusive(owner);
            
            PAS_ASSERT(exclusive);
            
            fallback_lock = &exclusive->ownership_lock;
            
            if (lock_ptr != fallback_lock) {
                pas_lock_lock(fallback_lock);
                page->lock_ptr = fallback_lock;
                pas_lock_unlock(lock_ptr);
                *held_lock = fallback_lock;
            }
        }
        
        return true;
    }
    
    return false;
}

pas_lock* pas_segregated_page_switch_lock_slow(
    pas_segregated_page* page,
    pas_lock* held_lock,
    pas_lock* page_lock)
{
    static const bool verbose = false;
    
    PAS_ASSERT(held_lock != page_lock);
    
    for (;;) {
        if (verbose) {
            pas_log("Trying to actually get a different lock (%p -> %p).\n",
                    held_lock, page_lock);
        }
        
        if (held_lock)
            pas_lock_unlock(held_lock);
        
        if (pas_segregated_page_lock_with_unbias_not_utility(page, &held_lock, page_lock))
            return held_lock;

        page_lock = page->lock_ptr;
    }
    PAS_ASSERT(!"Should not be reached");
}

void pas_segregated_page_switch_lock_and_rebias_while_ineligible_impl(
    pas_segregated_page* page,
    pas_lock** held_lock,
    pas_thread_local_cache_node* cache_node)
{
    for (;;) {
        pas_segregated_view owner;
        pas_segregated_exclusive_view* exclusive;
        pas_lock* page_lock;
        bool did_lock_quickly;
        bool got_right_lock;
    
        page_lock = page->lock_ptr;
        PAS_TESTING_ASSERT(page_lock);

        if (*held_lock == page_lock && cache_node && *held_lock == &cache_node->page_lock) {
            pas_compiler_fence();
            return;
        }

        owner = page->owner;

        if (!pas_segregated_view_is_some_exclusive(owner) || !cache_node) {
            pas_lock_switch(held_lock, page_lock);
            if (page->lock_ptr != page_lock)
                continue;
            return;
        }

        did_lock_quickly =
            (*held_lock == &cache_node->page_lock && pas_lock_try_lock(page_lock)) ||
            (*held_lock == page_lock && pas_lock_try_lock(&cache_node->page_lock));

        if (!did_lock_quickly) {
            if (*held_lock)
                pas_lock_unlock(*held_lock);

            if (&cache_node->page_lock == page_lock) {
                pas_lock_lock(page_lock);
                *held_lock = page_lock;
                if (page->lock_ptr != page_lock)
                    continue;
                return;
            }
        
            exclusive = pas_segregated_view_get_exclusive(owner);

            /* This enforces that:
               
               - Cache node page locks must be acquired before page locks.
               
               - Cache node page locks are acquired in pointer-as-integer order relative to one
                 another. */
            if (&exclusive->ownership_lock == page_lock || &cache_node->page_lock < page_lock) {
                pas_lock_lock(&cache_node->page_lock);
                pas_lock_lock(page_lock);
            } else {
                pas_lock_lock(page_lock);
                pas_lock_lock(&cache_node->page_lock);
            }
        }

        PAS_ASSERT(page_lock != &cache_node->page_lock);

        got_right_lock = (page->lock_ptr == page_lock);
        if (got_right_lock)
            page->lock_ptr = &cache_node->page_lock;
        
        pas_lock_unlock(page_lock);
        *held_lock = &cache_node->page_lock;

        if (got_right_lock)
            return;
    }
}

void pas_segregated_page_construct(pas_segregated_page* page,
                                   pas_segregated_view owner,
                                   bool was_stolen,
                                   const pas_segregated_page_config* page_config_ptr)
{
    static const bool verbose = false;
    
    pas_segregated_page_config page_config;
    pas_segregated_page_role role;

    page_config = *page_config_ptr;

    PAS_ASSERT(page_config.base.page_config_kind == pas_page_config_kind_segregated);

    role = pas_segregated_view_get_page_role_for_owner(owner);

    /* This is essential for medium deallocation. */
    pas_page_base_construct(
        &page->base, pas_page_kind_for_segregated_variant_and_role(page_config.variant, role));

    if (verbose) {
        pas_log("Constructing page %p with boundary %p for view %p and config %s.\n",
                page, pas_segregated_page_boundary(page, page_config),
                owner, pas_segregated_page_config_kind_get_string(page_config.kind));
    }

    if (pas_segregated_page_config_is_utility(page_config))
        page->lock_ptr = NULL;
    else
        page->lock_ptr = pas_segregated_view_get_ownership_lock(owner);
    
    page->owner = owner;
    pas_zero_memory(page->alloc_bits, pas_segregated_page_config_num_alloc_bytes(page_config));

    pas_segregated_page_emptiness emptiness = {
        .use_epoch = PAS_EPOCH_INVALID,
        .num_non_empty_words = 0,
    };
    pas_atomic_store_pair_relaxed(&page->emptiness, *(pas_pair*)&emptiness);

    page->view_cache_index = (pas_allocator_index)UINT_MAX;

    switch (role) {
    case pas_segregated_page_exclusive_role:{
        pas_segregated_size_directory* directory;
        pas_segregated_size_directory_data* data;

        directory = pas_segregated_view_get_size_directory(owner);
        data = pas_segregated_size_directory_data_ptr_load_non_null(&directory->data);
        PAS_UNUSED_PARAM(data);

        PAS_ASSERT(directory->object_size);
        page->object_size = directory->object_size;
        PAS_ASSERT(page->object_size == directory->object_size); /* Check for overflows. */

        if (pas_segregated_size_directory_view_cache_capacity(directory)) {
            PAS_ASSERT(directory->view_cache_index);
            page->view_cache_index = directory->view_cache_index;
        } else
            PAS_ASSERT(directory->view_cache_index == (pas_allocator_index)UINT_MAX);
        break;
    }
        
    case pas_segregated_page_shared_role:
        page->object_size = 0;
        break;
    }

    page->is_in_use_for_allocation = false;
    page->is_committing_fully = false;

    if (page_config.base.page_size != page_config.base.granule_size) {
        pas_page_granule_use_count* use_counts;
        size_t num_granules;
        uintptr_t start_of_payload;
        uintptr_t end_of_payload;
        
        use_counts = pas_segregated_page_get_granule_use_counts(page, page_config);
        num_granules = page_config.base.page_size / page_config.base.granule_size;

        if (was_stolen) {
            size_t granule_index;
            
            for (granule_index = num_granules; granule_index--;) {
                if (use_counts[granule_index] != PAS_PAGE_GRANULE_DECOMMITTED)
                    use_counts[granule_index] = 0;
            }
        } else
            pas_zero_memory(use_counts, num_granules * sizeof(pas_page_granule_use_count));
        
        /* If there are any bytes in the page not made available for allocation then make sure
           that the use counts know about it. */
        start_of_payload = pas_segregated_page_config_payload_offset_for_role(page_config, role);
        end_of_payload = pas_segregated_page_config_payload_end_offset_for_role(page_config, role);

        pas_page_granule_increment_uses_for_range(
            use_counts, 0, start_of_payload,
            page_config.base.page_size, page_config.base.granule_size);
        pas_page_granule_increment_uses_for_range(
            use_counts, end_of_payload, page_config.base.page_size,
            page_config.base.page_size, page_config.base.granule_size);
    }
    
    /* These are only used by exclusive views but we initialize them unconditionally to whatever
       the exclusive views want initially. */
    page->eligibility_notification_has_been_deferred = false;
}

void pas_segregated_page_note_emptiness(pas_segregated_page* page, pas_note_emptiness_action action)
{
    static const bool verbose = false;
    if (page->lock_ptr)
        pas_lock_testing_assert_held(page->lock_ptr);
    if (verbose) {
        pas_log("page %p (owner %p, boundary %p) becoming empty\n",
                page, page->owner,
                pas_segregated_page_boundary(
                    page, *pas_segregated_view_get_page_config(page->owner)));
    }
    switch (action) {
    case pas_note_emptiness_clear_num_non_empty_words: {
        pas_segregated_page_emptiness emptiness = {
            .use_epoch = pas_get_epoch(),
            .num_non_empty_words = 0,
        };
        pas_atomic_store_pair_relaxed(&page->emptiness, *(pas_pair*)&emptiness);
        break;
    }
    case pas_note_emptiness_keep_num_non_empty_words:
        page->emptiness.use_epoch = pas_get_epoch();
        break;
    }
    pas_segregated_view_note_emptiness(page->owner, page);
}

static pas_lock* commit_lock_for(pas_segregated_page* page)
{
    return pas_segregated_view_get_commit_lock(page->owner);
}

bool pas_segregated_page_take_empty_granules(
    pas_segregated_page* page,
    pas_deferred_decommit_log* decommit_log,
    pas_lock** held_lock,
    pas_range_locked_mode range_locked_mode,
    pas_lock_hold_mode heap_lock_hold_mode)
{
    static const bool verbose = false;
    
    pas_page_granule_use_count* use_counts;
    pas_segregated_view owner;
    const pas_segregated_page_config* page_config_ptr;
    pas_segregated_page_config page_config;
    uintptr_t num_granules;
    char* boundary;
    pas_free_granules free_granules;

    owner = page->owner;
    page_config_ptr = pas_segregated_view_get_page_config(owner);
    page_config = *page_config_ptr;
    use_counts = pas_segregated_page_get_granule_use_counts(page, page_config);
    num_granules = page_config.base.page_size / page_config.base.granule_size;

    PAS_ASSERT(num_granules >= 2);
    PAS_ASSERT(num_granules <= PAS_MAX_GRANULES);
    PAS_ASSERT(!pas_segregated_page_config_is_utility(page_config));

    pas_segregated_page_switch_lock(page, held_lock, page_config);

    PAS_ASSERT(!page->is_committing_fully);

    pas_free_granules_compute_and_mark_decommitted(&free_granules, use_counts, num_granules);
    
    pas_lock_switch(held_lock, NULL);

    PAS_ASSERT(free_granules.num_free_granules);

    boundary = pas_segregated_page_boundary(page, page_config);
    PAS_UNUSED_PARAM(boundary);
    
    if (verbose)
        pas_log("Taking %zu empty granules from %p.\n", free_granules.num_free_granules, page);

    if (range_locked_mode == pas_range_is_not_locked
        && !pas_deferred_decommit_log_lock_for_adding(decommit_log,
                                                      commit_lock_for(page),
                                                      heap_lock_hold_mode)) {
        pas_segregated_page_switch_lock(page, held_lock, page_config);
        
        PAS_ASSERT(!page->is_committing_fully);

        pas_free_granules_unmark_decommitted(&free_granules, use_counts, num_granules);
        
        return false;
    }

    pas_free_granules_decommit_after_locking_range(
        &free_granules, &page->base, decommit_log, commit_lock_for(page),
        &page_config_ptr->base, heap_lock_hold_mode);
    
    return true;
}

bool pas_segregated_page_take_physically(
    pas_segregated_page* page,
    pas_deferred_decommit_log* decommit_log,
    pas_range_locked_mode range_locked_mode,
    pas_lock_hold_mode heap_lock_hold_mode)
{
    pas_segregated_page_config page_config;
    uintptr_t base;
    pas_virtual_range range;

    page_config = *pas_segregated_view_get_page_config(page->owner);

    PAS_ASSERT(!pas_segregated_page_config_is_utility(page_config));

    if (page_config.base.page_size > page_config.base.granule_size) {
        pas_lock* held_lock;
        bool result;
        held_lock = NULL;
        result = pas_segregated_page_take_empty_granules(
            page, decommit_log, &held_lock, range_locked_mode, heap_lock_hold_mode);
        pas_lock_switch(&held_lock, NULL);
        return result;
    }

    PAS_ASSERT(!page->emptiness.num_non_empty_words);
    
    base = (uintptr_t)pas_segregated_page_boundary(page, page_config);

    range = pas_virtual_range_create(
        base,
        base + page_config.base.page_size,
        commit_lock_for(page),
        page_config.base.heap_config_ptr->mmap_capability);
    
    return pas_deferred_decommit_log_add_maybe_locked(
        decommit_log, range, range_locked_mode, heap_lock_hold_mode);
}

void pas_segregated_page_commit_fully(
    pas_segregated_page* page,
    pas_lock** held_lock,
    pas_commit_fully_lock_hold_mode lock_hold_mode)
{
    static const bool verbose = false;
    
    const pas_segregated_page_config* page_config_ptr;
    pas_segregated_page_config page_config;
    pas_page_granule_use_count* use_counts;
    uintptr_t num_granules;
    uintptr_t granule_index;
    uintptr_t num_granules_to_commit;

    page_config_ptr = pas_segregated_view_get_page_config(page->owner);
    page_config = *page_config_ptr;

    if (page->lock_ptr)
        pas_lock_assert_held(page->lock_ptr);
    PAS_ASSERT(*held_lock == page->lock_ptr);
    PAS_ASSERT(!page->is_committing_fully);
    if (lock_hold_mode == pas_commit_fully_holding_page_and_commit_locks)
        pas_lock_assert_held(commit_lock_for(page));

    PAS_ASSERT(page_config.base.page_size > page_config.base.granule_size);

    PAS_ASSERT(pas_segregated_page_config_heap_lock_hold_mode(page_config) == pas_lock_is_not_held);
    
    use_counts = pas_segregated_page_get_granule_use_counts(page, page_config);
    num_granules = page_config.base.page_size / page_config.base.granule_size;

    num_granules_to_commit = 0;

    for (granule_index = num_granules; granule_index--;) {
        if (use_counts[granule_index] == PAS_PAGE_GRANULE_DECOMMITTED)
            num_granules_to_commit++;
    }

    if (num_granules_to_commit) {
        /* This is the hard part. */

        pas_commit_span commit_span;
        uintptr_t num_granules_to_commit_at_end;
        pas_lock* commit_lock;
        unsigned num_held_locks;

        /* It's a strong invariant that nobody else tries to mess with granule commit state when
           we release this lock. Some callers ensure this by holding the commit lock around this
           call, while others ensure it by using eligibility. */
        page->is_committing_fully = true;

        pas_lock_switch(held_lock, NULL);

        commit_lock = commit_lock_for(page);
        num_held_locks = lock_hold_mode == pas_commit_fully_holding_page_and_commit_locks;

        pas_physical_page_sharing_pool_take_for_page_config(
            num_granules_to_commit * page_config.base.granule_size,
            &page_config_ptr->base,
            pas_lock_is_not_held,
            &commit_lock, num_held_locks);

        if (verbose) {
            pas_segregated_view owner;
            owner = page->owner;
            pas_log("Trying to lock commit lock for page %p owned by %p (%s)\n",
                    page, owner,
                    pas_segregated_view_kind_get_string(pas_segregated_view_get_kind(owner)));
        }

        if (lock_hold_mode == pas_commit_fully_holding_page_lock)
            pas_lock_lock(commit_lock);
        pas_compiler_fence();

        pas_commit_span_construct(&commit_span, page_config.base.heap_config_ptr->mmap_capability);

        for (granule_index = 0; granule_index < num_granules; ++granule_index) {
            if (use_counts[granule_index] != PAS_PAGE_GRANULE_DECOMMITTED) {
                pas_commit_span_add_unchanged_and_commit(&commit_span, &page->base, granule_index,
                                                         &page_config_ptr->base);
                continue;
            }
            
            pas_commit_span_add_to_change(&commit_span, granule_index);
        }

        pas_commit_span_add_unchanged_and_commit(&commit_span, &page->base, granule_index,
                                                 &page_config_ptr->base);

        pas_compiler_fence();
        if (lock_hold_mode == pas_commit_fully_holding_page_lock)
            pas_lock_unlock(commit_lock);

        if (PAS_DEBUG_SPECTRUM_USE_FOR_COMMIT) {
            pas_segregated_view owner;
            owner = page->owner;
            pas_heap_lock_lock();
            if (pas_segregated_view_is_shared_handle(owner)) {
                pas_debug_spectrum_add(
                    pas_segregated_view_get_shared_handle(owner)->directory,
                    pas_segregated_shared_page_directory_dump_for_spectrum,
                    commit_span.total_bytes);
            } else {
                pas_debug_spectrum_add(
                    pas_segregated_view_get_size_directory(owner),
                    pas_segregated_size_directory_dump_for_spectrum,
                    commit_span.total_bytes);
            }
            pas_heap_lock_unlock();
        }

        pas_segregated_page_switch_lock(page, held_lock, page_config);

        PAS_ASSERT(page->is_committing_fully);
        page->is_committing_fully = false;

        num_granules_to_commit_at_end = 0;
        for (granule_index = num_granules; granule_index--;) {
            if (use_counts[granule_index] == PAS_PAGE_GRANULE_DECOMMITTED) {
                num_granules_to_commit_at_end++;
                use_counts[granule_index] = 0;
            }
        }

        PAS_ASSERT(num_granules_to_commit == num_granules_to_commit_at_end);
    }
}

bool pas_segregated_page_deallocate_should_verify_granules = false;

typedef struct {
    pas_page_granule_use_count* correct_use_counts;
    uintptr_t page_boundary;
    size_t page_size;
    size_t granule_size;
} verify_granules_data;

static bool verify_granules_live_object_callback(pas_segregated_view view,
                                                 pas_range range,
                                                 void* arg)
{
    static const bool verbose = false;
    
    verify_granules_data* data;

    PAS_UNUSED_PARAM(view);

    data = arg;

    if (verbose) {
        pas_log("Got live object at %p, size %zu.\n",
                (void*)range.begin,
                pas_range_size(range));
    }
    
    pas_page_granule_increment_uses_for_range(
        data->correct_use_counts,
        range.begin - data->page_boundary,
        range.end - data->page_boundary,
        data->page_size, data->granule_size);

    return true;
}

void pas_segregated_page_verify_granules(pas_segregated_page* page)
{
    static const bool verbose = false;
    
    pas_segregated_page_config page_config;
    pas_segregated_page_role role;
    pas_page_granule_use_count correct_use_counts[PAS_MAX_GRANULES];
    pas_page_granule_use_count* use_counts;
    uintptr_t num_granules;
    uintptr_t start_of_payload;
    uintptr_t end_of_payload;
    uintptr_t granule_index;
    verify_granules_data data;

    page_config = *pas_segregated_view_get_page_config(page->owner);
    role = pas_page_kind_get_segregated_role(pas_page_base_get_kind(&page->base));

    if (verbose)
        pas_log("Verifying granules in page %p.\n", page);

    num_granules = page_config.base.page_size / page_config.base.granule_size;
    PAS_ASSERT(num_granules <= PAS_MAX_GRANULES);

    pas_zero_memory(correct_use_counts, num_granules * sizeof(pas_page_granule_use_count));
    
    /* If there are any bytes in the page not made available for allocation then make sure
       that the use counts know about it. */
    start_of_payload = pas_segregated_page_config_payload_offset_for_role(page_config, role);
    end_of_payload = pas_segregated_page_config_payload_end_offset_for_role(page_config, role);
    
    pas_page_granule_increment_uses_for_range(
        correct_use_counts, 0, start_of_payload,
        page_config.base.page_size, page_config.base.granule_size);

    data.correct_use_counts = correct_use_counts;
    data.page_boundary = (uintptr_t)pas_segregated_page_boundary(page, page_config);
    data.page_size = page_config.base.page_size;
    data.granule_size = page_config.base.granule_size;

    /* We actually don't hold the ownership lock, but we lie and say that we do, since we can
       guarantee that the page and views aren't going away right now, since this gets called from
       deallocation code. Also, if we're dealing with a shared page, then we _are_ holding the
       ownership lock, so we aren't even lying. */
    pas_segregated_view_for_each_live_object(
        page->owner, verify_granules_live_object_callback, &data, pas_lock_is_held);
    
    pas_page_granule_increment_uses_for_range(
        correct_use_counts, end_of_payload, page_config.base.page_size,
        page_config.base.page_size, page_config.base.granule_size);

    use_counts = pas_segregated_page_get_granule_use_counts(page, page_config);

    for (granule_index = num_granules; granule_index--;) {
        if (!correct_use_counts[granule_index]) {
            PAS_ASSERT(!use_counts[granule_index] ||
                       use_counts[granule_index] == PAS_PAGE_GRANULE_DECOMMITTED);
        } else
            PAS_ASSERT(use_counts[granule_index] == correct_use_counts[granule_index]);
    }
}

void pas_segregated_page_deallocation_did_fail(uintptr_t begin)
{
    pas_deallocation_did_fail(
        "Alloc bit not set in pas_segregated_page_deallocate_with_page",
        begin);
}

size_t pas_segregated_page_get_num_empty_granules(pas_segregated_page* page)
{
    const pas_segregated_page_config* page_config_ptr;
    pas_segregated_page_config page_config;
    size_t result;
    
    page_config_ptr = pas_segregated_view_get_page_config(page->owner);
    page_config = *page_config_ptr;

    result = 0;

    if (page_config.base.page_size > page_config.base.granule_size) {
        pas_page_granule_use_count* use_counts;
        uintptr_t num_granules;
        uintptr_t granule_index;

        use_counts = pas_segregated_page_get_granule_use_counts(page, page_config);
        num_granules = page_config.base.page_size / page_config.base.granule_size;

        for (granule_index = num_granules; granule_index--;) {
            if (!use_counts[granule_index])
                result++;
        }
    }
    
    return result;
}

size_t pas_segregated_page_get_num_committed_granules(pas_segregated_page* page)
{
    const pas_segregated_page_config* page_config_ptr;
    pas_segregated_page_config page_config;
    size_t result;
    pas_page_granule_use_count* use_counts;
    uintptr_t num_granules;
    uintptr_t granule_index;
    
    page_config_ptr = pas_segregated_view_get_page_config(page->owner);
    page_config = *page_config_ptr;
    
    PAS_ASSERT(page_config.base.page_size > page_config.base.granule_size);
    
    result = 0;
    
    use_counts = pas_segregated_page_get_granule_use_counts(page, page_config);
    num_granules = page_config.base.page_size / page_config.base.granule_size;
    
    for (granule_index = num_granules; granule_index--;) {
        if (use_counts[granule_index] != PAS_PAGE_GRANULE_DECOMMITTED)
            result++;
    }
    
    return result;
}

const pas_segregated_page_config* pas_segregated_page_get_config(pas_segregated_page* page)
{
    return pas_segregated_view_get_page_config(page->owner);
}

void pas_segregated_page_add_commit_range(pas_segregated_page* page,
                                          pas_heap_summary* result,
                                          pas_range range)
{
    const pas_segregated_page_config* page_config_ptr;
    pas_segregated_page_config page_config;
    pas_page_granule_use_count* use_counts;
    uintptr_t first_granule_index;
    uintptr_t last_granule_index;
    uintptr_t granule_index;
    
    if (pas_range_is_empty(range))
        return;

    PAS_ASSERT(range.end > range.begin);

    page_config_ptr = pas_segregated_view_get_page_config(page->owner);
    page_config = *page_config_ptr;

    PAS_ASSERT(range.end <= page_config.base.page_size);

    if (page_config.base.page_size == page_config.base.granule_size) {
        result->committed += pas_range_size(range);
        return;
    }

    use_counts = pas_segregated_page_get_granule_use_counts(page, page_config);

    first_granule_index = range.begin / page_config.base.granule_size;
    last_granule_index = (range.end - 1) / page_config.base.granule_size;

    for (granule_index = first_granule_index;
         granule_index <= last_granule_index;
         ++granule_index) {
        pas_range granule_range;
        size_t overlap_size;
        
        granule_range = pas_range_create(
            granule_index * page_config.base.granule_size,
            (granule_index + 1) * page_config.base.granule_size);

        PAS_ASSERT(pas_range_overlaps(range, granule_range));

        overlap_size = pas_range_size(pas_range_create_intersection(range,
                                                                    granule_range));

        if (use_counts[granule_index] == PAS_PAGE_GRANULE_DECOMMITTED)
            result->decommitted += overlap_size;
        else
            result->committed += overlap_size;
    }
}

pas_segregated_page_and_config
pas_segregated_page_and_config_for_address_and_heap_config(uintptr_t begin,
                                                           const pas_heap_config* config)
{
    switch (config->fast_megapage_kind_func(begin)) {
    case pas_small_exclusive_segregated_fast_megapage_kind:
        return pas_segregated_page_and_config_create(
            pas_segregated_page_for_address_and_page_config(
                begin, config->small_segregated_config),
            &config->small_segregated_config);
    case pas_small_other_fast_megapage_kind: {
        pas_page_base_and_kind page_and_kind;
        page_and_kind = pas_get_page_base_and_kind_for_small_other_in_fast_megapage(begin, *config);
        switch (page_and_kind.page_kind) {
        case pas_small_shared_segregated_page_kind:
            return pas_segregated_page_and_config_create(
                pas_page_base_get_segregated(page_and_kind.page_base),
                &config->small_segregated_config);
        case pas_small_bitfit_page_kind:
            return pas_segregated_page_and_config_create_empty();
        default:
            PAS_ASSERT(!"Should not be reached");
            return pas_segregated_page_and_config_create_empty();
        }
    }
    case pas_not_a_fast_megapage_kind: {
        pas_page_base* page_base;
        page_base = config->page_header_func(begin);
        if (page_base) {
            switch (pas_page_base_get_kind(page_base)) {
            case pas_small_exclusive_segregated_page_kind:
            case pas_small_shared_segregated_page_kind:
                return pas_segregated_page_and_config_create(
                    pas_page_base_get_segregated(page_base),
                    &config->small_segregated_config);
            case pas_medium_exclusive_segregated_page_kind:
            case pas_medium_shared_segregated_page_kind:
                return pas_segregated_page_and_config_create(
                    pas_page_base_get_segregated(page_base),
                    &config->medium_segregated_config);
            default:
                return pas_segregated_page_and_config_create_empty();
            }
        }
        return pas_segregated_page_and_config_create_empty();
    } }
    PAS_ASSERT(!"Should not be reached");
    return pas_segregated_page_and_config_create_empty();
}

#endif /* LIBPAS_ENABLED */
