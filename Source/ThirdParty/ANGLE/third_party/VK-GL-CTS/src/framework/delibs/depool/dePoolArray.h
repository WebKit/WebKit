#ifndef _DEPOOLARRAY_H
#define _DEPOOLARRAY_H
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
 * \brief Memory pool array class.
 *//*--------------------------------------------------------------------*/

#include "deDefs.h"
#include "deMemPool.h"

enum
{
    DE_ARRAY_ELEMENTS_PER_PAGE_LOG2 = 4 /*!< \internal 16 */
};

/*--------------------------------------------------------------------*//*!
 * \internal
 * \brief Type-independent version of the array template class.
 *//*--------------------------------------------------------------------*/
typedef struct dePoolArray_s
{
    deMemPool *pool; /*!< Pool from which all memory is allocated from.    */

    int elementSize; /*!< Size of the element (in bytes).                */
    int numElements; /*!< Number of elements in the array.                */
    int capacity;    /*!< Number of allocated elements in the array.        */

    int pageTableCapacity; /*!< Size of the page table.                        */
    void **pageTable;      /*!< Pointer to the page table.                        */
} dePoolArray;

DE_BEGIN_EXTERN_C

dePoolArray *dePoolArray_create(deMemPool *pool, int elementSize);
bool dePoolArray_reserve(dePoolArray *arr, int capacity);
bool dePoolArray_setSize(dePoolArray *arr, int size);

void dePoolArray_selfTest(void);

DE_END_EXTERN_C

/*--------------------------------------------------------------------*//*!
 * \brief Declare a template pool array class.
 * \param TYPENAME    Type name of the declared array.
 * \param VALUETYPE    Type of the value contained in the array.
 *
 * This macro declares a pool array with all the necessary functions for
 * operating with it. All allocated memory is taken from the memory pool
 * given to the constructor.
 *
 * The array is implemented by having a set of pages (which store the
 * elements) and a page table with pointers to each of them. The pages
 * are allocated individually whenever they are needed, but the page
 * table grows exponentially. This keeps the memory overhead for large
 * arrays very small. On the other hand, the relative overhead for very
 * small arrays is prohibitive (the minimum allocation is 16 elements).
 *
 * The functions for operating the array are:
 * \todo [petri] Figure out how to comment these in Doxygen-style.
 *
 * \code
 * Array*   Array_create            (deMemPool* pool);
 * int      Array_getNumElements    (const Array* array);
 * bool   Array_reserve           (Array* array, int size);
 * bool   Array_setSize           (Array* array, int size);
 * void        Array_reset                (Array* array);
 * Element  Array_get               (Array* array, int ndx);
 * bool   Array_set               (Array* array, int ndx, Element elem);
 * bool   Array_pushBack          (Array* array, Element elem);
 * Element  Array_popBack           (Array* array);
 * void     Array_swap              (Array* array, int aNdx, int bNdx);
 * \endcode
*//*--------------------------------------------------------------------*/
#define DE_DECLARE_POOL_ARRAY(TYPENAME, VALUETYPE)                                                               \
                                                                                                                 \
    typedef struct TYPENAME##_s                                                                                  \
    {                                                                                                            \
        deMemPool *pool;                                                                                         \
                                                                                                                 \
        int elementSize;                                                                                         \
        int numElements;                                                                                         \
        int capacity;                                                                                            \
                                                                                                                 \
        int pageTableCapacity;                                                                                   \
        DE_PTR_TYPE(VALUETYPE) * pageTable;                                                                      \
    } TYPENAME; /* NOLINT(TYPENAME) */                                                                           \
                                                                                                                 \
    static inline TYPENAME *TYPENAME##_create(deMemPool *pool);                                                  \
    static inline int TYPENAME##_getNumElements(const TYPENAME *arr) DE_UNUSED_FUNCTION;                         \
    static inline bool TYPENAME##_reserve(DE_PTR_TYPE(TYPENAME) arr, int capacity) DE_UNUSED_FUNCTION;           \
    static inline bool TYPENAME##_setSize(DE_PTR_TYPE(TYPENAME) arr, int size) DE_UNUSED_FUNCTION;               \
    static inline void TYPENAME##_reset(DE_PTR_TYPE(TYPENAME) arr) DE_UNUSED_FUNCTION;                           \
    static inline VALUETYPE TYPENAME##_get(const TYPENAME *arr, int ndx) DE_UNUSED_FUNCTION;                     \
    static inline void TYPENAME##_set(DE_PTR_TYPE(TYPENAME) arr, int ndx, VALUETYPE elem) DE_UNUSED_FUNCTION;    \
    static inline bool TYPENAME##_pushBack(DE_PTR_TYPE(TYPENAME) arr, VALUETYPE elem) DE_UNUSED_FUNCTION;        \
    static inline VALUETYPE TYPENAME##_popBack(DE_PTR_TYPE(TYPENAME) arr) DE_UNUSED_FUNCTION;                    \
    static inline bool TYPENAME##_copy(DE_PTR_TYPE(TYPENAME) dst, const TYPENAME *src) DE_UNUSED_FUNCTION;       \
    static inline void TYPENAME##_swap(DE_PTR_TYPE(TYPENAME) arr, int aNdx, int bNdx) DE_UNUSED_FUNCTION;        \
                                                                                                                 \
    static inline TYPENAME *TYPENAME##_create(deMemPool *pool)                                                   \
    {                                                                                                            \
        return (TYPENAME *)dePoolArray_create(pool, sizeof(VALUETYPE));                                          \
    }                                                                                                            \
                                                                                                                 \
    static inline int TYPENAME##_getNumElements(const TYPENAME *arr)                                             \
    {                                                                                                            \
        return arr->numElements;                                                                                 \
    }                                                                                                            \
                                                                                                                 \
    static inline bool TYPENAME##_reserve(DE_PTR_TYPE(TYPENAME) arr, int capacity)                               \
    {                                                                                                            \
        if (capacity > arr->capacity)                                                                            \
            return dePoolArray_reserve((dePoolArray *)arr, capacity);                                            \
        return true;                                                                                             \
    }                                                                                                            \
                                                                                                                 \
    static inline bool TYPENAME##_setSize(DE_PTR_TYPE(TYPENAME) arr, int size)                                   \
    {                                                                                                            \
        if (size > arr->capacity)                                                                                \
            return dePoolArray_setSize((dePoolArray *)arr, size);                                                \
                                                                                                                 \
        arr->numElements = size;                                                                                 \
        return true;                                                                                             \
    }                                                                                                            \
                                                                                                                 \
    static inline void TYPENAME##_reset(DE_PTR_TYPE(TYPENAME) arr)                                               \
    {                                                                                                            \
        arr->numElements = 0;                                                                                    \
    }                                                                                                            \
                                                                                                                 \
    static inline VALUETYPE TYPENAME##_get(const TYPENAME *arr, int ndx)                                         \
    {                                                                                                            \
        DE_ASSERT(ndx >= 0 && ndx < arr->numElements);                                                           \
        {                                                                                                        \
            int pageNdx = (ndx >> DE_ARRAY_ELEMENTS_PER_PAGE_LOG2);                                              \
            int subNdx  = ndx & ((1 << DE_ARRAY_ELEMENTS_PER_PAGE_LOG2) - 1);                                    \
            return ((VALUETYPE *)arr->pageTable[pageNdx])[subNdx];                                               \
        }                                                                                                        \
    }                                                                                                            \
                                                                                                                 \
    static inline void TYPENAME##_set(DE_PTR_TYPE(TYPENAME) arr, int ndx, VALUETYPE elem)                        \
    {                                                                                                            \
        DE_ASSERT(ndx >= 0 && ndx < arr->numElements);                                                           \
        {                                                                                                        \
            int pageNdx                                    = (ndx >> DE_ARRAY_ELEMENTS_PER_PAGE_LOG2);           \
            int subNdx                                     = ndx & ((1 << DE_ARRAY_ELEMENTS_PER_PAGE_LOG2) - 1); \
            ((VALUETYPE *)arr->pageTable[pageNdx])[subNdx] = elem;                                               \
        }                                                                                                        \
    }                                                                                                            \
                                                                                                                 \
    static inline bool TYPENAME##_pushBack(DE_PTR_TYPE(TYPENAME) arr, VALUETYPE elem)                            \
    {                                                                                                            \
        if ((arr->numElements + 1 >= arr->capacity) && !TYPENAME##_reserve(arr, arr->numElements + 1))           \
            return false;                                                                                        \
        arr->numElements++;                                                                                      \
        TYPENAME##_set(arr, arr->numElements - 1, elem);                                                         \
        return true;                                                                                             \
    }                                                                                                            \
                                                                                                                 \
    static inline VALUETYPE TYPENAME##_popBack(DE_PTR_TYPE(TYPENAME) arr)                                        \
    {                                                                                                            \
        int ndx     = arr->numElements - 1;                                                                      \
        int pageNdx = (ndx >> DE_ARRAY_ELEMENTS_PER_PAGE_LOG2);                                                  \
        int subNdx  = ndx & ((1 << DE_ARRAY_ELEMENTS_PER_PAGE_LOG2) - 1);                                        \
        DE_ASSERT(arr->numElements > 0);                                                                         \
        arr->numElements--;                                                                                      \
        /* \note We access a value which is out-of-bounds, but we know it to be safe. */                         \
        return ((VALUETYPE *)arr->pageTable[pageNdx])[subNdx];                                                   \
    }                                                                                                            \
                                                                                                                 \
    static inline bool TYPENAME##_copy(DE_PTR_TYPE(TYPENAME) dst, const TYPENAME *src)                           \
    {                                                                                                            \
        DE_ASSERT(dst &&src);                                                                                    \
        {                                                                                                        \
            int numElements = src->numElements;                                                                  \
            int ndx;                                                                                             \
            if (!TYPENAME##_setSize(dst, numElements))                                                           \
                return false;                                                                                    \
            for (ndx = 0; ndx < numElements; ndx++)                                                              \
                TYPENAME##_set(dst, ndx, TYPENAME##_get(src, ndx));                                              \
        }                                                                                                        \
        return true;                                                                                             \
    }                                                                                                            \
                                                                                                                 \
    static inline void TYPENAME##_swap(DE_PTR_TYPE(TYPENAME) arr, int aNdx, int bNdx)                            \
    {                                                                                                            \
        VALUETYPE tmp = TYPENAME##_get(arr, aNdx);                                                               \
        TYPENAME##_set(arr, aNdx, TYPENAME##_get(arr, bNdx));                                                    \
        TYPENAME##_set(arr, bNdx, tmp);                                                                          \
    }                                                                                                            \
                                                                                                                 \
    struct TYPENAME##Unused_s                                                                                    \
    {                                                                                                            \
        int unused;                                                                                              \
    }

/*--------------------------------------------------------------------*//*!
 * \brief Declare a sort function for an array.
 * \param TYPENAME    Type name of the declared array.
 * \param VALUETYPE    Type of the value contained in the array.
 * \param SORTNAME    Name for this specific sort.
 * \param CMPFUNC    Comparison function for sorting.
 *
 * This macro declares a sort function for an array declared using
 * DE_DECLARE_POOL_ARRAY macro.
 *
 * Sorting algorithm is heap sort since it requires constant amount of
 * auxiliary space and is in-place sort. Worst-case run-time is O(n log n)
 * and sort is NOT stable.
 *
 * CMPFUNC is used to compare elements in array. It must accept two
 * parameters and return negative integer if first is smaller than, 0 if
 * both are equal and positive integer if first is larger than second.
 *
 * The functions for sorting array are:
 * \todo [petri] Figure out how to comment these in Doxygen-style.
 *
 * \code
 * void        Array_sortName            (Array* array);
 * void        Array_sortNameHeapify    (Array* array);
 * void        Array_sortNameShiftDown    (Array* array, int start, int end);
 * \endcode
*//*--------------------------------------------------------------------*/
#define DE_DECLARE_POOL_ARRAY_SORT(TYPENAME, VALUETYPE, SORTNAME, CMPFUNC)                                   \
                                                                                                             \
    static inline void TYPENAME##_##SORTNAME##ShiftDown(DE_PTR_TYPE(TYPENAME) arr, int startNdx, int endNdx) \
    {                                                                                                        \
        int rootNdx = startNdx;                                                                              \
                                                                                                             \
        while (rootNdx * 2 + 1 <= endNdx)                                                                    \
        {                                                                                                    \
            int childNdx = rootNdx * 2 + 1;                                                                  \
                                                                                                             \
            if ((childNdx + 1 <= endNdx) &&                                                                  \
                (CMPFUNC(TYPENAME##_get(arr, childNdx), TYPENAME##_get(arr, childNdx + 1)) < 0))             \
                childNdx += 1;                                                                               \
                                                                                                             \
            if (CMPFUNC(TYPENAME##_get(arr, rootNdx), TYPENAME##_get(arr, childNdx)) < 0)                    \
            {                                                                                                \
                TYPENAME##_swap(arr, rootNdx, childNdx);                                                     \
                rootNdx = childNdx;                                                                          \
            }                                                                                                \
            else                                                                                             \
                break;                                                                                       \
        }                                                                                                    \
    }                                                                                                        \
                                                                                                             \
    static inline void TYPENAME##_##SORTNAME##Heapify(DE_PTR_TYPE(TYPENAME) arr)                             \
    {                                                                                                        \
        int startNdx = (TYPENAME##_getNumElements(arr) - 2) / 2;                                             \
                                                                                                             \
        while (startNdx >= 0)                                                                                \
        {                                                                                                    \
            TYPENAME##_##SORTNAME##ShiftDown(arr, startNdx, TYPENAME##_getNumElements(arr) - 1);             \
            startNdx -= 1;                                                                                   \
        }                                                                                                    \
    }                                                                                                        \
                                                                                                             \
    static inline void TYPENAME##_##SORTNAME(DE_PTR_TYPE(TYPENAME) arr)                                      \
    {                                                                                                        \
        int endNdx = TYPENAME##_getNumElements(arr) - 1;                                                     \
                                                                                                             \
        TYPENAME##_##SORTNAME##Heapify(arr);                                                                 \
                                                                                                             \
        while (endNdx > 0)                                                                                   \
        {                                                                                                    \
            TYPENAME##_swap(arr, endNdx, 0);                                                                 \
            endNdx -= 1;                                                                                     \
            TYPENAME##_##SORTNAME##ShiftDown(arr, 0, endNdx);                                                \
        }                                                                                                    \
    }                                                                                                        \
                                                                                                             \
    struct TYPENAME##SORTNAME##unused_s                                                                      \
    {                                                                                                        \
        int unused;                                                                                          \
    }

/* Basic array types. */

DE_DECLARE_POOL_ARRAY(deIntArray, int);
DE_DECLARE_POOL_ARRAY(deInt8Array, int8_t);
DE_DECLARE_POOL_ARRAY(deUint8Array, uint8_t);
DE_DECLARE_POOL_ARRAY(deInt16Array, int16_t);
DE_DECLARE_POOL_ARRAY(deUint16Array, uint16_t);
DE_DECLARE_POOL_ARRAY(deInt32Array, int32_t);
DE_DECLARE_POOL_ARRAY(deUint32Array, uint32_t);

#endif /* _DEPOOLARRAY_H */
