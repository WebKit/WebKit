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

#include "pas_bitfit_directory.h"

#include "pas_bitfit_directory_inlines.h"
#include "pas_bitfit_size_class.h"
#include "pas_bitfit_view_inlines.h"
#include "pas_deferred_decommit_log.h"
#include "pas_epoch.h"
#include "pas_free_granules.h"
#include "pas_heap_lock.h"
#include "pas_page_sharing_pool.h"
#include "pas_segregated_heap.h"
#include "pas_stream.h"

void pas_bitfit_directory_construct(pas_bitfit_directory* directory,
                                    const pas_bitfit_page_config* config,
                                    pas_segregated_heap* heap)
{
    static const bool verbose = false;

    /* NOTE - this works even if the config is disabled, and produces a directory that is empty and
       does nothing. This makes sense because it makes it easy to iterate over the directories in a heap
       without knowing what your config is. */

    if (verbose)
        pas_log("Creating directory %p\n", directory);
    
    pas_versioned_field_construct(&directory->first_unprocessed_free, 0);
    pas_versioned_field_construct(&directory->first_empty, 0);
    pas_bitfit_directory_max_free_vector_construct(&directory->max_frees);
    pas_bitfit_directory_view_vector_construct(&directory->views);
    pas_compact_atomic_bitfit_size_class_ptr_store(&directory->largest_size_class, NULL);
    directory->config_kind = config->base.is_enabled ? config->kind : pas_bitfit_page_config_kind_null;

    directory->heap = heap;
    pas_bitfit_directory_segmented_bitvectors_construct(&directory->bitvectors);
    pas_page_sharing_participant_payload_construct(&directory->physical_sharing_payload);

    /* We could have been lazy about this - but it's probably not super necessary since the point
       of bitfit global directories is that there won't be too many of them. */
    if (config->base.is_enabled && pas_bitfit_directory_does_sharing(directory)) {
        pas_page_sharing_pool_add(
            &pas_physical_page_sharing_pool,
            pas_page_sharing_participant_create(
                directory, pas_page_sharing_participant_bitfit_directory));
    }
}

void pas_bitfit_directory_max_free_did_become_unprocessed(pas_bitfit_directory* directory,
                                                          size_t index,
                                                          const char* reason)
{
    pas_bitfit_directory_set_unprocessed_max_free(directory, index, reason);
    pas_versioned_field_minimize(&directory->first_unprocessed_free, index);
}

void pas_bitfit_directory_max_free_did_become_unprocessed_unchecked(pas_bitfit_directory* directory,
                                                                    size_t index,
                                                                    const char* reason)
{
    pas_bitfit_directory_set_unprocessed_max_free_unchecked(directory, index, reason);
    pas_versioned_field_minimize(&directory->first_unprocessed_free, index);
}

void pas_bitfit_directory_max_free_did_become_empty(
    pas_bitfit_directory* directory,
    size_t index,
    const char* reason)
{
    pas_bitfit_directory_set_empty_max_free(directory, index, reason);
    pas_versioned_field_minimize(&directory->first_empty, index);
}

pas_bitfit_view_and_index
pas_bitfit_directory_get_first_free_view(pas_bitfit_directory* directory,
                                         unsigned start_index,
                                         unsigned size,
                                         const pas_bitfit_page_config* page_config)
{
    static const bool verbose = false;

    PAS_ASSERT(page_config->base.is_enabled);
    
    for (;;) {
        pas_found_index found_index;
        size_t max_frees_size;
        pas_bitfit_view* view;

        view = NULL;

        max_frees_size = directory->max_frees.size;

        directory += pas_depend(max_frees_size);
        
        found_index = pas_bitfit_directory_find_first_free(directory, start_index, size, *page_config);
        
        if (!found_index.did_succeed) {
            pas_compact_atomic_bitfit_view_ptr null_ptr = PAS_COMPACT_ATOMIC_PTR_INITIALIZER;
            pas_found_index found_empty_index;
            pas_versioned_field first_empty;

            /* Before we try to grow things, check if we can find an empty. */
            first_empty = pas_versioned_field_read_to_watch(&directory->first_empty);
            PAS_ASSERT((unsigned)first_empty.value == first_empty.value);
            found_empty_index = pas_bitfit_directory_find_first_empty(directory,
                                                                      (unsigned)first_empty.value);
            if (found_empty_index.did_succeed) {
                pas_versioned_field_maximize_watched(
                    &directory->first_empty, first_empty, found_empty_index.index);

                found_index = found_empty_index;
            } else {
                pas_versioned_field_maximize_watched(
                    &directory->first_empty, first_empty, max_frees_size);
            
                pas_heap_lock_lock();
                if (max_frees_size != directory->max_frees.size) {
                    pas_heap_lock_unlock();
                    continue;
                }

                PAS_ASSERT(max_frees_size == directory->views.size);

                pas_bitfit_directory_view_vector_append(
                    &directory->views, null_ptr, pas_lock_is_held);
                pas_bitfit_directory_max_free_vector_append(
                    &directory->max_frees, PAS_BITFIT_MAX_FREE_EMPTY, pas_lock_is_held);
                if (verbose) {
                    pas_log("Directory %p views, max_frees sizes = %u, %u\n",
                            directory, directory->views.size, directory->max_frees.size);
                }

                if (PAS_BITVECTOR_NUM_WORDS(directory->views.size)
                    != directory->bitvectors.size) {
                    PAS_ASSERT(PAS_BITVECTOR_NUM_WORDS(directory->views.size)
                               == directory->bitvectors.size + 1);
                    pas_bitfit_directory_segmented_bitvectors_append(
                        &directory->bitvectors,
                        PAS_BITFIT_DIRECTORY_BITVECTOR_SEGMENT_INITIALIZER,
                        pas_lock_is_held);
                    PAS_ASSERT(PAS_BITVECTOR_NUM_WORDS(directory->views.size)
                               == directory->bitvectors.size);
                }
            
                pas_heap_lock_unlock();
            }
        }

        if (verbose) {
            pas_log("%p:%u: getting view for get_first_free_view\n",
                    directory, (unsigned)found_index.index);
        }
        view = pas_bitfit_directory_get_view(directory, found_index.index);
        if (verbose) {
            pas_log("%p:%u: got view for get_first_free_view: %p\n",
                    directory, (unsigned)found_index.index, view);
        }

        if (!view) {
            pas_heap_lock_lock();
            view = pas_bitfit_directory_get_view(directory, found_index.index);
            if (!view) {
                PAS_ASSERT((unsigned)found_index.index == found_index.index);
                view = pas_bitfit_view_create(directory, (unsigned)found_index.index);
                pas_store_store_fence();
                pas_compact_atomic_bitfit_view_ptr_store(
                    pas_bitfit_directory_get_view_ptr(directory, found_index.index), view);
            }
            pas_heap_lock_unlock();
        }
        
        PAS_TESTING_ASSERT(
            pas_compact_bitfit_directory_ptr_load_non_null(&view->directory)
            == directory);
        PAS_TESTING_ASSERT(view->index == found_index.index);

        return pas_bitfit_view_and_index_create(view, found_index.index);
    }
}

pas_heap_summary pas_bitfit_directory_compute_summary(pas_bitfit_directory* directory)
{
    pas_heap_summary result;
    size_t index;

    result = pas_heap_summary_create_empty();

    for (index = 0; index < pas_bitfit_directory_size(directory); ++index) {
        pas_bitfit_view* view;

        view = pas_bitfit_directory_get_view(directory, index);
        if (!view)
            continue;
        
        result = pas_heap_summary_add(result, pas_bitfit_view_compute_summary(view));
    }

    return result;
}

typedef struct {
    pas_bitfit_directory* directory;
    pas_bitfit_directory_for_each_live_object_callback callback;
    void* arg;
} for_each_live_object_data;

static bool for_each_live_object_callback(
    pas_bitfit_view* view,
    uintptr_t begin,
    size_t size,
    void* arg)
{
    for_each_live_object_data* data;

    data = arg;

    return data->callback(data->directory, view, begin, size, data->arg);
}

bool pas_bitfit_directory_for_each_live_object(
    pas_bitfit_directory* directory,
    pas_bitfit_directory_for_each_live_object_callback callback,
    void* arg)
{
    for_each_live_object_data data;
    size_t index;
    
    data.directory = directory;
    data.callback = callback;
    data.arg = arg;

    for (index = 0; index < pas_bitfit_directory_size(directory); ++index) {
        pas_bitfit_view* view;

        view = pas_bitfit_directory_get_view(directory, index);
        if (!view)
            continue;

        if (!pas_bitfit_view_for_each_live_object(view, for_each_live_object_callback, &data))
            return false;
    }

    return true;
}

bool pas_bitfit_directory_does_sharing(pas_bitfit_directory* directory)
{
    return pas_page_sharing_mode_does_sharing(
        directory->heap->runtime_config->sharing_mode);
}
    
uint64_t pas_bitfit_directory_get_use_epoch(pas_bitfit_directory* directory)
{
    uintptr_t last_empty_plus_one;
    size_t index;
    pas_bitfit_page_config config;

    last_empty_plus_one = directory->last_empty_plus_one.value;
    config = *pas_bitfit_page_config_kind_get_config(directory->config_kind);

    for (index = last_empty_plus_one; index--;) {
        pas_bitfit_view* view;
        uint64_t epoch;

        /* Protect against unset epochs. */
        if (!pas_bitfit_directory_get_empty_bit_at_index(directory, index))
            continue;

        view = pas_bitfit_directory_get_view(directory, index);
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

bool pas_bitfit_directory_get_empty_bit_at_index(
    pas_bitfit_directory* directory,
    size_t index)
{
    return pas_bitvector_get_from_word(
        pas_bitfit_directory_segmented_bitvectors_get_ptr_checked(
            &directory->bitvectors, PAS_BITVECTOR_WORD_INDEX(index))->empty_bits,
        index);
}

/* Returns whether we changed the value. */
bool pas_bitfit_directory_set_empty_bit_at_index(
    pas_bitfit_directory* directory,
    size_t index,
    bool value)
{
    return pas_bitvector_set_atomic_in_word(
        &pas_bitfit_directory_segmented_bitvectors_get_ptr_checked(
            &directory->bitvectors, PAS_BITVECTOR_WORD_INDEX(index))->empty_bits,
        index,
        value);
}

void pas_bitfit_directory_view_did_become_empty_at_index(
    pas_bitfit_directory* directory,
    size_t index)
{
    if (!pas_bitfit_directory_set_empty_bit_at_index(directory, index, true))
        return;

    /* If we already knew about empties, then we have nothing more to do. */
    if (pas_versioned_field_maximize(&directory->last_empty_plus_one, index + 1))
        return;

    if (!pas_bitfit_directory_does_sharing(directory))
        return;
    
    pas_page_sharing_pool_did_create_delta(
        &pas_physical_page_sharing_pool,
        pas_page_sharing_participant_create(
            directory, pas_page_sharing_participant_bitfit_directory));
}

void pas_bitfit_directory_view_did_become_empty(
    pas_bitfit_directory* directory,
    pas_bitfit_view* view)
{
    PAS_TESTING_ASSERT(
        pas_compact_bitfit_directory_ptr_load_non_null(&view->directory)
        == directory);
    pas_bitfit_directory_view_did_become_empty_at_index(
        directory, view->index);
}

pas_page_sharing_pool_take_result pas_bitfit_directory_take_last_empty(
    pas_bitfit_directory* directory,
    pas_deferred_decommit_log* decommit_log,
    pas_lock_hold_mode heap_lock_hold_mode)
{
    static const bool verbose = false;
    
    pas_versioned_field last_empty_plus_one_value;
    size_t index;
    const pas_bitfit_page_config* page_config;
    size_t num_granules;

    last_empty_plus_one_value = pas_versioned_field_read(&directory->last_empty_plus_one);

    page_config = pas_bitfit_page_config_kind_get_config(directory->config_kind);
    num_granules = page_config->base.page_size / page_config->base.granule_size;

    for (index = last_empty_plus_one_value.value; index--;) {
        pas_bitfit_view* view;
        pas_bitfit_page* page;
        uintptr_t base;
        pas_lock* commit_lock;
        bool page_is_dead;
        pas_bitfit_max_free max_free;
        
        if (!pas_bitfit_directory_set_empty_bit_at_index(directory, index, false))
            continue;

        view = pas_bitfit_directory_get_view(directory, index);

        PAS_ASSERT(view);
        PAS_ASSERT(view->page_boundary); /* You don't become empty until you have a page and then
                                            you never lose the page. */
        PAS_ASSERT(pas_compact_bitfit_directory_ptr_load_non_null(&view->directory) == directory);
        PAS_ASSERT(view->index == index);

        commit_lock = &view->commit_lock;

        if (verbose)
            pas_log("Grabbing commit lock %p\n", commit_lock);
        if (!pas_deferred_decommit_log_lock_for_adding(
                decommit_log, commit_lock, heap_lock_hold_mode)) {
            pas_bitfit_directory_view_did_become_empty_at_index(directory, index);
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

        max_free = pas_bitfit_directory_get_max_free(directory, index);
        if (page_is_dead) {
            /* If a page's num_live_bits drops to zero then we will mark it empty in the directory that
               owns it while holding the lock. Newly created pages start out empty.  */
            if (max_free != PAS_BITFIT_MAX_FREE_EMPTY) {
                pas_log("%p:%zu: found non-empty page that is dead when taking last empty.\n", page, index);
                PAS_ASSERT(max_free == PAS_BITFIT_MAX_FREE_EMPTY);
            }
        } else {
            /* If a page's num_live_bits goes above zero then we mark it unprocessed while holding the
               lock. */
            if (max_free == PAS_BITFIT_MAX_FREE_EMPTY) {
                pas_log("%p:%zu: found empty page that is not dead when taking last empty.\n", page, index);
                PAS_ASSERT(max_free != PAS_BITFIT_MAX_FREE_EMPTY);
            }
        }
        
        if (num_granules > 1) {
            pas_free_granules free_granules;
            pas_page_granule_use_count* use_counts;
            
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

            pas_deferred_decommit_log_add_already_locked(
                decommit_log,
                pas_virtual_range_create(base, base + page_config->base.page_size, commit_lock,
                                         page_config->base.heap_config_ptr->mmap_capability),
                heap_lock_hold_mode);
        }
        
        pas_versioned_field_try_write(&directory->last_empty_plus_one,
                                      last_empty_plus_one_value,
                                      index);
        
        if (page_is_dead)
            page_config->base.destroy_page_header(&page->base, heap_lock_hold_mode);

        PAS_ASSERT(pas_bitfit_directory_does_sharing(directory));
        
        pas_page_sharing_pool_did_create_delta(
            &pas_physical_page_sharing_pool,
            pas_page_sharing_participant_create(
                directory, pas_page_sharing_participant_bitfit_directory));
        
        return pas_page_sharing_pool_take_success;
    }

    pas_versioned_field_try_write(&directory->last_empty_plus_one,
                                  last_empty_plus_one_value,
                                  0);
    
    return pas_page_sharing_pool_take_none_available;
}

void pas_bitfit_directory_dump_reference(
    pas_bitfit_directory* directory, pas_stream* stream)
{
    pas_stream_printf(
        stream, "%p(bitfit_directory, %s)",
        directory, pas_bitfit_page_config_kind_get_string(directory->config_kind));
}

void pas_bitfit_directory_dump_for_spectrum(pas_stream* stream, void* directory)
{
    pas_bitfit_directory_dump_reference(directory, stream);
}

#endif /* LIBPAS_ENABLED */
