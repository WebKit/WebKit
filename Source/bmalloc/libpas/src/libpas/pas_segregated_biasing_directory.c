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

#include "pas_segregated_biasing_directory.h"

#include "pas_all_biasing_directories.h"
#include "pas_epoch.h"
#include "pas_page_sharing_pool.h"
#include "pas_segregated_biasing_view.h"
#include "pas_segregated_exclusive_view.h"
#include "pas_segregated_global_size_directory.h"
#include "pas_segregated_page_inlines.h"
#include "pas_segregated_size_directory_inlines.h"

pas_segregated_biasing_directory* pas_segregated_biasing_directory_create(
    pas_segregated_global_size_directory* parent_global, unsigned magazine_index)
{
    pas_segregated_biasing_directory* result;
    pas_segregated_page_config* page_config;

    /* This creates a biasing directory with just index_in_all initialized and everything else
       zeroed. */
    result = pas_immortal_heap_allocate_with_alignment(
        sizeof(pas_segregated_biasing_directory),
        alignof(pas_segregated_biasing_directory),
        "pas_segregated_biasing_directory",
        pas_object_allocation);

    page_config = pas_segregated_page_config_kind_get_config(parent_global->base.page_config_kind);
    PAS_ASSERT(page_config);

    pas_segregated_size_directory_construct(
        &result->base, page_config->kind,
        pas_do_not_share_pages,
        pas_segregated_biasing_directory_kind);

    pas_compact_segregated_global_size_directory_ptr_store(&result->parent_global, parent_global);

    pas_biasing_directory_construct(
        &result->biasing_base, pas_biasing_directory_segregated_kind, magazine_index);

    return result;
}

static void take_first_eligible_loop_head_callback(pas_segregated_directory* directory)
{
    /* Nothing to do for biasing directories. This callback is meaningful for global size directories,
       which use it to take from the biasing pool. */
    PAS_UNUSED_PARAM(directory);
}

static pas_segregated_view take_first_eligible_create_new_view_callback(
    pas_segregated_directory_iterate_config* config)
{
    pas_segregated_biasing_directory* biasing_directory;

    PAS_TESTING_ASSERT(config->directory->directory_kind == pas_segregated_biasing_directory_kind);
    
    biasing_directory = (pas_segregated_biasing_directory*)config->directory;

    return pas_segregated_biasing_view_as_view_non_null(
        pas_segregated_biasing_view_create(biasing_directory, config->index));
}

pas_segregated_view pas_segregated_biasing_directory_take_first_eligible(
    pas_segregated_biasing_directory* biasing_directory,
    pas_segregated_global_size_directory* parent_global)
{
    pas_segregated_directory* directory;
    pas_segregated_page_config* page_config_ptr;
    pas_segregated_page_config page_config;
    pas_segregated_view view;
    pas_segregated_directory_iterate_config config;

    PAS_TESTING_ASSERT(
        parent_global ==
        pas_compact_segregated_global_size_directory_ptr_load_non_null(
            &biasing_directory->parent_global));

    directory = &biasing_directory->base;
    page_config_ptr = pas_segregated_page_config_kind_get_config(directory->page_config_kind);
    page_config = *page_config_ptr;

    PAS_TESTING_ASSERT(
        pas_segregated_page_config_heap_lock_hold_mode(page_config) == pas_lock_is_not_held);
    
    view = pas_segregated_size_directory_take_first_eligible_impl(
        directory, &config,
        take_first_eligible_loop_head_callback,
        take_first_eligible_create_new_view_callback);

    pas_biasing_directory_did_use_index(
        &biasing_directory->biasing_base, config.index, pas_get_epoch());
    
    PAS_TESTING_ASSERT(!PAS_SEGREGATED_DIRECTORY_GET_BIT(
                           directory, pas_segregated_view_get_index(view), eligible));

    return view;
}

pas_page_sharing_pool_take_result
pas_segregated_biasing_directory_take_last_unused(
    pas_segregated_biasing_directory* biasing_directory)
{
    static const bool verbose = false;
    
    pas_segregated_global_size_directory* size_directory;
    unsigned ownership_threshold;
    bool did_find_eligible_page;
    size_t index;
    pas_versioned_field eligible_high_watermark;
    size_t new_eligible_watermark;
    pas_segregated_page_config page_config;

    page_config = *pas_segregated_page_config_kind_get_config(biasing_directory->base.page_config_kind);

    PAS_ASSERT(pas_segregated_page_config_heap_lock_hold_mode(page_config) == pas_lock_is_not_held);
    
    size_directory = pas_compact_segregated_global_size_directory_ptr_load_non_null(
        &biasing_directory->parent_global);

    ownership_threshold = pas_biasing_directory_ownership_threshold(&biasing_directory->biasing_base);

    /* Read to watch so that if anyone updates the eligible_high_watermark during our scan, we won't
       overwrite it. */
    eligible_high_watermark = pas_versioned_field_read_to_watch(
        &biasing_directory->biasing_base.eligible_high_watermark);

    PAS_ASSERT(eligible_high_watermark.value
               <= pas_segregated_directory_size(&biasing_directory->base));

    if ((size_t)ownership_threshold >= eligible_high_watermark.value)
        return pas_page_sharing_pool_take_none_available;

    did_find_eligible_page = false;

    new_eligible_watermark = ownership_threshold;

    for (index = ownership_threshold; index < eligible_high_watermark.value; ++index) {
        pas_segregated_biasing_view* biasing_view;
        pas_segregated_exclusive_view* exclusive_view;
        pas_lock* held_lock;
        bool result;
        bool should_notify_emptiness;
        
        if (!PAS_SEGREGATED_DIRECTORY_SET_BIT(&biasing_directory->base, index, eligible, false)) {
            /* If it's ineligible then we just don't do anything to it and leave it be. If it becomes
               eligible then that'll trigger this code to rerun. */
            continue;
        }

        did_find_eligible_page = true;

        biasing_view = pas_segregated_view_get_biasing(
            pas_segregated_directory_get(&biasing_directory->base, index));
        exclusive_view = pas_compact_atomic_segregated_exclusive_view_ptr_load(
            &biasing_view->exclusive);

        if (!exclusive_view) {
            result = pas_segregated_directory_view_did_become_eligible_at_index_without_biasing_update(
                &biasing_directory->base, index);
            PAS_ASSERT(result);
            continue;
        }
        
        PAS_ASSERT(pas_segregated_exclusive_view_ownership_kind_is_biased(
                       exclusive_view->ownership_kind));
        PAS_ASSERT(!PAS_SEGREGATED_DIRECTORY_GET_BIT(
                       &size_directory->base, exclusive_view->index, eligible));
        
        /* The empty bits shouldn't have ever been set. */
        PAS_ASSERT(!PAS_SEGREGATED_DIRECTORY_GET_BIT(
                       &biasing_directory->base, index, empty));
        
        held_lock = NULL;
        pas_lock_switch(&held_lock, &exclusive_view->ownership_lock);
        PAS_ASSERT(pas_segregated_exclusive_view_ownership_kind_is_biased(
                       exclusive_view->ownership_kind));
        exclusive_view->ownership_kind =
            pas_segregated_exclusive_view_ownership_kind_with_biased(
                exclusive_view->ownership_kind, false);
        if (verbose) {
            pas_log("view %p has ownership kind = %s due to take_last_unused\n",
                    exclusive_view,
                    pas_segregated_exclusive_view_ownership_kind_get_string(
                        exclusive_view->ownership_kind));
        }

        should_notify_emptiness = false;
        
        if (pas_segregated_exclusive_view_ownership_kind_is_owned(
                exclusive_view->ownership_kind)) {
            pas_segregated_page* page;

            page = pas_segregated_page_for_boundary(exclusive_view->page_boundary, page_config);
            
            pas_segregated_page_switch_lock(page, &held_lock, page_config);
            
            /* If this exclusive already had a page, then the page must be saying that it's
               eligible. */
            PAS_ASSERT(pas_segregated_view_is_biasing(page->owner));
            
            page->owner = pas_segregated_exclusive_view_as_view_non_null(exclusive_view);

            should_notify_emptiness = pas_segregated_page_qualifies_for_decommit(page, page_config);
        }

        /* This just needs to happen before we release eligibility. */
        pas_compact_atomic_segregated_exclusive_view_ptr_store(&biasing_view->exclusive, NULL);

        result = pas_segregated_directory_view_did_become_eligible_at_index(
            &size_directory->base, exclusive_view->index);
        PAS_ASSERT(result);
        
        result = pas_segregated_directory_view_did_become_eligible_at_index_without_biasing_update(
            &biasing_directory->base, index);
        PAS_ASSERT(result);

        if (should_notify_emptiness) {
            pas_segregated_directory_view_did_become_empty_at_index(
                &size_directory->base, exclusive_view->index);
        }
        
        pas_lock_switch(&held_lock, NULL);
    }

    pas_versioned_field_try_write(
        &biasing_directory->biasing_base.eligible_high_watermark,
        eligible_high_watermark, new_eligible_watermark);

    return did_find_eligible_page
        ? pas_page_sharing_pool_take_success
        : pas_page_sharing_pool_take_none_available;
}

#endif /* LIBPAS_ENABLED */
