/*
 * Copyright (c) 2021 Apple Inc. All rights reserved.
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

#include "jit_heap_config.h"

#if PAS_ENABLE_JIT

#include "bmalloc_heap_config.h"
#include "pas_basic_heap_config_enumerator_data.h"
#include "pas_bitfit_page_config_inlines.h"
#include "pas_heap_config_inlines.h"
#include "pas_root.h"

pas_heap_config jit_heap_config = JIT_HEAP_CONFIG;

pas_simple_large_free_heap jit_fresh_memory_heap = PAS_SIMPLE_LARGE_FREE_HEAP_INITIALIZER;

static pas_aligned_allocation_result fresh_memory_aligned_allocator(size_t size,
                                                                    pas_alignment alignment,
                                                                    void *arg)
{
    PAS_UNUSED_PARAM(size);
    PAS_UNUSED_PARAM(alignment);
    PAS_ASSERT(!arg);
    return pas_aligned_allocation_result_create_empty();
}

static void initialize_fresh_memory_config(pas_large_free_heap_config* config)
{
    config->type_size = 1;
    config->min_alignment = 1;
    config->aligned_allocator = fresh_memory_aligned_allocator;
    config->aligned_allocator_arg = NULL;
    config->deallocator = NULL;
    config->deallocator_arg = NULL;
}

static pas_allocation_result allocate_from_fresh(size_t size, pas_alignment alignment)
{
    pas_large_free_heap_config config;

    pas_heap_lock_assert_held();

    initialize_fresh_memory_config(&config);
    return pas_simple_large_free_heap_try_allocate(&jit_fresh_memory_heap, size, alignment, &config);
}

static pas_allocation_result page_provider(
    size_t size, pas_alignment alignment, const char* name, void* arg)
{
    PAS_UNUSED_PARAM(name);
    PAS_ASSERT(!arg);
    return allocate_from_fresh(size, alignment);
}

pas_large_heap_physical_page_sharing_cache jit_large_fresh_memory_heap = {
    .free_heap = PAS_SIMPLE_LARGE_FREE_HEAP_INITIALIZER,
    .provider = page_provider,
    .provider_arg = NULL
};

pas_page_header_table jit_small_bitfit_page_header_table =
    PAS_PAGE_HEADER_TABLE_INITIALIZER(JIT_SMALL_PAGE_SIZE);
pas_page_header_table jit_medium_bitfit_page_header_table =
    PAS_PAGE_HEADER_TABLE_INITIALIZER(JIT_MEDIUM_PAGE_SIZE);

pas_heap_runtime_config jit_heap_runtime_config = {
    .lookup_kind = pas_segregated_heap_lookup_primitive,
    .sharing_mode = pas_share_pages,
    .statically_allocated = true,
    .is_part_of_heap = true,
    .directory_size_bound_for_partial_views = 0,
    .directory_size_bound_for_baseline_allocators = 0,
    .max_segregated_object_size = 0,
    .max_bitfit_object_size = UINT_MAX
};

jit_heap_config_root_data jit_root_data = {
    .small_bitfit_page_header_table = &jit_small_bitfit_page_header_table,
    .medium_bitfit_page_header_table = &jit_medium_bitfit_page_header_table
};

void jit_heap_config_activate(void)
{
    /* Make sure that the bmalloc heap config initializes before we do anything else, since that
       one will want to be designated. */
    pas_heap_config_activate(&bmalloc_heap_config);
}

pas_page_base* jit_page_header_for_boundary_remote(pas_enumerator* enumerator, void* boundary)
{
    pas_basic_heap_config_enumerator_data* data;
    
    data = enumerator->heap_config_datas[pas_heap_config_kind_jit];
    PAS_ASSERT(data);
    
    return pas_ptr_hash_map_get(&data->page_header_table, boundary).value;
}

void* jit_small_bitfit_allocate_page(pas_segregated_heap* heap)
{
    PAS_UNUSED_PARAM(heap);
    return (void*)allocate_from_fresh(
        JIT_SMALL_PAGE_SIZE, pas_alignment_create_traditional(JIT_SMALL_PAGE_SIZE)).begin;
}

pas_page_base* jit_small_bitfit_create_page_header(
    void* boundary, pas_lock_hold_mode heap_lock_hold_mode)
{
    pas_page_base* result;
    pas_heap_lock_lock_conditionally(heap_lock_hold_mode);
    result = pas_page_header_table_add(&jit_small_bitfit_page_header_table,
                                       JIT_SMALL_PAGE_SIZE,
                                       JIT_HEAP_CONFIG.small_bitfit_config.base.page_header_size,
                                       boundary);
    pas_heap_lock_unlock_conditionally(heap_lock_hold_mode);
    return result;
}

void jit_small_bitfit_destroy_page_header(
    pas_page_base* page, pas_lock_hold_mode heap_lock_hold_mode)
{
    pas_heap_lock_lock_conditionally(heap_lock_hold_mode);
    pas_page_header_table_remove(&jit_small_bitfit_page_header_table,
                                 JIT_SMALL_PAGE_SIZE,
                                 page);
    pas_heap_lock_unlock_conditionally(heap_lock_hold_mode);
}

void* jit_medium_bitfit_allocate_page(pas_segregated_heap* heap)
{
    PAS_UNUSED_PARAM(heap);
    return (void*)allocate_from_fresh(
        JIT_MEDIUM_PAGE_SIZE, pas_alignment_create_traditional(JIT_MEDIUM_PAGE_SIZE)).begin;
}

pas_page_base* jit_medium_bitfit_create_page_header(
    void* boundary, pas_lock_hold_mode heap_lock_hold_mode)
{
    pas_page_base* result;
    pas_heap_lock_lock_conditionally(heap_lock_hold_mode);
    result = pas_page_header_table_add(&jit_medium_bitfit_page_header_table,
                                       JIT_MEDIUM_PAGE_SIZE,
                                       JIT_HEAP_CONFIG.medium_bitfit_config.base.page_header_size,
                                       boundary);
    pas_heap_lock_unlock_conditionally(heap_lock_hold_mode);
    return result;
}

void jit_medium_bitfit_destroy_page_header(
    pas_page_base* page, pas_lock_hold_mode heap_lock_hold_mode)
{
    pas_heap_lock_lock_conditionally(heap_lock_hold_mode);
    pas_page_header_table_remove(&jit_medium_bitfit_page_header_table,
                                 JIT_MEDIUM_PAGE_SIZE,
                                 page);
    pas_heap_lock_unlock_conditionally(heap_lock_hold_mode);
}

pas_aligned_allocation_result jit_aligned_allocator(
    size_t size, pas_alignment alignment, pas_large_heap* large_heap, pas_heap_config* config)
{
    pas_aligned_allocation_result result;
    size_t aligned_size;
    pas_allocation_result allocation_result;

    PAS_UNUSED_PARAM(large_heap);
    PAS_UNUSED_PARAM(config);
    
    pas_zero_memory(&result, sizeof(result));

    aligned_size = pas_round_up_to_power_of_2(size, alignment.alignment);

    allocation_result = pas_large_heap_physical_page_sharing_cache_try_allocate_with_alignment(
        &jit_large_fresh_memory_heap, aligned_size, alignment, false);
    if (!allocation_result.did_succeed)
        return result;

    result.result = (void*)allocation_result.begin;
    result.result_size = size;
    result.left_padding = (void*)allocation_result.begin;
    result.left_padding_size = 0;
    result.right_padding = (char*)(void*)allocation_result.begin + size;
    result.right_padding_size = aligned_size - size;
    result.zero_mode = allocation_result.zero_mode;

    return result;
}

void* jit_prepare_to_enumerate(pas_enumerator* enumerator)
{
    pas_basic_heap_config_enumerator_data* result;
    pas_heap_config** configs;
    pas_heap_config* config;
    jit_heap_config_root_data* root_data;

    configs = pas_enumerator_read(
        enumerator, enumerator->root->heap_configs,
        sizeof(pas_heap_config*) * pas_heap_config_kind_num_kinds);
    if (!configs)
        return NULL;
    
    config = pas_enumerator_read(
        enumerator, configs[pas_heap_config_kind_jit], sizeof(pas_heap_config));
    if (!config)
        return NULL;

    root_data = pas_enumerator_read(
        enumerator, config->root_data, sizeof(jit_heap_config_root_data));
    if (!root_data)
        return NULL;

    result = pas_enumerator_allocate(enumerator, sizeof(pas_basic_heap_config_enumerator_data));
    
    pas_ptr_hash_map_construct(&result->page_header_table);

    if (!pas_basic_heap_config_enumerator_data_add_page_header_table(
            result,
            enumerator,
            pas_enumerator_read(
                enumerator, root_data->small_bitfit_page_header_table, sizeof(pas_page_header_table))))
        return NULL;
    
    if (!pas_basic_heap_config_enumerator_data_add_page_header_table(
            result,
            enumerator,
            pas_enumerator_read(
                enumerator, root_data->medium_bitfit_page_header_table, sizeof(pas_page_header_table))))
        return NULL;
    
    return result;
}

bool jit_heap_config_for_each_shared_page_directory(
    pas_segregated_heap* heap,
    bool (*callback)(pas_segregated_shared_page_directory* directory,
                     void* arg),
    void* arg)
{
    PAS_UNUSED_PARAM(heap);
    PAS_UNUSED_PARAM(callback);
    PAS_UNUSED_PARAM(arg);
    return true;
}

bool jit_heap_config_for_each_shared_page_directory_remote(
    pas_enumerator* enumerator,
    pas_segregated_heap* heap,
    bool (*callback)(pas_enumerator* enumerator,
                     pas_segregated_shared_page_directory* directory,
                     void* arg),
    void* arg)
{
    PAS_UNUSED_PARAM(enumerator);
    PAS_UNUSED_PARAM(heap);
    PAS_UNUSED_PARAM(callback);
    PAS_UNUSED_PARAM(arg);
    return true;
}

void jit_heap_config_add_fresh_memory(pas_range range)
{
    pas_large_free_heap_config config;

    pas_heap_lock_assert_held();
    initialize_fresh_memory_config(&config);
    pas_simple_large_free_heap_deallocate(
        &jit_fresh_memory_heap, range.begin, range.end, pas_zero_mode_is_all_zero, &config);
}

PAS_BITFIT_PAGE_CONFIG_SPECIALIZATION_DEFINITIONS(
    jit_small_bitfit_page_config, JIT_HEAP_CONFIG.small_bitfit_config);
PAS_BITFIT_PAGE_CONFIG_SPECIALIZATION_DEFINITIONS(
    jit_medium_bitfit_page_config, JIT_HEAP_CONFIG.medium_bitfit_config);
PAS_HEAP_CONFIG_SPECIALIZATION_DEFINITIONS(
    jit_heap_config, JIT_HEAP_CONFIG);

#endif /* PAS_ENABLE_JIT */

#endif /* LIBPAS_ENABLED */
