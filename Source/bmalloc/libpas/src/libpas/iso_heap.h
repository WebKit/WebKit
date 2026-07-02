/*
 * Copyright (c) 2019-2020 Apple Inc. All rights reserved.
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

#ifndef ISO_HEAP_H
#define ISO_HEAP_H

#include "iso_heap_ref.h"
#include "pas_primitive_heap_ref.h"
#include "pas_reallocate_free_mode.h"
#include "pas_allocation_mode.h"

#if PAS_ENABLE_ISO

PAS_BEGIN_EXTERN_C;

PAS_API void* iso_try_allocate_common_primitive(size_t size, pas_allocation_mode allocation_mode);
PAS_API void* iso_try_allocate_common_primitive_with_alignment(size_t size,
                                                               size_t alignment,
                                                               pas_allocation_mode allocation_mode);

PAS_API void* iso_try_allocate_common_primitive_zeroed(size_t size, pas_allocation_mode allocation_mode);

PAS_API void* iso_allocate_common_primitive(size_t size, pas_allocation_mode allocation_mode);
PAS_API void* iso_allocate_common_primitive_with_alignment(size_t size,
                                                           size_t alignment,
                                                           pas_allocation_mode allocation_mode);

PAS_API void* iso_allocate_common_primitive_zeroed(size_t size, pas_allocation_mode allocation_mode);

PAS_API void* iso_try_reallocate_common_primitive(void* old_ptr, size_t new_size,
                                                  pas_reallocate_free_mode free_mode,
                                                  pas_allocation_mode allocation_mode);

PAS_API void* iso_reallocate_common_primitive(void* old_ptr, size_t new_size,
                                              pas_reallocate_free_mode free_mode,
                                              pas_allocation_mode allocation_mode);

PAS_API void* iso_try_allocate_dynamic_primitive(const void* key, size_t size, pas_allocation_mode allocation_mode);
PAS_API void* iso_try_allocate_dynamic_primitive_with_alignment(const void* key,
                                                                size_t size,
                                                                size_t alignment,
                                                                pas_allocation_mode allocation_mode);

PAS_API void* iso_try_allocate_dynamic_primitive_zeroed(const void* key,
                                                        size_t size,
                                                        pas_allocation_mode allocation_mode);

PAS_API void* iso_try_reallocate_dynamic_primitive(void* old_ptr,
                                                   const void* key,
                                                   size_t new_size,
                                                   pas_reallocate_free_mode free_mode,
                                                   pas_allocation_mode allocation_mode);

PAS_API void iso_heap_ref_construct(pas_heap_ref* heap_ref,
                                    pas_simple_type type);

PAS_API void* iso_try_allocate(pas_heap_ref* heap_ref, pas_allocation_mode pas_allocation_mode);
PAS_API void* iso_allocate(pas_heap_ref* heap_ref, pas_allocation_mode pas_allocation_mode);

PAS_API void* iso_try_allocate_array_by_count(pas_heap_ref* heap_ref, size_t count, size_t alignment, pas_allocation_mode allocation_mode);
PAS_API void* iso_allocate_array_by_count(pas_heap_ref* heap_ref, size_t count, size_t alignment, pas_allocation_mode allocation_mode);

PAS_API void* iso_try_allocate_array_by_count_zeroed(pas_heap_ref* heap_ref, size_t count, size_t alignment, pas_allocation_mode allocation_mode);
PAS_API void* iso_allocate_array_by_count_zeroed(pas_heap_ref* heap_ref, size_t count, size_t alignment, pas_allocation_mode allocation_mode);

PAS_API void* iso_try_reallocate_array_by_count(void* old_ptr, pas_heap_ref* heap_ref,
                                                size_t new_count,
                                                pas_reallocate_free_mode free_mode,
                                                pas_allocation_mode allocation_mode);
PAS_API void* iso_reallocate_array_by_count(void* old_ptr, pas_heap_ref* heap_ref,
                                            size_t new_count,
                                            pas_reallocate_free_mode free_mode,
                                            pas_allocation_mode allocation_mode);

PAS_API pas_heap* iso_heap_ref_get_heap(pas_heap_ref* heap_ref);

PAS_API void iso_primitive_heap_ref_construct(pas_primitive_heap_ref* heap_ref,
                                              pas_simple_type type);

PAS_API void* iso_try_allocate_primitive(pas_primitive_heap_ref* heap_ref,
                                         size_t size,
                                         pas_allocation_mode allocation_mode);
PAS_API void* iso_allocate_primitive(pas_primitive_heap_ref* heap_ref,
                                     size_t size,
                                     pas_allocation_mode allocation_mode);

PAS_API void* iso_try_allocate_primitive_zeroed(pas_primitive_heap_ref* heap_ref,
                                                size_t size,
                                                pas_allocation_mode allocation_mode);
PAS_API void* iso_allocate_primitive_zeroed(pas_primitive_heap_ref* heap_ref,
                                            size_t size,
                                            pas_allocation_mode allocation_mode);

PAS_API void* iso_try_allocate_primitive_with_alignment(pas_primitive_heap_ref* heap_ref,
                                                        size_t size,
                                                        size_t alignment,
                                                        pas_allocation_mode allocation_mode);
PAS_API void* iso_allocate_primitive_with_alignment(pas_primitive_heap_ref* heap_ref,
                                                    size_t size,
                                                    size_t alignment,
                                                    pas_allocation_mode allocation_mode);

PAS_API void* iso_try_reallocate_primitive(void* old_ptr,
                                           pas_primitive_heap_ref* heap_ref,
                                           size_t new_size,
                                           pas_reallocate_free_mode free_mode,
                                           pas_allocation_mode allocation_mode);
PAS_API void* iso_reallocate_primitive(void* old_ptr,
                                       pas_primitive_heap_ref* heap_ref,
                                       size_t new_size,
                                       pas_reallocate_free_mode free_mode,
                                       pas_allocation_mode allocation_mode);

PAS_API pas_heap* iso_primitive_heap_ref_get_heap(pas_primitive_heap_ref* heap_ref);

PAS_API void* iso_try_allocate_for_flex(const void* cls, size_t size, pas_allocation_mode allocation_mode);

PAS_API bool iso_has_object(void*);
PAS_API size_t iso_get_allocation_size(void*);
PAS_API pas_heap* iso_get_heap(void*);

PAS_API void iso_deallocate(void*);

PAS_API pas_heap* iso_force_primitive_heap_into_reserved_memory(pas_primitive_heap_ref* heap_ref,
                                                                uintptr_t begin,
                                                                uintptr_t end);

PAS_END_EXTERN_C;

#endif /* PAS_ENABLE_ISO */

#endif /* ISO_HEAP_H */

