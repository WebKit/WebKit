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

#include "pas_bitfit_view.h"

#include "pas_bitfit_directory.h"
#include "pas_bitfit_page.h"
#include "pas_bitfit_view_inlines.h"
#include "pas_epoch.h"
#include "pas_page_sharing_pool.h"

pas_bitfit_view* pas_bitfit_view_create(pas_bitfit_directory* directory,
                                        unsigned index)
{
    static const bool verbose = false;
    
    pas_bitfit_view* result;

    result = pas_immortal_heap_allocate(
        sizeof(pas_bitfit_view),
        "pas_bitfit_view",
        pas_object_allocation);

    if (verbose) {
        pas_log("Creating view %p with config %s\n",
                result, pas_bitfit_page_config_kind_get_string(directory->config_kind));
    }

    result->page_boundary = NULL;
    pas_compact_bitfit_directory_ptr_store(&result->directory, directory);
    result->is_owned = false;
    result->index = index;
    pas_lock_construct(&result->ownership_lock);
    pas_lock_construct(&result->commit_lock);

    return result;
}

void pas_bitfit_view_note_nonemptiness(pas_bitfit_view* view)
{
    pas_bitfit_directory_max_free_did_become_unprocessed_unchecked(
        pas_compact_bitfit_directory_ptr_load_non_null(&view->directory),
        view->index,
        "become unprocessed on note_nonemptiness");
}

static void did_become_empty_for_bits(pas_bitfit_view* view, pas_bitfit_page* page)
{
    page->use_epoch = pas_get_epoch();

    pas_bitfit_directory_view_did_become_empty(
        pas_compact_bitfit_directory_ptr_load_non_null(&view->directory), view);
}

void pas_bitfit_view_note_full_emptiness(pas_bitfit_view* view, pas_bitfit_page* page)
{
    did_become_empty_for_bits(view, page);
    
    pas_bitfit_directory_max_free_did_become_empty(
        pas_compact_bitfit_directory_ptr_load_non_null(&view->directory),
        view->index,
        "become empty on note_emptiness");
}

void pas_bitfit_view_note_partial_emptiness(pas_bitfit_view* view, pas_bitfit_page* page)
{
    did_become_empty_for_bits(view, page);
}

void pas_bitfit_view_note_max_free(pas_bitfit_view* view)
{
    pas_bitfit_directory_max_free_did_become_unprocessed(
        pas_compact_bitfit_directory_ptr_load_non_null(&view->directory),
        view->index,
        "become unprocessed on note_max_free");
}

static pas_heap_summary compute_summary(pas_bitfit_view* view)
{
    static const bool verbose = false;
    
    const pas_bitfit_page_config* config_ptr;
    pas_bitfit_page_config config;
    void* boundary;
    pas_bitfit_page* page;
    pas_heap_summary result;
    size_t begin;
    size_t end;
    size_t offset;

    config_ptr = pas_bitfit_page_config_kind_get_config(
        pas_compact_bitfit_directory_ptr_load_non_null(
            &view->directory)->config_kind);
    config = *config_ptr;

    result = pas_heap_summary_create_empty();

    if (!view->is_owned) {
        size_t payload_size;

        payload_size = pas_bitfit_page_payload_size(config);
        
        result.decommitted += config.base.page_size;
        result.free += payload_size;
        result.free_decommitted += payload_size;

        return result;
    }

    if (verbose) {
        pas_log("Getting page boundary for view %p and config %s\n",
                view, pas_bitfit_page_config_kind_get_string(config.kind));
    }
    
    boundary = view->page_boundary;
    page = pas_bitfit_page_for_boundary(boundary, config);

    pas_page_base_compute_committed_when_owned(&page->base, &result);

    begin = pas_bitfit_page_offset_to_first_object(config);
    end = pas_bitfit_page_offset_to_end_of_last_object(config);

    pas_page_base_add_free_range(
        &page->base, &result, pas_range_create(0, begin), pas_free_meta_range);
    pas_page_base_add_free_range(
        &page->base, &result, pas_range_create(end, config.base.page_size),
        pas_free_meta_range);

    for (offset = begin; offset < end; offset += pas_page_base_config_min_align(config.base)) {
        if (pas_bitvector_get(pas_bitfit_page_free_bits(page),
                              offset >> config.base.min_align_shift)) {
            pas_page_base_add_free_range(
                &page->base, &result,
                pas_range_create(offset, offset + pas_page_base_config_min_align(config.base)),
                pas_free_object_range);
        } else
            result.allocated += pas_page_base_config_min_align(config.base);
    }

    return result;
}

pas_heap_summary pas_bitfit_view_compute_summary(pas_bitfit_view* view)
{
    pas_heap_summary result;
    pas_lock_lock(&view->ownership_lock);
    result = compute_summary(view);
    pas_lock_unlock(&view->ownership_lock);
    return result;
}

typedef struct {
    pas_bitfit_view* view;
    pas_bitfit_view_for_each_live_object_callback callback;
    void* arg;
} for_each_live_object_data;

static bool for_each_live_object_callback(uintptr_t begin,
                                          size_t size,
                                          void* arg)
{
    for_each_live_object_data* data;

    data = arg;

    return data->callback(data->view, begin, size, data->arg);
}

static bool for_each_live_object(
    pas_bitfit_view* view,
    pas_bitfit_view_for_each_live_object_callback callback,
    void* arg)
{
    const pas_bitfit_page_config* config_ptr;
    pas_bitfit_page_config config;
    void* boundary;
    pas_bitfit_page* page;
    for_each_live_object_data data;

    if (!view->is_owned)
        return true;

    config_ptr = pas_bitfit_page_config_kind_get_config(
        pas_compact_bitfit_directory_ptr_load_non_null(&view->directory)->config_kind);
    config = *config_ptr;

    boundary = view->page_boundary;
    page = pas_bitfit_page_for_boundary(boundary, config);

    data.view = view;
    data.callback = callback;
    data.arg = arg;

    return pas_bitfit_page_for_each_live_object(page, for_each_live_object_callback, &data);
}

bool pas_bitfit_view_for_each_live_object(
    pas_bitfit_view* view,
    pas_bitfit_view_for_each_live_object_callback callback,
    void* arg)
{
    bool result;
    pas_lock_lock(&view->ownership_lock);
    result = for_each_live_object(view, callback, arg);
    pas_lock_unlock(&view->ownership_lock);
    return result;
}

#endif /* LIBPAS_ENABLED */
