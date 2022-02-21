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

#ifndef PAS_SEGREGATED_EXCLUSIVE_VIEW_INLINES_H
#define PAS_SEGREGATED_EXCLUSIVE_VIEW_INLINES_H

#include "pas_designated_intrinsic_heap_inlines.h"
#include "pas_local_view_cache.h"
#include "pas_log.h"
#include "pas_segregated_deallocation_mode.h"
#include "pas_segregated_exclusive_view.h"
#include "pas_segregated_size_directory.h"
#include "pas_segregated_page.h"
#include "pas_thread_local_cache.h"
#include "pas_thread_local_cache_node.h"

PAS_BEGIN_EXTERN_C;

static PAS_ALWAYS_INLINE void pas_segregated_exclusive_view_did_start_allocating(
    pas_segregated_exclusive_view* view,
    pas_segregated_view owner_view, /* Could be the biasing view, or the exclusive view. */
    pas_segregated_size_directory* global_directory,
    pas_segregated_page* page,
    pas_lock** held_lock,
    pas_segregated_page_config page_config)
{
    /* This is called with the page lock held. */

    static const bool verbose = false;

    PAS_UNUSED_PARAM(view);
    PAS_UNUSED_PARAM(global_directory);

    if (verbose)
        pas_log("Did start allocating in %p.\n", page);

    /* NOTE: We cannot assert that the page is not in use for allocation since we may be coming here
       via the view cache, which always says that pages are in use for allocation. */

    /* We want to defer notifying anyone about any newly freed objects. Do this here since
       the code below may release (and then reacquire) the page lock. That could cause some
       additional objects to be freed. We don't want that to cause eligibility/emptiness
       notifications to happen! */
    page->is_in_use_for_allocation = true;

    if (page_config.base.page_size != page_config.base.granule_size)
        pas_segregated_page_commit_fully(page, held_lock, pas_commit_fully_holding_page_lock);

    page->owner = pas_segregated_view_as_ineligible(owner_view);
}

static PAS_ALWAYS_INLINE void pas_segregated_exclusive_view_did_stop_allocating(
    pas_segregated_exclusive_view* view,
    pas_segregated_size_directory* directory,
    pas_segregated_page* page,
    pas_segregated_page_config page_config,
    bool should_notify_eligibility)
{
    static const bool verbose = false;

    unsigned view_index;
    bool should_notify_emptiness;

    view_index = view->index;
    
    if (verbose)
        pas_log("Stopping allocation in exclusive %p/%p.\n", view, page);
    
    if (!pas_segregated_page_config_is_utility(page_config))
        pas_lock_testing_assert_held(page->lock_ptr);
    PAS_TESTING_ASSERT(page->is_in_use_for_allocation);
    
    should_notify_emptiness = pas_segregated_page_qualifies_for_decommit(page, page_config);
    
    page->is_in_use_for_allocation = false;
    
    if (should_notify_eligibility) {
        if (verbose)
            pas_log("Telling directory about deferred eligibility.\n");
        
        /* Page must know that we are eligible first. */
        PAS_TESTING_ASSERT(
            pas_segregated_view_get_kind(page->owner) == pas_segregated_exclusive_view_kind);
        
        pas_segregated_directory_view_did_become_eligible_at_index(
            &directory->base,
            view_index);
    }
    
    /* It's important to notify emptiness last, so that we never find outselves in a case where the
       scavenger takes the empty bit, cannot take the eligible bit, and this code had already set the
       empty bit but not the eligible bit. */
    if (should_notify_emptiness) {
        pas_segregated_directory_view_did_become_empty_at_index(
            &directory->base,
            view_index);
    }
}

static PAS_ALWAYS_INLINE void pas_segregated_exclusive_view_note_eligibility(
    pas_segregated_exclusive_view* view,
    pas_segregated_page* page,
    pas_segregated_deallocation_mode deallocation_mode,
    pas_thread_local_cache* cache,
    pas_segregated_page_config page_config)
{
    static const bool verbose = false;
    
    pas_segregated_size_directory* size_directory;
    pas_segregated_directory* directory;

    size_directory = pas_compact_segregated_size_directory_ptr_load_non_null(&view->directory);
    directory = &size_directory->base;
    
    if (verbose)
        pas_log("Noting eligibility in exclusive %p/%p.\n", view, page);
        
    if (page->lock_ptr)
        pas_lock_testing_assert_held(page->lock_ptr);
    
    if (page->is_in_use_for_allocation) {
        if (verbose)
            pas_log("Deferring eligibility notification.\n");
        page->eligibility_notification_has_been_deferred = true;
    } else {
        bool did_cache_view;

        did_cache_view = false;

        switch (deallocation_mode) {
        case pas_segregated_deallocation_direct_mode:
            if (verbose)
                pas_log("Using direct deallocation mode (%u).\n", page->object_size);
            break;

        case pas_segregated_deallocation_to_view_cache_mode: {
            pas_local_allocator_result allocator_result;
            pas_local_view_cache* view_cache;

            if (verbose)
                pas_log("Using to_view_cache deallocation mode (%u).\n", page->object_size);
            
            if (!page_config.enable_view_cache) {
                if (verbose)
                    pas_log("%p: View cache disabled in config", cache->node);
                break;
            }

            pas_lock_testing_assert_held(&cache->node->scavenger_lock);

            allocator_result =
                pas_thread_local_cache_try_get_local_allocator_for_possibly_uninitialized_but_not_unselected_index(
                    cache, page->view_cache_index);
            if (!allocator_result.did_succeed) {
                if (verbose) {
                    pas_log("%p: View cache is disabled for this page or it doesn't exist yet in this "
                            "TLC.\n", cache->node);
                }
                break;
            }
            
            view_cache = (pas_local_view_cache*)allocator_result.allocator;
            
            if (verbose)
                pas_log("Trying to push to view cache.\n");

            if (!pas_local_view_cache_prepare_to_push(view_cache)) {
                if (verbose)
                    pas_log("%p, %p: Cache is full or is stopped (%u).\n", cache->node, view_cache, page->object_size);
                break;
            }
            
            did_cache_view = true;
            
            page->is_in_use_for_allocation = true;
            
            if (verbose)
                pas_log("%p, %p: Pushing to cache (%u).\n", cache->node, view_cache, page->object_size);

            pas_local_view_cache_push(view_cache, view);
            break;
        } }

        if (!did_cache_view) {
            if (verbose)
                pas_log("Telling the directory that we are eligible (%u).\n", page->object_size);
            pas_segregated_directory_view_did_become_eligible_at_index(directory, view->index);
        }
    }

    /* Clear the ineligible bit from the owner field. */
    page->owner = pas_segregated_exclusive_view_as_view_non_null(view);
}

PAS_END_EXTERN_C;

#endif /* PAS_SEGREGATED_EXCLUSIVE_VIEW_INLINES_H */

