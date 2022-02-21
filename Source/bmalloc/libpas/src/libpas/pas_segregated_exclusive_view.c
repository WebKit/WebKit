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

#include "pas_config.h"

#if LIBPAS_ENABLED

#include "pas_segregated_exclusive_view.h"

#include "pas_epoch.h"
#include "pas_immortal_heap.h"
#include "pas_log.h"
#include "pas_page_malloc.h"
#include "pas_page_sharing_pool.h"
#include "pas_segregated_heap.h"
#include "pas_segregated_page_inlines.h"
#include "pas_segregated_size_directory.h"

size_t pas_segregated_exclusive_view_count;

pas_segregated_exclusive_view* pas_segregated_exclusive_view_create(
    pas_segregated_size_directory* directory,
    size_t index)
{
    static const bool verbose = false;
    
    pas_segregated_exclusive_view* result;

    if (verbose)
        pas_log("Creating exclusive view.\n");

    result = pas_immortal_heap_allocate(
        sizeof(pas_segregated_exclusive_view),
        "pas_segregated_exclusive_view",
        pas_object_allocation);

    pas_segregated_exclusive_view_count++;

    /* We allocate this lazily. */
    result->page_boundary = NULL;
    pas_compact_segregated_size_directory_ptr_store(&result->directory, directory);
    result->index = (unsigned)index;
    PAS_ASSERT(result->index == index);
    result->is_owned = false;
    pas_lock_construct(&result->commit_lock);
    pas_lock_construct(&result->ownership_lock);

    return result;
}

void pas_segregated_exclusive_view_note_emptiness(
    pas_segregated_exclusive_view* view,
    pas_segregated_page* page)
{
    pas_segregated_directory* directory;
    unsigned view_index;
    
    if (page->is_in_use_for_allocation) {
        /* We currently don't want to notify emptiness until after we're done allocating in the
           page. However, this is a relatively weak requirement. */
        return;
    }

    directory = &pas_compact_segregated_size_directory_ptr_load_non_null(&view->directory)->base;
    view_index = view->index;
    
    pas_segregated_directory_view_did_become_empty_at_index(directory, view_index);
}

static pas_heap_summary compute_summary_impl(pas_segregated_exclusive_view* view)
{
    pas_segregated_size_directory* size_directory;
    pas_segregated_directory* directory;
    pas_segregated_page_config* page_config_ptr;
    pas_segregated_page_config page_config;
    pas_heap_summary result;
    void* boundary;
    pas_segregated_page* page;
    size_t object_size;
    size_t offset;
    size_t begin;
    size_t end;
    pas_segregated_size_directory_data* data;

    size_directory = pas_compact_segregated_size_directory_ptr_load_non_null(&view->directory);

    if (!view->is_owned) {
        return pas_segregated_size_directory_compute_summary_for_unowned_exclusive(
            size_directory);
    }

    directory = &size_directory->base;
    page_config_ptr = pas_segregated_page_config_kind_get_config(directory->page_config_kind);
    page_config = *page_config_ptr;
    
    result = pas_heap_summary_create_empty();

    data = pas_segregated_size_directory_data_ptr_load(&size_directory->data);
    boundary = view->page_boundary;
    page = pas_segregated_page_for_boundary(boundary, page_config);

    pas_page_base_compute_committed_when_owned(&page->base, &result);

    object_size = size_directory->object_size;

    begin = data->offset_from_page_boundary_to_first_object;
    end = data->offset_from_page_boundary_to_end_of_last_object;

    pas_page_base_add_free_range(
        &page->base, &result, pas_range_create(0, begin), pas_free_meta_range);
    pas_page_base_add_free_range(
        &page->base, &result, pas_range_create(end, page_config.base.page_size),
        pas_free_meta_range);
    
    for (offset = begin; offset < end; offset += object_size) {
        if (pas_bitvector_get(
                page->alloc_bits,
                pas_page_base_index_of_object_at_offset_from_page_boundary(
                    offset, page_config.base)))
            result.allocated += object_size;
        else {
            pas_page_base_add_free_range(
                &page->base, &result,
                pas_range_create(offset, offset + object_size),
                pas_free_object_range);
        }
    }

    if (page->is_in_use_for_allocation)
        result.cached += page_config.base.page_size;
    
    return result;
}

pas_heap_summary pas_segregated_exclusive_view_compute_summary(
    pas_segregated_exclusive_view* view)
{
    pas_heap_summary result;
    
    pas_lock_lock(&view->ownership_lock);
    result = compute_summary_impl(view);
    pas_lock_unlock(&view->ownership_lock);

    return result;
}

void pas_segregated_exclusive_view_install_full_use_counts(
    pas_segregated_exclusive_view* view)
{
    void* boundary;
    pas_segregated_page* page;
    pas_segregated_size_directory* directory;
    pas_segregated_page_config page_config;
    pas_page_granule_use_count* use_counts;
    pas_page_granule_use_count* full_use_counts;
    size_t num_granules;

    directory = pas_compact_segregated_size_directory_ptr_load_non_null(&view->directory);
    page_config = *pas_segregated_page_config_kind_get_config(directory->base.page_config_kind);
    boundary = view->page_boundary;
    page = pas_segregated_page_for_boundary(boundary, page_config);

    PAS_ASSERT(page_config.base.page_size > page_config.base.granule_size);

    use_counts = pas_segregated_page_get_granule_use_counts(page, page_config);
    num_granules = (page_config.base.page_size / page_config.base.granule_size);

    full_use_counts = pas_compact_tagged_page_granule_use_count_ptr_load_non_null(
        &pas_segregated_size_directory_get_extended_data(directory)->full_use_counts);
    
    memcpy(use_counts,
           full_use_counts,
           num_granules * sizeof(pas_page_granule_use_count));
}

bool pas_segregated_exclusive_view_is_eligible(pas_segregated_exclusive_view* view)
{
    return PAS_SEGREGATED_DIRECTORY_GET_BIT(
        &pas_compact_segregated_size_directory_ptr_load_non_null(&view->directory)->base,
        view->index, eligible);
}

bool pas_segregated_exclusive_view_is_empty(pas_segregated_exclusive_view* view)
{
    return PAS_SEGREGATED_DIRECTORY_GET_BIT(
        &pas_compact_segregated_size_directory_ptr_load_non_null(&view->directory)->base,
        view->index, empty);
}

#endif /* LIBPAS_ENABLED */
