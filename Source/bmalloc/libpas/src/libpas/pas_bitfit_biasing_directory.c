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

#include "pas_config.h"

#if LIBPAS_ENABLED

#include "pas_bitfit_biasing_directory.h"

#include "pas_bitfit_global_directory.h"
#include "pas_bitfit_page_config.h"
#include "pas_bitfit_view_inlines.h"
#include "pas_immortal_heap.h"

pas_bitfit_biasing_directory* pas_bitfit_biasing_directory_create(
    pas_bitfit_global_directory* parent_global,
    unsigned magazine_index)
{
    pas_bitfit_biasing_directory* result;
    pas_bitfit_page_config* config_ptr;

    config_ptr = pas_bitfit_page_config_kind_get_config(parent_global->base.config_kind);

    result = pas_immortal_heap_allocate_with_alignment(
        sizeof(pas_bitfit_biasing_directory),
        alignof(pas_bitfit_biasing_directory),
        "pas_bitfit_biasing_directory",
        pas_object_allocation);
    
    pas_compact_bitfit_global_directory_ptr_store(&result->parent_global, parent_global);
    pas_bitfit_directory_construct(&result->base, pas_bitfit_biasing_directory_kind, config_ptr);
    pas_biasing_directory_construct(
        &result->biasing_base, pas_biasing_directory_bitfit_kind, magazine_index);

    return result;
}

pas_page_sharing_pool_take_result pas_bitfit_biasing_directory_take_last_unused(
    pas_bitfit_biasing_directory* directory)
{
    static const bool verbose = false;
    
    pas_bitfit_global_directory* global_directory;
    unsigned ownership_threshold;
    bool did_find_eligible_page;
    size_t index;
    pas_versioned_field eligible_high_watermark;
    size_t new_eligible_watermark;
    pas_bitfit_page_config page_config;

    page_config = *pas_bitfit_page_config_kind_get_config(directory->base.config_kind);

    global_directory =
        pas_compact_bitfit_global_directory_ptr_load_non_null(&directory->parent_global);

    ownership_threshold = pas_biasing_directory_ownership_threshold(&directory->biasing_base);

    /* Read to watch so that if anyone updates the eligible_high_watermark during our scan, we won't
       overwrite it. */
    eligible_high_watermark = pas_versioned_field_read_to_watch(
        &directory->biasing_base.eligible_high_watermark);

    PAS_ASSERT(eligible_high_watermark.value <= pas_bitfit_directory_size(&directory->base));

    if ((size_t)ownership_threshold >= eligible_high_watermark.value)
        return pas_page_sharing_pool_take_none_available;

    did_find_eligible_page = false;

    new_eligible_watermark = ownership_threshold;

    for (index = ownership_threshold; index < eligible_high_watermark.value; ++index) {
        pas_compact_atomic_bitfit_view_ptr* view_ptr;
        pas_bitfit_view* view;

        view_ptr = pas_bitfit_directory_get_view_ptr(&directory->base, index);
        for (;;) {
            view = pas_compact_atomic_bitfit_view_ptr_load(view_ptr);
            if (!view)
                break;

            did_find_eligible_page = true;

            PAS_ASSERT(pas_compact_bitfit_global_directory_ptr_load_non_null(&view->global_directory)
                       == global_directory);

            pas_lock_lock(&view->ownership_lock);
            if (pas_compact_atomic_bitfit_biasing_directory_ptr_load(&view->biasing_directory)
                == directory) {
                if (verbose)
                    pas_log("Unbiasing view %p from %p:%zu\n", view, directory, index);

                /* It's possible that this particular view got unbiased and then rebiased at a different
                   index already. */
                if (view->index_in_biasing != index) {
                    pas_lock_unlock(&view->ownership_lock);
                    continue;
                }
                
                PAS_ASSERT(view->index_in_biasing == index);

                /* If the view believes that it is biased to this index, then the directory must also
                   agree with this. */
                PAS_ASSERT(pas_compact_atomic_bitfit_view_ptr_load(view_ptr) == view);
            
                /* We say that unbiased entries are empty, not unprocessed. */
                pas_bitfit_directory_max_free_did_become_empty_without_biasing_update(
                    &directory->base, index, "becoming empty on normal unbias");

                /* Need a fence here because as soon as we start clearing out the view from the biasing
                   directory, it's possible for someone else to start messing with this max_free entry
                   in the biasing directory. So the store above has to happen before any of this. */
                pas_compact_atomic_bitfit_biasing_directory_ptr_store(&view->biasing_directory, NULL);
                view->index_in_biasing = 0;
            
                /* Make sure that we don't clear the view out of the directory until we're done telling the
                   view that it's not biased. */
                pas_fence();
                pas_compact_atomic_bitfit_view_ptr_store(view_ptr, NULL);

                if (pas_bitfit_view_is_empty(view, page_config)) {
                    pas_bitfit_directory_max_free_did_become_empty(
                        &global_directory->base, view->index_in_global,
                        "initialize to empty in global on normal unbias");
                } else {
                    pas_bitfit_directory_max_free_did_become_unprocessed(
                        &global_directory->base, view->index_in_global,
                        "initialize to unprocessed in global on normal unbias");
                }
            }
            pas_lock_unlock(&view->ownership_lock);
            break;
        }
    }
    
    pas_versioned_field_try_write(
        &directory->biasing_base.eligible_high_watermark,
        eligible_high_watermark, new_eligible_watermark);

    return did_find_eligible_page
        ? pas_page_sharing_pool_take_success
        : pas_page_sharing_pool_take_none_available;
}

#endif /* LIBPAS_ENABLED */
