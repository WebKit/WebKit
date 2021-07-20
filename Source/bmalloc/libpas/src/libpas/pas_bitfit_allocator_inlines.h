/*
 * Copyright (c) 2020 Apple Inc. All rights reserved.
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

#ifndef PAS_BITFIT_ALLOCATOR_INLINES_H
#define PAS_BITFIT_ALLOCATOR_INLINES_H

#include "pas_bitfit_allocation_result.h"
#include "pas_bitfit_allocator.h"
#include "pas_bitfit_global_directory.h"
#include "pas_bitfit_global_size_class.h"
#include "pas_bitfit_page_inlines.h"
#include "pas_bitfit_view.h"
#include "pas_debug_spectrum.h"
#include "pas_epoch.h"
#include "pas_fast_path_allocation_result.h"
#include "pas_segregated_global_size_directory_inlines.h"

PAS_BEGIN_EXTERN_C;

static inline void pas_bitfit_allocator_reset(pas_bitfit_allocator* allocator)
{
    allocator->global_size_class = NULL;
    allocator->size_class = NULL;
    allocator->view = NULL;
}

static inline void pas_bitfit_allocator_assert_reset(pas_bitfit_allocator* allocator)
{
    PAS_TESTING_ASSERT(!allocator->global_size_class);
    PAS_TESTING_ASSERT(!allocator->size_class);
    PAS_TESTING_ASSERT(!allocator->view);
}

PAS_API bool pas_bitfit_allocator_commit_view(pas_bitfit_view* view,
                                              pas_local_allocator* local,
                                              pas_bitfit_page_config* config,
                                              pas_lock_hold_mode commit_lock_hold_mode);

PAS_API pas_bitfit_view*
pas_bitfit_allocator_finish_failing(pas_bitfit_allocator* allocator,
                                    pas_bitfit_view* view,
                                    size_t size,
                                    size_t alignment,
                                    size_t largest_available,
                                    pas_bitfit_page_config* config);

static PAS_ALWAYS_INLINE pas_fast_path_allocation_result
pas_bitfit_allocator_try_allocate(pas_bitfit_allocator* allocator,
                                  pas_local_allocator* local,
                                  size_t size,
                                  size_t alignment,
                                  pas_bitfit_page_config config)
{
    static const bool verbose = false;
    
    pas_bitfit_view* view;

    if (verbose)
        pas_log("bitfit allocating %zu bytes with %zu alignment\n", size, alignment);

    PAS_ASSERT(config.base.is_enabled);

    if (!size)
        size = pas_page_base_config_min_align(config.base);
    else
        size = pas_round_up_to_power_of_2(size, pas_page_base_config_min_align(config.base));
    
    view = allocator->view;
    
    /* We only loop around in case of alignment issues. */
    for (;;) {
        pas_bitfit_allocation_result bitfit_result;
        pas_bitfit_page* page;
        size_t bytes_committed;
        
        PAS_TESTING_ASSERT(view);

        bytes_committed = 0;
        bitfit_result = pas_bitfit_allocation_result_create_empty();

        for (;;) {
            bool need_to_lock_commit_lock;
            pas_lock_hold_mode commit_lock_hold_mode;

            need_to_lock_commit_lock = !!bitfit_result.pages_to_commit_on_reloop;
            commit_lock_hold_mode =
                need_to_lock_commit_lock ? pas_lock_is_held : pas_lock_is_not_held;

            /* Note - conditionally acquiring a lock is bad because the fencing story is weird.
               Except we are acquiring another lock right after, so it doesn't matter. */
            if (need_to_lock_commit_lock) {
                pas_physical_page_sharing_pool_take_for_page_config(
                    bitfit_result.pages_to_commit_on_reloop * config.base.granule_size,
                    config.base.page_config_ptr,
                    pas_lock_is_not_held, NULL, 0);
                pas_lock_lock(&view->commit_lock);
            }

            pas_bitfit_view_lock_ownership_lock(view);

            if (PAS_UNLIKELY(!view->is_owned)) {
                /* Note that this would have flashed the ownership lock possibly. */
                if (!pas_bitfit_allocator_commit_view(
                        view, local, (pas_bitfit_page_config*)config.base.page_config_ptr,
                        commit_lock_hold_mode)) {
                    if (verbose)
                        pas_log("bitfit is out of memory\n");
                    pas_lock_unlock(&view->ownership_lock);
                    if (need_to_lock_commit_lock)
                        pas_lock_unlock(&view->commit_lock);
                    return pas_fast_path_allocation_result_create_out_of_memory();
                }
            }

            PAS_TESTING_ASSERT(view->is_owned);
            
            page = pas_bitfit_page_for_boundary(view->page_boundary, config);

            if (verbose)
                pas_log("About to allocate in view %p, page %p.\n", view, page);
            
            bitfit_result = pas_bitfit_page_allocate(
                page, view, size, alignment, config, commit_lock_hold_mode, &bytes_committed);

            if (need_to_lock_commit_lock)
                pas_lock_unlock(&view->commit_lock);

            if (!bitfit_result.pages_to_commit_on_reloop)
                break;

            PAS_ASSERT(!bytes_committed);

            PAS_ASSERT(!need_to_lock_commit_lock);
            pas_lock_unlock(&view->ownership_lock);
        }

        PAS_ASSERT(!bitfit_result.pages_to_commit_on_reloop);

        if (!bitfit_result.did_succeed) {
            pas_bitfit_view* next_view;

            PAS_ASSERT(!bytes_committed);

            if (verbose)
                pas_log("bitfit page allocation did not succeed.\n");
            
            next_view = pas_bitfit_allocator_finish_failing(
                allocator, view, size, alignment, bitfit_result.u.largest_available,
                (pas_bitfit_page_config*)config.base.page_config_ptr);
            
            if (next_view) {
                PAS_TESTING_ASSERT(alignment > pas_page_base_config_min_align(config.base));

                view = next_view;
                continue;
            }
            
            return pas_fast_path_allocation_result_create_need_slow();
        }
        
        pas_lock_unlock(&view->ownership_lock);

        if (PAS_DEBUG_SPECTRUM_USE_FOR_COMMIT && bytes_committed) {
            pas_heap_lock_lock();
            pas_debug_spectrum_add(
                pas_compact_bitfit_directory_ptr_load(&allocator->global_size_class->base.directory),
                pas_bitfit_global_directory_dump_for_spectrum,
                bytes_committed);
            pas_heap_lock_unlock();
        }

        if (verbose)
            pas_log("bitfit allocation succeeded with %p\n", (void*)bitfit_result.u.result);
        
        return pas_fast_path_allocation_result_create_success(bitfit_result.u.result);
    }
}

PAS_END_EXTERN_C;

#endif /* PAS_BITFIT_ALLOCATOR_INLINES_H */

