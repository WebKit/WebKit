/*
 * Copyright (c) 2021-2022 Apple Inc. All rights reserved.
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

#ifndef BMALLOC_HEAP_H
#define BMALLOC_HEAP_H

#include "bmalloc_heap_ref.h"
#include "pas_allocation_mode.h"
#include "pas_primitive_heap_ref.h"
#include "pas_reallocate_free_mode.h"

#if PAS_ENABLE_BMALLOC

PAS_BEGIN_EXTERN_C;

PAS_API void* bmalloc_try_allocate(size_t size, pas_allocation_mode allocation_mode);
PAS_API void* bmalloc_try_allocate_with_alignment(size_t size,
                                                  size_t alignment,
                                                  pas_allocation_mode allocation_mode);

PAS_API void* bmalloc_try_allocate_zeroed(size_t size, pas_allocation_mode allocation_mode);

PAS_API void* bmalloc_allocate(size_t size, pas_allocation_mode allocation_mode);
PAS_API void* bmalloc_allocate_with_alignment(size_t size,
                                              size_t alignment,
                                              pas_allocation_mode allocation_mode);

PAS_API void* bmalloc_allocate_zeroed(size_t size, pas_allocation_mode allocation_mode);

PAS_API void* bmalloc_try_reallocate(void* old_ptr, size_t new_size,
                                     pas_allocation_mode allocation_mode,
                                     pas_reallocate_free_mode free_mode);

PAS_API void* bmalloc_reallocate(void* old_ptr, size_t new_size,
                                 pas_allocation_mode allocation_mode,
                                 pas_reallocate_free_mode free_mode);

PAS_BAPI void* bmalloc_try_iso_allocate(pas_heap_ref* heap_ref, pas_allocation_mode allocation_mode);
PAS_BAPI void* bmalloc_iso_allocate(pas_heap_ref* heap_ref, pas_allocation_mode allocation_mode);

PAS_BAPI void* bmalloc_try_iso_allocate_array_by_size(pas_heap_ref* heap_ref, size_t size, pas_allocation_mode allocation_mode);
PAS_BAPI void* bmalloc_iso_allocate_array_by_size(pas_heap_ref* heap_ref, size_t size, pas_allocation_mode allocation_mode);

PAS_BAPI void* bmalloc_try_iso_allocate_zeroed_array_by_size(pas_heap_ref* heap_ref, size_t size, pas_allocation_mode allocation_mode);
PAS_BAPI void* bmalloc_iso_allocate_zeroed_array_by_size(pas_heap_ref* heap_ref, size_t size, pas_allocation_mode allocation_mode);

PAS_BAPI void* bmalloc_try_iso_allocate_array_by_size_with_alignment(
    pas_heap_ref* heap_ref, size_t size, size_t alignment, pas_allocation_mode allocation_mode);
PAS_BAPI void* bmalloc_iso_allocate_array_by_size_with_alignment(
    pas_heap_ref* heap_ref, size_t size, size_t alignment, pas_allocation_mode allocation_mode);

PAS_BAPI void* bmalloc_try_iso_reallocate_array_by_size(pas_heap_ref* heap_ref, void* ptr, size_t size, pas_allocation_mode allocation_mode);
PAS_BAPI void* bmalloc_iso_reallocate_array_by_size(pas_heap_ref* heap_ref, void* ptr, size_t size, pas_allocation_mode allocation_mode);

PAS_API void* bmalloc_try_iso_allocate_array_by_count(pas_heap_ref* heap_ref, size_t count, pas_allocation_mode allocation_mode);
PAS_API void* bmalloc_iso_allocate_array_by_count(pas_heap_ref* heap_ref, size_t count, pas_allocation_mode allocation_mode);

PAS_API void* bmalloc_try_iso_allocate_array_by_count_with_alignment(
    pas_heap_ref* heap_ref, size_t count, size_t alignment, pas_allocation_mode allocation_mode);
PAS_API void* bmalloc_iso_allocate_array_by_count_with_alignment(
    pas_heap_ref* heap_ref, size_t count, size_t alignment, pas_allocation_mode allocation_mode);

PAS_API void* bmalloc_try_iso_reallocate_array_by_count(pas_heap_ref* heap_ref, void* ptr, size_t count, pas_allocation_mode allocation_mode);
PAS_API void* bmalloc_iso_reallocate_array_by_count(pas_heap_ref* heap_ref, void* ptr, size_t count, pas_allocation_mode allocation_mode);

PAS_API pas_heap* bmalloc_heap_ref_get_heap(pas_heap_ref* heap_ref);

PAS_BAPI void* bmalloc_try_allocate_flex(pas_primitive_heap_ref* heap_ref, size_t size, pas_allocation_mode allocation_mode);
PAS_BAPI void* bmalloc_allocate_flex(pas_primitive_heap_ref* heap_ref, size_t size, pas_allocation_mode allocation_mode);

PAS_BAPI void* bmalloc_try_allocate_zeroed_flex(pas_primitive_heap_ref* heap_ref, size_t size, pas_allocation_mode allocation_mode);
PAS_BAPI void* bmalloc_allocate_zeroed_flex(pas_primitive_heap_ref* heap_ref, size_t size, pas_allocation_mode allocation_mode);

PAS_API void* bmalloc_try_allocate_flex_with_alignment(
    pas_primitive_heap_ref* heap_ref, size_t size, size_t alignment, pas_allocation_mode allocation_mode);
PAS_API void* bmalloc_allocate_flex_with_alignment(
    pas_primitive_heap_ref* heap_ref, size_t size, size_t alignment, pas_allocation_mode allocation_mode);

PAS_BAPI void* bmalloc_try_reallocate_flex(pas_primitive_heap_ref* heap_ref, void* old_ptr, size_t new_size, pas_allocation_mode allocation_mode);
PAS_BAPI void* bmalloc_reallocate_flex(pas_primitive_heap_ref* heap_ref, void* old_ptr, size_t new_size, pas_allocation_mode allocation_mode);

PAS_API pas_heap* bmalloc_flex_heap_ref_get_heap(pas_primitive_heap_ref* heap_ref);

PAS_API void* bmalloc_try_allocate_auxiliary(pas_primitive_heap_ref* heap_ref,
                                             size_t size,
                                             pas_allocation_mode allocation_mode);
PAS_API void* bmalloc_allocate_auxiliary(pas_primitive_heap_ref* heap_ref,
                                         size_t size,
                                         pas_allocation_mode allocation_mode);

PAS_API void* bmalloc_try_allocate_auxiliary_zeroed(pas_primitive_heap_ref* heap_ref,
                                                    size_t size,
                                                    pas_allocation_mode allocation_mode);
PAS_API void* bmalloc_allocate_auxiliary_zeroed(pas_primitive_heap_ref* heap_ref,
                                                size_t size,
                                                pas_allocation_mode allocation_mode);

PAS_API void* bmalloc_try_allocate_auxiliary_with_alignment(pas_primitive_heap_ref* heap_ref,
                                                            size_t size,
                                                            size_t alignment,
                                                            pas_allocation_mode allocation_mode);
PAS_API void* bmalloc_allocate_auxiliary_with_alignment(pas_primitive_heap_ref* heap_ref,
                                                        size_t size,
                                                        size_t alignment,
                                                        pas_allocation_mode allocation_mode);

PAS_API void* bmalloc_try_reallocate_auxiliary(void* old_ptr,
                                               pas_primitive_heap_ref* heap_ref,
                                               size_t new_size,
                                               pas_allocation_mode allocation_mode,
                                               pas_reallocate_free_mode free_mode);
PAS_API void* bmalloc_reallocate_auxiliary(void* old_ptr,
                                           pas_primitive_heap_ref* heap_ref,
                                           size_t new_size,
                                           pas_allocation_mode allocation_mode,
                                           pas_reallocate_free_mode free_mode);

PAS_API pas_heap* bmalloc_auxiliary_heap_ref_get_heap(pas_primitive_heap_ref* heap_ref);

PAS_API void bmalloc_deallocate(void*);

PAS_API pas_heap* bmalloc_force_auxiliary_heap_into_reserved_memory(pas_primitive_heap_ref* heap_ref,
                                                                    uintptr_t begin,
                                                                    uintptr_t end);

PAS_BAPI size_t bmalloc_heap_ref_get_type_size(pas_heap_ref* heap_ref);
PAS_API pas_heap* bmalloc_get_heap(void* ptr);
PAS_BAPI size_t bmalloc_get_allocation_size(void* ptr);

PAS_END_EXTERN_C;

#endif /* PAS_ENABLE_BMALLOC */

#endif /* BMALLOC_HEAP_H */

