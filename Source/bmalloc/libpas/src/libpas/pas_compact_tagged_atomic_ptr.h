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

#ifndef PAS_COMPACT_TAGGED_ATOMIC_PTR_H
#define PAS_COMPACT_TAGGED_ATOMIC_PTR_H

#include "pas_compact_tagged_ptr.h"
#include "pas_utils.h"

PAS_BEGIN_EXTERN_C;

#if PAS_COMPACT_TAGGED_PTR_SIZE <= 4
typedef unsigned pas_compact_tagged_atomic_ptr_impl;
#elif PAS_COMPACT_TAGGED_PTR_SIZE <= 8
typedef uint64_t pas_compact_tagged_atomic_ptr_impl;
#else
#error "Cannot use PAS_COMPACT_TAGGED_PTR_SIZE > 8"
#endif

#define PAS_COMPACT_TAGGED_ATOMIC_PTR_INITIALIZER { .payload = 0 }

#define PAS_DEFINE_COMPACT_TAGGED_ATOMIC_PTR(type, name) \
    struct name; \
    typedef struct name name; \
    \
    struct name { \
        pas_compact_tagged_atomic_ptr_impl payload; \
    }; \
    \
    PAS_DEFINE_COMPACT_TAGGED_PTR_HELPERS(type, name); \
    \
    static inline void name ## _store(name* ptr, type value) \
    { \
        ptr->payload = (pas_compact_tagged_atomic_ptr_impl)name ## _offset_for_ptr(value); \
    } \
    \
    static inline type name ## _load(name* ptr) \
    { \
        return name ## _ptr_for_offset(ptr->payload); \
    } \
    \
    static inline type name ## _load_non_null(name* ptr) \
    { \
        return name ## _ptr_for_offset_non_null(ptr->payload); \
    } \
    \
    static inline bool name ## _is_null(name* ptr) \
    { \
        return !ptr->payload; \
    } \
    \
    static inline type name ## _load_remote(pas_enumerator* enumerator, name* ptr) \
    { \
        return name ## _ptr_for_remote_offset(enumerator, ptr->payload); \
    } \
    \
    struct pas_dummy

PAS_END_EXTERN_C;

#endif /* PAS_COMPACT_TAGGED_ATOMIC_PTR_H */

