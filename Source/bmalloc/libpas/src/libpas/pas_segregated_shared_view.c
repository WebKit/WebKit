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

#include "pas_segregated_shared_view.h"

#include "pas_debug_spectrum.h"
#include "pas_heap_lock.h"
#include "pas_immortal_heap.h"
#include "pas_page_malloc.h"
#include "pas_page_sharing_pool.h"
#include "pas_physical_memory_transaction.h"
#include "pas_range16.h"
#include "pas_segregated_page_inlines.h"
#include "pas_segregated_shared_handle.h"
#include "pas_segregated_shared_page_directory.h"
#include "pas_segregated_shared_view_inlines.h"
#include "pas_shared_handle_or_page_boundary_inlines.h"

size_t pas_segregated_shared_view_count = 0;

pas_segregated_shared_view* pas_segregated_shared_view_create(size_t index)
{
    pas_segregated_shared_view* result;
    
    result = pas_immortal_heap_allocate(
        sizeof(pas_segregated_shared_view),
        "pas_segregated_shared_view",
        pas_object_allocation);

    pas_segregated_shared_view_count++;

    result->shared_handle_or_page_boundary = NULL;

    /* NULL must look like a page. Code that tests if things are pages can implicitly assume that
       this is true, but it's not fundamental. */
    PAS_ASSERT(pas_is_wrapped_page_boundary(result->shared_handle_or_page_boundary));
    
    pas_lock_construct(&result->commit_lock);
    pas_lock_construct(&result->ownership_lock);

    /* We cannot initialize this until we allocate the page. */
    result->bump_offset = 0;

    result->is_in_use_for_allocation_count = 0;
    
    result->index = (unsigned)index;
    PAS_ASSERT(result->index == index);
    result->is_owned = false;

    return result;
}

pas_segregated_shared_handle* pas_segregated_shared_view_commit_page(
    pas_segregated_shared_view* view,
    pas_segregated_heap* heap,
    pas_segregated_shared_page_directory* shared_page_directory,
    pas_segregated_partial_view* partial_view,
    const pas_segregated_page_config* page_config_ptr)
{
    pas_segregated_directory* directory;
    pas_segregated_shared_handle* handle;
    pas_segregated_page_config page_config;
    pas_physical_memory_transaction transaction;

    PAS_UNUSED_PARAM(partial_view);

    page_config = *page_config_ptr;
    
    PAS_ASSERT(pas_is_wrapped_page_boundary(view->shared_handle_or_page_boundary));

    directory = &shared_page_directory->base;

    if (pas_segregated_directory_can_do_sharing(directory)) {
        pas_lock* held_lock;

        if (pas_segregated_page_config_is_utility(page_config))
            held_lock = NULL;
        else {
            held_lock = &view->commit_lock;
            pas_lock_assert_held(held_lock);
        }
        
        pas_physical_page_sharing_pool_take_for_page_config(
            page_config.base.page_size, &page_config_ptr->base,
            pas_segregated_page_config_heap_lock_hold_mode(page_config),
            &held_lock, !pas_segregated_page_config_is_utility(page_config));
    }

    pas_heap_lock_lock_conditionally(pas_segregated_page_config_heap_lock_hold_mode(page_config));
    handle = pas_segregated_shared_handle_create(view, shared_page_directory, page_config_ptr);

    if (!handle->page_boundary) {
        pas_segregated_page* page;
        void* page_boundary;

        page_boundary = NULL;

        pas_heap_lock_unlock_conditionally(
            pas_segregated_page_config_heap_lock_hold_mode(page_config));
        
        pas_physical_memory_transaction_construct(&transaction);
        do {
            PAS_ASSERT(!page_boundary);
            pas_physical_memory_transaction_begin(&transaction);
            pas_heap_lock_lock_conditionally(
                pas_segregated_page_config_heap_lock_hold_mode(page_config));
            page_boundary = page_config.page_allocator(
                heap,
                pas_segregated_page_config_heap_lock_hold_mode(page_config) ? NULL : &transaction,
                pas_segregated_page_shared_role);
            pas_heap_lock_unlock_conditionally(
                pas_segregated_page_config_heap_lock_hold_mode(page_config));
        } while (!pas_physical_memory_transaction_end(&transaction));
        
        pas_heap_lock_lock_conditionally(
            pas_segregated_page_config_heap_lock_hold_mode(page_config));
        if (!page_boundary) {
            pas_segregated_shared_handle_destroy(handle);
            pas_heap_lock_unlock_conditionally(
                pas_segregated_page_config_heap_lock_hold_mode(page_config));
            return NULL;
        }
        
        page = (pas_segregated_page*)
            page_config.base.create_page_header(
                page_boundary,
                pas_page_kind_for_segregated_variant_and_role(
                    page_config.variant, pas_segregated_page_shared_role),
                pas_lock_is_held);
        
        pas_heap_lock_unlock_conditionally(
            pas_segregated_page_config_heap_lock_hold_mode(page_config));

        handle->page_boundary = pas_segregated_page_boundary(page, page_config);

        view->bump_offset = (unsigned)pas_round_up_to_power_of_2(
            page_config.shared_payload_offset, pas_segregated_page_config_min_align(page_config));
    } else {
        if (PAS_DEBUG_SPECTRUM_USE_FOR_COMMIT) {
            pas_debug_spectrum_add(
                shared_page_directory, pas_segregated_shared_page_directory_dump_for_spectrum,
                page_config.base.page_size);
        }
        
        pas_heap_lock_unlock_conditionally(
            pas_segregated_page_config_heap_lock_hold_mode(page_config));

        pas_page_malloc_commit(handle->page_boundary, page_config.base.page_size,
                               page_config.base.heap_config_ptr->mmap_capability);
        page_config.base.create_page_header(
            handle->page_boundary,
            pas_page_kind_for_segregated_variant_and_role(
                page_config.variant, pas_segregated_page_shared_role),
            pas_lock_is_not_held);
    }

    pas_segregated_page_construct(
        pas_segregated_page_for_boundary_unchecked(handle->page_boundary, page_config),
        pas_segregated_shared_handle_as_view_non_null(handle),
        false,
        page_config_ptr);

    pas_lock_lock(&view->ownership_lock);
    view->is_owned = true;
    pas_lock_unlock(&view->ownership_lock);

    return handle;
}

typedef struct {
    uintptr_t page_boundary;
    uintptr_t min_align_shift;
    pas_range16 live_objects[PAS_MAX_OBJECTS_PER_PAGE];
    unsigned num_live_objects;
} compute_summary_data;

static bool compute_summary_for_each_live_object_callback(
    pas_segregated_view view,
    pas_range range,
    void* arg)
{
    static const bool verbose = PAS_SHOULD_LOG(PAS_LOG_SEGREGATED_HEAPS);
    
    compute_summary_data* data;
    unsigned index;
    unsigned insertion_point;
    pas_range16 my_range;

    PAS_UNUSED_PARAM(view);

    data = arg;

    PAS_ASSERT(data->num_live_objects < PAS_MAX_OBJECTS_PER_PAGE);

    /* Try to find a good insertion point for this live object. This doesn't have to be super
       fast since this is in debugging code. But anyway, this probably isn't bad, since the
       live objects are reported to us in _almost_ sorted order. */

    if (verbose) {
        pas_log("object at %p...%p, boundary = %p, so %p...%p.\n",
                (void*)range.begin, (void*)range.end, (void*)data->page_boundary,
                (void*)(range.begin - data->page_boundary),
                (void*)(range.end - data->page_boundary));
    }

    my_range = pas_range16_create(
        (range.begin - data->page_boundary) >> data->min_align_shift,
        (range.end - data->page_boundary) >> data->min_align_shift);

    for (index = data->num_live_objects; index--;) {
        pas_range16 other_range;
        other_range = data->live_objects[index];
        if (my_range.begin > other_range.begin) {
            PAS_ASSERT(my_range.begin >= other_range.end);
            break;
        }
    }
    insertion_point = index + 1;
    PAS_ASSERT(insertion_point <= PAS_MAX_OBJECTS_PER_PAGE);

    for (index = data->num_live_objects; index-- > insertion_point;)
        data->live_objects[index + 1] = data->live_objects[index];

    data->live_objects[insertion_point] = my_range;
    data->num_live_objects++;

    return true;
}

static pas_heap_summary compute_summary(pas_segregated_shared_view* view,
                                        const pas_segregated_page_config* page_config_ptr)
{
    static const bool verbose = PAS_SHOULD_LOG(PAS_LOG_SEGREGATED_HEAPS);
    
    pas_segregated_page_config page_config;
    pas_segregated_page* page;
    uintptr_t start_of_page;
    uintptr_t start_of_payload;
    uintptr_t end_of_payload;
    uintptr_t end_of_page;
    pas_heap_summary result;
    uintptr_t size_of_meta;
    uintptr_t size_of_payload;
    compute_summary_data data;
    unsigned start_of_last_free;
    unsigned index;

    page_config = *page_config_ptr;

    start_of_page = 0;
    start_of_payload = pas_round_up_to_power_of_2(
        page_config.shared_payload_offset,
        pas_segregated_page_config_min_align(page_config));
    end_of_payload = view->bump_offset;
    end_of_page = page_config.base.page_size;

    if (verbose)
        pas_log("index = %d, bump_offset = %lu/%lu.\n", view->index, end_of_payload, end_of_page);
    
    PAS_ASSERT(start_of_payload >= start_of_page);
    PAS_ASSERT(end_of_payload >= start_of_payload);
    PAS_ASSERT(end_of_page >= end_of_payload);

    result = pas_heap_summary_create_empty();

    size_of_payload = end_of_payload - start_of_payload;
    size_of_meta = page_config.base.page_size - size_of_payload;
    PAS_UNUSED_PARAM(size_of_meta);

    if (!view->is_owned) {
        if (verbose)
            pas_log("    not owned.\n");
        result.decommitted += page_config.base.page_size;
        result.free += size_of_payload;
        result.free_decommitted += size_of_payload;
        return result;
    }

    page = pas_segregated_page_for_boundary(
        pas_shared_handle_or_page_boundary_get_page_boundary(
            view->shared_handle_or_page_boundary, page_config),
        page_config);

    if (verbose)
        pas_log("    owned.\n");
    
    pas_segregated_page_add_commit_range(
        page, &result, pas_range_create(start_of_page, end_of_page));

    pas_page_base_add_free_range(
        &page->base, &result, pas_range_create(start_of_page, start_of_payload),
        pas_free_meta_range);
    pas_page_base_add_free_range(
        &page->base, &result, pas_range_create(end_of_payload, end_of_page),
        pas_free_meta_range);

    data.page_boundary = (uintptr_t)pas_segregated_page_boundary(page, page_config);
    data.min_align_shift = page_config.base.min_align_shift;
    data.num_live_objects = 0;
    pas_segregated_view_for_each_live_object(
        pas_segregated_shared_view_as_view_non_null(view),
        compute_summary_for_each_live_object_callback,
        &data,
        pas_lock_is_held);

    start_of_last_free = (unsigned)start_of_payload;
    for (index = 0; index < data.num_live_objects; ++index) {
        pas_range16 range;
        unsigned offset;
        unsigned offset_end;

        range = data.live_objects[index];
        offset = (unsigned)range.begin << page_config.base.min_align_shift;
        offset_end = (unsigned)range.end << page_config.base.min_align_shift;

        PAS_ASSERT(offset >= start_of_last_free);
        pas_page_base_add_free_range(
            &page->base, &result, pas_range_create(start_of_last_free, offset),
            pas_free_object_range);
        result.allocated += offset_end - offset;
        start_of_last_free = offset_end;
    }
    PAS_ASSERT(start_of_last_free <= end_of_payload);
    pas_page_base_add_free_range(
        &page->base, &result, pas_range_create(start_of_last_free, end_of_payload),
        pas_free_object_range);

    if (view->is_in_use_for_allocation_count)
        result.cached += page_config.base.page_size;

    return result;
}

pas_heap_summary
pas_segregated_shared_view_compute_summary(pas_segregated_shared_view* view,
                                           const pas_segregated_page_config* page_config)
{
    pas_heap_summary result;

    pas_lock_lock(&view->ownership_lock);
    result = compute_summary(view, page_config);
    pas_lock_unlock(&view->ownership_lock);

    return result;
}

bool pas_segregated_shared_view_is_empty(pas_segregated_shared_view* view)
{
    pas_shared_handle_or_page_boundary shared_handle_or_page_boundary;

    shared_handle_or_page_boundary = view->shared_handle_or_page_boundary;
    if (pas_is_wrapped_page_boundary(shared_handle_or_page_boundary)) {
        /* This happens when the page is empty and we free it. */
        return true;
    }
    
    return PAS_SEGREGATED_DIRECTORY_GET_BIT(
        &pas_unwrap_shared_handle_no_liveness_checks(
            shared_handle_or_page_boundary)->directory->base,
        view->index,
        empty);
}

#endif /* LIBPAS_ENABLED */
