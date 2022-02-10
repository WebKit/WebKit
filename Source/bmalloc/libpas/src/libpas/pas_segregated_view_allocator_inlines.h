/*
 * Copyright (c) 2021-2022 Apple Inc. All rights reserved.
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

#include "pas_epoch.h"
#include "pas_physical_memory_transaction.h"
#include "pas_segregated_exclusive_view_inlines.h"
#include "pas_segregated_page_inlines.h"
#include "pas_segregated_partial_view.h"
#include "pas_segregated_view.h"

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
    static const bool verbose = false;

    pas_segregated_exclusive_view* exclusive;
    pas_segregated_partial_view* partial;
    pas_segregated_view ineligible_owning_view;
    pas_segregated_size_directory* size_directory;
    pas_segregated_directory* size_directory_base;
    pas_lock_hold_mode heap_lock_hold_mode;
    pas_segregated_shared_page_directory* shared_page_directory;
    pas_segregated_heap* heap;
    pas_segregated_shared_view* shared_view;
    pas_segregated_shared_handle* shared_handle;

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
                            heap_lock_hold_mode ? NULL : &transaction,
                            pas_segregated_page_exclusive_role);
                        if (exclusive->page_boundary) {
                            if (verbose) {
                                pas_log("Creating page header for new exclusive page, boundary = %p.\n",
                                        exclusive->page_boundary);
                            }
                            page_config.base.create_page_header(
                                exclusive->page_boundary,
                                pas_page_kind_for_segregated_variant_and_role(
                                    page_config.variant, pas_segregated_page_exclusive_role),
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
                                       page_config.base.heap_config_ptr->mmap_capability);
                if (verbose) {
                    pas_log("Creating page header when committing exclusive page, view = %p, "
                            "boundary = %p.\n",
                            exclusive, exclusive->page_boundary);
                }
                page_config.base.create_page_header(
                    exclusive->page_boundary,
                    pas_page_kind_for_segregated_variant_and_role(
                        page_config.variant, pas_segregated_page_exclusive_role),
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
                (pas_segregated_page_config*)page_config.base.page_config_ptr);

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
        
    case pas_segregated_partial_view_kind: {
        partial = pas_segregated_view_get_partial(view);
    
        size_directory = pas_compact_segregated_size_directory_ptr_load_non_null(&partial->directory);
        size_directory_base = &size_directory->base;
        heap = size_directory->heap;
        shared_page_directory = page_config.shared_page_directory_selector(heap, size_directory);
        heap_lock_hold_mode = pas_segregated_page_config_heap_lock_hold_mode(page_config);

        shared_view = pas_compact_segregated_shared_view_ptr_load(&partial->shared_view);
        if (!shared_view) {
            /* We have a primordial partial view! Let the local allocator worry about it. There's
               nothing we can do since we don't really have enough information. This path has to do
               this careful dance where it atomically picks a shared view and allocates something
               from it, and then that something has to be immediately shoved into the local
               allocator. */
            return view;
        }

        pas_lock_lock_conditionally(&shared_view->commit_lock, heap_lock_hold_mode);

        shared_handle = pas_segregated_shared_view_commit_page_if_necessary(
            shared_view, heap, shared_page_directory, partial, page_config);

        if (!shared_handle) {
            /* This means OOM */
            pas_lock_unlock_conditionally(&shared_view->commit_lock, heap_lock_hold_mode);
            return NULL;
        }

        if (!partial->is_attached_to_shared_handle) {
            unsigned* full_alloc_bits;
            unsigned begin_word_index;
            unsigned end_word_index;
            unsigned word_index;
        
            full_alloc_bits = pas_lenient_compact_unsigned_ptr_load_compact_non_null(&partial->alloc_bits);
        
            begin_word_index = partial->alloc_bits_offset;
            end_word_index = begin_word_index + partial->alloc_bits_size;
        
            if (page_config.sharing_shift == PAS_BITVECTOR_WORD_SHIFT) {
                for (word_index = begin_word_index; word_index < end_word_index; ++word_index) {
                    if (full_alloc_bits[word_index]) {
                        PAS_ASSERT(!pas_compact_atomic_segregated_partial_view_ptr_load(
                                       shared_handle->partial_views + word_index));
                        pas_compact_atomic_segregated_partial_view_ptr_store(
                            shared_handle->partial_views + word_index, partial);
                    }
                }
            } else {
                for (word_index = begin_word_index; word_index < end_word_index; ++word_index) {
                    unsigned full_alloc_word;
                
                    full_alloc_word = full_alloc_bits[word_index];
                    if (!full_alloc_word)
                        continue;
                
                    pas_segregated_partial_view_tell_shared_handle_for_word_general_case(
                        partial, shared_handle, word_index, full_alloc_word, page_config.sharing_shift);
                }
            }
        
            pas_lock_lock(&shared_view->ownership_lock);
            partial->is_attached_to_shared_handle = true;
            pas_lock_unlock(&shared_view->ownership_lock);
        }

        if (page_config.base.page_size > page_config.base.granule_size) {
            /* We always manage partial commit of medium pages under the commit lock. Otherwise,
               multiple runs of this function could interleave in weird ways. */
            pas_segregated_page* page;
            pas_lock* held_lock;

            page = pas_segregated_page_for_boundary(shared_handle->page_boundary, page_config);
            held_lock = NULL;
        
            pas_segregated_page_switch_lock(page, &held_lock, page_config);
            pas_segregated_page_commit_fully(
                page, &held_lock, pas_commit_fully_holding_page_and_commit_locks);
            pas_lock_switch(&held_lock, NULL);
        }

        /* At this point the shared handle knows about this partial view and this partial view
           is ineligible. This acts as a hold on the shared view: even if we release the commit
           lock, that shared view cannot be decommitted, even though it may be empty. */

        pas_lock_unlock_conditionally(&shared_view->commit_lock, heap_lock_hold_mode);

        return view;
    }
    default:
        PAS_ASSERT(!"Should not be reached");
        return NULL;
    }
}

static PAS_ALWAYS_INLINE void
pas_segregated_view_did_stop_allocating(pas_segregated_view view,
                                        pas_segregated_page* page,
                                        pas_segregated_page_config page_config)
{
    static const bool verbose = false;

    bool should_notify_eligibility;
    bool should_notify_emptiness;

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
    case pas_segregated_partial_view_kind: {
        pas_segregated_partial_view* partial_view;
        pas_segregated_shared_view* shared_view;
        pas_segregated_shared_handle* shared_handle;
        pas_segregated_size_directory* size_directory;
        pas_segregated_directory* size_directory_base;
        pas_segregated_shared_page_directory* shared_page_directory;
        pas_segregated_directory* shared_page_directory_base;
        bool should_consider_emptiness;

        if (verbose)
            pas_log("It's partial.\n");

        partial_view = (pas_segregated_partial_view*)pas_segregated_view_get_ptr(view);
        
        shared_view = pas_compact_segregated_shared_view_ptr_load_non_null(&partial_view->shared_view);
        size_directory =
            pas_compact_segregated_size_directory_ptr_load_non_null(&partial_view->directory);
        size_directory_base = &size_directory->base;
        shared_handle = pas_unwrap_shared_handle(shared_view->shared_handle_or_page_boundary,
                                                 page_config);
        shared_page_directory = shared_handle->directory;
        shared_page_directory_base = &shared_page_directory->base;

        if (verbose)
            pas_log("Stopping allocation in partial %p, shared %p.\n", partial_view, shared_view);

        PAS_ASSERT(partial_view->is_in_use_for_allocation);
        if (page->lock_ptr)
            pas_lock_assert_held(page->lock_ptr);

        should_notify_eligibility = false;
        should_notify_emptiness = false;
    
        should_consider_emptiness = shared_view->is_in_use_for_allocation_count == 1;

        if (should_consider_emptiness) {
            should_notify_emptiness =
                pas_segregated_page_qualifies_for_decommit(
                    page, page_config);
        }

        if (partial_view->eligibility_notification_has_been_deferred) {
            if (verbose)
                pas_log("Eligibility notification has been deferred.\n");
            partial_view->eligibility_notification_has_been_deferred = false;
            should_notify_eligibility = true;
        }

        partial_view->is_in_use_for_allocation = false;
    
        PAS_ASSERT(shared_view->is_in_use_for_allocation_count);
        shared_view->is_in_use_for_allocation_count--;

        if (should_notify_eligibility) {
            if (verbose)
                pas_log("Telling directory that %p is eligible.\n", partial_view);

            /* View must know that we are eligible first. */
            PAS_ASSERT(partial_view->eligibility_has_been_noted);

            pas_segregated_directory_view_did_become_eligible(
                size_directory_base, pas_segregated_partial_view_as_view_non_null(partial_view));

            /* At this point some other thread may be holding onto this page without holding the
               page lock. */
        }

        if (should_notify_emptiness) {
            PAS_ASSERT(!shared_view->is_in_use_for_allocation_count);
            if (verbose) {
                pas_log("Notifying emptiness on shared %p after stopping partial %p.\n",
                        shared_view, partial_view);
            }
            pas_segregated_directory_view_did_become_empty(
                shared_page_directory_base,
                pas_segregated_shared_view_as_view_non_null(shared_view));
        }
        return;
    }
    default:
        PAS_ASSERT(!"Should not be reached");
        return;
    }
}

PAS_END_EXTERN_C;

#endif /* PAS_SEGREGATED_VIEW_ALLOCATOR_INLINES_H */

