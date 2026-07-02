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
 * \brief Memory pool hash-set class.
 *//*--------------------------------------------------------------------*/

#include "dePoolHashSet.h"

#include <string.h>

DE_DECLARE_POOL_HASH_SET(deTestHashSet, int16_t, int);
DE_IMPLEMENT_POOL_HASH_SET(deTestHashSet, int16_t, int, deInt16Hash, deInt16Equal, deInt32Hash, deInt32Equal);

void dePoolHashSet_selfTest(void)
{
    deMemPool *pool        = deMemPool_createRoot(NULL, 0);
    deTestHashSet *hashSet = deTestHashSet_create(pool);
    int i;

    /* Insert a bunch of values. */
    DE_TEST_ASSERT(deTestHashSet_getNumElements(hashSet) == 0);
    for (i = 0; i < 20; i++)
    {
        int j;
        for (j = 0; j < i; j++)
            deTestHashSet_insert(hashSet, (int16_t)i, 2 * j + 5);
    }
    DE_TEST_ASSERT(deTestHashSet_getNumElements(hashSet) == 19);

    /* delete(). */
    for (i = 0; i < 20; i++)
    {
        int j;
        for (j = 0; j < i; j += 2)
            deTestHashSet_delete(hashSet, (int16_t)i, 2 * j + 5);
    }
    DE_TEST_ASSERT(deTestHashSet_getNumElements(hashSet) == 19);

#if 0
    /* Test find() on empty hash. */
    DE_TEST_ASSERT(deTestHash_getNumElements(hashSet) == 0);
    for (i = 0; i < 15000; i++)
    {
        const int* val = deTestHash_find(hash, (int16_t)i);
        DE_TEST_ASSERT(!val);
    }

    /* Test insert(). */
    for (i = 0; i < 5000; i++)
    {
        deTestHash_insert(hash, (int16_t)i, -i);
    }

    DE_TEST_ASSERT(deTestHash_getNumElements(hash) == 5000);
    for (i = 0; i < 5000; i++)
    {
        const int* val = deTestHash_find(hash, (int16_t)i);
        DE_TEST_ASSERT(val && (*val == -i));
    }

    /* Test delete(). */
    for (i = 0; i < 1000; i++)
        deTestHash_delete(hash, (int16_t)i);

    DE_TEST_ASSERT(deTestHash_getNumElements(hash) == 4000);
    for (i = 0; i < 25000; i++)
    {
        const int* val = deTestHash_find(hash, (int16_t)i);
        if (deInBounds32(i, 1000, 5000))
            DE_TEST_ASSERT(val && (*val == -i));
        else
            DE_TEST_ASSERT(!val);
    }

    /* Test insert() after delete(). */
    for (i = 10000; i < 12000; i++)
        deTestHash_insert(hash, (int16_t)i, -i);

    for (i = 0; i < 25000; i++)
    {
        const int* val = deTestHash_find(hash, (int16_t)i);
        if (deInBounds32(i, 1000, 5000) || deInBounds32(i, 10000, 12000))
            DE_TEST_ASSERT(val && (*val == -i));
        else
            DE_TEST_ASSERT(!val);
    }

    /* Test iterator. */
    {
        deTestHashIter    iter;
        int                numFound = 0;

        for (deTestHashIter_init(hash, &iter); deTestHashIter_hasItem(&iter); deTestHashIter_next(&iter))
        {
            int16_t    key = deTestHashIter_getKey(&iter);
            int        val = deTestHashIter_getValue(&iter);
            DE_TEST_ASSERT(deInBounds32(key, 1000, 5000) || deInBounds32(key, 10000, 12000));
            DE_TEST_ASSERT(*deTestHash_find(hash, key) == -key);
            DE_TEST_ASSERT(val == -key);
            numFound++;
        }

        DE_TEST_ASSERT(numFound == deTestHash_getNumElements(hash));
    }

    /* Test copy-to-array. */
    {
        deTestInt16Array*    keyArray = deTestInt16Array_create(pool);
        deTestIntArray*        valueArray = deTestIntArray_create(pool);
        int                    numElements = deTestHash_getNumElements(hash);
        int                    ndx;

        deTestHash_copyToArray(hash, keyArray, NULL);
        DE_TEST_ASSERT(deTestInt16Array_getNumElements(keyArray) == numElements);

        deTestHash_copyToArray(hash, NULL, valueArray);
        DE_TEST_ASSERT(deTestIntArray_getNumElements(valueArray) == numElements);

        deTestInt16Array_setSize(keyArray, 0);
        deTestIntArray_setSize(valueArray, 0);
        deTestHash_copyToArray(hash, keyArray, valueArray);
        DE_TEST_ASSERT(deTestInt16Array_getNumElements(keyArray) == numElements);
        DE_TEST_ASSERT(deTestIntArray_getNumElements(valueArray) == numElements);

        for (ndx = 0; ndx < numElements; ndx++)
        {
            int16_t key = deTestInt16Array_get(keyArray, ndx);
            int        val = deTestIntArray_get(valueArray, ndx);

            DE_TEST_ASSERT(val == -key);
            DE_TEST_ASSERT(*deTestHash_find(hash, key) == val);
        }
    }
#endif

    deMemPool_destroy(pool);
}
