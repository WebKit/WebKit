/*
 * Copyright (c) 2023 Apple Inc. All rights reserved.
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

#ifndef IR_HEAP_CONFIG_H
#define IR_HEAP_CONFIG_H

#include "pas_config.h"

#if PAS_ENABLE_JIT

#include "pas_bitfit_max_free.h"
#include "pas_heap_config_utils.h"
#include "ir_heap_config_root_data.h"

PAS_BEGIN_EXTERN_C;

#define IR_SMALL_SEGREGATED_MIN_ALIGN_SHIFT 4u
#define IR_SMALL_SEGREGATED_MIN_ALIGN (1u << IR_SMALL_SEGREGATED_MIN_ALIGN_SHIFT)
#define IR_SMALL_BITFIT_MIN_ALIGN_SHIFT 2u
#define IR_SMALL_BITFIT_MIN_ALIGN (1u << IR_SMALL_BITFIT_MIN_ALIGN_SHIFT)
#define IR_SMALL_PAGE_SIZE 16384u
#define IR_SMALL_GRANULE_SIZE 16384u
#define IR_MEDIUM_BITFIT_MIN_ALIGN_SHIFT 8u
#define IR_MEDIUM_BITFIT_MIN_ALIGN (1u << IR_MEDIUM_BITFIT_MIN_ALIGN_SHIFT)
#define IR_MEDIUM_PAGE_SIZE 131072u
#if PAS_ARM64
#define IR_MEDIUM_GRANULE_SIZE 16384u
#else
#define IR_MEDIUM_GRANULE_SIZE 4096u
#endif

PAS_API void ir_heap_config_activate(void);

static PAS_ALWAYS_INLINE size_t ir_type_size(const pas_heap_type* type)
{
    PAS_TESTING_ASSERT(!type);
    return 1;
}
static PAS_ALWAYS_INLINE size_t ir_type_alignment(const pas_heap_type* type)
{
    PAS_TESTING_ASSERT(!type);
    return 1;
}

PAS_API void ir_type_dump(const pas_heap_type* type, pas_stream* stream);

PAS_API pas_page_base* ir_page_header_for_boundary_remote(pas_enumerator* enumerator, void* boundary);

static PAS_ALWAYS_INLINE pas_page_base* ir_small_page_header_for_boundary(void* boundary);
static PAS_ALWAYS_INLINE void* ir_small_boundary_for_page_header(pas_page_base* page);
PAS_API void* ir_small_segregated_allocate_page(
    pas_segregated_heap* heap, pas_physical_memory_transaction* transaction, pas_segregated_page_role role);
PAS_API pas_page_base* ir_small_segregated_create_page_header(
    void* boundary, pas_page_kind kind, pas_lock_hold_mode heap_lock_hold_mode);
PAS_API void ir_small_destroy_page_header(
    pas_page_base* page, pas_lock_hold_mode heap_lock_hold_mode);
PAS_API pas_segregated_shared_page_directory* ir_small_segregated_shared_page_directory_selector(
    pas_segregated_heap* heap, pas_segregated_size_directory* directory);

PAS_SEGREGATED_PAGE_CONFIG_SPECIALIZATION_DECLARATIONS(ir_small_segregated_page_config);

PAS_API void* ir_small_bitfit_allocate_page(
    pas_segregated_heap* heap, pas_physical_memory_transaction* transaction);
PAS_API pas_page_base* ir_small_bitfit_create_page_header(
    void* boundary, pas_page_kind kind, pas_lock_hold_mode heap_lock_hold_mode);

PAS_BITFIT_PAGE_CONFIG_SPECIALIZATION_DECLARATIONS(ir_small_bitfit_page_config);

static PAS_ALWAYS_INLINE pas_page_base* ir_medium_page_header_for_boundary(void* boundary);
static PAS_ALWAYS_INLINE void* ir_medium_boundary_for_page_header(pas_page_base* page);
PAS_API void* ir_medium_bitfit_allocate_page(
    pas_segregated_heap* heap, pas_physical_memory_transaction* transaction);
PAS_API pas_page_base* ir_medium_bitfit_create_page_header(
    void* boundary, pas_page_kind kind, pas_lock_hold_mode heap_lock_hold_mode);
PAS_API void ir_medium_destroy_page_header(
    pas_page_base* page, pas_lock_hold_mode heap_lock_hold_mode);

PAS_BITFIT_PAGE_CONFIG_SPECIALIZATION_DECLARATIONS(ir_medium_bitfit_page_config);

static PAS_ALWAYS_INLINE pas_fast_megapage_kind
ir_heap_config_fast_megapage_kind(uintptr_t begin)
{
    PAS_UNUSED_PARAM(begin);
    return pas_not_a_fast_megapage_kind;
}

static PAS_ALWAYS_INLINE pas_page_base* ir_heap_config_page_header(uintptr_t begin);
PAS_API pas_aligned_allocation_result ir_aligned_allocator(
    size_t size, pas_alignment alignment, pas_large_heap* large_heap, const pas_heap_config* config);
PAS_API void* ir_prepare_to_enumerate(pas_enumerator* enumerator);
PAS_API bool ir_heap_config_for_each_shared_page_directory(
    pas_segregated_heap* heap,
    bool (*callback)(pas_segregated_shared_page_directory* directory,
                     void* arg),
    void* arg);
PAS_API bool ir_heap_config_for_each_shared_page_directory_remote(
    pas_enumerator* enumerator,
    pas_segregated_heap* heap,
    bool (*callback)(pas_enumerator* enumerator,
                     pas_segregated_shared_page_directory* directory,
                     void* arg),
    void* arg);
PAS_API void ir_heap_config_dump_shared_page_directory_arg(
    pas_stream* stream, pas_segregated_shared_page_directory* directory);

PAS_HEAP_CONFIG_SPECIALIZATION_DECLARATIONS(ir_heap_config);

#define IR_BITFIT_PAGE_CONFIG(variant_lowercase, variant_uppercase) { \
        .base = { \
            .is_enabled = true, \
            .heap_config_ptr = &ir_heap_config, \
            .page_config_ptr = &ir_heap_config.variant_lowercase ## _bitfit_config.base, \
            .page_config_kind = pas_page_config_kind_bitfit, \
            .min_align_shift = IR_ ## variant_uppercase ## _BITFIT_MIN_ALIGN_SHIFT, \
            .page_size = IR_ ## variant_uppercase ## _PAGE_SIZE, \
            .granule_size = IR_ ## variant_uppercase ## _GRANULE_SIZE, \
            .max_object_size = \
                PAS_BITFIT_MAX_FREE_MAX_VALID << IR_ ## variant_uppercase ## _BITFIT_MIN_ALIGN_SHIFT, \
            .page_header_for_boundary = ir_ ## variant_lowercase ## _page_header_for_boundary, \
            .boundary_for_page_header = ir_ ## variant_lowercase ## _boundary_for_page_header, \
            .page_header_for_boundary_remote = ir_page_header_for_boundary_remote, \
            .create_page_header = ir_ ## variant_lowercase ## _bitfit_create_page_header, \
            .destroy_page_header = ir_ ## variant_lowercase ## _destroy_page_header \
        }, \
        .variant = pas_ ## variant_lowercase ## _bitfit_page_config_variant, \
        .kind = pas_bitfit_page_config_kind_ir_ ## variant_lowercase ## _bitfit, \
        .page_object_payload_offset = 0, \
        .page_object_payload_size = IR_ ## variant_uppercase ## _PAGE_SIZE, \
        .page_allocator = ir_ ## variant_lowercase ## _bitfit_allocate_page, \
        PAS_BITFIT_PAGE_CONFIG_SPECIALIZATIONS(ir_ ## variant_lowercase ## _bitfit_page_config) \
    }

#define IR_HEAP_CONFIG ((pas_heap_config){ \
        .config_ptr = &ir_heap_config, \
        .kind = pas_heap_config_kind_ir, \
        .activate_callback = ir_heap_config_activate, \
        .get_type_size = ir_type_size, \
        .get_type_alignment = ir_type_alignment, \
        .dump_type = ir_type_dump, \
        .large_alignment = PAS_MIN_CONST(IR_SMALL_SEGREGATED_MIN_ALIGN, IR_SMALL_BITFIT_MIN_ALIGN), \
        .small_segregated_config = { \
            .base = { \
                .is_enabled = true, \
                .heap_config_ptr = &ir_heap_config, \
                .page_config_ptr = &ir_heap_config.small_segregated_config.base, \
                .page_config_kind = pas_page_config_kind_segregated, \
                .min_align_shift = IR_SMALL_SEGREGATED_MIN_ALIGN_SHIFT, \
                .page_size = IR_SMALL_PAGE_SIZE, \
                .granule_size = IR_SMALL_GRANULE_SIZE, \
                .max_object_size = PAS_MAX_OBJECT_SIZE(IR_SMALL_PAGE_SIZE), \
                .page_header_for_boundary = ir_small_page_header_for_boundary, \
                .boundary_for_page_header = ir_small_boundary_for_page_header, \
                .page_header_for_boundary_remote = ir_page_header_for_boundary_remote, \
                .create_page_header = ir_small_segregated_create_page_header, \
                .destroy_page_header = ir_small_destroy_page_header \
            }, \
            .variant = pas_small_segregated_page_config_variant, \
            .kind = pas_segregated_page_config_kind_ir_small_segregated, \
            .wasteage_handicap = 1., \
            .sharing_shift = PAS_SMALL_SHARING_SHIFT, \
            .num_alloc_bits = PAS_BASIC_SEGREGATED_NUM_ALLOC_BITS(IR_SMALL_SEGREGATED_MIN_ALIGN_SHIFT, \
                                                                  IR_SMALL_PAGE_SIZE), \
            .shared_payload_offset = 0, \
            .exclusive_payload_offset = 0, \
            .shared_payload_size = 0, \
            .exclusive_payload_size = IR_SMALL_PAGE_SIZE, \
            .shared_logging_mode = pas_segregated_deallocation_no_logging_mode, \
            .exclusive_logging_mode = pas_segregated_deallocation_size_oblivious_logging_mode, \
            .use_reversed_current_word = PAS_ARM64, \
            .check_deallocation = false, \
            .enable_empty_word_eligibility_optimization_for_shared = false, \
            .enable_empty_word_eligibility_optimization_for_exclusive = true, \
            .enable_view_cache = true, \
            .page_allocator = ir_small_segregated_allocate_page, \
            .shared_page_directory_selector = ir_small_segregated_shared_page_directory_selector, \
            PAS_SEGREGATED_PAGE_CONFIG_SPECIALIZATIONS(ir_small_segregated_page_config) \
        }, \
        .medium_segregated_config = { \
            .base = { \
                .is_enabled = false \
            } \
        }, \
        .small_bitfit_config = IR_BITFIT_PAGE_CONFIG(small, SMALL), \
        .medium_bitfit_config = IR_BITFIT_PAGE_CONFIG(medium, MEDIUM), \
        .marge_bitfit_config = { \
            .base = { \
                .is_enabled = false \
            } \
        }, \
        .small_lookup_size_upper_bound = PAS_SMALL_LOOKUP_SIZE_UPPER_BOUND, \
        .fast_megapage_kind_func = ir_heap_config_fast_megapage_kind, \
        .small_segregated_is_in_megapage = false, \
        .small_bitfit_is_in_megapage = false, \
        .page_header_func = ir_heap_config_page_header, \
        .aligned_allocator = ir_aligned_allocator, \
        .aligned_allocator_talks_to_sharing_pool = true, \
        .deallocator = NULL, \
        .mmap_capability = pas_may_not_mmap, \
        .root_data = &ir_root_data, \
        .prepare_to_enumerate = ir_prepare_to_enumerate, \
        .for_each_shared_page_directory = ir_heap_config_for_each_shared_page_directory, \
        .for_each_shared_page_directory_remote = \
            ir_heap_config_for_each_shared_page_directory_remote, \
        .dump_shared_page_directory_arg = ir_heap_config_dump_shared_page_directory_arg, \
        PAS_HEAP_CONFIG_SPECIALIZATIONS(ir_heap_config) \
    })

PAS_API extern const pas_heap_config ir_heap_config;

/* Like the JIT heap, the IR heap manages fresh memory given to it by clients,
   and doesn't map memory on its own. */
PAS_API extern pas_simple_large_free_heap ir_fresh_memory_heap;

PAS_API extern pas_large_heap_physical_page_sharing_cache ir_large_fresh_memory_heap;
PAS_API extern pas_page_header_table ir_small_page_header_table;
PAS_API extern pas_page_header_table ir_medium_page_header_table;
PAS_API extern pas_heap_runtime_config ir_heap_runtime_config;
PAS_API extern ir_heap_config_root_data ir_root_data;

static PAS_ALWAYS_INLINE pas_page_base* ir_small_page_header_for_boundary(void* boundary)
{
    return pas_page_header_table_get_for_boundary(
        &ir_small_page_header_table, IR_SMALL_PAGE_SIZE, boundary);
}

static PAS_ALWAYS_INLINE void* ir_small_boundary_for_page_header(pas_page_base* page)
{
    return pas_page_header_table_get_boundary(
        &ir_small_page_header_table, IR_SMALL_PAGE_SIZE, page);
}

static PAS_ALWAYS_INLINE pas_page_base* ir_medium_page_header_for_boundary(void* boundary)
{
    return pas_page_header_table_get_for_boundary(
        &ir_medium_page_header_table, IR_MEDIUM_PAGE_SIZE, boundary);
}

static PAS_ALWAYS_INLINE void* ir_medium_boundary_for_page_header(pas_page_base* page)
{
    return pas_page_header_table_get_boundary(
        &ir_medium_page_header_table, IR_MEDIUM_PAGE_SIZE, page);
}

static PAS_ALWAYS_INLINE pas_page_base* ir_heap_config_page_header(uintptr_t begin)
{
    pas_page_base* result;

    result = pas_page_header_table_get_for_address(
        &ir_small_page_header_table, IR_SMALL_PAGE_SIZE, (void*)begin);
    if (result)
        return result;

    return pas_page_header_table_get_for_address(
        &ir_medium_page_header_table, IR_MEDIUM_PAGE_SIZE, (void*)begin);
}

PAS_API void ir_heap_config_add_fresh_memory(pas_range range);

PAS_END_EXTERN_C;

#endif /* PAS_ENABLE_JIT */

#endif /* IR_HEAP_CONFIG_H */

