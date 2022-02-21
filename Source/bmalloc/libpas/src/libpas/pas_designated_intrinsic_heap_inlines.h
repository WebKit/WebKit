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

#ifndef PAS_DESIGNATED_INTRINSIC_HEAP_INLINES_H
#define PAS_DESIGNATED_INTRINSIC_HEAP_INLINES_H

#include "pas_allocator_index.h"
#include "pas_designated_intrinsic_heap.h"
#include "pas_local_allocator.h"
#include "pas_segregated_size_directory_inlines.h"

PAS_BEGIN_EXTERN_C;

static PAS_ALWAYS_INLINE pas_allocator_index
pas_designated_intrinsic_heap_num_allocator_indices(pas_heap_config config)
{
    return PAS_MAX(
        pas_segregated_size_directory_num_allocator_indices_for_config(config.small_segregated_config),
        pas_segregated_size_directory_num_allocator_indices_for_config(config.medium_segregated_config));
}

typedef struct {
    size_t index;
    bool did_succeed;
} pas_designated_index_result;

static PAS_ALWAYS_INLINE pas_designated_index_result
pas_designated_index_result_create_failure(void)
{
    pas_designated_index_result result;
    result.index = 0;
    result.did_succeed = false;
    return result;
}

static PAS_ALWAYS_INLINE pas_designated_index_result
pas_designated_index_result_create_success(size_t index)
{
    pas_designated_index_result result;
    result.index = index;
    result.did_succeed = true;
    return result;
}

static PAS_ALWAYS_INLINE size_t
pas_designated_index_result_get_allocator_index(pas_designated_index_result result,
                                                pas_heap_config config)
{
    PAS_TESTING_ASSERT(result.did_succeed);

    return PAS_LOCAL_ALLOCATOR_UNSELECTED_NUM_INDICES +
        result.index * pas_designated_intrinsic_heap_num_allocator_indices(config);
}

static PAS_ALWAYS_INLINE size_t
pas_designated_intrinsic_heap_num_designated_indices_for_small_config(
    pas_segregated_page_config small_config)
{
    /* FIXME: These constants have to match what we do with set_up_range in
       pas_designated_intrinsic_heap_initialize. */
    switch (pas_segregated_page_config_min_align(small_config)) {
    case 8:
        return 38;
    case 16:
        return 26;
    case 32:
        return 14;
    default:
        PAS_ASSERT(!"Unsupported minalign");
        return 0;
    }
}

static PAS_ALWAYS_INLINE pas_designated_index_result
pas_designated_intrinsic_heap_designated_index_for_small_config(
    size_t index,
    pas_intrinsic_heap_designation_mode designation_mode,
    pas_segregated_page_config small_config)
{
    if (!designation_mode)
        return pas_designated_index_result_create_failure();

    /* NOTE: We could do math here. We choose not to because so far that has proved to be
       a perf problem. */
    if (index <= pas_designated_intrinsic_heap_num_designated_indices_for_small_config(small_config))
        return pas_designated_index_result_create_success(index);

    return pas_designated_index_result_create_failure();
}

static PAS_ALWAYS_INLINE size_t
pas_designated_intrinsic_heap_num_designated_indices(pas_heap_config config)
{
    return pas_designated_intrinsic_heap_num_designated_indices_for_small_config(
        config.small_segregated_config);
}

static PAS_ALWAYS_INLINE pas_designated_index_result pas_designated_intrinsic_heap_designated_index(
    size_t index,
    pas_intrinsic_heap_designation_mode designation_mode,
    pas_heap_config config)
{
    return pas_designated_intrinsic_heap_designated_index_for_small_config(
        index, designation_mode, config.small_segregated_config);
}

PAS_END_EXTERN_C;

#endif /* PAS_DESIGNATED_INTRINSIC_HEAP_INLINES_H */

