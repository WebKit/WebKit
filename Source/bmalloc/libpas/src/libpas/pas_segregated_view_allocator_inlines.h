/*
 * Copyright (c) 2021-2025 Apple Inc. All rights reserved.
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

#ifndef PAS_SEGREGATED_VIEW_ALLOCATOR_INLINES_H
#define PAS_SEGREGATED_VIEW_ALLOCATOR_INLINES_H

#include "pas_debug_spectrum.h"
#include "pas_page_sharing_pool.h"
#include "pas_physical_memory_transaction.h"
#include "pas_segregated_exclusive_view_inlines.h"
#include "pas_segregated_page_inlines.h"
#include "pas_segregated_view.h"

#if LIBPAS_ENABLED

PAS_BEGIN_EXTERN_C;

/* This is a preparation before allocation that doesn't involve holding the same locks as
   did_start_allocating.
   
   It's possible that this will decide that there is an OOM or that it wants you to use a different
   view, in which case that view will function as if you'd called will_start_allocating on it. For
   OOM, this returns NULL. Otherwise it returns the view you're supposed to use (which is likely to
   be this view but could be a different one).
   
   Note that after this is called, we're inevitably going to call did_start_allocating. This may have
   effects that cannot be undone except by plowing ahead with did_start_allocating and then
   did_stop_allocating. Crucially, this function is called with no locks held, which makes a lot of
   stuff easier. */
static PAS_ALWAYS_INLINE pas_segregated_view
pas_segregated_view_will_start_allocating(pas_segregated_view view,
                                          pas_segregated_page_config page_config)
{
    static const bool verbose = PAS_SHOULD_LOG(PAS_LOG_SEGREGATED_HEAPS);

    pas_segregated_exclusive_view* exclusive;
    pas_segregated_view ineligible_owning_view;
    pas_segregated_size_directory* size_directory;
    pas_segregated_directory* size_directory_base;
    pas_lock_hold_mode heap_lock_hold_mode;

    switch (pas_segregated_view_get_kind(view)) {
    case pas_segregated_exclusive_view_kind:
    case pas_segregated_ineligible_exclusive_view_kind: {
        exclusive = (pas_segregated_exclusive_view*)pas_segregated_view_get_ptr(view);
        ineligible_owning_view = pas_segregated_exclusive_view_as_ineligible_view_non_null(exclusive);

        /* This is called with the page lock NOT held. */

        PAS_TESTING_ASSERT(
            pas_segregated_view_get_kind(ineligible_owning_view)
            == pas_segregated_ineligible_exclusive_view_kind);
    
        size_directory =
            pas_compact_segregated_size_directory_ptr_load_non_null(&exclusive->directory);
        size_directory_base = &size_directory->base;
        heap_lock_hold_mode = pas_segregated_page_config_heap_lock_hold_mode(page_config);

        /* Cool fact: this won't cause the size directory to have sharing payloads until that
           size directory allocates exclusive views. */

        /* Even though we hold no locks, we can totally check this bit. The eligible bit is a secondary
           protection here since it is only legal to change the value of this bit if you've stolen
           eligibility and locked the page. */
        if (!exclusive->is_owned) {
            bool was_stolen;

            was_stolen = false;
        
            if (!exclusive->page_boundary) {
                if (verbose)
                    pas_log("No page, trying to get one somehow.\n");
                if (!exclusive->page_boundary
                    && pas_segregated_directory_can_do_sharing(size_directory_base)) {
                    if (verbose)
                        pas_log("Trying physical.\n");
                    pas_physical_page_sharing_pool_take_for_page_config(
                        page_config.base.page_size, page_config.base.page_config_ptr,
                        heap_lock_hold_mode, NULL, 0);
                }
            
                if (!exclusive->page_boundary) {
                    pas_physical_memory_transaction transaction;
                    
                    if (verbose)
                        pas_log("Allocating a page.\n");

                    pas_physical_memory_transaction_construct(&transaction);
                    do {
                        PAS_ASSERT(!exclusive->page_boundary);

                        pas_physical_memory_transaction_begin(&transaction);
                        pas_heap_lock_lock_conditionally(heap_lock_hold_mode);
                        exclusive->page_boundary = page_config.page_allocator(
                            size_directory->heap,
                            heap_lock_hold_mode ? NULL : &transaction);
                        if (exclusive->page_boundary) {
                            if (verbose) {
                                pas_log("Creating page header for new exclusive page, boundary = %p.\n",
                                        exclusive->page_boundary);
                            }
                            page_config.base.create_page_header(
                                exclusive->page_boundary,
                                pas_page_kind_for_segregated_variant(page_config.variant),
                                pas_lock_is_held);
                        }
                        pas_heap_lock_unlock_conditionally(heap_lock_hold_mode);
                        if (verbose)
                            pas_log("Got page = %p.\n", exclusive->page_boundary);
                    } while (!pas_physical_memory_transaction_end(&transaction));
                }
            
                if (!exclusive->page_boundary) {
                    /* Out of memory. */
                    return NULL;
                }
            } else {
                enum { max_num_locks_held = 1 };
                pas_lock* locks_held[max_num_locks_held];
                size_t num_locks_held;
                bool did_lock_lock;
            
                /* At this point nobody but us can *initiate* anything on this page. But it's
                   possible that whoever decommitted this page isn't actually done decommitting
                   yet. So we grab the commit lock for this page to ensure that we are the ones
                   who get to do it. Note that if the heap lock is held then we have to do this
                   via the physical memory transaction. Also note that it's very unlikely that
                   the commit lock is held here, since we try to only decommit pages that are
                   unlikely to be used anytime soon. */
            
                num_locks_held = 0;
                did_lock_lock = false;

                /* The utility heap does all commit/decommit under the heap lock, so we don't need to
                   play any games with the commit lock.  */
                if (pas_segregated_page_config_is_utility(page_config))
                    pas_heap_lock_assert_held();
                else {
                    PAS_ASSERT(heap_lock_hold_mode == pas_lock_is_not_held);
                    pas_lock_lock(&exclusive->commit_lock);
                    locks_held[num_locks_held++] = &exclusive->commit_lock;
                    did_lock_lock = true;
                }
                pas_compiler_fence();
            
                PAS_ASSERT(num_locks_held <= max_num_locks_held);

                if (pas_segregated_directory_can_do_sharing(size_directory_base)) {
                    pas_physical_page_sharing_pool_take_for_page_config(
                        page_config.base.page_size,
                        page_config.base.page_config_ptr,
                        heap_lock_hold_mode,
                        locks_held,
                        num_locks_held);
                }
            
                pas_page_malloc_commit(exclusive->page_boundary, page_config.base.page_size,
                                       page_config.base.heap_config_ptr->page_flags);
                if (verbose) {
                    pas_log("Creating page header when committing exclusive page, view = %p, "
                            "boundary = %p.\n",
                            exclusive, exclusive->page_boundary);
                }
                page_config.base.create_page_header(
                    exclusive->page_boundary,
                    pas_page_kind_for_segregated_variant(page_config.variant),
                    heap_lock_hold_mode);
            
                pas_compiler_fence();
                if (did_lock_lock)
                    pas_lock_unlock(&exclusive->commit_lock);

                if (PAS_DEBUG_SPECTRUM_USE_FOR_COMMIT) {
                    pas_heap_lock_lock_conditionally(heap_lock_hold_mode);
                    pas_debug_spectrum_add(
                        size_directory, pas_segregated_size_directory_dump_for_spectrum,
                        page_config.base.page_size);
                    pas_heap_lock_unlock_conditionally(heap_lock_hold_mode);
                }
            }
        
            pas_segregated_page_construct(
                pas_segregated_page_for_boundary_unchecked(exclusive->page_boundary, page_config),
                ineligible_owning_view,
                was_stolen,
                (const pas_segregated_page_config*)page_config.base.page_config_ptr);

            pas_lock_lock_conditionally(&exclusive->ownership_lock, heap_lock_hold_mode);
            exclusive->is_owned = true;
            if (verbose) {
                pas_log("view %p has is owned = true due to exclusive will_start_allocating\n",
                        exclusive);
            }
            pas_lock_unlock_conditionally(&exclusive->ownership_lock, heap_lock_hold_mode);
        }

        PAS_TESTING_ASSERT(exclusive->page_boundary);

        return view;
    }
        
    default:
        PAS_ASSERT_NOT_REACHED();
        return NULL;
    }
}

static PAS_ALWAYS_INLINE void
pas_segregated_view_did_stop_allocating(pas_segregated_view view,
                                        pas_segregated_page* page,
                                        pas_segregated_page_config page_config)
{
    static const bool verbose = PAS_SHOULD_LOG(PAS_LOG_SEGREGATED_HEAPS);

    bool should_notify_eligibility;

    if (verbose)
        pas_log("Did stop allocating in view %p, page %p.\n", view, page);
    
    switch (pas_segregated_view_get_kind(view)) {
    case pas_segregated_exclusive_view_kind:
    case pas_segregated_ineligible_exclusive_view_kind: {
        pas_segregated_exclusive_view* exclusive;
        
        if (verbose)
            pas_log("It's exclusive.\n");
        exclusive = (pas_segregated_exclusive_view*)pas_segregated_view_get_ptr(view);

        should_notify_eligibility = false;
        
        if (page->eligibility_notification_has_been_deferred) {
            if (verbose)
                pas_log("Eligibility notification has been deferred.\n");
            page->eligibility_notification_has_been_deferred = false;
            should_notify_eligibility = true;
        } else {
            if (verbose)
                pas_log("No deferred eligibility notification.\n");
        }
        
        pas_segregated_exclusive_view_did_stop_allocating(
            exclusive, pas_compact_segregated_size_directory_ptr_load_non_null(&exclusive->directory),
            page, page_config, should_notify_eligibility);
        return;
    }
    default:
        PAS_ASSERT_NOT_REACHED();
        return;
    }
}

PAS_END_EXTERN_C;

#endif /* LIBPAS_ENABLED */
#endif /* PAS_SEGREGATED_VIEW_ALLOCATOR_INLINES_H */
