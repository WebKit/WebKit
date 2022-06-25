/*
 * Copyright (c) 2022 Apple Inc. All rights reserved.
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

#ifndef PAS_LENIENT_COMPACT_PTR_INLINES_H
#define PAS_LENIENT_COMPACT_PTR_INLINES_H

#include "pas_lenient_compact_ptr.h"
#include "pas_utility_heap.h"

PAS_BEGIN_EXTERN_C;

#define PAS_DEFINE_LENIENT_COMPACT_PTR(type, name) \
    void name ## _destruct(name* ptr) \
    { \
        type* old_value; \
        old_value = name ## _compact_tagged_atomic_ptr_load(&ptr->ptr); \
        if ((uintptr_t)old_value & PAS_LENIENT_COMPACT_PTR_FULL_PTR_BIT) { \
            pas_utility_heap_deallocate( \
                (void*)((uintptr_t)old_value & ~PAS_LENIENT_COMPACT_PTR_FULL_PTR_BIT)); \
        } \
    } \
    \
    void name ## _store(name* ptr, type* value) \
    { \
        PAS_TESTING_ASSERT(!((uintptr_t)value & PAS_LENIENT_COMPACT_PTR_FULL_PTR_BIT)); \
        name ## _destruct(ptr); \
        if ((uintptr_t)value >= PAS_INTERNAL_MIN_ALIGN \
            && (uintptr_t)value - pas_compact_heap_reservation_base >= pas_compact_heap_reservation_size) { \
            type** box; \
            box = pas_utility_heap_allocate(sizeof(type*), #name "/box"); \
            *box = value; \
            value = (type*)((uintptr_t)box | PAS_LENIENT_COMPACT_PTR_FULL_PTR_BIT); \
        } \
        name ## _compact_tagged_atomic_ptr_store(&ptr->ptr, value); \
    } \
    \
    type* name ## _load(name* ptr) \
    { \
        type* result; \
        result = name ## _compact_tagged_atomic_ptr_load(&ptr->ptr); \
        if ((uintptr_t)result & PAS_LENIENT_COMPACT_PTR_FULL_PTR_BIT) \
            return *(type**)((uintptr_t)result & ~PAS_LENIENT_COMPACT_PTR_FULL_PTR_BIT); \
        return result; \
    } \
    \
    type* name ## _load_remote(pas_enumerator* enumerator, name* ptr, size_t size) \
    { \
        type* result; \
        result = name ## _compact_tagged_atomic_ptr_load_remote(enumerator, &ptr->ptr); \
        if ((uintptr_t)result & PAS_LENIENT_COMPACT_PTR_FULL_PTR_BIT) { \
            return (type*)pas_enumerator_read( \
                enumerator, \
                *(type**)((uintptr_t)result & ~PAS_LENIENT_COMPACT_PTR_FULL_PTR_BIT), \
                size); \
        } \
        return result; \
    } \
    \
    struct pas_dummy

PAS_END_EXTERN_C;

#endif /* PAS_LENIENT_COMPACT_PTR_INLINES_H */

