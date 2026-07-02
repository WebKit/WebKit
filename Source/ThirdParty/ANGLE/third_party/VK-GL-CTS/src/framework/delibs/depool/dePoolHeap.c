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

#include "dePoolHeap.h"
#include "deInt32.h"

#include <stdlib.h>
#include <string.h>

typedef struct HeapItem_s
{
    int priority;
    int value;
} HeapItem;

HeapItem HeapItem_create(int priority, int value)
{
    HeapItem h;
    h.priority = priority;
    h.value    = value;
    return h;
}

int HeapItem_cmp(HeapItem a, HeapItem b)
{
    if (a.priority < b.priority)
        return -1;
    if (a.priority > b.priority)
        return +1;
    return 0;
}

DE_DECLARE_POOL_HEAP(TestHeap, HeapItem, HeapItem_cmp);

/*--------------------------------------------------------------------*//*!
 * \internal
 * \brief Test heap functionality.
 *//*--------------------------------------------------------------------*/
void dePoolHeap_selfTest(void)
{
    deMemPool *pool = deMemPool_createRoot(NULL, 0);
    TestHeap *heap  = TestHeap_create(pool);
    int i;

    TestHeap_push(heap, HeapItem_create(10, 10));
    TestHeap_push(heap, HeapItem_create(0, 10));
    TestHeap_push(heap, HeapItem_create(20, 10));
    DE_TEST_ASSERT(TestHeap_getNumElements(heap) == 3);

    DE_TEST_ASSERT(TestHeap_popMin(heap).priority == 0);
    DE_TEST_ASSERT(TestHeap_popMin(heap).priority == 10);
    DE_TEST_ASSERT(TestHeap_popMin(heap).priority == 20);
    DE_TEST_ASSERT(TestHeap_getNumElements(heap) == 0);

    /* Push items -1000..1000 into heap. */
    for (i = -1000; i < 1000; i++)
    {
        /* Unused alloc to try to break alignments. */
        deMemPool_alloc(pool, 1);
        TestHeap_push(heap, HeapItem_create(i, -i));
    }
    DE_TEST_ASSERT(TestHeap_getNumElements(heap) == 2000);

    /* Push items -2500..-3000 into heap. */
    for (i = -2501; i >= -3000; i--)
        TestHeap_push(heap, HeapItem_create(i, -i));
    DE_TEST_ASSERT(TestHeap_getNumElements(heap) == 2500);

    /* Push items 6000..7500 into heap. */
    for (i = 6000; i < 7500; i++)
        TestHeap_push(heap, HeapItem_create(i, -i));
    DE_TEST_ASSERT(TestHeap_getNumElements(heap) == 4000);

    /* Pop -3000..-2500 from heap. */
    for (i = -3000; i < -2500; i++)
    {
        HeapItem h = TestHeap_popMin(heap);
        DE_TEST_ASSERT(h.priority == i);
        DE_TEST_ASSERT(h.value == -h.priority);
    }
    DE_TEST_ASSERT(TestHeap_getNumElements(heap) == 3500);

    /* Pop -1000..1000 from heap. */
    for (i = -1000; i < 1000; i++)
    {
        HeapItem h = TestHeap_popMin(heap);
        DE_TEST_ASSERT(h.priority == i);
        DE_TEST_ASSERT(h.value == -h.priority);
    }
    DE_TEST_ASSERT(TestHeap_getNumElements(heap) == 1500);

    /* Pop 6000..7500 from heap. */
    for (i = 6000; i < 7500; i++)
    {
        HeapItem h = TestHeap_popMin(heap);
        DE_TEST_ASSERT(h.priority == i);
        DE_TEST_ASSERT(h.value == -h.priority);
    }
    DE_TEST_ASSERT(TestHeap_getNumElements(heap) == 0);

    deMemPool_destroy(pool);
}
