/*
 * Copyright (c) 2019-2022 Apple Inc. All rights reserved.
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

#include "pas_segregated_partial_view.h"

#include "pas_epoch.h"
#include "pas_immortal_heap.h"
#include "pas_segregated_page_inlines.h"
#include "pas_segregated_partial_view_inlines.h"
#include "pas_segregated_shared_page_directory.h"
#include "pas_segregated_shared_view_inlines.h"
#include "pas_shared_handle_or_page_boundary_inlines.h"

size_t pas_segregated_partial_view_count = 0;

pas_segregated_partial_view*
pas_segregated_partial_view_create(
    pas_segregated_size_directory* directory,
    size_t index)
{
    static const bool verbose = false;
    
    pas_segregated_partial_view* result;

    if (verbose)
        pas_log("Creating partial view.\n");

    result = pas_immortal_heap_allocate(
        sizeof(pas_segregated_partial_view),
        "pas_segregated_partial_view",
        pas_object_allocation);

    pas_segregated_partial_view_count++;

    /* We attach to a shared view lazily - once we know what we're allocating. */
    pas_compact_segregated_shared_view_ptr_store(&result->shared_view, NULL);

    pas_compact_segregated_size_directory_ptr_store(&result->directory, directory);

    PAS_ASSERT((uint8_t)index == index);
    result->index = (uint8_t)index;

    result->alloc_bits_offset = 0;
    result->alloc_bits_size = 0;
    pas_lenient_compact_unsigned_ptr_store(&result->alloc_bits, NULL);

    result->inline_alloc_bits = 0; /* There isn't a real big need to do this, but it helps keep
                                      things sane. */

    result->is_in_use_for_allocation = false;
    result->eligibility_notification_has_been_deferred = false;
    result->eligibility_has_been_noted = false;
    result->noted_in_scan = false;
    result->is_attached_to_shared_handle = false;

    return result;
}

void pas_segregated_partial_view_note_eligibility(
    pas_segregated_partial_view* view,
    pas_segregated_page* page)
{
    static const bool verbose = false;
    if (page->lock_ptr)
        pas_lock_assert_held(page->lock_ptr);
    PAS_ASSERT(!view->eligibility_has_been_noted);
    if (verbose)
        pas_log("Noting eligibility in page %p, view %p.\n", page, view);
    if (view->is_in_use_for_allocation)
        view->eligibility_notification_has_been_deferred = true;
    else {
        if (verbose)
            pas_log("%p: Actually telling the directory that we're eligible.\n", view);
        pas_segregated_directory_view_did_become_eligible(
            &pas_compact_segregated_size_directory_ptr_load_non_null(&view->directory)->base,
            pas_segregated_partial_view_as_view_non_null(view));
    }
    view->eligibility_has_been_noted = true;
}

void pas_segregated_partial_view_set_is_in_use_for_allocation(
    pas_segregated_partial_view* view,
    pas_segregated_shared_view* shared_view,
    pas_segregated_shared_handle* shared_handle)
{
    static const bool verbose = false;
    
    pas_segregated_shared_page_directory* shared_page_directory;
    size_t index;

    shared_page_directory = shared_handle->directory;
    index = shared_view->index;

    PAS_UNUSED_PARAM(shared_page_directory);

    if (verbose) {
        pas_log("Setting partial %p, shared %p (index %zu) as in use for allocation.\n",
                view, shared_view, index);
    }
    
    PAS_TESTING_ASSERT(!pas_segregated_partial_view_is_eligible(view));
    PAS_TESTING_ASSERT(!view->is_in_use_for_allocation);

    view->is_in_use_for_allocation = true;
    shared_view->is_in_use_for_allocation_count++;
    PAS_ASSERT(shared_view->is_in_use_for_allocation_count); /* Check that it didn't overflow. */

    if (verbose) {
        pas_log("After setting partial %p, shared %p as in use, in use count is %u.\n",
                view, shared_view, shared_view->is_in_use_for_allocation_count);
    }

    view->eligibility_has_been_noted = false;
}

bool pas_segregated_partial_view_should_table(
    pas_segregated_partial_view* view,
    pas_segregated_page_config* page_config)
{
    pas_segregated_shared_view* shared_view;
    pas_shared_handle_or_page_boundary shared_handle_or_page_boundary;
    pas_segregated_shared_handle* shared_handle;
    pas_segregated_page* page;

    shared_view = pas_compact_segregated_shared_view_ptr_load(&view->shared_view);
    if (!shared_view) {
        /* It would be weird if we were asked if we should be tabled in the primordial case. But if
           it does happen, then let's not table. */
        return false;
    }

    if (!shared_view->is_owned)
        return true;

    /* If the shared view is owned, then it should have a shared handle. */
    shared_handle_or_page_boundary = shared_view->shared_handle_or_page_boundary;
    shared_handle = pas_unwrap_shared_handle(shared_handle_or_page_boundary, *page_config);
    page = pas_segregated_page_for_boundary(shared_handle->page_boundary, *page_config);
    return !page->num_non_empty_words;
}

static pas_heap_summary compute_summary(pas_segregated_partial_view* view)
{
    pas_segregated_size_directory* size_directory;
    pas_segregated_directory* directory;
    pas_segregated_shared_view* shared_view;
    pas_segregated_page_config* page_config_ptr;
    pas_segregated_page_config page_config;
    pas_segregated_page* page;
    unsigned* full_alloc_bits;
    unsigned* alloc_bits;
    uintptr_t page_boundary;
    size_t object_size;
    size_t index;
    size_t begin_index;
    size_t end_index;
    pas_heap_summary result;

    size_directory = pas_compact_segregated_size_directory_ptr_load_non_null(&view->directory);
    directory = &size_directory->base;
    page_config_ptr = pas_segregated_page_config_kind_get_config(directory->page_config_kind);
    page_config = *page_config_ptr;
    object_size = size_directory->object_size;

    shared_view = pas_compact_segregated_shared_view_ptr_load_non_null(&view->shared_view);

    full_alloc_bits = pas_lenient_compact_unsigned_ptr_load(&view->alloc_bits);

    if (shared_view->is_owned) {
        page_boundary = (uintptr_t)pas_shared_handle_or_page_boundary_get_page_boundary(
            shared_view->shared_handle_or_page_boundary, page_config);
        page = pas_segregated_page_for_boundary((void*)page_boundary, page_config);
        alloc_bits = page->alloc_bits;

        /* We rely on the fact that the ownership lock is the page lock. That has to be true for
           partial views, since they do not do biasing. */
        if (!pas_segregated_page_config_is_utility(page_config) && shared_view->is_owned)
            PAS_ASSERT(page->lock_ptr == &shared_view->ownership_lock);
    } else {
        page = NULL;
        alloc_bits = NULL;
        page_boundary = 0;
    }

    /* This doesn't have to be optimized since this is just for internal introspection.
     
       Note that this logic magically works even for */
    begin_index = PAS_BITVECTOR_BIT_INDEX((unsigned)view->alloc_bits_offset);
    end_index = PAS_BITVECTOR_BIT_INDEX((unsigned)view->alloc_bits_offset +
                                        (unsigned)view->alloc_bits_size);

    result = pas_heap_summary_create_empty();
    
    for (index = begin_index; index < end_index; ++index) {
        uintptr_t offset;
        
        if (!pas_bitvector_get(full_alloc_bits, index))
            continue;

        if (!shared_view->is_owned) {
            result.decommitted += object_size;
            result.free_decommitted += object_size;
            result.free += object_size;
            continue;
        }
 
        offset = pas_page_base_object_offset_from_page_boundary_at_index(
            (unsigned)index, page_config.base);
        
        pas_segregated_page_add_commit_range(
            page, &result, pas_range_create(offset, offset + object_size));
        
        if (pas_bitvector_get(alloc_bits, index))
            result.allocated += object_size;
        else {
            pas_page_base_add_free_range(
                &page->base, &result,
                pas_range_create(offset, offset + object_size),
                pas_free_object_range);
        }
    }

    if (view->is_in_use_for_allocation)
        result.cached += pas_heap_summary_total(result);

    return result;
}

pas_heap_summary pas_segregated_partial_view_compute_summary(
    pas_segregated_partial_view* view)
{
    pas_segregated_shared_view* shared_view;
    pas_heap_summary result;

    shared_view = pas_compact_segregated_shared_view_ptr_load_non_null(&view->shared_view);

    pas_lock_lock(&shared_view->ownership_lock);
    result = compute_summary(view);
    pas_lock_unlock(&shared_view->ownership_lock);

    return result;
}

bool pas_segregated_partial_view_is_eligible(pas_segregated_partial_view* view)
{
    return PAS_SEGREGATED_DIRECTORY_GET_BIT(
        &pas_compact_segregated_size_directory_ptr_load_non_null(&view->directory)->base,
        view->index, eligible);
}

#endif /* LIBPAS_ENABLED */
