/*
 * Copyright (c) 2026 Apple Inc. All rights reserved.
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

#include "marked_block_heap_config.h"

#if PAS_ENABLE_MARKED_BLOCK

#include "bmalloc_heap_config.h"
#include "pas_basic_heap_config_enumerator_data.h"
#include "pas_enumerator.h"
#include "pas_heap_config_utils_inlines.h"
#include "pas_medium_megapage_cache.h"
#include "pas_root.h"

PAS_BEGIN_EXTERN_C;

const pas_heap_config marked_block_heap_config = MARKED_BLOCK_HEAP_CONFIG;

pas_page_header_table marked_block_medium_page_header_table =
    PAS_PAGE_HEADER_TABLE_INITIALIZER(MARKED_BLOCK_PAGE_SIZE);

marked_block_heap_config_root_data marked_block_root_data = {
    .medium_page_header_table = &marked_block_medium_page_header_table
};

static pas_basic_heap_page_caches marked_block_page_caches =
    PAS_BASIC_HEAP_PAGE_CACHES_INITIALIZER(MARKED_BLOCK_HEAP_CONFIG.memory_flavor);

/* A pas_basic_heap_runtime_config rather than a plain one, because that is the template
   pas_ensure_heap_forced_into_reserved_memory needs to give a heap its own page caches. */
pas_basic_heap_runtime_config marked_block_primitive_runtime_config = {
    .base = {
        .lookup_kind = pas_segregated_heap_lookup_primitive,
        .sharing_mode = pas_share_pages,
        .statically_allocated = false,
        .is_part_of_heap = true,
        .directory_size_bound_for_baseline_allocators = PAS_PRIMITIVE_BOUND_FOR_BASELINE_ALLOCATORS,
        .directory_size_bound_for_no_view_cache = PAS_PRIMITIVE_BOUND_FOR_NO_VIEW_CACHE,
        .max_segregated_object_size = PAS_PRIMITIVE_MAX_SEGREGATED_OBJECT_SIZE,
        .max_bitfit_object_size = 0,
        .view_cache_capacity_for_object_size = pas_heap_runtime_config_zero_view_cache_capacity,
    },
    .page_caches = &marked_block_page_caches
};

/* bmalloc reserves its designated allocator indices when it activates, which only works before any
   thread local cache exists. Activate it first so this heap cannot be the one that creates one. */
void marked_block_heap_config_activate(void)
{
    pas_heap_config_activate(&bmalloc_heap_config);
}

void* marked_block_medium_segregated_allocate_page(
    pas_segregated_heap* heap, pas_physical_memory_transaction* transaction)
{
    pas_megapage_cache* megapage_cache;

    megapage_cache = &((pas_basic_heap_runtime_config*)heap->runtime_config)->page_caches->medium_megapage_cache;

    PAS_PROFILE(MEDIUM_SEGREGATED_PAGE_ALLOCATION, heap, megapage_cache);
    return pas_medium_megapage_cache_try_allocate(
        megapage_cache,
        &marked_block_heap_config.medium_segregated_config.base,
        false,
        pas_heap_for_segregated_heap(heap),
        transaction);
}

pas_aligned_allocation_result marked_block_aligned_allocator(
    size_t size, pas_alignment alignment, pas_large_heap* large_heap, const pas_heap_config* config)
{
    return pas_heap_config_utils_allocate_aligned(size, alignment, large_heap, config, false);
}

void* marked_block_prepare_to_enumerate(pas_enumerator* enumerator)
{
    pas_basic_heap_config_enumerator_data* result;

    pas_heap_config* config_ptr;
    marked_block_heap_config_root_data* root_data_ptr;
    marked_block_heap_config_root_data root_data;
    pas_page_header_table medium_page_header_table;

    if (!pas_enumerator_copy_remote(
            enumerator, &config_ptr, enumerator->root->heap_configs + pas_heap_config_kind_marked_block,
            sizeof(pas_heap_config*)))
        return NULL;

    if (!pas_enumerator_copy_remote(
            enumerator, &root_data_ptr, &config_ptr->root_data, sizeof(marked_block_heap_config_root_data*)))
        return NULL;

    if (!pas_enumerator_copy_remote(
            enumerator, &root_data, root_data_ptr, sizeof(marked_block_heap_config_root_data)))
        return NULL;

    if (!pas_enumerator_copy_remote(
            enumerator, &medium_page_header_table, root_data.medium_page_header_table,
            sizeof(pas_page_header_table)))
        return NULL;

    result = (pas_basic_heap_config_enumerator_data*)pas_enumerator_allocate(
        enumerator, sizeof(pas_basic_heap_config_enumerator_data));

    pas_ptr_hash_map_construct(&result->page_header_table);

    if (!pas_basic_heap_config_enumerator_data_add_page_header_table(
            result,
            enumerator,
            &medium_page_header_table))
        return NULL;

    return result;
}

PAS_BASIC_SEGREGATED_PAGE_CONFIG_DEFINITIONS(
    marked_block_medium_segregated,
    .page_config = MARKED_BLOCK_HEAP_CONFIG.medium_segregated_config,
    .header_table = &marked_block_medium_page_header_table);

PAS_HEAP_CONFIG_SPECIALIZATION_DEFINITIONS(
    marked_block_heap_config, MARKED_BLOCK_HEAP_CONFIG);

PAS_END_EXTERN_C;

#endif /* PAS_ENABLE_MARKED_BLOCK */

#endif /* LIBPAS_ENABLED */
