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
 * \brief Memory pool multiset class.
 *//*--------------------------------------------------------------------*/

#include "dePoolMultiSet.h"

DE_DECLARE_POOL_MULTISET(deTestMultiSet, int16_t);
DE_IMPLEMENT_POOL_MULTISET(deTestMultiSet, int16_t, deInt16Hash, deInt16Equal);

void dePoolMultiSet_selfTest(void)
{
    deMemPool *pool     = deMemPool_createRoot(NULL, 0);
    deTestMultiSet *set = deTestMultiSet_create(pool);
    int i;

    /* Test exists() on empty set. */
    DE_TEST_ASSERT(deTestMultiSet_getNumElements(set) == 0);
    for (i = 0; i < 15000; i++)
        DE_TEST_ASSERT(!deTestMultiSet_exists(set, (int16_t)i));

    /* Test insert(). */
    for (i = 0; i < 5000; i++)
        deTestMultiSet_insert(set, (int16_t)i);

    DE_TEST_ASSERT(deTestMultiSet_getNumElements(set) == 5000);
    for (i = 0; i < 25000; i++)
    {
        bool inserted = deInBounds32(i, 0, 5000);
        bool found    = deTestMultiSet_exists(set, (int16_t)i);
        DE_TEST_ASSERT(found == inserted);
    }

    /* Test delete(). */
    for (i = 0; i < 1000; i++)
        deTestMultiSet_delete(set, (int16_t)i);

    DE_TEST_ASSERT(deTestMultiSet_getNumElements(set) == 4000);
    for (i = 0; i < 25000; i++)
    {
        bool inserted = deInBounds32(i, 1000, 5000);
        bool found    = deTestMultiSet_exists(set, (int16_t)i);
        DE_TEST_ASSERT(found == inserted);
    }

    /* Test insert() after delete(). */
    for (i = 10000; i < 12000; i++)
        deTestMultiSet_insert(set, (int16_t)i);

    DE_TEST_ASSERT(deTestMultiSet_getNumElements(set) == 6000);

    for (i = 0; i < 25000; i++)
    {
        bool inserted = (deInBounds32(i, 1000, 5000) || deInBounds32(i, 10000, 12000));
        bool found    = deTestMultiSet_exists(set, (int16_t)i);
        DE_TEST_ASSERT(found == inserted);
    }

    /* Test reset. */
    deTestMultiSet_reset(set);
    DE_TEST_ASSERT(deTestMultiSet_getNumElements(set) == 0);

    /* Test insertion multiple times. */
    for (i = 0; i < 1000; i++)
        deTestMultiSet_insert(set, (int16_t)i);
    for (i = 0; i < 500; i++)
        deTestMultiSet_insert(set, (int16_t)i);
    for (i = 0; i < 250; i++)
        deTestMultiSet_insert(set, (int16_t)i);

    DE_TEST_ASSERT(deTestMultiSet_getNumElements(set) == 1000 + 500 + 250);

    for (i = 0; i < 2000; i++)
    {
        int count    = 0;
        bool found   = deTestMultiSet_exists(set, (int16_t)i);
        int gotCount = deTestMultiSet_getKeyCount(set, (int16_t)i);

        count += deInBounds32(i, 0, 1000) ? 1 : 0;
        count += deInBounds32(i, 0, 500) ? 1 : 0;
        count += deInBounds32(i, 0, 250) ? 1 : 0;

        DE_TEST_ASSERT(found == (count > 0));
        DE_TEST_ASSERT(count == gotCount);
    }

    /* Test multiset deletion rules. */
    for (i = 0; i < 1000; i++)
        deTestMultiSet_delete(set, (int16_t)i);

    DE_TEST_ASSERT(deTestMultiSet_getNumElements(set) == 500 + 250);

    for (i = 0; i < 2000; i++)
    {
        int count    = 0;
        bool found   = deTestMultiSet_exists(set, (int16_t)i);
        int gotCount = deTestMultiSet_getKeyCount(set, (int16_t)i);

        count += deInBounds32(i, 0, 500) ? 1 : 0;
        count += deInBounds32(i, 0, 250) ? 1 : 0;

        DE_TEST_ASSERT(found == (count > 0));
        DE_TEST_ASSERT(count == gotCount);
    }

    /* Test setKeyCount(). */
    for (i = 0; i < 250; i++)
        deTestMultiSet_setKeyCount(set, (int16_t)i, 0);
    for (i = 750; i < 1000; i++)
        deTestMultiSet_setKeyCount(set, (int16_t)i, 3);

    DE_TEST_ASSERT(deTestMultiSet_getNumElements(set) == 250 * 4);

    for (i = 0; i < 2000; i++)
    {
        int count    = 0;
        bool found   = deTestMultiSet_exists(set, (int16_t)i);
        int gotCount = deTestMultiSet_getKeyCount(set, (int16_t)i);

        count += deInBounds32(i, 250, 500) ? 1 : 0;
        count += deInBounds32(i, 750, 1000) ? 3 : 0;

        DE_TEST_ASSERT(found == (count > 0));
        DE_TEST_ASSERT(gotCount == count);
    }

    deMemPool_destroy(pool);
}
