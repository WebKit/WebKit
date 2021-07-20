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

#ifndef PAS_HEAP_CONFIG_INLINES_H
#define PAS_HEAP_CONFIG_INLINES_H

#include "pas_deallocate.h"
#include "pas_heap_config.h"
#include "pas_local_allocator_inlines.h"
#include "pas_try_allocate_common.h"

PAS_BEGIN_EXTERN_C;

#define PAS_HEAP_CONFIG_SPECIALIZATION_DEFINITIONS(lower_case_heap_config_name, heap_config_value) \
    PAS_NEVER_INLINE pas_allocation_result \
    lower_case_heap_config_name ## _specialized_local_allocator_try_allocate_small_segregated_slow( \
        pas_local_allocator* allocator, pas_allocator_counts* count) \
    { \
        return pas_local_allocator_try_allocate_small_segregated_slow( \
            allocator, (heap_config_value), count); \
    } \
    \
    PAS_NEVER_INLINE pas_allocation_result \
    lower_case_heap_config_name ## _specialized_local_allocator_try_allocate_inline_cases( \
        pas_local_allocator* allocator) \
    { \
        return pas_local_allocator_try_allocate_inline_cases(allocator, (heap_config_value)); \
    } \
    \
    PAS_NEVER_INLINE pas_allocation_result \
    lower_case_heap_config_name ## _specialized_local_allocator_try_allocate_medium_segregated_with_free_bits( \
        pas_local_allocator* allocator) \
    { \
        return pas_local_allocator_try_allocate_with_free_bits( \
            allocator, (heap_config_value).medium_segregated_config); \
    } \
    \
    PAS_NEVER_INLINE pas_allocation_result \
    lower_case_heap_config_name ## _specialized_local_allocator_try_allocate_slow( \
        pas_local_allocator* allocator, \
        size_t size, \
        size_t alignment, \
        pas_allocator_counts* counts) \
    { \
        return pas_local_allocator_try_allocate_slow( \
            allocator, size, alignment, (heap_config_value), counts); \
    } \
    \
    PAS_NEVER_INLINE pas_intrinsic_allocation_result \
    lower_case_heap_config_name ## _specialized_try_allocate_common_impl_slow( \
        pas_heap_ref* heap_ref, \
        pas_heap_ref_kind heap_ref_kind, \
        size_t aligned_count, \
        size_t size, \
        size_t alignment, \
        pas_heap_runtime_config* runtime_config, \
        pas_allocator_counts* allocator_counts, \
        pas_count_lookup_mode count_lookup_mode) \
    { \
        return pas_try_allocate_common_impl_slow( \
            heap_ref, heap_ref_kind, aligned_count, size, alignment, (heap_config_value), \
            runtime_config, allocator_counts, count_lookup_mode); \
    } \
    \
    bool lower_case_heap_config_name ## _specialized_try_deallocate_not_small( \
        pas_thread_local_cache* thread_local_cache, \
        uintptr_t begin, \
        pas_deallocation_mode deallocation_mode) \
    { \
        return pas_try_deallocate_not_small( \
            thread_local_cache, begin, (heap_config_value), deallocation_mode); \
    } \
    \
    struct pas_dummy

PAS_END_EXTERN_C;

#endif /* PAS_HEAP_CONFIG_INLINES_H */

