/*
 * Copyright (c) 2018-2021 Apple Inc. All rights reserved.
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

#include "pas_all_heaps.h"

#include "bmalloc_heap_innards.h"
#include "hotbit_heap_innards.h"
#include "iso_heap_innards.h"
#include "iso_test_heap.h"
#include "jit_heap.h"
#include "minalign32_heap.h"
#include "pagesize64k_heap.h"
#include "pas_biasing_directory_inlines.h"
#include "pas_bitfit_heap.h"
#include "pas_heap.h"
#include "pas_heap_lock.h"
#include "pas_large_utility_free_heap.h"
#include "pas_ptr_hash_set.h"
#include "pas_scavenger.h"
#include "pas_segregated_biasing_directory.h"
#include "pas_segregated_heap.h"
#include "pas_utility_heap.h"
#include "pas_utility_heap_config.h"
#include "thingy_heap.h"

pas_heap* pas_all_heaps_first_heap = NULL;
size_t pas_all_heaps_count = 0;

void pas_all_heaps_add_heap(pas_heap* heap)
{
    pas_heap_lock_assert_held();
    pas_compact_heap_ptr_store(&heap->next_heap, pas_all_heaps_first_heap);
    pas_all_heaps_first_heap = heap;
    pas_all_heaps_count++;
}

bool pas_all_heaps_for_each_static_heap(pas_all_heaps_for_each_heap_callback callback,
                                        void* arg)
{
#if PAS_ENABLE_THINGY
    if (!callback(&thingy_primitive_heap, arg))
        return false;
    if (!callback(&thingy_utility_heap, arg))
        return false;
#endif
    
#if PAS_ENABLE_ISO
    if (!callback(&iso_common_primitive_heap, arg))
        return false;
#endif

#if PAS_ENABLE_ISO_TEST
    if (!callback(&iso_test_common_primitive_heap, arg))
        return false;
#endif

#if PAS_ENABLE_MINALIGN32
    if (!callback(&minalign32_common_primitive_heap, arg))
        return false;
#endif

#if PAS_ENABLE_PAGESIZE64K
    if (!callback(&pagesize64k_common_primitive_heap, arg))
        return false;
#endif

#if PAS_ENABLE_BMALLOC
    if (!callback(&bmalloc_common_primitive_heap, arg))
        return false;
#endif

#if PAS_ENABLE_HOTBIT
    if (!callback(&hotbit_common_primitive_heap, arg))
        return false;
#endif

#if PAS_ENABLE_JIT
    if (!callback(&jit_common_primitive_heap, arg))
        return false;
#endif

    return true;
}

bool pas_all_heaps_for_each_dynamic_heap(pas_all_heaps_for_each_heap_callback callback,
                                         void* arg)
{
    pas_heap* heap;
    
    pas_heap_lock_assert_held();

    for (heap = pas_all_heaps_first_heap; heap; heap = pas_compact_heap_ptr_load(&heap->next_heap)) {
        if (!callback(heap, arg))
            return false;
    }
    
    return true;
}

bool pas_all_heaps_for_each_heap(pas_all_heaps_for_each_heap_callback callback,
                                 void* arg)
{
    pas_heap_lock_assert_held();

    if (!pas_all_heaps_for_each_static_heap(callback, arg))
        return false;
    if (!pas_all_heaps_for_each_dynamic_heap(callback, arg))
        return false;
    return true;
}

bool pas_all_heaps_for_each_static_segregated_heap_not_part_of_a_heap(
    pas_all_heaps_for_each_segregated_heap_callback callback,
    void* arg)
{
    if (!callback(&pas_utility_segregated_heap, &pas_utility_heap_config, arg))
        return false;
    return true;
}

typedef struct {
    pas_all_heaps_for_each_segregated_heap_callback callback;
    void* arg;
} for_each_segregated_heap_data;

static bool for_each_segregated_heap_callback(pas_heap* heap, void* arg)
{
    for_each_segregated_heap_data* data = arg;
    return data->callback(
        &heap->segregated_heap, pas_heap_config_kind_get_config(heap->config_kind), data->arg);
}

bool pas_all_heaps_for_each_static_segregated_heap(
    pas_all_heaps_for_each_segregated_heap_callback callback,
    void* arg)
{
    for_each_segregated_heap_data data;
    
    if (!pas_all_heaps_for_each_static_segregated_heap_not_part_of_a_heap(callback, arg))
        return false;

    data.callback = callback;
    data.arg = arg;
    return pas_all_heaps_for_each_static_heap(for_each_segregated_heap_callback, &data);
}

bool pas_all_heaps_for_each_segregated_heap(pas_all_heaps_for_each_segregated_heap_callback callback,
                                            void* arg)
{
    for_each_segregated_heap_data data;
    
    pas_heap_lock_assert_held();

    if (!pas_all_heaps_for_each_static_segregated_heap_not_part_of_a_heap(callback, arg))
        return false;
    
    data.callback = callback;
    data.arg = arg;
    
    return pas_all_heaps_for_each_heap(for_each_segregated_heap_callback, &data);
}

static bool get_num_free_bytes_for_each_heap_callback(pas_heap* heap, void* arg)
{
    size_t* result = arg;
    (*result) += pas_heap_get_num_free_bytes(heap);
    return true;
}

size_t pas_all_heaps_get_num_free_bytes(pas_lock_hold_mode heap_lock_hold_mode)
{
    size_t result;
    pas_heap_lock_lock_conditionally(heap_lock_hold_mode);
    pas_all_heaps_for_each_heap(get_num_free_bytes_for_each_heap_callback, &result);
    result += pas_utility_heap_get_num_free_bytes();
    pas_heap_lock_unlock_conditionally(heap_lock_hold_mode);
    return result;
}

static bool reset_heap_ref_for_each_heap_callback(pas_heap* heap, void* arg)
{
    PAS_UNUSED_PARAM(arg);
    pas_heap_reset_heap_ref(heap);
    return true;
}

void pas_all_heaps_reset_heap_ref(pas_lock_hold_mode heap_lock_hold_mode)
{
    pas_heap_lock_lock_conditionally(heap_lock_hold_mode);
    pas_all_heaps_for_each_heap(reset_heap_ref_for_each_heap_callback, NULL);
    pas_heap_lock_unlock_conditionally(heap_lock_hold_mode);
}

typedef struct {
    pas_ptr_hash_set seen_shared_page_directories;
    pas_all_heaps_for_each_segregated_directory_callback callback;
    void* arg;
} for_each_segregated_directory_data;

static bool for_each_segregated_directory_global_size_directory_callback(
    pas_segregated_heap* heap,
    pas_segregated_global_size_directory* directory,
    void* arg)
{
    for_each_segregated_directory_data* data;
    pas_page_sharing_pool* pool;
    size_t index;

    PAS_UNUSED_PARAM(heap);

    data = arg;

    if (!data->callback(&directory->base, data->arg))
        return false;

    pool = pas_segregated_global_size_directory_bias_sharing_pool(directory);
    for (index = pas_page_sharing_pool_num_participants(pool); index--;) {
        pas_page_sharing_participant participant;
        pas_segregated_biasing_directory* biasing_directory;

        participant = pas_page_sharing_pool_get_participant(pool, index);

        if (!participant)
            continue;

        PAS_ASSERT(pas_page_sharing_participant_get_kind(participant)
                   == pas_page_sharing_participant_biasing_directory);

        biasing_directory = pas_unwrap_segregated_biasing_directory(
            pas_page_sharing_participant_get_ptr(participant));

        if (!data->callback(&biasing_directory->base, data->arg))
            return false;
    }
    
    return true;
}

static bool for_each_segregated_directory_shared_page_directory_callback(
    pas_segregated_shared_page_directory* directory, void* arg)
{
    for_each_segregated_directory_data* data;

    data = arg;

    if (pas_ptr_hash_set_set(&data->seen_shared_page_directories, directory, NULL,
                             &pas_large_utility_free_heap_allocation_config)) {
        /* We added a new shared page directory! */
        
        if (!data->callback(&directory->base, data->arg))
            return false;
    }

    return true;
}

static bool for_each_segregated_directory_segregated_heap_callback(
    pas_segregated_heap* heap, pas_heap_config* config, void* arg)
{
    for_each_segregated_directory_data* data;

    data = arg;

    if (!pas_segregated_heap_for_each_global_size_directory(
            heap, for_each_segregated_directory_global_size_directory_callback, data))
        return false;

    if (!config->for_each_shared_page_directory(
            heap, for_each_segregated_directory_shared_page_directory_callback, data))
        return false;
    
    return true;
}

bool pas_all_heaps_for_each_segregated_directory(
    pas_all_heaps_for_each_segregated_directory_callback callback,
    void* arg)
{
    for_each_segregated_directory_data data;
    bool result;

    pas_heap_lock_assert_held();
    
    pas_ptr_hash_set_construct(&data.seen_shared_page_directories);
    data.callback = callback;
    data.arg = arg;
    
    result = pas_all_heaps_for_each_segregated_heap(
        for_each_segregated_directory_segregated_heap_callback, &data);
    
    pas_ptr_hash_set_destruct(
        &data.seen_shared_page_directories,
        &pas_large_utility_free_heap_allocation_config);

    return result;
}

static void dump_directory_nicely(pas_segregated_directory* directory)
{
    pas_log("Directory %p (%s, %s",
            directory,
            pas_segregated_page_config_kind_get_string(directory->page_config_kind),
            pas_segregated_directory_kind_get_string(directory->directory_kind));
    switch (directory->directory_kind) {
    case pas_segregated_global_size_directory_kind:
        pas_log(", %u", ((pas_segregated_global_size_directory*)directory)->object_size);
        break;
    case pas_segregated_biasing_directory_kind:
        pas_log(", %u",
                pas_compact_segregated_global_size_directory_ptr_load(
                    &((pas_segregated_biasing_directory*)directory)->parent_global)->object_size);
        break;
    case pas_segregated_shared_page_directory_kind:
        break;
    }
    pas_log(")");
}

static void dump_view_nicely(size_t index, pas_segregated_view view)
{
    pas_log(", index %zu, view %p (%s, page boundary %p)",
            index, view, pas_segregated_view_kind_get_string(pas_segregated_view_get_kind(view)),
            pas_segregated_view_get_page_boundary(view));
}

static bool verify_in_steady_state_segregated_directory_callback(
    pas_segregated_directory* directory, void* arg)
{
    size_t index;
    pas_segregated_page_config* page_config;
    
    PAS_UNUSED_PARAM(arg);

    page_config = pas_segregated_page_config_kind_get_config(directory->page_config_kind);

    for (index = pas_segregated_directory_size(directory); index--;) {
        pas_segregated_view view;
        pas_tri_state should_be_eligible;
        bool is_eligible;
        bool is_empty;
        bool is_payload_empty;
        bool is_owned;

        view = pas_segregated_directory_get(directory, index);

        if (!view || !pas_segregated_view_get_ptr(view)) {
            dump_directory_nicely(directory);
            pas_log(", index %zu: got null view %p.\n",
                    index, view);
            PAS_ASSERT(view);
            PAS_ASSERT(pas_segregated_view_get_ptr(view));
        }

        if (pas_segregated_view_get_index(view) != index) {
            dump_directory_nicely(directory);
            dump_view_nicely(index, view);
            pas_log(": expected index %zu but got %zu.\n",
                    index, pas_segregated_view_get_index(view));
            PAS_ASSERT(pas_segregated_view_get_index(view) == index);
        }
        
        should_be_eligible = pas_segregated_view_should_be_eligible(view, page_config);
        is_eligible = pas_segregated_directory_is_eligible(directory, index);
        if (!pas_tri_state_equals_boolean(should_be_eligible, is_eligible)) {
            dump_directory_nicely(directory);
            dump_view_nicely(index, view);
            pas_log(": expected eligibility to be %s, but got %s.\n",
                    pas_tri_state_get_string(should_be_eligible), is_eligible ? "true" : "false");
            PAS_ASSERT(pas_tri_state_equals_boolean(should_be_eligible, is_eligible));
        }

        is_empty = pas_segregated_directory_is_empty(directory, index);
        if (is_empty) {
            dump_directory_nicely(directory);
            dump_view_nicely(index, view);
            pas_log(": didn't expect it to be empty.\n",
                    index, view);
            PAS_ASSERT(!is_empty);
        }
        
        is_payload_empty = pas_segregated_view_is_payload_empty(view);
        is_owned = pas_segregated_view_is_owned(view);
        if (pas_segregated_view_is_partial(view)) {
            if (!is_payload_empty && !is_owned) {
                dump_directory_nicely(directory);
                dump_view_nicely(index, view);
                pas_log(": didn't expect a non-empty payload in a decommitted partial view.\n");
                PAS_ASSERT(is_payload_empty || is_owned);
            }
        } else {
            if (is_payload_empty != !is_owned) {
                dump_directory_nicely(directory);
                dump_view_nicely(index, view);
                pas_log(": bad combination of is_empty_payload (%s) and is_owned (%s).\n",
                        is_payload_empty ? "true" : "false", is_owned ? "true" : "false");
                PAS_ASSERT(is_payload_empty == !is_owned);
            }
        }
    }
    
    return true;
}

void pas_all_heaps_verify_in_steady_state(void)
{
    pas_heap_lock_assert_held();

    /* FIXME: Should add support for verifying other things, like the bitfit heap and the large heap. */

    pas_all_heaps_for_each_segregated_directory(
        verify_in_steady_state_segregated_directory_callback, NULL);
}

static bool compute_total_non_utility_segregated_summary_directory_callback(
    pas_segregated_directory* directory,
    void* arg)
{
    pas_heap_summary* result;
    size_t index;
    pas_segregated_page_config* page_config;

    if (directory->directory_kind == pas_segregated_biasing_directory_kind)
        return true;

    if (!pas_segregated_page_config_kind_is_utility(directory->page_config_kind))
        return true;
    
    page_config = pas_segregated_page_config_kind_get_config(directory->page_config_kind);

    result = arg;

    for (index = pas_segregated_directory_size(directory); index--;) {
        pas_segregated_view view;

        view = pas_segregated_directory_get(directory, index);

        if (pas_segregated_view_is_partial(view))
            continue;

        *result = pas_heap_summary_add(
            *result, pas_segregated_view_compute_summary(view, page_config));
    }

    return true;
}

pas_heap_summary pas_all_heaps_compute_total_non_utility_segregated_summary(void)
{
    /* This is harder than it should be, but that's sort of inherent in the fact that we have
       shared page directories. */

    pas_heap_summary result;

    result = pas_heap_summary_create_empty();

    pas_all_heaps_for_each_segregated_directory(
        compute_total_non_utility_segregated_summary_directory_callback, &result);

    return result;
}

static bool compute_total_non_utility_bitfit_summary_heap_callback(pas_heap* heap, void* arg)
{
    pas_heap_summary* result;
    pas_bitfit_heap* bitfit_heap;

    result = arg;

    bitfit_heap = pas_compact_atomic_bitfit_heap_ptr_load(&heap->segregated_heap.bitfit_heap);
    if (!bitfit_heap)
        return true;

    *result = pas_heap_summary_add(*result, pas_bitfit_heap_compute_summary(bitfit_heap));
    
    return true;
}

pas_heap_summary pas_all_heaps_compute_total_non_utility_bitfit_summary(void)
{
    pas_heap_summary result;

    result = pas_heap_summary_create_empty();

    pas_all_heaps_for_each_heap(
        compute_total_non_utility_bitfit_summary_heap_callback, &result);

    return result;
}

static bool compute_total_non_utility_large_summary_heap_callback(pas_heap* heap, void* arg)
{
    pas_heap_summary* result;

    result = arg;

    *result = pas_heap_summary_add(*result, pas_large_heap_compute_summary(&heap->large_heap));
    
    return true;
}

pas_heap_summary pas_all_heaps_compute_total_non_utility_large_summary(void)
{
    pas_heap_summary result;

    result = pas_heap_summary_create_empty();

    pas_all_heaps_for_each_heap(
        compute_total_non_utility_large_summary_heap_callback, &result);

    return result;
}

pas_heap_summary pas_all_heaps_compute_total_non_utility_summary(void)
{
    return pas_heap_summary_add(
        pas_heap_summary_add(
            pas_all_heaps_compute_total_non_utility_segregated_summary(),
            pas_all_heaps_compute_total_non_utility_bitfit_summary()),
        pas_all_heaps_compute_total_non_utility_large_summary());
}

#endif /* LIBPAS_ENABLED */
