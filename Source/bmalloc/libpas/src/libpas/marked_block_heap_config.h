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

#ifndef MARKED_BLOCK_HEAP_CONFIG_H
#define MARKED_BLOCK_HEAP_CONFIG_H

#include "pas_config.h"

#if LIBPAS_ENABLED

#if PAS_ENABLE_MARKED_BLOCK

#include "marked_block_heap.h"
#include "pas_allocator_counts.h"
#include "pas_basic_heap_runtime_config.h"
#include "pas_heap_config.h"
#include "pas_page_header_table.h"
#include "pas_segregated_page_config_utils.h"
#include "pas_simple_type.h"

PAS_BEGIN_EXTERN_C;

/* The heap config that serves MARKED_BLOCK_SIZE. See marked_block_heap.h for what the heap is
   and why it exists. Every allocation is one block, so the config has exactly one page config,
   medium segregated, whose minimum alignment is the block size. */

/* How many blocks a medium page holds. pas_segregated_page_deallocate_impl indexes a page's alloc
   bits by word, computing the word count as page_size >> (min_align_shift + 5) and requiring it to
   be a power of two, so with the minimum alignment at the block size a page needs at least 32
   blocks; 64 leaves a spare alloc word above that. Sizes from 16 to 128 blocks measure the same on
   JetStream3, in both throughput and footprint. */
#define MARKED_BLOCK_BLOCKS_PER_PAGE 64

#define MARKED_BLOCK_PAGE_SIZE (MARKED_BLOCK_BLOCKS_PER_PAGE * MARKED_BLOCK_SIZE)

struct marked_block_heap_config_root_data;
typedef struct marked_block_heap_config_root_data marked_block_heap_config_root_data;

/* If the layout of this structure is changed, the value of
 * pas_crash_report_version in pas_root.h should be incremented.
 */
struct marked_block_heap_config_root_data {
    pas_page_header_table* medium_page_header_table;
};

PAS_API void marked_block_heap_config_activate(void);

PAS_BASIC_SEGREGATED_PAGE_CONFIG_FORWARD_DECLARATIONS(marked_block_medium_segregated);

PAS_API void* marked_block_medium_segregated_allocate_page(
    pas_segregated_heap* heap, pas_physical_memory_transaction* transaction);

static PAS_ALWAYS_INLINE pas_fast_megapage_kind
marked_block_heap_config_fast_megapage_kind(uintptr_t begin)
{
    PAS_UNUSED_PARAM(begin);
    return pas_not_a_fast_megapage_kind;
}

static PAS_ALWAYS_INLINE pas_page_base* marked_block_heap_config_page_header(uintptr_t begin);
PAS_API pas_aligned_allocation_result marked_block_aligned_allocator(
    size_t size, pas_alignment alignment, pas_large_heap* large_heap, const pas_heap_config* config);
PAS_API void* marked_block_prepare_to_enumerate(pas_enumerator* enumerator);

PAS_HEAP_CONFIG_SPECIALIZATION_DECLARATIONS(marked_block_heap_config);

#define MARKED_BLOCK_HEAP_CONFIG ((pas_heap_config){ \
        .config_ptr = &marked_block_heap_config, \
        .kind = pas_heap_config_kind_marked_block, \
        .activate_callback = marked_block_heap_config_activate, \
        .get_type_size = pas_simple_type_as_heap_type_get_type_size, \
        .get_type_alignment = pas_simple_type_as_heap_type_get_type_alignment, \
        .dump_type = pas_simple_type_as_heap_type_dump, \
        /* Nothing reaches the large heap, since every allocation is one block. */ \
        .large_alignment = PAS_MIN_ALIGN, \
        .small_segregated_config = { \
            .base = { \
                .is_enabled = false \
            } \
        }, \
        .medium_segregated_config = { \
            .base = { \
                .is_enabled = true, \
                .allow_profiling = PAS_SHOULD_PROFILE_BASIC_HEAP_PAGE(pas_page_config_size_category_medium), \
                .allow_mte_tagging = false, \
                .heap_config_ptr = &marked_block_heap_config, \
                .page_config_ptr = &marked_block_heap_config.medium_segregated_config.base, \
                .page_config_kind = pas_page_config_kind_segregated, \
                .page_config_size_category = pas_page_config_size_category_medium, \
                .min_align_shift = MARKED_BLOCK_SHIFT, \
                .page_size = MARKED_BLOCK_PAGE_SIZE, \
                .granule_size = MARKED_BLOCK_SIZE, \
                .max_object_size = MARKED_BLOCK_SIZE, \
                .page_header_for_boundary = marked_block_medium_segregated_page_header_for_boundary, \
                .boundary_for_page_header = marked_block_medium_segregated_boundary_for_page_header, \
                .page_header_for_boundary_remote = \
                    marked_block_medium_segregated_page_header_for_boundary_remote, \
                .create_page_header = marked_block_medium_segregated_create_page_header, \
                .destroy_page_header = marked_block_medium_segregated_destroy_page_header \
            }, \
            .variant = pas_medium_segregated_page_config_variant, \
            .kind = pas_segregated_page_config_kind_marked_block_medium_segregated, \
            .wasteage_handicap = PAS_MEDIUM_PAGE_HANDICAP, \
            .sharing_shift = PAS_MEDIUM_SHARING_SHIFT, \
            .num_alloc_bits = PAS_BASIC_SEGREGATED_NUM_ALLOC_BITS(MARKED_BLOCK_SHIFT, \
                                                                  MARKED_BLOCK_PAGE_SIZE), \
            .exclusive_payload_offset = 0, \
            .exclusive_payload_size = MARKED_BLOCK_PAGE_SIZE, \
            .exclusive_logging_mode = pas_segregated_deallocation_size_oblivious_logging_mode, \
            .use_reversed_current_word = false, \
            .check_deallocation = true, \
            .enable_empty_word_eligibility_optimization_for_exclusive = false, \
            .enable_view_cache = false, \
            .page_allocator = marked_block_medium_segregated_allocate_page, \
            PAS_SEGREGATED_PAGE_CONFIG_SPECIALIZATIONS(marked_block_medium_segregated_page_config) \
        }, \
        .small_bitfit_config = { \
            .base = { \
                .is_enabled = false \
            } \
        }, \
        .medium_bitfit_config = { \
            .base = { \
                .is_enabled = false \
            } \
        }, \
        .marge_bitfit_config = { \
            .base = { \
                .is_enabled = false \
            } \
        }, \
        .small_lookup_size_upper_bound = PAS_SMALL_LOOKUP_SIZE_UPPER_BOUND, \
        .fast_megapage_kind_func = marked_block_heap_config_fast_megapage_kind, \
        .small_segregated_is_in_megapage = false, \
        .small_bitfit_is_in_megapage = false, \
        .page_header_func = marked_block_heap_config_page_header, \
        .aligned_allocator = marked_block_aligned_allocator, \
        .aligned_allocator_talks_to_sharing_pool = true, \
        .deallocator = NULL, \
        .memory_flavor = pas_memory_flavor_plain, \
        .root_data = &marked_block_root_data, \
        .prepare_to_enumerate = marked_block_prepare_to_enumerate, \
        PAS_HEAP_CONFIG_SPECIALIZATIONS(marked_block_heap_config), \
        .pgm_enabled = false, \
        .delegate_large_user_allocations = false, \
        .large_map_variant = pas_default_large_map_variant, \
    })

PAS_API extern const pas_heap_config marked_block_heap_config;

PAS_API extern pas_page_header_table marked_block_medium_page_header_table;
PAS_API extern pas_basic_heap_runtime_config marked_block_primitive_runtime_config;
PAS_API extern marked_block_heap_config_root_data marked_block_root_data;

/* Untyped, because the block size is conveyed as the allocation size rather than as a type, and the
   size is a constant at every call site anyway. Being primitive is also what lets
   marked_block_heap_force_into_reserved_memory give a heap its own page caches. */
PAS_API extern pas_primitive_heap_ref marked_block_primitive_heap_ref;
PAS_API extern pas_allocator_counts marked_block_allocator_counts;

PAS_BASIC_SEGREGATED_PAGE_CONFIG_DECLARATIONS(
    marked_block_medium_segregated, MARKED_BLOCK_HEAP_CONFIG.medium_segregated_config,
    pas_page_header_in_table,
    &marked_block_medium_page_header_table);

static PAS_ALWAYS_INLINE pas_page_base* marked_block_heap_config_page_header(uintptr_t begin)
{
    return pas_page_header_table_get_for_address(
        &marked_block_medium_page_header_table, MARKED_BLOCK_PAGE_SIZE, (void*)begin);
}

PAS_END_EXTERN_C;

#endif /* PAS_ENABLE_MARKED_BLOCK */

#endif /* LIBPAS_ENABLED */
#endif /* MARKED_BLOCK_HEAP_CONFIG_H */
