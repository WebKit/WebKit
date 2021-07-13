/*
 * Copyright (c) 2020 Apple Inc. All rights reserved.
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

#ifndef PAS_COMPACT_TAGGED_PTR_H
#define PAS_COMPACT_TAGGED_PTR_H

#include "pas_compact_heap_reservation.h"
#include "pas_enumerator.h"
#include "pas_utils.h"

PAS_BEGIN_EXTERN_C;

#define PAS_COMPACT_TAGGED_PTR_SIZE (PAS_COMPACT_PTR_SIZE + 1)
#define PAS_COMPACT_TAGGED_PTR_BITS (PAS_COMPACT_TAGGED_PTR_SIZE << 3)
#define PAS_COMPACT_TAGGED_PTR_MASK (((uintptr_t)1 << PAS_COMPACT_TAGGED_PTR_BITS) - 1)

#define PAS_COMPACT_TAGGED_PTR_INITIALIZER { .payload = {[0 ... PAS_COMPACT_TAGGED_PTR_SIZE - 1] = 0} }

#define PAS_DEFINE_COMPACT_TAGGED_PTR_HELPERS(type, name) \
    static inline uintptr_t name ## _offset_for_ptr(type value) \
    { \
        uintptr_t ptr; \
        uintptr_t offset; \
        ptr = (uintptr_t)value; \
        if (ptr < PAS_INTERNAL_MIN_ALIGN) \
            return ptr; \
        offset = ptr - pas_compact_heap_reservation_base; \
        PAS_ASSERT(offset < pas_compact_heap_reservation_size); \
        PAS_ASSERT(offset); \
        return offset; \
    } \
    \
    static inline type name ## _ptr_for_offset(uintptr_t offset) \
    { \
        if (offset < PAS_INTERNAL_MIN_ALIGN) \
            return (type)offset; \
        return (type)(offset + pas_compact_heap_reservation_base); \
    } \
    \
    static inline type name ## _ptr_for_offset_non_null(uintptr_t offset) \
    { \
        PAS_ASSERT(offset >= PAS_INTERNAL_MIN_ALIGN); \
        return (type)(offset + pas_compact_heap_reservation_base); \
    } \
    \
    static inline type name ## _ptr_for_remote_offset(pas_enumerator* enumerator, uintptr_t offset) \
    { \
        if (offset < PAS_INTERNAL_MIN_ALIGN) \
            return (type)offset; \
        return (type)(offset + (uintptr_t)enumerator->compact_heap_copy_base); \
    } \
    \
    struct pas_dummy

/* Supports using the low bits as tag bits. */
#define PAS_DEFINE_COMPACT_TAGGED_PTR(type, name) \
    struct name; \
    typedef struct name name; \
    \
    struct name { \
        uint8_t payload[PAS_COMPACT_TAGGED_PTR_SIZE]; \
    }; \
    \
    PAS_DEFINE_COMPACT_TAGGED_PTR_HELPERS(type, name); \
    \
    static inline void name ## _store(name* ptr, type value) \
    { \
        uintptr_t offset; \
        size_t index; \
        offset = name ## _offset_for_ptr(value); \
        for (index = 0; index < PAS_COMPACT_TAGGED_PTR_SIZE; ++index) { \
            ptr->payload[index] = (uintptr_t)offset; \
            offset >>= 8; \
        } \
    } \
    \
    static inline type name ## _load(name* ptr) \
    { \
        return name ## _ptr_for_offset(*(uintptr_t*)ptr & PAS_COMPACT_TAGGED_PTR_MASK); \
    } \
    \
    static inline type name ## _load_non_null(name* ptr) \
    { \
        return name ## _ptr_for_offset_non_null(*(uintptr_t*)ptr & PAS_COMPACT_TAGGED_PTR_MASK); \
    } \
    \
    static inline type name ## _load_remote(pas_enumerator* enumerator, name* ptr) \
    { \
        uintptr_t ptr_as_offset; \
        memcpy(&ptr_as_offset, ptr->payload, PAS_COMPACT_TAGGED_PTR_SIZE); \
        ptr_as_offset &= PAS_COMPACT_TAGGED_PTR_MASK; \
        return name ## _ptr_for_remote_offset(enumerator, ptr_as_offset); \
    } \
    \
    struct pas_dummy

PAS_END_EXTERN_C;

#endif /* PAS_COMPACT_TAGGED_PTR_H */

