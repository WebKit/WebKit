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
 * \brief Memory pool hash-array class.
 *//*--------------------------------------------------------------------*/

#include "dePoolHashArray.h"

#include <string.h>

DE_DECLARE_POOL_HASH_ARRAY(deTestHashArray, int16_t, int, deInt16Array, deIntArray);
DE_IMPLEMENT_POOL_HASH_ARRAY(deTestHashArray, int16_t, int, deInt16Array, deIntArray, deInt16Hash, deInt16Equal);

void dePoolHashArray_selfTest(void)
{
    deMemPool *pool            = deMemPool_createRoot(NULL, 0);
    deTestHashArray *hashArray = deTestHashArray_create(pool);
    deInt16Array *keyArray     = deInt16Array_create(pool);
    deIntArray *valueArray     = deIntArray_create(pool);
    int iter;

    for (iter = 0; iter < 3; iter++)
    {
        int i;

        /* Insert a bunch of values. */
        DE_TEST_ASSERT(deTestHashArray_getNumElements(hashArray) == 0);
        for (i = 0; i < 20; i++)
        {
            deTestHashArray_insert(hashArray, (int16_t)(-i ^ 0x5), 2 * i + 5);
        }
        DE_TEST_ASSERT(deTestHashArray_getNumElements(hashArray) == 20);

        deTestHashArray_copyToArray(hashArray, keyArray, NULL);
        deTestHashArray_copyToArray(hashArray, NULL, valueArray);
        DE_TEST_ASSERT(deInt16Array_getNumElements(keyArray) == 20);
        DE_TEST_ASSERT(deIntArray_getNumElements(valueArray) == 20);

        for (i = 0; i < 20; i++)
        {
            DE_TEST_ASSERT(deInt16Array_get(keyArray, i) == (int16_t)(-i ^ 0x5));
            DE_TEST_ASSERT(deIntArray_get(valueArray, i) == 2 * i + 5);
        }

        deTestHashArray_reset(hashArray);
        DE_TEST_ASSERT(deTestHashArray_getNumElements(hashArray) == 0);

        deTestHashArray_copyToArray(hashArray, keyArray, NULL);
        deTestHashArray_copyToArray(hashArray, NULL, valueArray);

        DE_TEST_ASSERT(deInt16Array_getNumElements(keyArray) == 0);
        DE_TEST_ASSERT(deIntArray_getNumElements(valueArray) == 0);
    }

    deMemPool_destroy(pool);
}
