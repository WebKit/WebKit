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

#ifndef PAS_SEGMENTED_VECTOR_H
#define PAS_SEGMENTED_VECTOR_H

#include "pas_compact_atomic_ptr.h"
#include "pas_found_index.h"
#include "pas_immortal_heap.h"
#include "pas_utils.h"
#include <stdalign.h>

PAS_BEGIN_EXTERN_C;

#define PAS_SEGMENTED_VECTOR_INITIALIZER { .spine = PAS_COMPACT_ATOMIC_PTR_INITIALIZER, \
                                           .size = 0, \
                                           .spine_size = 0 }

/* If you want perf, you want segment_size to be a power of 2. */
#define PAS_DECLARE_SEGMENTED_VECTOR(name, type, segment_size) \
    struct name; \
    typedef struct name name; \
    \
    PAS_DEFINE_COMPACT_ATOMIC_PTR(type, name##_chunk_ptr); \
    PAS_DEFINE_COMPACT_ATOMIC_PTR(name##_chunk_ptr, name##_spine_ptr); \
    \
    struct name { \
        name##_spine_ptr spine; /* behind this is a next pointer. */ \
        unsigned size; \
        unsigned spine_size; \
    }; \
    \
    PAS_UNUSED static inline void name##_construct(name* vector) \
    { \
        pas_zero_memory(vector, sizeof(name)); \
    } \
    \
    PAS_UNUSED static inline type* name##_append( \
        name* vector, type value, pas_lock_hold_mode heap_lock_hold_mode) \
    { \
        size_t spine_index; \
        size_t segment_index; \
        size_t used_spine_size; \
        type* ptr; \
        name##_chunk_ptr* spine; \
        \
        used_spine_size = (vector->size + (segment_size) - 1) / (segment_size); \
        PAS_ASSERT(used_spine_size <= vector->spine_size); \
        \
        spine_index = vector->size / (segment_size); \
        segment_index = vector->size % (segment_size); \
        \
        spine = name##_spine_ptr_load(&vector->spine); \
        \
        if (spine_index >= vector->spine_size) { \
            name##_chunk_ptr* new_spine; \
            size_t new_spine_size; \
            \
            new_spine_size = (vector->spine_size + 1) << 1; \
            PAS_ASSERT(used_spine_size < new_spine_size); \
            \
            new_spine = (name##_chunk_ptr*)pas_immortal_heap_allocate_with_heap_lock_hold_mode( \
                new_spine_size * sizeof(name##_chunk_ptr), \
                #name "/spine", pas_object_allocation, \
                heap_lock_hold_mode); \
            \
            memcpy(new_spine, spine, used_spine_size * sizeof(name##_chunk_ptr)); \
            pas_zero_memory(new_spine + used_spine_size, \
                        (new_spine_size - used_spine_size) * sizeof(name##_chunk_ptr)); \
            pas_fence(); \
            \
            name##_spine_ptr_store(&vector->spine, new_spine); \
            pas_fence(); \
            PAS_ASSERT((unsigned)new_spine_size == new_spine_size); \
            vector->spine_size = (unsigned)new_spine_size; \
            \
            spine = new_spine; \
        } \
        \
        if (spine_index == used_spine_size) { \
            type* segment; \
            \
            segment = name##_chunk_ptr_load(spine + spine_index); \
            PAS_ASSERT(!segment); \
            PAS_ASSERT(!segment_index); \
            \
            segment = (type*)pas_immortal_heap_allocate_with_alignment_and_heap_lock_hold_mode( \
                (segment_size) * sizeof(type), PAS_ALIGNOF(type), \
                #name "/segment", pas_object_allocation, \
                heap_lock_hold_mode); \
            pas_zero_memory(segment, (segment_size) * sizeof(type)); \
            \
            pas_fence(); \
            \
            name##_chunk_ptr_store(spine + spine_index, segment); \
        } \
        \
        ptr = name##_chunk_ptr_load(spine + spine_index) + segment_index; \
        *ptr = value; \
        pas_fence(); \
        vector->size++; \
        PAS_ASSERT(vector->size); \
        return ptr; \
    } \
    \
    PAS_UNUSED static inline type* name##_get_ptr(name* vector, size_t index) \
    { \
        size_t spine_index; \
        size_t segment_index; \
        \
        PAS_TESTING_ASSERT(index < vector->size); \
        \
        spine_index = index / (segment_size); \
        segment_index = index % (segment_size); \
        \
        return name##_chunk_ptr_load( \
            name##_spine_ptr_load(&vector->spine) + spine_index) + segment_index; \
    } \
    \
    PAS_UNUSED static inline type* name##_get_ptr_checked(name* vector, size_t index) \
    { \
        PAS_ASSERT(index < vector->size); \
        \
        return name##_get_ptr(vector, index); \
    } \
    \
    PAS_UNUSED static PAS_ALWAYS_INLINE pas_found_index name##_iterate( \
        name* vector, \
        size_t start_index, \
        bool (*callback)(type* entry, \
                         size_t index, \
                         void* arg), \
        void* arg) \
    { \
        size_t spine_index; \
        size_t segment_index; \
        size_t size; \
        name##_chunk_ptr* spine; \
        \
        spine_index = start_index / (segment_size); \
        segment_index = start_index % (segment_size); \
        \
        size = vector->size; \
        spine = name##_spine_ptr_load(&vector[pas_depend(size)].spine); \
        \
        for (; spine_index * segment_size < size; spine_index++) { \
            type* segment; \
            size_t segment_index_bound; \
            \
            segment = name##_chunk_ptr_load(spine + spine_index); \
            \
            segment_index_bound = PAS_MIN((size_t)(segment_size), \
                                          size - spine_index * segment_size); \
            \
            for (; segment_index < segment_index_bound; segment_index++) { \
                if (!callback(segment + segment_index, \
                              spine_index * (segment_size) + segment_index, \
                              arg)) \
                    return pas_found_index_create(spine_index * (segment_size) + segment_index); \
            } \
            \
            segment_index = 0; \
        } \
        \
        return pas_found_index_create_unsuccessful(size); \
    } \
    \
    PAS_UNUSED static PAS_ALWAYS_INLINE bool name##_iterate_remote( \
        name* vector, \
        pas_enumerator* enumerator, \
        size_t start_index, \
        bool (*callback)(pas_enumerator* enumerator, \
                         type* entry, \
                         size_t index, \
                         void* arg), \
        void* arg) \
    { \
        size_t spine_index; \
        size_t segment_index; \
        size_t size; \
        name##_chunk_ptr* spine; \
        \
        spine_index = start_index / (segment_size); \
        segment_index = start_index % (segment_size); \
        \
        size = vector->size; \
        spine = name##_spine_ptr_load_remote(enumerator, &vector->spine); \
        \
        for (; spine_index * segment_size < size; spine_index++) { \
            type* segment; \
            size_t segment_index_bound; \
            \
            segment = name##_chunk_ptr_load_remote(enumerator, spine + spine_index); \
            \
            segment_index_bound = PAS_MIN((size_t)(segment_size), \
                                          size - spine_index * segment_size); \
            \
            for (; segment_index < segment_index_bound; segment_index++) { \
                if (!callback(enumerator, \
                              segment + segment_index, \
                              spine_index * (segment_size) + segment_index, \
                              arg)) \
                    return false; \
            } \
            \
            segment_index = 0; \
        } \
        \
        return true; \
    } \
    \
    PAS_UNUSED static PAS_ALWAYS_INLINE bool name##_iterate_backward( \
        name* vector, \
        size_t start_index, \
        bool (*callback)(type* entry, \
                         size_t index, \
                         void* arg), \
        void* arg) \
    { \
        size_t spine_index; \
        size_t segment_index; \
        size_t size; \
        name##_chunk_ptr* spine; \
        \
        size = vector->size; \
        \
        /* Bail without asserting anything if size is zero. It so happens that it's inconvenient */\
        /* for the caller to pass any thing smart for start_index if the size is zero. So we */\
        /* make it easy and just bail for zero size. */\
        if (!size) \
            return false; \
        \
        spine_index = start_index / (segment_size); \
        segment_index = start_index % (segment_size); \
        \
        spine = name##_spine_ptr_load(&vector[pas_depend(size)].spine); \
        \
        spine_index++; \
        \
        PAS_ASSERT(spine_index <= size); \
        \
        while (spine_index--) { \
            type* segment; \
            \
            segment = name##_chunk_ptr_load(spine + spine_index); \
            \
            for (segment_index++; segment_index--;) { \
                if (!callback(segment + segment_index, \
                              spine_index * (segment_size) + segment_index, \
                              arg)) \
                    return true; \
            } \
            \
            segment_index = (segment_size) - 1; \
        } \
        \
        return false; \
    } \
    \
    struct pas_dummy

PAS_END_EXTERN_C;

#endif /* PAS_SEGMENTED_VECTOR_H */

