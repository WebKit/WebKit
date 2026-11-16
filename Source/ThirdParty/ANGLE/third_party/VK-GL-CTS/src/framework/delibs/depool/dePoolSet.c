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
 * \brief Memory pool set class.
 *//*--------------------------------------------------------------------*/

#include "dePoolSet.h"

#include <string.h>

DE_DECLARE_POOL_SET(deTestSet, int16_t);
DE_IMPLEMENT_POOL_SET(deTestSet, int16_t, deInt16Hash, deInt16Equal);

void dePoolSet_selfTest(void)
{
    deMemPool *pool = deMemPool_createRoot(NULL, 0);
    deTestSet *set  = deTestSet_create(pool);
    int i;

    /* Test exists() on empty set. */
    DE_TEST_ASSERT(deTestSet_getNumElements(set) == 0);
    for (i = 0; i < 15000; i++)
        DE_TEST_ASSERT(!deTestSet_exists(set, (int16_t)i));

    /* Test insert(). */
    for (i = 0; i < 5000; i++)
        deTestSet_insert(set, (int16_t)i);

    DE_TEST_ASSERT(deTestSet_getNumElements(set) == 5000);
    for (i = 0; i < 25000; i++)
    {
        bool inserted = deInBounds32(i, 0, 5000);
        bool found    = deTestSet_exists(set, (int16_t)i);
        DE_TEST_ASSERT(found == inserted);
    }

    /* Test delete(). */
    for (i = 0; i < 1000; i++)
        deTestSet_delete(set, (int16_t)i);

    DE_TEST_ASSERT(deTestSet_getNumElements(set) == 4000);
    for (i = 0; i < 25000; i++)
    {
        bool inserted = deInBounds32(i, 1000, 5000);
        bool found    = deTestSet_exists(set, (int16_t)i);
        DE_TEST_ASSERT(found == inserted);
    }

    /* Test insert() after delete(). */
    for (i = 10000; i < 12000; i++)
        deTestSet_insert(set, (int16_t)i);

    DE_TEST_ASSERT(deTestSet_getNumElements(set) == 6000);

    for (i = 0; i < 25000; i++)
    {
        bool inserted = (deInBounds32(i, 1000, 5000) || deInBounds32(i, 10000, 12000));
        bool found    = deTestSet_exists(set, (int16_t)i);
        DE_TEST_ASSERT(found == inserted);
    }

    /* Test iterator. */
    {
        deTestSetIter iter;
        int numFound = 0;

        for (deTestSetIter_init(set, &iter); deTestSetIter_hasItem(&iter); deTestSetIter_next(&iter))
        {
            int16_t key = deTestSetIter_getKey(&iter);
            DE_TEST_ASSERT(deInBounds32(key, 1000, 5000) || deInBounds32(key, 10000, 12000));
            DE_TEST_ASSERT(deTestSet_exists(set, key));
            numFound++;
        }

        DE_TEST_ASSERT(numFound == deTestSet_getNumElements(set));
    }

    deMemPool_destroy(pool);
}
