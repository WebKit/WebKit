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

#ifndef PAS_LENIENT_COMPACT_PTR_H
#define PAS_LENIENT_COMPACT_PTR_H

#include "pas_compact_tagged_atomic_ptr.h"

PAS_BEGIN_EXTERN_C;

/* You can use this pointer to point at something that is likely to be in the compact heap but that
   sometimes won't be. To make this work, it's necessary to be able to destruct the pointer, and it's
   not legal to pass the pointer around by value. Also, the thing being pointed to must have the lowest
   bit available (i.e. that bit must always be zero). */

#define PAS_LENIENT_COMPACT_PTR_INITIALIZER { .ptr = PAS_COMPACT_TAGGED_ATOMIC_PTR_INITIALIZER }

#define PAS_LENIENT_COMPACT_PTR_FULL_PTR_BIT ((uintptr_t)1)

#define PAS_DECLARE_LENIENT_COMPACT_PTR(type, name) \
    \
    PAS_DEFINE_COMPACT_TAGGED_ATOMIC_PTR(type*, name ## _compact_tagged_atomic_ptr); \
    \
    struct name; \
    typedef struct name name; \
    \
    struct name { \
        name ## _compact_tagged_atomic_ptr ptr; \
    }; \
    \
    PAS_API void name ## _destruct(name* ptr); \
    PAS_API void name ## _store(name* ptr, type* value); \
    PAS_API type* name ## _load(name* ptr); \
    \
    static inline type* name ## _load_compact(name* ptr) \
    { \
        type* result; \
        result = name ## _compact_tagged_atomic_ptr_load(&ptr->ptr); \
        PAS_TESTING_ASSERT(!((uintptr_t)result & PAS_LENIENT_COMPACT_PTR_FULL_PTR_BIT)); \
        return result; \
    } \
    \
    static inline type* name ## _load_compact_non_null(name* ptr) \
    { \
        type* result; \
        result = name ## _compact_tagged_atomic_ptr_load_non_null(&ptr->ptr); \
        PAS_TESTING_ASSERT(!((uintptr_t)result & PAS_LENIENT_COMPACT_PTR_FULL_PTR_BIT)); \
        return result; \
    } \
    \
    PAS_API type* name ## _load_remote(pas_enumerator* enumerator, name* ptr, size_t size); \
    \
    struct pas_dummy

PAS_END_EXTERN_C;

#endif /* PAS_LENIENT_COMPACT_PTR_H */

