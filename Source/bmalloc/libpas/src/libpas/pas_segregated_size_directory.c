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

#include "pas_segregated_size_directory.h"

#include "pas_bitfit_heap.h"
#include "pas_deferred_decommit_log.h"
#include "pas_exclusive_view_template_memo_table.h"
#include "pas_monotonic_time.h"
#include "pas_page_malloc.h"
#include "pas_page_sharing_pool.h"
#include "pas_segregated_directory_inlines.h"
#include "pas_segregated_size_directory_inlines.h"
#include "pas_segregated_page_inlines.h"
#include "pas_segregated_size_directory_inlines.h"
#include "pas_stream.h"
#include "pas_thread_local_cache_layout.h"
#include "pas_utility_heap.h"
#include <unistd.h>

pas_segregated_size_directory* pas_segregated_size_directory_create(
    pas_segregated_heap* heap,
    unsigned object_size,
    unsigned alignment,
    const pas_heap_config* heap_config,
    const pas_segregated_page_config* page_config,
    pas_segregated_size_directory_creation_mode creation_mode)
{
    static const bool verbose = false;
    
    pas_segregated_size_directory* result;
    pas_segregated_page_config_kind page_config_kind;

    pas_heap_lock_assert_held();

    if (page_config)
        page_config_kind = page_config->kind;
    else
        page_config_kind = pas_segregated_page_config_kind_null;

    if (page_config)
        PAS_ASSERT(heap_config == page_config->base.heap_config_ptr);

    pas_heap_config_activate(heap_config);

    if (verbose && page_config) {
        pas_log("local allocator size for %s: %zu\n",
                pas_segregated_page_config_kind_get_string(page_config_kind),
                pas_segregated_size_directory_local_allocator_size_for_config(*page_config));
    }

    if (page_config) {
        result = pas_immortal_heap_allocate(
            sizeof(pas_segregated_size_directory),
            "pas_segregated_size_directory",
            pas_object_allocation);
    } else {
        result = pas_immortal_heap_allocate_with_alignment(
            sizeof(pas_segregated_size_directory) + sizeof(pas_bitfit_size_class),
            PAS_MAX(PAS_ALIGNOF(pas_segregated_size_directory), PAS_ALIGNOF(pas_bitfit_size_class)),
            "pas_segregated_size_directory+pas_bitfit_size_class",
            pas_object_allocation);
    }

    if (verbose) {
        pas_log("%d: Creating directory %p for %s with object_size = %u, alignment = %u\n",
                getpid(), result, pas_segregated_page_config_kind_get_string(page_config_kind),
                object_size, alignment);
    }

    pas_segregated_directory_construct(
        &result->base, page_config_kind,
        heap->runtime_config->sharing_mode,
        pas_segregated_size_directory_kind);

    result->heap = heap;
    result->object_size = object_size;

    PAS_ASSERT(pas_is_power_of_2(alignment));
    PAS_ASSERT(pas_is_aligned(object_size, alignment));
    result->alignment_shift = pas_log2(alignment);

    if (page_config)
        result->alignment_shift = pas_log2(alignment);
    else {
        PAS_ASSERT(PAS_SEGREGATED_SIZE_DIRECTORY_ALIGNMENT_SHIFT_BITS == 5);
        result->alignment_shift = 31;
    }

    result->encoded_stuff =
        pas_segregated_size_directory_encode_stuff(2 * PAS_NUM_BASELINE_ALLOCATORS, UINT_MAX);
    
    result->view_cache_index = (pas_allocator_index)UINT_MAX;
    result->allocator_index = 0;
    
    pas_segregated_size_directory_data_ptr_store(&result->data, NULL);

    if (!page_config) {
        pas_bitfit_heap* bitfit_heap;
        pas_bitfit_size_class* bitfit_size_class;

        bitfit_heap = pas_segregated_heap_get_bitfit(heap, heap_config, pas_lock_is_held);
        PAS_ASSERT(bitfit_heap);
        
        bitfit_size_class = pas_segregated_size_directory_get_bitfit_size_class(result);
        
        pas_bitfit_heap_construct_and_insert_size_class(
            bitfit_heap, bitfit_size_class, object_size, heap_config, heap->runtime_config);
    }
    
    pas_compact_atomic_segregated_size_directory_ptr_store(&result->next_for_heap, NULL);

    pas_segregated_size_directory_set_did_try_to_create_view_cache(result, false);

    switch (creation_mode) {
    case pas_segregated_size_directory_initial_creation_mode:
        break;

    case pas_segregated_size_directory_full_creation_mode:
        pas_segregated_size_directory_finish_creation(result);
        break;
    }

    return result;
}

void pas_segregated_size_directory_finish_creation(pas_segregated_size_directory* directory)
{
    pas_segregated_heap* heap;

    heap = directory->heap;
    
    if (!heap->runtime_config->directory_size_bound_for_baseline_allocators)
        pas_segregated_size_directory_create_tlc_allocator(directory);
    if (!heap->runtime_config->directory_size_bound_for_partial_views)
        pas_segregated_size_directory_enable_exclusive_views(directory);
    if (!heap->runtime_config->directory_size_bound_for_no_view_cache)
        pas_segregated_size_directory_create_tlc_view_cache(directory);
}

pas_segregated_size_directory_data* pas_segregated_size_directory_ensure_data(
    pas_segregated_size_directory* directory,
    pas_lock_hold_mode heap_lock_hold_mode)
{
    pas_segregated_size_directory_data* data;
    const pas_segregated_page_config* page_config;

    data = pas_segregated_size_directory_data_ptr_load(&directory->data);

    if (data)
        return data;

    page_config = pas_segregated_page_config_kind_get_config(directory->base.page_config_kind);

    pas_heap_lock_lock_conditionally(heap_lock_hold_mode);

    data = pas_segregated_size_directory_data_ptr_load(&directory->data);
    if (data) {
        pas_heap_lock_unlock_conditionally(heap_lock_hold_mode);
        return data;
    }

    if (page_config && page_config->base.page_size > page_config->base.granule_size) {
        data = pas_immortal_heap_allocate(
            sizeof(pas_extended_segregated_size_directory_data),
            "pas_extended_segregated_size_directory_data",
            pas_object_allocation);

        pas_compact_tagged_page_granule_use_count_ptr_store(
            &((pas_extended_segregated_size_directory_data*)data)->full_use_counts, NULL);
    } else {
        data = pas_immortal_heap_allocate(
            sizeof(pas_segregated_size_directory_data),
            "pas_segregated_size_directory_data",
            pas_object_allocation);
    }

    pas_compact_tagged_unsigned_ptr_store(&data->full_alloc_bits, NULL);
    data->offset_from_page_boundary_to_first_object = 0;
    data->offset_from_page_boundary_to_end_of_last_object = 0;
    data->full_num_non_empty_words = 0;

    pas_fence();

    pas_segregated_size_directory_data_ptr_store(&directory->data, data);

    pas_heap_lock_unlock_conditionally(heap_lock_hold_mode);

    return data;
}

pas_extended_segregated_size_directory_data*
pas_segregated_size_directory_get_extended_data(
    pas_segregated_size_directory* directory)
{
    const pas_segregated_page_config* page_config;

    page_config = pas_segregated_page_config_kind_get_config(directory->base.page_config_kind);
    PAS_ASSERT(page_config);
    PAS_ASSERT(page_config->base.page_size > page_config->base.granule_size);

    return (pas_extended_segregated_size_directory_data*)
        pas_segregated_size_directory_data_ptr_load(&directory->data);
}

void pas_segregated_size_directory_create_tlc_allocator(
    pas_segregated_size_directory* directory)
{
    pas_heap_lock_assert_held();

    if (pas_segregated_page_config_kind_is_utility(directory->base.page_config_kind))
        return;

    pas_segregated_size_directory_ensure_data(directory, pas_lock_is_held);

    if (directory->allocator_index) {
        PAS_ASSERT(directory->allocator_index != (pas_allocator_index)UINT_MAX);
        return;
    }
    
    pas_thread_local_cache_layout_add(directory);

    PAS_ASSERT(directory->allocator_index);
    PAS_ASSERT(directory->allocator_index < (pas_allocator_index)UINT_MAX);
}

void pas_segregated_size_directory_create_tlc_view_cache(
    pas_segregated_size_directory* directory)
{
    size_t index;
    
    pas_heap_lock_assert_held();

    if (pas_segregated_page_config_kind_is_utility(directory->base.page_config_kind))
        return;

    if (!pas_segregated_size_directory_set_did_try_to_create_view_cache(directory, true))
        return;

    if (!pas_segregated_size_directory_view_cache_capacity(directory))
        return;

    PAS_ASSERT(directory->view_cache_index == (pas_allocator_index)UINT_MAX);
    
    pas_thread_local_cache_layout_add_view_cache(directory);

    PAS_ASSERT(directory->view_cache_index);
    PAS_ASSERT(directory->view_cache_index < (pas_allocator_index)UINT_MAX);

    for (index = pas_segregated_directory_size(&directory->base); index--;) {
        pas_segregated_view view;

        view = pas_segregated_directory_get(&directory->base, index);

        if (!pas_segregated_view_is_some_exclusive(view))
            continue;

        if (!pas_segregated_view_lock_ownership_lock_if_owned(view))
            continue;

        pas_segregated_view_get_page(view)->view_cache_index = directory->view_cache_index;
        
        pas_segregated_view_unlock_ownership_lock(view);
    }
}

void pas_segregated_size_directory_enable_exclusive_views(
    pas_segregated_size_directory* directory)
{
    pas_segregated_size_directory_data* data;
    pas_segregated_size_directory* template_directory;
    pas_segregated_page_config_kind page_config_kind;
    const pas_segregated_page_config* page_config_ptr;
    pas_segregated_page_config page_config;
    unsigned object_size;
    size_t alloc_bits_bytes;
    unsigned offset;
    unsigned begin;
    unsigned end;
    unsigned* full_alloc_bits;
    unsigned full_num_non_empty_words;
    size_t index;
    pas_allocation_config bootstrap_allocation_config;

    pas_heap_lock_assert_held();

    data = pas_segregated_size_directory_ensure_data(directory, pas_lock_is_held);
    if (data->full_num_non_empty_words)
        return;

    page_config_kind = directory->base.page_config_kind;
    if (page_config_kind == pas_segregated_page_config_kind_null)
        return;
    
    page_config_ptr = pas_segregated_page_config_kind_get_config(page_config_kind);
    object_size = directory->object_size;
    
    template_directory = pas_exclusive_view_template_memo_table_get(
        &pas_exclusive_view_template_memo_table_instance,
        pas_exclusive_view_template_memo_key_create(object_size, page_config_kind));
    if (template_directory) {
        pas_segregated_size_directory_data* template_data;

        template_data = pas_segregated_size_directory_data_ptr_load(&template_directory->data);
        
        data->full_alloc_bits = template_data->full_alloc_bits;
        data->offset_from_page_boundary_to_first_object =
            template_data->offset_from_page_boundary_to_first_object;
        data->offset_from_page_boundary_to_end_of_last_object =
            template_data->offset_from_page_boundary_to_end_of_last_object;

        if (page_config_ptr->base.page_size > page_config_ptr->base.granule_size) {
            pas_segregated_size_directory_get_extended_data(directory)->full_use_counts =
                pas_segregated_size_directory_get_extended_data(
                    template_directory)->full_use_counts;
        }

        pas_fence();
        data->full_num_non_empty_words =
            template_data->full_num_non_empty_words;
        return;
    }

    page_config = *page_config_ptr;
    
    alloc_bits_bytes = pas_segregated_page_config_num_alloc_bytes(page_config);
    full_alloc_bits = pas_immortal_heap_allocate_with_manual_alignment(
        alloc_bits_bytes, sizeof(unsigned),
        "pas_segregated_size_directory_data/full_alloc_bits",
        pas_object_allocation);
    pas_compact_tagged_unsigned_ptr_store(&data->full_alloc_bits, full_alloc_bits);
    pas_zero_memory(full_alloc_bits, alloc_bits_bytes);

    begin = pas_segregated_page_offset_from_page_boundary_to_first_object_exclusive(object_size,
                                                                                    page_config);
    end = pas_segregated_page_offset_from_page_boundary_to_end_of_last_object_exclusive(object_size,
                                                                                        page_config);

    data->offset_from_page_boundary_to_first_object = begin;
    data->offset_from_page_boundary_to_end_of_last_object = end;

    for (offset = begin; offset < end; offset += object_size) {
        pas_bitvector_set(
            full_alloc_bits,
            pas_page_base_index_of_object_at_offset_from_page_boundary(
                offset, page_config.base),
            true);
    }
    
    full_num_non_empty_words = 0;
    for (index = pas_segregated_page_config_num_alloc_words(page_config); index--;) {
        if (full_alloc_bits[index])
            full_num_non_empty_words++;
    }
    PAS_ASSERT(full_num_non_empty_words);

    if (page_config.base.page_size > page_config.base.granule_size) {
        pas_extended_segregated_size_directory_data* extended_data;
        pas_page_granule_use_count* full_use_counts;
        uintptr_t num_granules;
        bool initialize_result;

        extended_data = pas_segregated_size_directory_get_extended_data(directory);

        num_granules = page_config.base.page_size / page_config.base.granule_size;

        full_use_counts = pas_immortal_heap_allocate_with_manual_alignment(
            num_granules * sizeof(pas_page_granule_use_count),
            sizeof(pas_page_granule_use_count),
            "pas_extended_segregated_size_directory_data/full_use_counts",
            pas_object_allocation);

        initialize_result = pas_segregated_page_initialize_full_use_counts(
            directory, full_use_counts, num_granules, page_config);

        /* This should have initialized all of the bits. */
        PAS_ASSERT(initialize_result);

        pas_compact_tagged_page_granule_use_count_ptr_store(
            &extended_data->full_use_counts,
            full_use_counts);
    }

    pas_fence();
    data->full_num_non_empty_words = full_num_non_empty_words;
    PAS_ASSERT(data->full_num_non_empty_words == full_num_non_empty_words);

    pas_bootstrap_free_heap_allocation_config_construct(&bootstrap_allocation_config, pas_lock_is_held);
    
    pas_exclusive_view_template_memo_table_add_new(
        &pas_exclusive_view_template_memo_table_instance,
        directory,
        NULL,
        &bootstrap_allocation_config);
}

pas_baseline_allocator*
pas_segregated_size_directory_select_allocator_slow(
    pas_segregated_size_directory* directory)
{
    pas_baseline_allocator_table_initialize_if_necessary();
    
    for (;;) {
        unsigned a_index;
        unsigned b_index;
        pas_baseline_allocator* a_allocator;
        pas_baseline_allocator* b_allocator;
        unsigned selected_index;
        pas_baseline_allocator* selected_allocator;
        pas_segregated_size_directory* selected_directory;

        selected_index = pas_segregated_size_directory_baseline_allocator_index(directory);
        if (selected_index < PAS_NUM_BASELINE_ALLOCATORS) {
            selected_allocator = pas_baseline_allocator_table + selected_index;

            pas_lock_lock(&selected_allocator->lock);

            if (pas_segregated_size_directory_baseline_allocator_index(directory) != selected_index) {
                pas_lock_unlock(&selected_allocator->lock);
                continue;
            }

            return selected_allocator;
        }

        if (selected_index < 2 * PAS_NUM_BASELINE_ALLOCATORS)
            selected_index -= PAS_NUM_BASELINE_ALLOCATORS;
        else {
            PAS_ASSERT(selected_index == 2 * PAS_NUM_BASELINE_ALLOCATORS);
            
            a_index = pas_baseline_allocator_table_get_random_index();
            b_index = pas_baseline_allocator_table_get_random_index();
    
            a_allocator = pas_baseline_allocator_table + a_index;
            b_allocator = pas_baseline_allocator_table + b_index;

            /* If it's cheap to stop one of these then just pick either one. */
            if (!pas_local_allocator_is_active(&a_allocator->u.allocator))
                selected_index = a_index;
            else if (!pas_local_allocator_is_active(&b_allocator->u.allocator))
                selected_index = b_index;
            else {
                pas_segregated_view a_view;
                pas_segregated_view b_view;

                a_view = a_allocator->u.allocator.view;
                b_view = b_allocator->u.allocator.view;

                PAS_UNUSED_PARAM(b_view);

                if (!a_view)
                    selected_index = a_index;
                else
                    selected_index = b_index;
            }

            if (!pas_segregated_size_directory_compare_and_swap_baseline_allocator_index_weak(
                    directory,
                    2 * PAS_NUM_BASELINE_ALLOCATORS,
                    PAS_NUM_BASELINE_ALLOCATORS + selected_index))
                continue;
        }
    
        selected_allocator = pas_baseline_allocator_table + selected_index;
    
        pas_lock_lock(&selected_allocator->lock);

        if (pas_segregated_size_directory_baseline_allocator_index(directory) == selected_index)
            return selected_allocator;

        if (pas_segregated_size_directory_baseline_allocator_index(directory)
            != PAS_NUM_BASELINE_ALLOCATORS + selected_index) {
            pas_lock_unlock(&selected_allocator->lock);
            continue;
        }

        if (!pas_local_allocator_is_null(&selected_allocator->u.allocator)) {
            pas_num_baseline_allocator_evictions++;
            selected_directory = pas_segregated_view_get_size_directory(selected_allocator->u.allocator.view);
            pas_baseline_allocator_detach_directory(selected_allocator);
            /* We rely on the set function having a fence because it uses a CAS. */
            pas_segregated_size_directory_set_baseline_allocator_index(
                selected_directory, 2 * PAS_NUM_BASELINE_ALLOCATORS);
        }

        pas_baseline_allocator_attach_directory(selected_allocator, directory);

        pas_segregated_size_directory_set_baseline_allocator_index(directory, selected_index);

        return selected_allocator;
    }
}

static pas_segregated_view take_first_eligible_direct_create_new_view_callback(
    pas_segregated_directory_iterate_config* config)
{
    static const bool verbose = false;
    
    pas_segregated_size_directory* size_directory;
    pas_segregated_view view;
    pas_heap_runtime_config* runtime_config;

    PAS_TESTING_ASSERT(config->directory->directory_kind == pas_segregated_size_directory_kind);
    
    size_directory = (pas_segregated_size_directory*)config->directory;

    runtime_config = size_directory->heap->runtime_config;

    if (config->index >= runtime_config->directory_size_bound_for_partial_views)
        pas_segregated_size_directory_enable_exclusive_views(size_directory);

    if (config->index >= runtime_config->directory_size_bound_for_baseline_allocators)
        pas_segregated_size_directory_create_tlc_allocator(size_directory);

    if (config->index >= runtime_config->directory_size_bound_for_no_view_cache)
        pas_segregated_size_directory_create_tlc_view_cache(size_directory);
    
    if (pas_segregated_size_directory_are_exclusive_views_enabled(size_directory)) {
        view = pas_segregated_exclusive_view_as_view_non_null(
            pas_segregated_exclusive_view_create(size_directory, config->index));
        if (verbose) {
            pas_log("%p: created exclusive %p.\n",
                    size_directory, pas_segregated_view_get_ptr(view));
        }
    } else {
        view = pas_segregated_partial_view_as_view_non_null(
            pas_segregated_partial_view_create(size_directory, config->index));
        if (verbose) {
            pas_log("%p: created partial %p.\n",
                    size_directory, pas_segregated_view_get_ptr(view));
        }
    }

    if (verbose) {
        pas_log("%d: %p (%s, %u): now have %zu views.\n",
                getpid(), size_directory,
                pas_segregated_page_config_kind_get_string(size_directory->base.page_config_kind),
                size_directory->object_size,
                pas_segregated_directory_size(&size_directory->base));
    }

    return view;
}

pas_segregated_view pas_segregated_size_directory_take_first_eligible(
    pas_segregated_size_directory* size_directory)
{
    static const bool verbose = false;
    
    pas_segregated_directory* directory;
    pas_segregated_directory_iterate_config config;
    pas_segregated_view view;

    PAS_TESTING_ASSERT(
        size_directory->base.page_config_kind != pas_segregated_page_config_kind_null);

    directory = &size_directory->base;

    view = pas_segregated_size_directory_take_first_eligible_impl(
        directory, &config,
        take_first_eligible_direct_create_new_view_callback);

    if (verbose) {
        pas_log("Taking eligible view %p (%s).\n",
                pas_segregated_view_get_ptr(view),
                pas_segregated_view_kind_get_string(pas_segregated_view_get_kind(view)));
    }

    PAS_TESTING_ASSERT(!PAS_SEGREGATED_DIRECTORY_GET_BIT(
                           directory, pas_segregated_view_get_index(view), eligible));
    
    return view;
}

typedef struct {
    pas_deferred_decommit_log* decommit_log;
    pas_lock_hold_mode heap_lock_hold_mode;
    const pas_segregated_page_config* my_page_config_ptr;
    pas_page_sharing_pool_take_result result;
} take_last_empty_data;

static PAS_ALWAYS_INLINE unsigned
take_last_empty_should_consider_view_parallel(
    pas_segregated_directory_bitvector_segment segment,
    pas_segregated_directory_iterate_config* config)
{
    take_last_empty_data* data;

    data = config->arg;

    PAS_UNUSED_PARAM(data);

    return segment.empty_bits;
}

static bool
take_last_empty_consider_view(pas_segregated_directory_iterate_config* config)
{
    static const bool verbose = false;

    /* We put our take_last_empty logic in consider_view because should_consider_view_parallel
       cannot really tell if a page can be taken. */

    take_last_empty_data* data;
    pas_segregated_directory* directory;
    pas_deferred_decommit_log* decommit_log;
    pas_lock_hold_mode heap_lock_hold_mode;
    const pas_segregated_page_config* my_page_config_ptr;
    pas_segregated_page_config my_page_config;
    size_t index;
    pas_segregated_view generic_view;
    pas_segregated_exclusive_view* view;
    pas_lock* held_lock;
    pas_segregated_page* page;
    bool did_take_bit;
    pas_lock* ownership_lock;

    directory = config->directory;
    index = config->index;

    did_take_bit = PAS_SEGREGATED_DIRECTORY_SET_BIT(directory, index, empty, false);
    
    if (!did_take_bit)
        return false;

    generic_view = pas_segregated_directory_get(directory, index);
    PAS_ASSERT(pas_segregated_view_is_exclusive(generic_view));
    view = pas_segregated_view_get_exclusive(generic_view);
    held_lock = NULL;
    data = config->arg;

    my_page_config_ptr = data->my_page_config_ptr;
    heap_lock_hold_mode = data->heap_lock_hold_mode;
    decommit_log = data->decommit_log;
    my_page_config = *my_page_config_ptr;
    
    if (verbose)
        pas_log("%p: considering empty index %zu, view = %p.\n", directory, index, view);

    if (pas_segregated_page_config_is_utility(my_page_config))
        ownership_lock = pas_lock_for_switch_conditionally(&pas_heap_lock, heap_lock_hold_mode);
    else
        ownership_lock = &view->ownership_lock;

    pas_lock_switch(&held_lock, ownership_lock);
    
    if (pas_segregated_page_config_is_utility(my_page_config))
        pas_heap_lock_assert_held();
    
    if (!view->is_owned) {
        if (verbose)
            pas_log("not owned.\n");
        
        goto release_held_lock_and_return_false;
    }
    
    page = pas_segregated_page_for_boundary(view->page_boundary, my_page_config);
    
    if (verbose)
        pas_log("exclusive mode.\n");
    
    if (!PAS_SEGREGATED_DIRECTORY_SET_BIT(directory, index, eligible, false))
        goto release_held_lock_and_return_false;
        
    if (!view->is_owned) {
        bool result;
        result = pas_segregated_directory_view_did_become_eligible_at_index(directory, index);
        PAS_ASSERT(result);
        goto release_held_lock_and_return_false;
    }
    
    /* The empty bit just means that the page *could* be empty. It's possible to come here and find
       a non-empty page. In that case we make the page eligible again but leave the empty bit clear. */
    if (!pas_segregated_page_qualifies_for_decommit(page, my_page_config)) {
        if (verbose)
            pas_log("was not empty.\n");
        pas_segregated_directory_view_did_become_eligible_at_index(directory, index);
        goto release_held_lock_and_return_false;
    }
    
    /* This has a very short race that causes the directory to possibly waste memory: if
       during this brief window we have someone trying to find an eligible page, they may
       end up allocating a new one.
       
       But we deliberately keep the duration of this race window short. We make the page
       ineligible only during the time it takes us to figure out how to decommit it and to
       acquire the commit lock. The commit lock should be trivially available here, so
       that's just a CAS. After that, the page is made either eligible or decommitted.
       Either way, if it's the first eligible, someone will try to use it. At that point,
       if we are not done, they will wait for us since we hold the commit lock. */

    pas_segregated_page_switch_lock(page, &held_lock, my_page_config);

    if (pas_segregated_page_config_is_utility(my_page_config)) {
        /* The utility heap requires us to hold the heap lock anytime we'd hold the ownership, commit,
           or page locks. It just so happens that we were already holding the heap lock, and
           page_switch_lock does nothing for utility. So we should still be holding the heap lock. */
        PAS_ASSERT(held_lock == pas_lock_for_switch_conditionally(&pas_heap_lock, heap_lock_hold_mode));
        pas_heap_lock_assert_held();
    }

    PAS_ASSERT(!PAS_SEGREGATED_DIRECTORY_GET_BIT(directory, index, eligible));

    /* It's totally possible that the empty bit got set again. We should clear it for sure now. */
    PAS_SEGREGATED_DIRECTORY_SET_BIT(directory, index, empty, false);

    PAS_TESTING_ASSERT(pas_segregated_page_qualifies_for_decommit(page, my_page_config));
    
    if (pas_segregated_page_config_is_utility(my_page_config)) {
        PAS_ASSERT(my_page_config.base.page_size == my_page_config.base.granule_size);
        
        pas_heap_lock_assert_held();

        view->is_owned = false;
        if (verbose) {
            pas_log("view %p has is_owned = false due to utility decommit in take_last_empty\n",
                    view);
        }
        pas_page_malloc_decommit(
            pas_segregated_page_boundary(page, my_page_config),
            my_page_config.base.page_size,
            my_page_config.base.heap_config_ptr->mmap_capability);
        decommit_log->total += my_page_config.base.page_size;
        my_page_config.base.destroy_page_header(&page->base, pas_lock_is_held);
        pas_segregated_directory_view_did_become_eligible_at_index(directory, index);
        pas_lock_switch(&held_lock, NULL);
        data->result = pas_page_sharing_pool_take_success;
        return true;
    }
    
    if (page->emptiness.num_non_empty_words) {
        bool decommit_result;
        size_t num_committed_granules;
        
        /* No partial decommit in the utility heap! */
        PAS_ASSERT(!pas_segregated_page_config_is_utility(my_page_config));
        
        /* We know that there must be empty granules in this page, since either the empty
           or the partly_empty bit was set. Those bits wouldn't get set unless we noticed
           an empty granule. If we did try to allocate from those granules, we would first
           have to take away its eligibility and then clear its empty bits. But we know that
           this could not have happened since *we* took away its eligibility. */
        
        decommit_result = pas_segregated_page_take_empty_granules(
            page, decommit_log, &held_lock, pas_range_is_not_locked, heap_lock_hold_mode);
        
        pas_lock_switch(&held_lock, NULL);
        
        num_committed_granules = pas_segregated_page_get_num_committed_granules(page);
        PAS_ASSERT(num_committed_granules);
        
        /* The page can become eligible again now that we've marked things for decommit. */
        pas_segregated_directory_view_did_become_eligible_at_index(directory, index);
        
        if (decommit_result) {
            if (verbose)
                pas_log("%p: did partly take index %zu\n", directory, index);
        } else {
            pas_segregated_directory_view_did_become_empty_at_index(directory, index);
            data->result = pas_page_sharing_pool_take_locks_unavailable;
            return true;
        }
    } else {
        pas_lock_switch(&held_lock, &view->ownership_lock);
        view->is_owned = false;
        if (verbose) {
            pas_log("view %p has is_owned = false due to normal decommit in take_last_empty\n",
                    view);
        }
        pas_lock_switch(&held_lock, NULL);
        
        if (!pas_segregated_page_take_physically(
                page, decommit_log, pas_range_is_not_locked, heap_lock_hold_mode)) {
            PAS_ASSERT(!pas_segregated_page_config_kind_is_utility(directory->page_config_kind));
            
            pas_lock_switch(&held_lock, &view->ownership_lock);
            view->is_owned = true;
            if (verbose) {
                pas_log("view %p has is_owned = true due to undo of decommit in take_last_empty\n",
                        view);
            }
            pas_lock_switch(&held_lock, NULL);
            
            pas_segregated_directory_view_did_become_eligible_at_index(directory, index);
            pas_segregated_directory_view_did_become_empty_at_index(directory, index);
            
            data->result = pas_page_sharing_pool_take_locks_unavailable;
            return true;
        }
        
        if (verbose) {
            pas_log("%p: did fully take index %zu\n", directory, index);
            pas_log("Destroying page header for view = %p, page = %p, boundary = %p (%p) "
                    "as part of decommit.\n",
                    view, page, pas_segregated_page_boundary(page, my_page_config),
                    view->page_boundary);
        }
        my_page_config.base.destroy_page_header(&page->base, heap_lock_hold_mode);

        pas_segregated_directory_view_did_become_eligible_at_index(directory, index);
    }
    
    data->result = pas_page_sharing_pool_take_success;
    return true;

release_held_lock_and_return_false:
    pas_lock_switch(&held_lock, NULL);

    /* Returning false means that the backwards iteration keeps going and this should get called
       again if there are any other views marked empty. */
    return false;
}

pas_page_sharing_pool_take_result
pas_segregated_size_directory_take_last_empty(
    pas_segregated_size_directory* size_directory,
    pas_deferred_decommit_log* decommit_log,
    pas_lock_hold_mode heap_lock_hold_mode)
{
    /* Be careful: heap_lock_hold_mode here can be unrelated to what page_config says, since
       we may be asked by the physical page sharing pool to decommit on behalf of any other heap,
       including ones that hold the heap lock while doing business. */

    take_last_empty_data data;
    pas_segregated_directory_iterate_config config;
    bool did_find_something;
    pas_segregated_directory* directory;

    directory = &size_directory->base;

    PAS_TESTING_ASSERT(directory->page_config_kind != pas_segregated_page_config_kind_null);

    data.decommit_log = decommit_log;
    data.heap_lock_hold_mode = heap_lock_hold_mode;
    data.result = pas_page_sharing_pool_take_none_available;

    data.my_page_config_ptr = pas_segregated_page_config_kind_get_config(directory->page_config_kind);

    config.directory = directory;
    config.should_consider_view_parallel = take_last_empty_should_consider_view_parallel;
    config.consider_view = take_last_empty_consider_view;
    config.arg = &data;

    did_find_something =
        pas_segregated_directory_iterate_backward_to_take_last_empty(&config);

    if (!did_find_something)
        return pas_page_sharing_pool_take_none_available;

    PAS_ASSERT(pas_segregated_directory_is_doing_sharing(directory));
    pas_page_sharing_pool_did_create_delta(
        &pas_physical_page_sharing_pool,
        pas_page_sharing_participant_create(
            directory,
            pas_page_sharing_participant_kind_select_for_segregated_directory(
                directory->directory_kind)));
    
    return data.result;
}

pas_segregated_size_directory* pas_segregated_size_directory_for_object(
    uintptr_t begin,
    const pas_heap_config* config)
{
    pas_segregated_view view;
    view = pas_segregated_view_for_object(begin, config);
    if (view)
        return pas_segregated_view_get_size_directory(view);
    return NULL;
}

pas_baseline_allocator_result
pas_segregated_size_directory_get_allocator_from_tlc(
    pas_segregated_size_directory* directory,
    size_t size,
    pas_size_lookup_mode size_lookup_mode,
    const pas_heap_config* config,
    unsigned* cached_index)
{
    pas_local_allocator_result tlc_result;
    unsigned baseline_allocator_index;

    PAS_ASSERT(pas_segregated_size_directory_has_tlc_allocator(directory));
    PAS_ASSERT(!pas_heap_config_is_utility(config));
    
    pas_heap_lock_lock();
    pas_segregated_heap_ensure_allocator_index(
        directory->heap,
        directory,
        size,
        size_lookup_mode,
        config,
        cached_index);
    pas_heap_lock_unlock();

    /* Currently as soon as we switch to TLC allocators, we cannot possibly have a baseline
       allocator. But if we do, we should stop it now. */
    baseline_allocator_index = pas_segregated_size_directory_baseline_allocator_index(directory);
    if (baseline_allocator_index < PAS_NUM_BASELINE_ALLOCATORS) {
        pas_baseline_allocator* baseline_allocator;
        
        baseline_allocator = pas_baseline_allocator_table + baseline_allocator_index;
        
        pas_lock_lock(&baseline_allocator->lock);

        /* It's possible that due to a race, the directory's index has been reassigned to something
           else. At worst, that race will mean that some baseline allocator will be used for
           allocation. There's not a whole lot we can do about that. At worst, it means letting the
           high watermark grow a bit in case of a weird race. */
        if (pas_segregated_size_directory_baseline_allocator_index(directory) == baseline_allocator_index) {
            pas_baseline_allocator_detach_directory(baseline_allocator);
            /* We rely on the set function having a fence because it uses a CAS. */
            pas_segregated_size_directory_set_baseline_allocator_index(
                directory, 2 * PAS_NUM_BASELINE_ALLOCATORS);
        }
        
        pas_lock_unlock(&baseline_allocator->lock);
    }
    
    tlc_result = pas_thread_local_cache_get_local_allocator_for_initialized_index(
        pas_thread_local_cache_get(config),
        directory->allocator_index,
        pas_lock_is_not_held);
    
    PAS_ASSERT(tlc_result.did_succeed);

    return pas_baseline_allocator_result_create_success(tlc_result.allocator, NULL);
}

pas_heap_summary pas_segregated_size_directory_compute_summary_for_unowned_exclusive(
    pas_segregated_size_directory* directory)
{
    pas_segregated_size_directory_data* data;
    pas_segregated_page_config page_config;
    pas_heap_summary result;
    size_t payload_size;

    PAS_ASSERT(directory->base.page_config_kind != pas_segregated_page_config_kind_null);

    data = pas_segregated_size_directory_data_ptr_load(&directory->data);
    page_config = *pas_segregated_page_config_kind_get_config(directory->base.page_config_kind);
    
    result = pas_heap_summary_create_empty();
    
    payload_size = (data->offset_from_page_boundary_to_end_of_last_object -
                    data->offset_from_page_boundary_to_first_object);
    
    result.decommitted += page_config.base.page_size;
    result.free += payload_size;
    result.free_decommitted += payload_size;
    
    return result;
}

typedef struct {
    pas_segregated_size_directory* directory;
    pas_segregated_size_directory_for_each_live_object_callback callback;
    void* arg;
} for_each_live_object_data;

static bool for_each_live_object_object_callback(
    pas_segregated_view view,
    pas_range range,
    void* arg)
{
    for_each_live_object_data* data;

    data = arg;

    PAS_ASSERT(data->directory->object_size == pas_range_size(range));

    return data->callback(data->directory, view, range.begin, data->arg);
}

bool pas_segregated_size_directory_for_each_live_object(
    pas_segregated_size_directory* directory,
    pas_segregated_size_directory_for_each_live_object_callback callback,
    void* arg)
{
    for_each_live_object_data data;
    size_t index;

    data.directory = directory;
    data.callback = callback;
    data.arg = arg;

    /* The cool thing about this loop is that in case of races it'll tend to go check out
       more view. */
    for (index = 0; index < pas_segregated_directory_size(&directory->base); ++index) {
        pas_segregated_view view;

        view = pas_segregated_directory_get(&directory->base, index);
        if (!pas_segregated_view_for_each_live_object(
                view, for_each_live_object_object_callback, &data, pas_lock_is_not_held))
            return false;
    }

    return true;
}

uint8_t pas_segregated_size_directory_view_cache_capacity(pas_segregated_size_directory* directory)
{
    pas_segregated_page_config_kind kind;
    const pas_segregated_page_config* config;

    kind = directory->base.page_config_kind;

    if (kind == pas_segregated_page_config_kind_null)
        return 0;

    config = pas_segregated_page_config_kind_get_config(kind);
    if (!config->enable_view_cache)
        return 0;
    
    return pas_heap_runtime_config_view_cache_capacity_for_object_size(
        directory->heap->runtime_config, directory->object_size, config);
}

size_t pas_segregated_size_directory_local_allocator_size(
    pas_segregated_size_directory* directory)
{
    pas_segregated_page_config_kind kind;

    kind = directory->base.page_config_kind;

    if (kind == pas_segregated_page_config_kind_null)
        return pas_segregated_size_directory_local_allocator_size_for_null_config();
    
    return pas_segregated_size_directory_local_allocator_size_for_config(
        *pas_segregated_page_config_kind_get_config(kind));
}

pas_allocator_index pas_segregated_size_directory_num_allocator_indices(
    pas_segregated_size_directory* directory)
{
    return pas_segregated_size_directory_num_allocator_indices_for_allocator_size(
        pas_segregated_size_directory_local_allocator_size(directory));
}

void pas_segregated_size_directory_dump_reference(
    pas_segregated_size_directory* directory, pas_stream* stream)
{
    if (!directory) {
        pas_stream_printf(stream, "null");
        return;
    }
    
    pas_stream_printf(
        stream,
        "%p(segregated_size_directory, %u, %p, %s)",
        directory,
        directory->object_size,
        directory->heap,
        pas_segregated_page_config_kind_get_string(directory->base.page_config_kind));
}

void pas_segregated_size_directory_dump_for_spectrum(
    pas_stream* stream, void* directory)
{
    pas_segregated_size_directory_dump_reference(directory, stream);
}

#endif /* LIBPAS_ENABLED */
