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

#include "pas_bitfit_global_directory.h"

#include "pas_bitfit_page.h"
#include "pas_bitfit_view_inlines.h"
#include "pas_deferred_decommit_log.h"
#include "pas_free_granules.h"
#include "pas_page_sharing_pool.h"
#include "pas_segregated_heap.h"
#include "pas_stream.h"
#include "pas_subpage_map.h"

bool pas_bitfit_global_directory_can_explode = true;
bool pas_bitfit_global_directory_force_explode = false;

void pas_bitfit_global_directory_construct(pas_bitfit_global_directory* directory,
                                           pas_bitfit_page_config* config,
                                           pas_segregated_heap* heap)
{
    pas_bitfit_directory_construct(&directory->base, pas_bitfit_global_directory_kind, config);
    directory->heap = heap;
    pas_bitfit_global_directory_segmented_bitvectors_construct(&directory->bitvectors);
    pas_compact_atomic_page_sharing_pool_ptr_store(&directory->bias_sharing_pool, NULL);
    pas_page_sharing_participant_payload_construct(&directory->physical_sharing_payload);
    directory->contention_did_trigger_explosion =
        pas_bitfit_global_directory_can_explode & pas_bitfit_global_directory_force_explode;

    /* We could have been lazy about this - but it's probably not super necessary since the point
       of bitfit global directories is that there won't be too many of them. */
    if (pas_bitfit_global_directory_does_sharing(directory)) {
        pas_page_sharing_pool_add(
            &pas_physical_page_sharing_pool,
            pas_page_sharing_participant_create(
                directory, pas_page_sharing_participant_bitfit_directory));
    }
}

bool pas_bitfit_global_directory_does_sharing(pas_bitfit_global_directory* directory)
{
    return pas_page_sharing_mode_does_sharing(
        directory->heap->runtime_config->sharing_mode);
}
    
uint64_t pas_bitfit_global_directory_get_use_epoch(pas_bitfit_global_directory* directory)
{
    uintptr_t last_empty_plus_one;
    size_t index;
    pas_bitfit_page_config config;

    last_empty_plus_one = directory->last_empty_plus_one.value;
    config = *pas_bitfit_page_config_kind_get_config(directory->base.config_kind);

    for (index = last_empty_plus_one; index--;) {
        pas_bitfit_view* view;
        uint64_t epoch;

        /* Protect against unset epochs. */
        if (!pas_bitfit_global_directory_get_empty_bit_at_index(directory, index))
            continue;

        view = pas_bitfit_directory_get_view(&directory->base, index);
        if (!view)
            continue;

        pas_lock_lock(&view->ownership_lock);
        epoch = 0;
        if (view->is_owned) {
            pas_bitfit_page* page;
            page = pas_bitfit_page_for_boundary(view->page_boundary, config);
            if (!page->num_live_bits) {
                epoch = page->use_epoch;
                PAS_ASSERT(epoch);
            }
        }
        pas_lock_unlock(&view->ownership_lock);

        if (epoch)
            return epoch;
    }

    return 0;
}

bool pas_bitfit_global_directory_get_empty_bit_at_index(
    pas_bitfit_global_directory* directory,
    size_t index)
{
    return pas_bitvector_get_from_word(
        pas_bitfit_global_directory_segmented_bitvectors_get_ptr_checked(
            &directory->bitvectors, PAS_BITVECTOR_WORD_INDEX(index))->empty_bits,
        index);
}

/* Returns whether we changed the value. */
bool pas_bitfit_global_directory_set_empty_bit_at_index(
    pas_bitfit_global_directory* directory,
    size_t index,
    bool value)
{
    return pas_bitvector_set_atomic_in_word(
        &pas_bitfit_global_directory_segmented_bitvectors_get_ptr_checked(
            &directory->bitvectors, PAS_BITVECTOR_WORD_INDEX(index))->empty_bits,
        index,
        value);
}

void pas_bitfit_global_directory_view_did_become_empty_at_index(
    pas_bitfit_global_directory* directory,
    size_t index)
{
    if (!pas_bitfit_global_directory_set_empty_bit_at_index(directory, index, true))
        return;

    /* If we already knew about empties, then we have nothing more to do. */
    if (pas_versioned_field_maximize(&directory->last_empty_plus_one, index + 1))
        return;

    if (!pas_bitfit_global_directory_does_sharing(directory))
        return;
    
    pas_page_sharing_pool_did_create_delta(
        &pas_physical_page_sharing_pool,
        pas_page_sharing_participant_create(
            directory, pas_page_sharing_participant_bitfit_directory));
}

void pas_bitfit_global_directory_view_did_become_empty(
    pas_bitfit_global_directory* directory,
    pas_bitfit_view* view)
{
    PAS_TESTING_ASSERT(
        pas_compact_bitfit_global_directory_ptr_load_non_null(&view->global_directory)
        == directory);
    pas_bitfit_global_directory_view_did_become_empty_at_index(
        directory, view->index_in_global);
}

pas_page_sharing_pool_take_result pas_bitfit_global_directory_take_last_empty(
    pas_bitfit_global_directory* global_directory,
    pas_deferred_decommit_log* decommit_log,
    pas_lock_hold_mode heap_lock_hold_mode)
{
    static const bool verbose = false;
    
    pas_bitfit_directory* directory;
    pas_versioned_field last_empty_plus_one_value;
    size_t index;
    pas_bitfit_page_config* page_config;
    size_t num_granules;
    bool uses_subpages;

    directory = &global_directory->base;

    last_empty_plus_one_value = pas_versioned_field_read(&global_directory->last_empty_plus_one);

    page_config = pas_bitfit_page_config_kind_get_config(directory->config_kind);
    num_granules = page_config->base.page_size / page_config->base.granule_size;
    uses_subpages = pas_bitfit_page_config_uses_subpages(*page_config);

    for (index = last_empty_plus_one_value.value; index--;) {
        pas_bitfit_view* view;
        pas_bitfit_page* page;
        uintptr_t base;
        pas_subpage_map_entry* subpage_entry;
        pas_lock* commit_lock;
        bool page_is_dead;
        pas_bitfit_directory_and_index directory_and_index;
        pas_bitfit_max_free max_free;
        
        if (!pas_bitfit_global_directory_set_empty_bit_at_index(global_directory, index, false))
            continue;

        view = pas_bitfit_directory_get_view(directory, index);

        PAS_ASSERT(view);
        PAS_ASSERT(view->page_boundary); /* You don't become empty until you have a page and then
                                            you never lose the page. */

        if (uses_subpages) {
            subpage_entry = pas_subpage_map_get(
                view->page_boundary, page_config->base.page_size, heap_lock_hold_mode);
            commit_lock = &subpage_entry->commit_lock;
        } else {
            subpage_entry = NULL;
            commit_lock = &view->commit_lock;
        }

        if (verbose)
            pas_log("Grabbing commit lock %p\n", commit_lock);
        if (!pas_deferred_decommit_log_lock_for_adding(
                decommit_log, commit_lock, heap_lock_hold_mode)) {
            pas_bitfit_global_directory_view_did_become_empty_at_index(global_directory, index);
            return pas_page_sharing_pool_take_locks_unavailable;
        }

        pas_lock_lock(&view->ownership_lock);

        if (!view->is_owned) {
            pas_lock_unlock(&view->ownership_lock);
            pas_deferred_decommit_log_unlock_after_aborted_add(decommit_log, commit_lock);
            continue;
        }

        base = (uintptr_t)view->page_boundary;
        PAS_ASSERT(base);
        page = pas_bitfit_page_for_boundary((void*)base, *page_config);
        PAS_ASSERT(page);

        page_is_dead = !page->num_live_bits;

        directory_and_index = pas_bitfit_view_current_directory_and_index(view);
        max_free = pas_bitfit_directory_get_max_free(directory_and_index.directory,
                                                     directory_and_index.index);
        if (page_is_dead) {
            /* If a page's num_live_bits drops to zero then we will mark it empty in the directory that
               owns it while holding the lock. Newly created pages start out empty.
            
               The fact that dead pages are empty in biasing directories has an interesting consequence.
               It means that when we decommit it then the biasing directory will skip this page. We also
               unbias it so that if we do come back to it, we'll get a fresh page from the global
               directory rather than trying to commit this one.  */
            if (max_free != PAS_BITFIT_MAX_FREE_EMPTY) {
                pas_log("%p:%u: found non-empty page that is dead when taking last empty.\n");
                PAS_ASSERT(max_free == PAS_BITFIT_MAX_FREE_EMPTY);
            }

            if (directory_and_index.kind == pas_bitfit_biasing_directory_kind) {
                view->index_in_biasing = 0;
                pas_bitfit_directory_max_free_did_become_empty(
                    directory, index, "become empty in global on unbias during page take");
                pas_compact_atomic_bitfit_biasing_directory_ptr_store(&view->biasing_directory, NULL);
                
                /* Make sure that we don't clear the view out of the directory until we're done telling
                   the view that it's not biased. */
                pas_fence();
                pas_compact_atomic_bitfit_view_ptr_store(
                    pas_bitfit_directory_get_view_ptr(directory_and_index.directory,
                                                      directory_and_index.index),
                    NULL);
            }
        } else {
            /* If a page's num_live_bits goes above zero then we mark it unprocessed while holding the
               lock. */
            if (max_free == PAS_BITFIT_MAX_FREE_EMPTY) {
                pas_log("%p:%u: found empty page that is not dead when taking last empty.\n");
                PAS_ASSERT(max_free != PAS_BITFIT_MAX_FREE_EMPTY);
            }
        }
        
        if (num_granules > 1) {
            pas_free_granules free_granules;
            pas_page_granule_use_count* use_counts;
            
            PAS_ASSERT(!uses_subpages); /* We should never set up subpages for a page with
                                           granules. We could change that (by adding a bit
                                           more logic) but for now it makes no sense. It would
                                           only make sense if we had to deal with a dynamic
                                           system page size and we didn't want to achieve that
                                           by shipping multiple configs. */
            
            if (verbose)
                pas_log("Decommitting granules for page %p\n", page);
            use_counts = pas_bitfit_page_get_granule_use_counts(page, *page_config);
            
            pas_free_granules_compute_and_mark_decommitted(
                &free_granules, use_counts, num_granules);

            if (page_is_dead != ((free_granules.num_already_decommitted_granules +
                                  free_granules.num_free_granules) == num_granules)) {
                pas_log("page_is_dead = %d\n", page_is_dead);
                pas_log("free_granules.num_free_granules = %zu\n",
                        free_granules.num_free_granules);
                pas_log("free_granules.num_already_decommitted_granules = %zu\n",
                        free_granules.num_already_decommitted_granules);
                PAS_ASSERT(!"page_is_dead does not match free_granules");
            }
            
            PAS_ASSERT(free_granules.num_free_granules <= num_granules);

            if (!free_granules.num_free_granules) {
                pas_lock_unlock(&view->ownership_lock);
                pas_deferred_decommit_log_unlock_after_aborted_add(decommit_log, commit_lock);
                continue;
            }
            
            if (page_is_dead)
                view->is_owned = false;
            
            /* We have to release the ownership lock to decommit because decommit wants the heap
               lock. That's safe since we've already marked the stuff decommitted. That means
               that any attempt to use those granules will have to grab the commit lock, which
               we hold. */
            pas_lock_unlock(&view->ownership_lock);
            
            if (verbose)
                pas_log("Doing free granule decommit with lock %p\n", commit_lock);
            pas_free_granules_decommit_after_locking_range(
                &free_granules, &page->base, decommit_log, commit_lock, &page_config->base,
                heap_lock_hold_mode);
            
            if (verbose)
                pas_log("Done, success!\n");
            
        } else {
            if (!page_is_dead) {
                pas_lock_unlock(&view->ownership_lock);
                pas_deferred_decommit_log_unlock_after_aborted_add(decommit_log, commit_lock);
                continue;
            }

            /* We do this while holding both locks. When we release the ownership lock, then if someone
               else gets it, they will immediately release it to try to compete for the commit lock,
               which we hold. */
            view->is_owned = false;

            /* And we have to release the ownership lock because decommitting may need to hold the heap
               lock, and the heap lock is outer to the ownership lock. */
            pas_lock_unlock(&view->ownership_lock);

            if (verbose)
                pas_log("Decomitting page %p\n", page);

            if (uses_subpages) {
                pas_subpage_map_entry_decommit(
                    subpage_entry,
                    (void*)base,
                    page_config->base.page_size,
                    decommit_log,
                    heap_lock_hold_mode);
            } else {
                pas_deferred_decommit_log_add_already_locked(
                    decommit_log,
                    pas_virtual_range_create(base, base + page_config->base.page_size, commit_lock),
                    heap_lock_hold_mode);
            }
        }
        
        pas_versioned_field_try_write(&global_directory->last_empty_plus_one,
                                      last_empty_plus_one_value,
                                      index);
        
        if (page_is_dead)
            page_config->base.destroy_page_header(&page->base, heap_lock_hold_mode);

        PAS_ASSERT(pas_bitfit_global_directory_does_sharing(global_directory));
        
        pas_page_sharing_pool_did_create_delta(
            &pas_physical_page_sharing_pool,
            pas_page_sharing_participant_create(
                global_directory, pas_page_sharing_participant_bitfit_directory));
        
        return pas_page_sharing_pool_take_success;
    }

    pas_versioned_field_try_write(&global_directory->last_empty_plus_one,
                                  last_empty_plus_one_value,
                                  0);
    
    return pas_page_sharing_pool_take_none_available;
}

void pas_bitfit_global_directory_dump_reference(
    pas_bitfit_global_directory* directory, pas_stream* stream)
{
    pas_stream_printf(
        stream, "%p(bitfit_global_directory, %s)",
        directory, pas_bitfit_page_config_kind_get_string(directory->base.config_kind));
}

void pas_bitfit_global_directory_dump_for_spectrum(pas_stream* stream, void* directory)
{
    pas_bitfit_global_directory_dump_reference(directory, stream);
}

#endif /* LIBPAS_ENABLED */
