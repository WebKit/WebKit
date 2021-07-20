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

#ifndef PAS_MIN_HEAP_H
#define PAS_MIN_HEAP_H

#include "pas_allocation_config.h"
#include "pas_utils.h"
#include <stdio.h>

PAS_BEGIN_EXTERN_C;

#define PAS_MIN_HEAP_INITIALIZER(name) ((name){ \
        .outline_array = NULL, \
        .size = 0, \
        .outline_capacity = 0 \
    })

/* A min_heap is a complete binary tree, so it is representable as an array. It's easiest to
   see what is going on by using one-based array indices:
   
                  1
           2            3
        4     5      6     7
       8 9  10 11  12 13 14 15

   This way, the indices of node X's children are at 2X and 2X+1.
   
   This also has built-in support for heapsort, but because it's primarily designed as a heap
   not as a sort, it will sort descending. Heapsort has this property that it sorts in the
   opposite order of the heap, so an ascending heapsort will be based on a maxheap. If you want
   to run this as an ascending heapsort, just invert your compare function. */

#define PAS_CREATE_MIN_HEAP(name, type, inline_capacity, ...) \
    struct name; \
    struct name##_config; \
    typedef struct name name; \
    typedef struct name##_config name##_config; \
    \
    struct name { \
        size_t size; \
        type inline_array[inline_capacity]; \
        type* outline_array; \
        size_t outline_capacity; \
    }; \
    \
    struct name##_config { \
        int (*compare)(type* left, type* right); \
        \
        size_t (*get_index)(type* element); /* Optional - only needed if you call */\
                                            /* pas_min_heap_remove(). */\
        \
        void (*set_index)(type* element, size_t index);  /* Can be a no-op unless you need to */\
                                                         /* know the index or you use */\
                                                         /* pas_min_heap_remove(). */\
    }; \
    \
    static inline type* name##_get_ptr_by_index(name* min_heap, size_t index) \
    { \
        PAS_ASSERT(index); \
        index--; \
        PAS_ASSERT(index < (inline_capacity) + min_heap->outline_capacity); \
        if (index < (inline_capacity)) \
            return min_heap->inline_array + index; \
        return min_heap->outline_array + index - (inline_capacity); \
    } \
    \
    PAS_UNUSED static inline void name##_construct(name* min_heap) \
    { \
        *min_heap = PAS_MIN_HEAP_INITIALIZER(name); \
        pas_zero_memory(min_heap->inline_array, sizeof(type) * (inline_capacity)); \
    } \
    \
    PAS_UNUSED static inline void name##_destruct( \
        name* min_heap, \
        const pas_allocation_config* allocation_config) \
    { \
        allocation_config->deallocate(min_heap->outline_array, \
                                      min_heap->outline_capacity * sizeof(type), \
                                      pas_object_allocation, \
                                      allocation_config->arg); \
    } \
    \
    /* This takes a one-based index. */ \
    PAS_UNUSED static inline type name##_remove_by_index(name* min_heap, \
                                                         size_t index) \
    { \
        name##_config config = {__VA_ARGS__}; \
        \
        type result; \
        type element_at_index; \
        size_t size; \
        size_t parent_index; \
        \
        PAS_ASSERT(index); \
        \
        size = min_heap->size; \
        \
        PAS_ASSERT(index <= size); \
        \
        result = *name##_get_ptr_by_index(min_heap, index); \
        element_at_index = *name##_get_ptr_by_index(min_heap, size); \
        \
        /* We keep the array zeroed in the parts that we're not using. That just makes */ \
        /* debugging easier. */ \
        pas_zero_memory(name##_get_ptr_by_index(min_heap, size), \
                    sizeof(type)); \
        \
        /* Doing this aids debugging. */ \
        pas_zero_memory(name##_get_ptr_by_index(min_heap, index), \
                    sizeof(type)); \
        \
        size--; \
        \
        min_heap->size = size; \
        \
        if (index == size + 1) { \
            /* This is a fantastic special case. If the last element is the one we are removing */ \
            /* then it's a waste of time for us to do any bubbling. Also, we cannot bubble the */ \
            /* last element if it is already out-of-order - in that case we will end up removing */ \
            /* some other element. */ \
            config.set_index(&result, 0); \
            return result; \
        } \
        \
        parent_index = index >> 1; \
        if (parent_index) { \
            type parent; \
            bool less_than_parent; \
            \
            parent = *name##_get_ptr_by_index(min_heap, parent_index); \
            less_than_parent = config.compare(&element_at_index, &parent) < 0; \
            \
            if (less_than_parent) { \
                do { \
                    config.set_index(&parent, index); \
                    \
                    *name##_get_ptr_by_index(min_heap, index) = parent; \
                    \
                    index = parent_index; \
                    PAS_ASSERT(index >= 1); \
                    if (index == 1) \
                        break; \
                    \
                    parent_index = index >> 1; \
                    \
                    parent = *name##_get_ptr_by_index(min_heap, parent_index); \
                    less_than_parent = config.compare(&element_at_index, &parent) < 0; \
                } while (less_than_parent); \
                \
                goto done; \
            } \
        } \
        \
        for (;;) { \
            size_t index_of_left; \
            size_t index_of_right; \
            type element_right; \
            bool less_equal_right; \
            type element_left; \
            bool less_equal_left; \
            \
            /* It already must be the case that the node at `index` is greater than its parent, */\
            /* since that node came from the bottom of the heap. But it may not be less than or */\
            /* equal to its children. */\
            \
            index_of_left = index << 1; \
            index_of_right = (index << 1) + 1; \
            \
            pas_zero_memory(&element_right, sizeof(element_right)); \
            less_equal_right = true; \
            pas_zero_memory(&element_left, sizeof(element_left)); \
            less_equal_left = true; \
            \
            if (index_of_right <= size) { \
                element_right = *name##_get_ptr_by_index(min_heap, index_of_right); \
                less_equal_right = config.compare(&element_at_index, &element_right) <= 0; \
            } \
            \
            if (index_of_left <= size) { \
                element_left = *name##_get_ptr_by_index(min_heap, index_of_left); \
                less_equal_left = config.compare(&element_at_index, &element_left) <= 0; \
            } \
            \
            if (!less_equal_right \
                && (less_equal_left || config.compare(&element_right, &element_left) < 0)) { \
                config.set_index(&element_right, index); \
                \
                *name##_get_ptr_by_index(min_heap, index) = element_right; \
                \
                index = index_of_right; \
                continue; \
            } \
            \
            if (!less_equal_left) { \
                config.set_index(&element_left, index); \
                \
                *name##_get_ptr_by_index(min_heap, index) = element_left; \
                \
                index = index_of_left; \
                continue; \
            } \
            \
        done: \
            *name##_get_ptr_by_index(min_heap, index) = element_at_index; \
            config.set_index(&element_at_index, index); \
            config.set_index(&result, 0); \
            return result; \
        } \
    } \
    \
    PAS_UNUSED static inline type name##_get_min(name* min_heap) \
    { \
        if (!min_heap->size) { \
            type result; \
            pas_zero_memory(&result, sizeof(type)); \
            return result; \
        } \
        \
        return *name##_get_ptr_by_index(min_heap, 1); \
    } \
    \
    PAS_UNUSED static inline type name##_take_min(name* min_heap) \
    { \
        if (!min_heap->size) { \
            type result; \
            pas_zero_memory(&result, sizeof(type)); \
            return result; \
        } \
        \
        return name##_remove_by_index(min_heap, 1); \
    } \
    \
    PAS_UNUSED static inline void name##_remove(name* min_heap, \
                                                type element) \
    { \
        name##_config config = {__VA_ARGS__}; \
        \
        size_t index = config.get_index(&element); \
        PAS_ASSERT(index); \
        PAS_ASSERT(!bcmp(name##_get_ptr_by_index(min_heap, index), &element, sizeof(type))); \
        name##_remove_by_index(min_heap, index); \
    } \
    \
    PAS_UNUSED static inline void name##_add( \
        name* min_heap, \
        type element, \
        const pas_allocation_config* allocation_config) \
    { \
        name##_config config = {__VA_ARGS__}; \
        \
        size_t size; \
        size_t outline_capacity; \
        size_t index; \
        \
        size = min_heap->size; \
        outline_capacity = min_heap->outline_capacity; \
        \
        if (size >= (inline_capacity) + outline_capacity) { \
            type* new_outline_array; \
            size_t new_outline_capacity; \
            \
            PAS_ASSERT(size == (inline_capacity) + outline_capacity); \
            \
            new_outline_capacity = PAS_MAX((size_t)4, outline_capacity << 1); \
            PAS_ASSERT(new_outline_capacity > outline_capacity); \
            new_outline_array = (type*)allocation_config->allocate( \
                new_outline_capacity * sizeof(type), \
                #name "/outline_array", \
                pas_object_allocation, \
                allocation_config->arg); \
            \
            PAS_ASSERT(size < (inline_capacity) + new_outline_capacity); \
            \
            pas_zero_memory(new_outline_array, sizeof(type) * new_outline_capacity); \
            \
            memcpy(new_outline_array, \
                   min_heap->outline_array, \
                   (size - (inline_capacity)) * sizeof(type)); \
            \
            allocation_config->deallocate( \
                min_heap->outline_array, \
                outline_capacity * sizeof(type), \
                pas_object_allocation, \
                allocation_config->arg); \
            \
            outline_capacity = new_outline_capacity; \
            \
            min_heap->outline_array = new_outline_array; \
            min_heap->outline_capacity = new_outline_capacity; \
        } \
        \
        index = ++size; \
        PAS_ASSERT(size <= (inline_capacity) + outline_capacity); \
        \
        /* Doing this aids debugging. */ \
        pas_zero_memory(name##_get_ptr_by_index(min_heap, index), sizeof(type)); \
        \
        min_heap->size = size; \
        \
        while (index > 1) { \
            size_t index_of_parent; \
            type parent; \
            bool greater_equal_parent; \
            \
            index_of_parent = index >> 1; \
            \
            parent = *name##_get_ptr_by_index(min_heap, index_of_parent); \
            greater_equal_parent = config.compare(&element, &parent) >= 0; \
            \
            if (greater_equal_parent) \
                break; \
            \
            config.set_index(&parent, index); \
            \
            *name##_get_ptr_by_index(min_heap, index) = parent; \
            \
            index = index_of_parent; \
        } \
        \
        config.set_index(&element, index); \
        *name##_get_ptr_by_index(min_heap, index) = element; \
    } \
    \
    PAS_UNUSED static inline bool name##_is_index_still_ordered(name* min_heap, \
                                                                size_t index, \
                                                                type element) \
    { \
        name##_config config = {__VA_ARGS__}; \
        \
        size_t size; \
        \
        size = min_heap->size; \
        \
        if (index > 1 \
            && config.compare(&element, name##_get_ptr_by_index(min_heap, \
                                                                index >> 1)) < 0) \
            return false; \
        \
        if ((index << 1) <= size \
            && config.compare(&element, name##_get_ptr_by_index(min_heap, \
                                                                index << 1)) > 0) \
            return false; \
        \
        if ((index << 1) + 1 <= size \
            && config.compare(&element, name##_get_ptr_by_index(min_heap, \
                                                                (index << 1) + 1)) > 0) \
            return false; \
        \
        return true; \
    } \
    \
    PAS_UNUSED static inline bool name##_is_still_ordered(name* min_heap, \
                                                          type element) \
    { \
        name##_config config = {__VA_ARGS__}; \
        \
        return name##_is_index_still_ordered( \
            min_heap, config.get_index(&element), element); \
    } \
    \
    PAS_UNUSED static inline void name##_update_order(name* min_heap, \
                                                      type element, \
                                                      const pas_allocation_config* allocation_config) \
    { \
        name##_config config = {__VA_ARGS__}; \
        \
        if (config.get_index(&element)) { \
            if (name##_is_still_ordered(min_heap, element)) \
                return; \
            \
            name##_remove(min_heap, element); \
        } \
        \
        name##_add(min_heap, element, allocation_config); \
    } \
    \
    PAS_UNUSED static inline void name##_sort_descending(name* min_heap) \
    { \
        size_t original_size = min_heap->size; \
        \
        while (min_heap->size) { \
            type value; \
            \
            value = name##_take_min(min_heap); \
            \
            *name##_get_ptr_by_index(min_heap, min_heap->size + 1) = value; \
        } \
        \
        /* At this point, this isn't a heap anymore. It's an array sorted in descending */ \
        /* order. */ \
        min_heap->size = original_size; \
    } \
    \
    struct pas_dummy

PAS_END_EXTERN_C;

#endif /* PAS_MIN_HEAP_H */


