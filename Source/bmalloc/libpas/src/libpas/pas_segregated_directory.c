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

#include "pas_segregated_directory.h"

#include "pas_log.h"
#include "pas_page_sharing_pool.h"
#include "pas_segregated_directory_inlines.h"
#include "pas_segregated_heap.h"
#include "pas_segregated_size_directory_inlines.h"

PAS_API void pas_segregated_directory_construct(
    pas_segregated_directory* directory,
    pas_segregated_page_config_kind page_config_kind,
    pas_page_sharing_mode page_sharing_mode,
    pas_segregated_directory_kind directory_kind)
{
    *directory = PAS_SEGREGATED_DIRECTORY_INITIALIZER(
        page_config_kind, page_sharing_mode, directory_kind);
}

pas_segregated_directory_data*
pas_segregated_directory_get_data_slow(pas_segregated_directory* directory,
                                       pas_lock_hold_mode heap_lock_hold_mode)
{
    pas_segregated_directory_data* result;
    
    pas_heap_lock_lock_conditionally(heap_lock_hold_mode);

    result = pas_segregated_directory_data_ptr_load(&directory->data);
    if (!result) {
        result = pas_immortal_heap_allocate_with_alignment(
            sizeof(pas_segregated_directory_data),
            sizeof(pas_versioned_field),
            "pas_segregated_directory_data",
            pas_object_allocation);

        pas_versioned_field_construct(&result->first_eligible, 0);
        pas_versioned_field_construct(&result->last_empty_plus_one, 0);
        pas_segregated_directory_segmented_bitvectors_construct(&result->bitvectors);
        pas_segregated_directory_view_vector_construct(&result->views);
        pas_segregated_directory_sharing_payload_ptr_store(&result->sharing_payload, 0);

        pas_fence();

        pas_segregated_directory_data_ptr_store(&directory->data, result);
    }
    
    pas_heap_lock_unlock_conditionally(heap_lock_hold_mode);

    return result;
}

uint64_t pas_segregated_directory_get_use_epoch(pas_segregated_directory* directory)
{
    static const bool verbose = false;
    
    uintptr_t last_empty_plus_one;
    size_t index;

    last_empty_plus_one = pas_segregated_directory_get_last_empty_plus_one_value(directory);

    for (index = last_empty_plus_one; index--;) {
        pas_segregated_view view;

        /* We need to check this because otherwise the page could have an unset epoch. */
        if (!pas_segregated_directory_is_empty(directory, index))
            continue;

        view = pas_segregated_directory_get(directory, index);

        if (pas_segregated_view_is_partial(view))
            continue;

        if (pas_segregated_view_lock_ownership_lock_if_owned(view)) {
            pas_segregated_page* page;
            uint64_t use_epoch;
            page = pas_segregated_view_get_page(view);
            if (page->num_non_empty_words)
                use_epoch = 0; /* it's not empty, so keep looking. */
            else {
                use_epoch = page->use_epoch;
                PAS_ASSERT(use_epoch);
            }
            pas_segregated_view_unlock_ownership_lock(view);
            if (use_epoch) {
                if (verbose)
                    pas_log("%p: returning epoch %llu for index %zu\n", directory, (unsigned long long)use_epoch, index);
                PAS_ASSERT(use_epoch);
                return use_epoch;
            }
        }
    }

    return 0;
}

pas_page_sharing_participant_payload*
pas_segregated_directory_get_sharing_payload(pas_segregated_directory* directory,
                                             pas_lock_hold_mode heap_lock_hold_mode)
{
    static const bool verbose = false;
    
    pas_segregated_directory_data* data;
    pas_segregated_directory_sharing_payload_ptr* payload_ptr;
    uintptr_t encoded_payload;
    pas_page_sharing_participant_payload* payload;

    PAS_ASSERT(pas_segregated_directory_can_do_sharing(directory));

    data = pas_segregated_directory_get_data(directory, heap_lock_hold_mode);

    payload_ptr = &data->sharing_payload;
    encoded_payload = pas_segregated_directory_sharing_payload_ptr_load(payload_ptr);
    if (encoded_payload & PAS_SEGREGATED_DIRECTORY_SHARING_PAYLOAD_IS_INITIALIZED_BIT) {
        payload = (pas_page_sharing_participant_payload*)(
            encoded_payload & ~PAS_SEGREGATED_DIRECTORY_SHARING_PAYLOAD_IS_INITIALIZED_BIT);
    } else {
        pas_heap_lock_lock_conditionally(heap_lock_hold_mode);

        encoded_payload = pas_segregated_directory_sharing_payload_ptr_load(payload_ptr);
        PAS_ASSERT(
            !encoded_payload || !!(
                encoded_payload & PAS_SEGREGATED_DIRECTORY_SHARING_PAYLOAD_IS_INITIALIZED_BIT));

        if (encoded_payload) {
            payload = (pas_page_sharing_participant_payload*)(
                encoded_payload & ~PAS_SEGREGATED_DIRECTORY_SHARING_PAYLOAD_IS_INITIALIZED_BIT);
        } else {
            payload = (pas_page_sharing_participant_payload*)(
                encoded_payload & ~PAS_SEGREGATED_DIRECTORY_SHARING_PAYLOAD_IS_INITIALIZED_BIT);
            
            payload = pas_immortal_heap_allocate(
                sizeof(pas_page_sharing_participant_payload),
                "pas_segregated_directory_data/sharing_payload",
                pas_object_allocation);
            
            pas_page_sharing_participant_payload_construct(payload);

            pas_segregated_directory_sharing_payload_ptr_store(payload_ptr, (uintptr_t)payload);

            if (verbose)
                pas_log("Adding directory participant to pool\n");
            
            pas_page_sharing_pool_add(
                &pas_physical_page_sharing_pool,
                pas_page_sharing_participant_create(
                    directory,
                    pas_page_sharing_participant_kind_select_for_segregated_directory(
                        directory->directory_kind)));
            
            pas_fence();

            pas_segregated_directory_sharing_payload_ptr_store(
                payload_ptr,
                (uintptr_t)payload | PAS_SEGREGATED_DIRECTORY_SHARING_PAYLOAD_IS_INITIALIZED_BIT);
        }
        
        pas_heap_lock_unlock_conditionally(heap_lock_hold_mode);
    }

    return payload;
}

PAS_API void pas_segregated_directory_minimize_first_eligible(
    pas_segregated_directory* directory,
    size_t index)
{
    pas_segregated_directory_data* data;

    data = pas_segregated_directory_data_ptr_load(&directory->data);
    if (!data) {
        PAS_ASSERT(!index);
        return;
    }

    pas_versioned_field_minimize(&data->first_eligible, index);
}

PAS_API void pas_segregated_directory_update_first_eligible_after_search(
    pas_segregated_directory* directory,
    pas_versioned_field first_eligible,
    size_t new_value)
{
    pas_segregated_directory_data* data;

    data = pas_segregated_directory_data_ptr_load(&directory->data);
    if (!data) {
        PAS_ASSERT(first_eligible.value <= 1);
        PAS_ASSERT(new_value <= 1);
        return;
    }

    pas_versioned_field_try_write_watched(
        &data->first_eligible,
        first_eligible,
        new_value);
}

bool pas_segregated_directory_view_did_become_eligible_at_index(
    pas_segregated_directory* directory,
    size_t index)
{
    if (PAS_SEGREGATED_DIRECTORY_SET_BIT(directory, index, eligible, true)) {
        pas_segregated_directory_minimize_first_eligible(directory, index);
        return true;
    }
    return false;
}

bool pas_segregated_directory_view_did_become_eligible(
    pas_segregated_directory* directory,
    pas_segregated_view view)
{
    return pas_segregated_directory_view_did_become_eligible_at_index(
        directory, pas_segregated_view_get_index(view));
}

static bool maximize_last_empty(
    pas_segregated_directory* directory,
    size_t index)
{
    pas_segregated_directory_data* data;

    data = pas_segregated_directory_data_ptr_load(&directory->data);
    if (!data) {
        PAS_ASSERT(!index);
        return true;
    }

    return !pas_versioned_field_maximize(
        &data->last_empty_plus_one,
        index + 1);
}

bool pas_segregated_directory_view_did_become_empty_at_index(
    pas_segregated_directory* directory,
    size_t index)
{
    static const bool verbose = false;
    
    bool did_add_first_empty;
    
    if (verbose)
        pas_log("%p: page at index %zu became empty!\n", directory, index);

    if (!pas_segregated_directory_set_empty_bit(directory, index, true))
        return false;

    /* It's cheap for us to do two maximize_last_empty's here because most likely there won't
       be anything to do. */
    did_add_first_empty = maximize_last_empty(directory, index);

    if (did_add_first_empty
        && pas_segregated_directory_can_do_sharing(directory)) {
        if (verbose)
            printf("Telling our physical pool that we created an empty page.\n");
        PAS_ASSERT(pas_segregated_directory_is_doing_sharing(directory));
        pas_page_sharing_pool_did_create_delta(
            &pas_physical_page_sharing_pool,
            pas_page_sharing_participant_create(
                directory,
                pas_page_sharing_participant_kind_select_for_segregated_directory(
                    directory->directory_kind)));
    }

    return true;
}

bool pas_segregated_directory_view_did_become_empty(
    pas_segregated_directory* directory,
    pas_segregated_view view)
{
    return pas_segregated_directory_view_did_become_empty_at_index(
        directory, pas_segregated_view_get_index(view));
}

bool pas_segregated_directory_is_committed(
    pas_segregated_directory* directory, size_t index)
{
    return pas_segregated_view_is_owned(pas_segregated_directory_get(directory, index));
}

size_t pas_segregated_directory_num_committed_views(
    pas_segregated_directory* directory)
{
    size_t index;
    size_t result;
    result = 0;
    for (index = pas_segregated_directory_size(directory); index--;)
        result += pas_segregated_view_is_owned(pas_segregated_directory_get(directory, index));
    return result;
}

typedef struct {
    size_t result;
} num_empty_views_data;

static PAS_ALWAYS_INLINE unsigned
num_empty_views_should_consider_view_parallel(
    pas_segregated_directory_bitvector_segment segment,
    pas_segregated_directory_iterate_config* config)
{
    num_empty_views_data* data;

    data = config->arg;

    data->result += (size_t)__builtin_popcount(segment.empty_bits);
    
    return 0;
}

size_t pas_segregated_directory_num_empty_views(
    pas_segregated_directory* directory)
{
    num_empty_views_data data;
    data.result = 0;
    pas_segregated_directory_iterate_config config;
    config.directory = directory;
    config.index = 0;
    config.should_consider_view_parallel = num_empty_views_should_consider_view_parallel;
    config.consider_view = NULL;
    config.arg = &data;
    pas_segregated_directory_iterate_forward(&config);
    return data.result;
}

void pas_segregated_directory_update_last_empty_plus_one_after_search(
    pas_segregated_directory* directory,
    pas_versioned_field last_empty_plus_one,
    size_t new_value)
{
    pas_segregated_directory_data* data;

    data = pas_segregated_directory_data_ptr_load(&directory->data);
    if (!data) {
        PAS_ASSERT(last_empty_plus_one.value <= 1);
        PAS_ASSERT(new_value <= 1);
        return;
    }

    pas_versioned_field_try_write(
        &data->last_empty_plus_one,
        last_empty_plus_one,
        new_value);
}

void pas_segregated_directory_append(
    pas_segregated_directory* directory, size_t index, pas_segregated_view view)
{
    pas_segregated_directory_data* data;
    pas_compact_atomic_segregated_view encoded_view;
    
    pas_heap_lock_assert_held();
    PAS_ASSERT(pas_segregated_directory_size(directory) == index);

    PAS_ASSERT(view);

    switch (pas_segregated_view_get_kind(view)) {
    case pas_segregated_exclusive_view_kind:
    case pas_segregated_ineligible_exclusive_view_kind:
    case pas_segregated_partial_view_kind:
        PAS_ASSERT(directory->directory_kind == pas_segregated_size_directory_kind);
        break;
    case pas_segregated_shared_view_kind:
        PAS_ASSERT(directory->directory_kind == pas_segregated_shared_page_directory_kind);
        break;
    default:
        PAS_ASSERT(!"Bad view kind");
    }

    if (pas_segregated_view_kind_can_become_empty(pas_segregated_view_get_kind(view))) {
        /* Ideally, we'd start sharing once things actually become empty. But starting sharing
           means holding the heap lock, and we cannot contend for the heap lock if we're in the
           free path. */
        
        if (pas_segregated_directory_can_do_sharing(directory))
            pas_segregated_directory_start_sharing_if_necessary(directory, pas_lock_is_held);
    }

    if (!index) {
        data = pas_segregated_directory_data_ptr_load(&directory->data);
        PAS_ASSERT(!data || !data->views.size);
        PAS_ASSERT(!(directory->bits & PAS_SEGREGATED_DIRECTORY_BITS_VIEW_MASK));
        pas_compact_atomic_segregated_view_store(&directory->first_view, view);
        return;
    }

    data = pas_segregated_directory_get_data(directory, pas_lock_is_held);

    if (PAS_BITVECTOR_NUM_WORDS(index) > data->bitvectors.size) {
        pas_segregated_directory_segmented_bitvectors_append(
            &data->bitvectors,
            PAS_SEGREGATED_DIRECTORY_BITVECTOR_SEGMENT_INITIALIZER,
            pas_lock_is_held);
        PAS_ASSERT(PAS_BITVECTOR_NUM_WORDS(index) == data->bitvectors.size);
    }

    pas_compact_atomic_segregated_view_store(&encoded_view, view);
    pas_segregated_directory_view_vector_append(&data->views, encoded_view, pas_lock_is_held);
}

pas_heap_summary pas_segregated_directory_compute_summary(pas_segregated_directory* directory)
{
    pas_heap_summary result;
    pas_segregated_page_config* page_config_ptr;
    size_t index;

    page_config_ptr = pas_segregated_page_config_kind_get_config(directory->page_config_kind);
    result = pas_heap_summary_create_empty();

    for (index = 0; index < pas_segregated_directory_size(directory); ++index) {
        result = pas_heap_summary_add(
            result,
            pas_segregated_view_compute_summary(
                pas_segregated_directory_get(directory, index),
                page_config_ptr));
    }

    return result;
}

size_t pas_segregated_directory_num_empty_granules(
    pas_segregated_directory* directory)
{
    size_t result;
    size_t index;

    result = 0;

    for (index = 0; index < pas_segregated_directory_size(directory); ++index) {
        pas_segregated_view view;

        view = pas_segregated_directory_get(directory, index);

        if (!pas_segregated_view_is_some_exclusive(view)
            && !pas_segregated_view_is_shared(view))
            continue;

        if (!pas_segregated_view_lock_ownership_lock_if_owned(view))
            continue;

        result += pas_segregated_page_get_num_empty_granules(pas_segregated_view_get_page(view));

        pas_segregated_view_unlock_ownership_lock(view);
    }

    return result;
}

#endif /* LIBPAS_ENABLED */
