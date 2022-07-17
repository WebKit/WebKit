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

#include "pas_bitfit_allocator.h"

#include "pas_bitfit_allocator_inlines.h"
#include "pas_bitfit_directory.h"
#include "pas_bitfit_size_class.h"
#include "pas_bitfit_view_inlines.h"
#include "pas_epoch.h"
#include "pas_physical_memory_transaction.h"

void pas_bitfit_allocator_construct(pas_bitfit_allocator* allocator,
                                    pas_bitfit_size_class* size_class)
{
    allocator->size_class = size_class;
    allocator->view = NULL;
}

void pas_bitfit_allocator_stop(pas_bitfit_allocator* allocator)
{
    allocator->view = NULL;
}

bool pas_bitfit_allocator_commit_view(pas_bitfit_view* view,
                                      const pas_bitfit_page_config* config,
                                      pas_lock_hold_mode commit_lock_hold_mode)
{
    static const bool verbose = false;
    
    pas_bitfit_directory* directory;
    pas_segregated_heap* heap;

    directory = pas_compact_bitfit_directory_ptr_load(&view->directory);
    heap = directory->heap;

    /* We're almost certainly gonna commit a page, so let's just get this out of the way. We need to
       release the ownership lock to do this. But, we can't do it at all if we already hold the commit
       lock -- and if we do, then we've already executed some kind of take. */
    if (!commit_lock_hold_mode) {
        pas_lock_unlock(&view->ownership_lock);
        pas_physical_page_sharing_pool_take_for_page_config(
            config->base.page_size, &config->base, pas_lock_is_not_held, NULL, 0);
        pas_lock_lock(&view->ownership_lock);
    }

    if (verbose)
        pas_log("committing bitfit view!\n");

    for (;;) {
        pas_lock* commit_lock;
        bool has_page_boundary;
        
        if (PAS_LIKELY(view->is_owned)) {
            if (verbose)
                pas_log("already owned.\n");
            
            PAS_ASSERT(view->page_boundary);
            return true;
        }

        /* Checking the presence of page boundary while holding the ownership lock ensures that if
           there are two concurrent commits on a page that is not yet allocated, then one of them
           will only proceed to the commit after the other has already finished. This avoids
           recommit and reconstruction of a page that is already constructed. */
        has_page_boundary = !!view->page_boundary;

        pas_lock_unlock(&view->ownership_lock);

        if (!has_page_boundary) {
            pas_physical_memory_transaction transaction;
            bool did_succeed;
            bool did_already_have_boundary;
            
            if (verbose)
                pas_log("need to allocate new page.\n");

            pas_physical_memory_transaction_construct(&transaction);

            did_succeed = false;
            did_already_have_boundary = false;
            
            for (;;) {
                PAS_ASSERT(!did_succeed);
                PAS_ASSERT(!did_already_have_boundary);
                
                pas_physical_memory_transaction_begin(&transaction);
                pas_heap_lock_lock();
                pas_lock_lock(&view->ownership_lock);
                if (view->page_boundary) {
                    bool end_result;
                    pas_heap_lock_unlock();
                    end_result = pas_physical_memory_transaction_end(&transaction);
                    PAS_ASSERT(end_result);
                    did_already_have_boundary = true;
                    break;
                }
                
                PAS_ASSERT(!view->is_owned);
                
                if (verbose) {
                    pas_log("really allocating new page.\n");
                    
                    pas_log("Giving view %p with config %s a page.\n",
                            view, pas_bitfit_page_config_kind_get_string(config->kind));
                }
                
                view->page_boundary = config->page_allocator(heap, &transaction);
                did_succeed = !!view->page_boundary;
                
                if (verbose)
                    pas_log("page_boundary = %p, did_succeed = %d\n", view->page_boundary, did_succeed);
                
                if (did_succeed) {
                    config->base.create_page_header(
                        view->page_boundary,
                        pas_page_kind_for_bitfit_variant(config->variant),
                        pas_lock_is_held);
                }
                
                pas_heap_lock_unlock();
                if (pas_physical_memory_transaction_end(&transaction))
                    break;

                PAS_ASSERT(!did_succeed);
                pas_lock_unlock(&view->ownership_lock);
            }

            if (did_already_have_boundary)
                continue;

            if (did_succeed) {
                view->is_owned = true;
                pas_bitfit_page_construct(
                    pas_bitfit_page_for_boundary_unchecked(view->page_boundary, *config),
                    view, config);
            }
            
            return did_succeed;
        }

        commit_lock = &view->commit_lock;

        pas_lock_lock_conditionally(commit_lock, commit_lock_hold_mode);

        if (view->is_owned) {
            pas_lock_unlock_conditionally(commit_lock, commit_lock_hold_mode);
            pas_lock_lock(&view->ownership_lock);
            continue;
        }
        
        if (verbose)
            pas_log("need to commit existing page.\n");
            
        /* If the view is not owned and we hold the commit lock then there is no way for it to
           become owned because anyone who notices that bit being zero will release the ownership
           lock and grab the commit lock.
        
           Also, there's no virtual page stealing. So, if a view ever has a page then it will still
           have it. */
        pas_compiler_fence();
        PAS_ASSERT(view->page_boundary);
        PAS_ASSERT(!view->is_owned);

        pas_page_malloc_commit(
            view->page_boundary, config->base.page_size, config->base.heap_config_ptr->mmap_capability);
        config->base.create_page_header(
            view->page_boundary,
            pas_page_kind_for_bitfit_variant(config->variant),
            pas_lock_is_not_held);

        pas_lock_lock(&view->ownership_lock);

        PAS_ASSERT(!view->is_owned);

        /* We shoud still think that the page is there but not owned! See comment above. */
        view->is_owned = true;
        pas_bitfit_page_construct(
            pas_bitfit_page_for_boundary_unchecked(view->page_boundary, *config), view, config);

        pas_lock_unlock_conditionally(commit_lock, commit_lock_hold_mode);

        if (PAS_DEBUG_SPECTRUM_USE_FOR_COMMIT) {
            pas_heap_lock_lock();
            pas_debug_spectrum_add(
                directory, pas_bitfit_directory_dump_for_spectrum, config->base.page_size);
            pas_heap_lock_unlock();
        }
        
        return true;
    }
}

pas_bitfit_view* pas_bitfit_allocator_finish_failing(pas_bitfit_allocator* allocator,
                                                     pas_bitfit_view* view,
                                                     size_t size,
                                                     size_t alignment,
                                                     size_t largest_available,
                                                     const pas_bitfit_page_config* config)
{
    static const bool verbose = false;
    
    pas_bitfit_directory* directory;
    pas_bitfit_size_class* size_class;
    pas_bitfit_view* new_view;
    unsigned view_index;
    pas_bitfit_view* allocator_view;

    PAS_UNUSED_PARAM(alignment);

    pas_lock_assert_held(&view->ownership_lock);

    size_class = allocator->size_class;

    directory = pas_compact_bitfit_directory_ptr_load_non_null(&size_class->directory);

    PAS_ASSERT(pas_compact_bitfit_directory_ptr_load_non_null(&view->directory) == directory);

    view_index = view->index;

    if (verbose) {
        pas_log("Finishing failing in view %p, size = %zu, alignment = %zu, largest_available = %zu\n",
                view, size, alignment, largest_available);
    }

    /* If we're still on the view that the allocator was on and we found that this view no longer
       has enough max_free for our size class, then tell everyone about this and bail.
    
       NOTE: There are some aspects of this that we could do even if the view was displaced. Like,
       we could update the max_free. */
    allocator_view = allocator->view;
    if ((view == allocator_view || !allocator_view) && largest_available < size_class->size) {
        unsigned index;
        pas_bitfit_size_class* current_size_class;
        pas_versioned_field first_free_value;

        allocator->view = NULL;
        
        PAS_ASSERT(view->page_boundary);
        pas_bitfit_page_for_boundary(view->page_boundary, *config)->did_note_max_free = false;
        
        index = view_index;
        
        pas_bitfit_directory_set_processed_max_free(
            directory, index, largest_available >> config->base.min_align_shift,
            "processing on finish_failing");

        /* If we're doing an aligned allocation, then we might now skip over this view even though the
           size we were allocating would have fit. The reason why we're doing it is that the largest size
           we could have fit is smaller than the size class, and although it's big enough for the size being
           requested, it's not aligned properly. */
        PAS_TESTING_ASSERT(largest_available < size
                           || alignment > pas_page_base_config_min_align(config->base));
        
        PAS_TESTING_ASSERT(largest_available < size_class->size);
        
        current_size_class = size_class;
        do {
            /* The induction hypothesis is that current_size_class was too big for
               largest_available. */
            
            current_size_class = pas_compact_atomic_bitfit_size_class_ptr_load(
                &current_size_class->next_smaller);
        } while (current_size_class && largest_available < current_size_class->size);

        if (current_size_class) {
            PAS_TESTING_ASSERT(largest_available >= current_size_class->size);
            
            do {
                first_free_value = pas_versioned_field_read(&current_size_class->first_free);

                if (first_free_value.value > index) {
                    if (!pas_versioned_field_try_write(&current_size_class->first_free,
                                                       first_free_value,
                                                       index))
                        continue;
                }
                
                current_size_class = pas_compact_atomic_bitfit_size_class_ptr_load(
                    &current_size_class->next_smaller);
            } while (current_size_class);
        }

        pas_lock_unlock(&view->ownership_lock);
        return NULL;
    }

    pas_lock_unlock(&view->ownership_lock);
    
    /* Try to continue searching for memory in the size class, but without altering its cursor.
       This is an unusual degenerate case that happens only for memalign. */
    PAS_ASSERT((unsigned)size == size);
    new_view = pas_bitfit_directory_get_first_free_view(
        directory, view_index + 1, (unsigned)size, config).view;
    PAS_ASSERT(new_view);
    return new_view;
}

#endif /* LIBPAS_ENABLED */
