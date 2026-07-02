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
 *
 * Features of the pooled arrays:
 * - single indirection layer (grows exponentially)
 * - constant # elements per page
 * - recycles old indirection tables as element pages
 * - about 10% overhead on large arrays
 *//*--------------------------------------------------------------------*/

#include "dePoolArray.h"
#include "deInt32.h"

#include <stdlib.h>
#include <string.h>

/*--------------------------------------------------------------------*//*!
 * \internal
 * \brief Create a new pool array.
 * \param pool            Pool to allocate memory from.
 * \param elementSize    Size of the element to be put in array.
 * \param Pointer to the created array, or null on failure.
 *//*--------------------------------------------------------------------*/
dePoolArray *dePoolArray_create(deMemPool *pool, int elementSize)
{
    /* Alloc struct. */
    dePoolArray *arr = DE_POOL_NEW(pool, dePoolArray);
    if (!arr)
        return NULL;

    /* Init array. */
    memset(arr, 0, sizeof(dePoolArray));
    arr->pool        = pool;
    arr->elementSize = elementSize;

    return arr;
}

/*--------------------------------------------------------------------*//*!
 * \internal
 * \brief Ensure that the array can hold at least N elements.
 * \param arr    Array pointer.
 * \param size    Number of elements for which to reserve memory.
 * \param True on success, false on failure.
 *//*--------------------------------------------------------------------*/
bool dePoolArray_reserve(dePoolArray *arr, int size)
{
    if (size >= arr->capacity)
    {
        void *oldPageTable   = NULL;
        int oldPageTableSize = 0;

        int newCapacity          = deAlign32(size, 1 << DE_ARRAY_ELEMENTS_PER_PAGE_LOG2);
        int reqPageTableCapacity = newCapacity >> DE_ARRAY_ELEMENTS_PER_PAGE_LOG2;

        if (arr->pageTableCapacity < reqPageTableCapacity)
        {
            int newPageTableCapacity = deMax32(2 * arr->pageTableCapacity, reqPageTableCapacity);
            void **newPageTable = (void **)deMemPool_alloc(arr->pool, (size_t)newPageTableCapacity * sizeof(void *));
            int i;

            if (!newPageTable)
                return false;

            for (i = 0; i < arr->pageTableCapacity; i++)
                newPageTable[i] = arr->pageTable[i];

            for (; i < newPageTableCapacity; i++)
                newPageTable[i] = NULL;

            /* Grab information about old page table for recycling purposes. */
            oldPageTable     = arr->pageTable;
            oldPageTableSize = arr->pageTableCapacity * (int)sizeof(void *);

            arr->pageTable         = newPageTable;
            arr->pageTableCapacity = newPageTableCapacity;
        }

        /* Allocate new pages. */
        {
            int pageAllocSize = arr->elementSize << DE_ARRAY_ELEMENTS_PER_PAGE_LOG2;
            int pageTableNdx  = arr->capacity >> DE_ARRAY_ELEMENTS_PER_PAGE_LOG2;

            /* Allocate new pages from recycled old page table. */
            while (oldPageTableSize >= pageAllocSize)
            {
                void *newPage = oldPageTable;
                DE_ASSERT(arr->pageTableCapacity > pageTableNdx); /* \todo [petri] is this always true? */
                DE_ASSERT(!arr->pageTable[pageTableNdx]);
                arr->pageTable[pageTableNdx++] = newPage;

                oldPageTable = (void *)((uint8_t *)oldPageTable + pageAllocSize);
                oldPageTableSize -= pageAllocSize;
            }

            /* Allocate the rest of the needed pages from the pool. */
            for (; pageTableNdx < reqPageTableCapacity; pageTableNdx++)
            {
                void *newPage = deMemPool_alloc(arr->pool, (size_t)pageAllocSize);
                if (!newPage)
                    return false;

                DE_ASSERT(!arr->pageTable[pageTableNdx]);
                arr->pageTable[pageTableNdx] = newPage;
            }

            arr->capacity = pageTableNdx << DE_ARRAY_ELEMENTS_PER_PAGE_LOG2;
            DE_ASSERT(arr->capacity >= newCapacity);
        }
    }

    return true;
}

/*--------------------------------------------------------------------*//*!
 * \internal
 * \brief Set the size of the array (also reserves capacity).
 * \param arr    Array pointer.
 * \param size    New size of the array (in elements).
 * \param True on success, false on failure.
 *//*--------------------------------------------------------------------*/
bool dePoolArray_setSize(dePoolArray *arr, int size)
{
    if (!dePoolArray_reserve(arr, size))
        return false;

    arr->numElements = size;
    return true;
}

DE_DECLARE_POOL_ARRAY(dePoolIntArray, int);
DE_DECLARE_POOL_ARRAY(dePoolInt16Array, int16_t);

/*--------------------------------------------------------------------*//*!
 * \internal
 * \brief Test array functionality.
 *//*--------------------------------------------------------------------*/
void dePoolArray_selfTest(void)
{
    deMemPool *pool         = deMemPool_createRoot(NULL, 0);
    dePoolIntArray *arr     = dePoolIntArray_create(pool);
    dePoolInt16Array *arr16 = dePoolInt16Array_create(pool);
    int i;

    /* Test pushBack(). */
    for (i = 0; i < 5000; i++)
    {
        /* Unused alloc to try to break alignments. */
        deMemPool_alloc(pool, 1);

        dePoolIntArray_pushBack(arr, i);
        dePoolInt16Array_pushBack(arr16, (int16_t)i);
    }

    DE_TEST_ASSERT(dePoolIntArray_getNumElements(arr) == 5000);
    DE_TEST_ASSERT(dePoolInt16Array_getNumElements(arr16) == 5000);
    for (i = 0; i < 5000; i++)
    {
        DE_TEST_ASSERT(dePoolIntArray_get(arr, i) == i);
        DE_TEST_ASSERT(dePoolInt16Array_get(arr16, i) == i);
    }

    /* Test popBack(). */
    for (i = 0; i < 1000; i++)
    {
        DE_TEST_ASSERT(dePoolIntArray_popBack(arr) == (4999 - i));
        DE_TEST_ASSERT(dePoolInt16Array_popBack(arr16) == (4999 - i));
    }

    DE_TEST_ASSERT(dePoolIntArray_getNumElements(arr) == 4000);
    DE_TEST_ASSERT(dePoolInt16Array_getNumElements(arr16) == 4000);
    for (i = 0; i < 4000; i++)
    {
        DE_TEST_ASSERT(dePoolIntArray_get(arr, i) == i);
        DE_TEST_ASSERT(dePoolInt16Array_get(arr16, i) == i);
    }

    /* Test setSize(). */
    dePoolIntArray_setSize(arr, 1000);
    dePoolInt16Array_setSize(arr16, 1000);
    for (i = 1000; i < 5000; i++)
    {
        dePoolIntArray_pushBack(arr, i);
        dePoolInt16Array_pushBack(arr16, (int16_t)i);
    }

    DE_TEST_ASSERT(dePoolIntArray_getNumElements(arr) == 5000);
    DE_TEST_ASSERT(dePoolInt16Array_getNumElements(arr16) == 5000);
    for (i = 0; i < 5000; i++)
    {
        DE_TEST_ASSERT(dePoolIntArray_get(arr, i) == i);
        DE_TEST_ASSERT(dePoolInt16Array_get(arr16, i) == i);
    }

    /* Test set() and pushBack() with reserve(). */
    arr = dePoolIntArray_create(pool);
    dePoolIntArray_setSize(arr, 1500);
    dePoolIntArray_reserve(arr, 2000);
    for (i = 0; i < 1500; i++)
        dePoolIntArray_set(arr, i, i);
    for (; i < 5000; i++)
        dePoolIntArray_pushBack(arr, i);

    DE_TEST_ASSERT(dePoolIntArray_getNumElements(arr) == 5000);
    for (i = 0; i < 5000; i++)
    {
        int val = dePoolIntArray_get(arr, i);
        DE_TEST_ASSERT(val == i);
    }

    deMemPool_destroy(pool);
}
