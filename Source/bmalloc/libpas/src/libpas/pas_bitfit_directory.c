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
#include "pas_bitfit_global_size_class.h"
#include "pas_bitfit_view_inlines.h"
#include "pas_epoch.h"
#include "pas_heap_lock.h"
#include "pas_page_sharing_pool.h"

void pas_bitfit_directory_construct(pas_bitfit_directory* directory,
                                    pas_bitfit_directory_kind directory_kind,
                                    pas_bitfit_page_config* config)
{
    static const bool verbose = false;

    if (verbose) {
        pas_log("Creating directory %p (%s)\n",
                directory, pas_bitfit_directory_kind_get_string(directory_kind));
    }
    
    pas_versioned_field_construct(&directory->first_unprocessed_free, 0);
    pas_versioned_field_construct(&directory->first_empty, 0);
    pas_bitfit_directory_max_free_vector_construct(&directory->max_frees);
    pas_bitfit_directory_view_vector_construct(&directory->views);
    pas_compact_atomic_bitfit_size_class_ptr_store(&directory->largest_size_class, NULL);
    directory->directory_kind = directory_kind;
    directory->config_kind = config->kind;
}

void pas_bitfit_directory_update_biasing_eligibility(pas_bitfit_directory* directory,
                                                     size_t index)
{
    pas_bitfit_biasing_directory* biasing_directory;
    
    if (directory->directory_kind != pas_bitfit_biasing_directory_kind)
        return;

    biasing_directory = (pas_bitfit_biasing_directory*)directory;

    pas_biasing_directory_index_did_become_eligible(&biasing_directory->biasing_base, index);
}

void pas_bitfit_directory_max_free_did_become_unprocessed(pas_bitfit_directory* directory,
                                                          size_t index,
                                                          const char* reason)
{
    pas_bitfit_directory_set_unprocessed_max_free(directory, index, reason);
    pas_versioned_field_minimize(&directory->first_unprocessed_free, index);
    pas_bitfit_directory_update_biasing_eligibility(directory, index);
}

void pas_bitfit_directory_max_free_did_become_unprocessed_unchecked(pas_bitfit_directory* directory,
                                                                    size_t index,
                                                                    const char* reason)
{
    pas_bitfit_directory_set_unprocessed_max_free_unchecked(directory, index, reason);
    pas_versioned_field_minimize(&directory->first_unprocessed_free, index);
    pas_bitfit_directory_update_biasing_eligibility(directory, index);
}

void pas_bitfit_directory_max_free_did_become_empty_without_biasing_update(
    pas_bitfit_directory* directory,
    size_t index,
    const char* reason)
{
    pas_bitfit_directory_set_empty_max_free(directory, index, reason);
    pas_versioned_field_minimize(&directory->first_empty, index);
}

void pas_bitfit_directory_max_free_did_become_empty(pas_bitfit_directory* directory,
                                                    size_t index,
                                                    const char* reason)
{
    pas_bitfit_directory_max_free_did_become_empty_without_biasing_update(directory, index, reason);
    pas_bitfit_directory_update_biasing_eligibility(directory, index);
}

pas_bitfit_view_and_index
pas_bitfit_directory_get_first_free_view(pas_bitfit_directory* directory,
                                         pas_bitfit_global_size_class* size_class,
                                         unsigned start_index,
                                         unsigned size,
                                         pas_bitfit_page_config* page_config)
{
    static const bool verbose = false;
    
    for (;;) {
        pas_found_index found_index;
        size_t max_frees_size;
        pas_bitfit_view* view;

        if (directory->directory_kind == pas_bitfit_global_directory_kind) {
            pas_bitfit_global_directory* global_directory;
            pas_page_sharing_pool* bias_pool;
            
            global_directory = (pas_bitfit_global_directory*)directory;
            
            bias_pool = pas_compact_atomic_page_sharing_pool_ptr_load(
                &global_directory->bias_sharing_pool);

            if (bias_pool)
                pas_bias_page_sharing_pool_take(bias_pool);
        }

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

                if (directory->directory_kind == pas_bitfit_global_directory_kind) {
                    pas_bitfit_global_directory* global_directory;
                
                    global_directory = (pas_bitfit_global_directory*)directory;
                
                    if (PAS_BITVECTOR_NUM_WORDS(directory->views.size)
                        != global_directory->bitvectors.size) {
                        PAS_ASSERT(PAS_BITVECTOR_NUM_WORDS(directory->views.size)
                                   == global_directory->bitvectors.size + 1);
                        pas_bitfit_global_directory_segmented_bitvectors_append(
                            &global_directory->bitvectors,
                            PAS_BITFIT_GLOBAL_DIRECTORY_BITVECTOR_SEGMENT_INITIALIZER,
                            pas_lock_is_held);
                        PAS_ASSERT(PAS_BITVECTOR_NUM_WORDS(directory->views.size)
                                   == global_directory->bitvectors.size);
                    }
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

        switch (directory->directory_kind) {
        case pas_bitfit_global_directory_kind: {
            pas_bitfit_global_directory* global_directory;
            
            global_directory = (pas_bitfit_global_directory*)directory;
            
            if (!view) {
                pas_heap_lock_lock();
                view = pas_bitfit_directory_get_view(directory, found_index.index);
                if (!view) {
                    PAS_ASSERT((unsigned)found_index.index == found_index.index);
                    view = pas_bitfit_view_create(global_directory,
                                                  (unsigned)found_index.index);
                    pas_store_store_fence();
                    pas_compact_atomic_bitfit_view_ptr_store(
                        pas_bitfit_directory_get_view_ptr(directory, found_index.index), view);
                }
                pas_heap_lock_unlock();
            }
            
            PAS_TESTING_ASSERT(
                pas_compact_bitfit_global_directory_ptr_load_non_null(&view->global_directory)
                == global_directory);
            PAS_TESTING_ASSERT(view->index_in_global == found_index.index);

            /* Note: it's possible for this to now have become a biased view. It should not happen
               often. If it is now a biased view, then it must have earlier had its max_free
               zeroed. */
            break;
        }

        case pas_bitfit_biasing_directory_kind: {
            pas_bitfit_biasing_directory* biasing_directory;
            pas_bitfit_global_directory* global_directory;

            biasing_directory = (pas_bitfit_biasing_directory*)directory;
            
            if (view) {
                /* If there was any view at all then it must have been biased to this directory at
                   some point or another. Of course it could be unbiased at any moment, but it could
                   even be unbiased right after we verify that it is ours. */
                pas_biasing_directory_did_use_index(&biasing_directory->biasing_base,
                                                    found_index.index,
                                                    pas_get_epoch());
                break;
            }

            global_directory =
                pas_compact_bitfit_global_directory_ptr_load_non_null(
                    &biasing_directory->parent_global);
            PAS_TESTING_ASSERT(
                &global_directory->base
                == pas_compact_bitfit_directory_ptr_load_non_null(&size_class->base.directory));
            
            for (;;) {
                pas_bitfit_view* previous_view;
                
                view = pas_bitfit_size_class_get_first_free_view(
                    &size_class->base, size_class, page_config);
            
                PAS_TESTING_ASSERT(
                    pas_compact_bitfit_global_directory_ptr_load_non_null(&view->global_directory)
                    == global_directory);

                pas_lock_lock(&view->ownership_lock);
                /* We should not bias a view that is already biased. The global directory would not have
                   given us a view that was already biased if it knew that it was biased at the time of
                   the search. */
                if (!pas_compact_atomic_bitfit_biasing_directory_ptr_is_null(&view->biasing_directory)) {
                    pas_lock_unlock(&view->ownership_lock);
                    /* Search the global directory again. */
                    if (verbose) {
                        pas_log("%p:%u: found already biased view = %p.\n",
                                directory, (unsigned)found_index.index, view);
                    }
                    continue;
                }

                previous_view = pas_compact_atomic_bitfit_view_ptr_strong_cas(
                    pas_bitfit_directory_get_view_ptr(directory, found_index.index),
                    NULL, view);
                if (previous_view) {
                    /* This is an unlikely race condition since biasing directories are usually owned
                       by a thread. Note that the previous_view cannot be our view, since we had just
                       detected that our view was not biased, and we still hold its lock. */
                    PAS_ASSERT(previous_view != view);
                    pas_lock_unlock(&view->ownership_lock);
                    view = previous_view;
                    break;
                }

                if (verbose)
                    pas_log("Biasing view %p to %p:%u\n", view, directory, (unsigned)found_index.index);

                pas_compact_atomic_bitfit_biasing_directory_ptr_store(
                    &view->biasing_directory, biasing_directory);
                PAS_ASSERT((unsigned)found_index.index == found_index.index);
                view->index_in_biasing = (unsigned)found_index.index;

                /* We avoid the checking here because this might have been empty, and that's OK. */
                pas_bitfit_directory_set_max_free_unchecked(
                    &global_directory->base, view->index_in_global, 0, "clearing global on bias");
            
                if (pas_bitfit_view_is_empty(view, *page_config)) {
                    /* This helps preserve consistency but it's not super interesting; as soon as we
                       allocate we will mark this unprocessed. */
                    pas_bitfit_directory_max_free_did_become_empty(
                        directory, found_index.index, "initialize to empty on bias");
                } else {
                    pas_bitfit_directory_max_free_did_become_unprocessed_unchecked(
                        directory, found_index.index, "initialize to unprocessed on bias");
                }
                pas_lock_unlock(&view->ownership_lock);
                break;
            }

            pas_biasing_directory_did_use_index(
                &biasing_directory->biasing_base, found_index.index, pas_get_epoch());

            break;
        } }

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

#endif /* LIBPAS_ENABLED */
