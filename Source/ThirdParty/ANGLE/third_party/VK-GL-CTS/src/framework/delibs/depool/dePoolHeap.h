#ifndef _DEPOOLHEAP_H
#define _DEPOOLHEAP_H
/*-------------------------------------------------------------------------
 * drawElements Memory Pool Library
 * --------------------------------
 *
 * Copyright 2014 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *//*!
 * \file
 * \brief Memory pool heap class.
 *//*--------------------------------------------------------------------*/

#include "deDefs.h"
#include "deMemPool.h"
#include "dePoolArray.h"

DE_BEGIN_EXTERN_C

void dePoolHeap_selfTest(void);

DE_END_EXTERN_C

/*--------------------------------------------------------------------*//*!
 * \brief Declare a template pool heap class.
 * \param TYPENAME    Type name of the declared heap.
 * \param VALUETYPE    Type of the value contained in the heap.
 * \param CMPFUNC    Comparison function of two elements returning (-1, 0, +1).
 *
 * This macro declares a pool heap with all the necessary functions for
 * operating with it. All allocated memory is taken from the memory pool
 * given to the constructor.
 *
 * The functions for operating the heap are:
 *
 * \code
 * Heap*    Heap_create            (deMemPool* pool);
 * int      Heap_getNumElements    (const Heap* heap);
 * bool   Heap_reserve           (Heap* heap, int size);
 * void        Heap_reset             (Heap* heap);
 * bool   Heap_push              (Heap* heap, Element elem);
 * Element  Heap_popMin            (Heap* heap);
 * \endcode
*//*--------------------------------------------------------------------*/
#define DE_DECLARE_POOL_HEAP(TYPENAME, VALUETYPE, CMPFUNC)                                                           \
                                                                                                                     \
    DE_DECLARE_POOL_ARRAY(TYPENAME##Array, VALUETYPE);                                                               \
                                                                                                                     \
    typedef struct TYPENAME##_s                                                                                      \
    {                                                                                                                \
        TYPENAME##Array *array;                                                                                      \
    } TYPENAME; /* NOLINT(TYPENAME) */                                                                               \
                                                                                                                     \
    static inline TYPENAME *TYPENAME##_create(deMemPool *pool);                                                      \
    static inline int TYPENAME##_getNumElements(const TYPENAME *heap) DE_UNUSED_FUNCTION;                            \
    static inline bool TYPENAME##_reserve(DE_PTR_TYPE(TYPENAME) heap, int capacity) DE_UNUSED_FUNCTION;              \
    static inline void TYPENAME##_reset(DE_PTR_TYPE(TYPENAME) heap) DE_UNUSED_FUNCTION;                              \
    static inline void TYPENAME##_moveDown(DE_PTR_TYPE(TYPENAME) heap, int ndx) DE_UNUSED_FUNCTION;                  \
    static inline void TYPENAME##_moveUp(DE_PTR_TYPE(TYPENAME) heap, int ndx) DE_UNUSED_FUNCTION;                    \
    static inline bool TYPENAME##_push(DE_PTR_TYPE(TYPENAME) heap, VALUETYPE elem) DE_UNUSED_FUNCTION;               \
    static inline VALUETYPE TYPENAME##_popMin(DE_PTR_TYPE(TYPENAME) heap) DE_UNUSED_FUNCTION;                        \
                                                                                                                     \
    static inline TYPENAME *TYPENAME##_create(deMemPool *pool)                                                       \
    {                                                                                                                \
        DE_PTR_TYPE(TYPENAME) heap = DE_POOL_NEW(pool, TYPENAME);                                                    \
        if (!heap)                                                                                                   \
            return NULL;                                                                                             \
        heap->array = TYPENAME##Array_create(pool);                                                                  \
        if (!heap->array)                                                                                            \
            return NULL;                                                                                             \
        return heap;                                                                                                 \
    }                                                                                                                \
                                                                                                                     \
    static inline int TYPENAME##_getNumElements(const TYPENAME *heap)                                                \
    {                                                                                                                \
        return TYPENAME##Array_getNumElements(heap->array);                                                          \
    }                                                                                                                \
                                                                                                                     \
    static inline bool TYPENAME##_reserve(DE_PTR_TYPE(TYPENAME) heap, int capacity)                                  \
    {                                                                                                                \
        return TYPENAME##Array_reserve(heap->array, capacity);                                                       \
    }                                                                                                                \
                                                                                                                     \
    static inline void TYPENAME##_reset(DE_PTR_TYPE(TYPENAME) heap)                                                  \
    {                                                                                                                \
        TYPENAME##Array_setSize(heap->array, 0);                                                                     \
    }                                                                                                                \
                                                                                                                     \
    static inline void TYPENAME##_moveDown(DE_PTR_TYPE(TYPENAME) heap, int ndx)                                      \
    {                                                                                                                \
        TYPENAME##Array *array = heap->array;                                                                        \
        int numElements        = TYPENAME##Array_getNumElements(array);                                              \
        for (;;)                                                                                                     \
        {                                                                                                            \
            int childNdx0 = 2 * ndx + 1;                                                                             \
            if (childNdx0 < numElements)                                                                             \
            {                                                                                                        \
                int childNdx1 = deMin32(childNdx0 + 1, numElements - 1);                                             \
                int childCmpRes =                                                                                    \
                    CMPFUNC(TYPENAME##Array_get(array, childNdx0), TYPENAME##Array_get(array, childNdx1));           \
                int minChildNdx = (childCmpRes == 1) ? childNdx1 : childNdx0;                                        \
                int cmpRes      = CMPFUNC(TYPENAME##Array_get(array, ndx), TYPENAME##Array_get(array, minChildNdx)); \
                if (cmpRes == 1)                                                                                     \
                {                                                                                                    \
                    TYPENAME##Array_swap(array, ndx, minChildNdx);                                                   \
                    ndx = minChildNdx;                                                                               \
                }                                                                                                    \
                else                                                                                                 \
                    break;                                                                                           \
            }                                                                                                        \
            else                                                                                                     \
                break;                                                                                               \
        }                                                                                                            \
    }                                                                                                                \
                                                                                                                     \
    static inline void TYPENAME##_moveUp(DE_PTR_TYPE(TYPENAME) heap, int ndx)                                        \
    {                                                                                                                \
        TYPENAME##Array *array = heap->array;                                                                        \
        while (ndx > 0)                                                                                              \
        {                                                                                                            \
            int parentNdx = (ndx - 1) >> 1;                                                                          \
            int cmpRes    = CMPFUNC(TYPENAME##Array_get(array, ndx), TYPENAME##Array_get(array, parentNdx));         \
            if (cmpRes == -1)                                                                                        \
            {                                                                                                        \
                TYPENAME##Array_swap(array, ndx, parentNdx);                                                         \
                ndx = parentNdx;                                                                                     \
            }                                                                                                        \
            else                                                                                                     \
                break;                                                                                               \
        }                                                                                                            \
    }                                                                                                                \
                                                                                                                     \
    static inline bool TYPENAME##_push(DE_PTR_TYPE(TYPENAME) heap, VALUETYPE elem)                                   \
    {                                                                                                                \
        TYPENAME##Array *array = heap->array;                                                                        \
        int numElements        = TYPENAME##Array_getNumElements(array);                                              \
        if (!TYPENAME##Array_setSize(array, numElements + 1))                                                        \
            return false;                                                                                            \
        TYPENAME##Array_set(array, numElements, elem);                                                               \
        TYPENAME##_moveUp(heap, numElements);                                                                        \
        return true;                                                                                                 \
    }                                                                                                                \
                                                                                                                     \
    static inline VALUETYPE TYPENAME##_popMin(DE_PTR_TYPE(TYPENAME) heap)                                            \
    {                                                                                                                \
        TYPENAME##Array *array = heap->array;                                                                        \
        VALUETYPE tmp          = TYPENAME##Array_get(array, 0);                                                      \
        int numElements        = TYPENAME##Array_getNumElements(array);                                              \
        TYPENAME##Array_set(array, 0, TYPENAME##Array_get(array, numElements - 1));                                  \
        TYPENAME##Array_setSize(array, numElements - 1);                                                             \
        TYPENAME##_moveDown(heap, 0);                                                                                \
        return tmp;                                                                                                  \
    }                                                                                                                \
                                                                                                                     \
    struct TYPENAME##Unused_s                                                                                        \
    {                                                                                                                \
        int unused;                                                                                                  \
    }

#endif /* _DEPOOLHEAP_H */
