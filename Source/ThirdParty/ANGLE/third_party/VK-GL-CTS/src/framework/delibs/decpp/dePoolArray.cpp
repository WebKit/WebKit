/*-------------------------------------------------------------------------
 * drawElements C++ Base Library
 * -----------------------------
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
 * \brief Array template backed by memory pool.
 *//*--------------------------------------------------------------------*/

#include "dePoolArray.hpp"

#include <algorithm>
#include <vector>

namespace de
{

static void intArrayTest(void)
{
    MemPool pool;
    PoolArray<int> arr(&pool);
    PoolArray<uint16_t> arr16(&pool);
    int i;

    /* Test pushBack(). */
    for (i = 0; i < 5000; i++)
    {
        /* Unused alloc to try to break alignments. */
        pool.alloc(1);

        arr.pushBack(i);
        arr16.pushBack((int16_t)i);
    }

    DE_TEST_ASSERT(arr.size() == 5000);
    DE_TEST_ASSERT(arr16.size() == 5000);
    for (i = 0; i < 5000; i++)
    {
        DE_TEST_ASSERT(arr[i] == i);
        DE_TEST_ASSERT(arr16[i] == i);
    }

    /* Test popBack(). */
    for (i = 0; i < 1000; i++)
    {
        DE_TEST_ASSERT(arr.popBack() == (4999 - i));
        DE_TEST_ASSERT(arr16.popBack() == (4999 - i));
    }

    DE_TEST_ASSERT(arr.size() == 4000);
    DE_TEST_ASSERT(arr16.size() == 4000);
    for (i = 0; i < 4000; i++)
    {
        DE_TEST_ASSERT(arr[i] == i);
        DE_TEST_ASSERT(arr16[i] == i);
    }

    /* Test resize(). */
    arr.resize(1000);
    arr16.resize(1000);
    for (i = 1000; i < 5000; i++)
    {
        arr.pushBack(i);
        arr16.pushBack((int16_t)i);
    }

    DE_TEST_ASSERT(arr.size() == 5000);
    DE_TEST_ASSERT(arr16.size() == 5000);
    for (i = 0; i < 5000; i++)
    {
        DE_TEST_ASSERT(arr[i] == i);
        DE_TEST_ASSERT(arr16[i] == i);
    }

    /* Test set() and pushBack() with reserve(). */
    PoolArray<int> arr2(&pool);
    arr2.resize(1500);
    arr2.reserve(2000);
    for (i = 0; i < 1500; i++)
        arr2[i] = i;
    for (; i < 5000; i++)
        arr2.pushBack(i);

    DE_TEST_ASSERT(arr2.size() == 5000);
    for (i = 0; i < 5000; i++)
    {
        int val = arr2[i];
        DE_TEST_ASSERT(val == i);
    }
}

static void alignedIntArrayTest(void)
{
    MemPool pool;
    PoolArray<int, 16> arr(&pool);
    PoolArray<uint16_t, 8> arr16(&pool);
    int i;

    /* Test pushBack(). */
    for (i = 0; i < 5000; i++)
    {
        /* Unused alloc to try to break alignments. */
        pool.alloc(1);

        arr.pushBack(i);
        arr16.pushBack((int16_t)i);
    }

    DE_TEST_ASSERT(arr.size() == 5000);
    DE_TEST_ASSERT(arr16.size() == 5000);
    for (i = 0; i < 5000; i++)
    {
        DE_TEST_ASSERT(arr[i] == i);
        DE_TEST_ASSERT(arr16[i] == i);
    }

    /* Test popBack(). */
    for (i = 0; i < 1000; i++)
    {
        DE_TEST_ASSERT(arr.popBack() == (4999 - i));
        DE_TEST_ASSERT(arr16.popBack() == (4999 - i));
    }

    DE_TEST_ASSERT(arr.size() == 4000);
    DE_TEST_ASSERT(arr16.size() == 4000);
    for (i = 0; i < 4000; i++)
    {
        DE_TEST_ASSERT(arr[i] == i);
        DE_TEST_ASSERT(arr16[i] == i);
    }

    /* Test resize(). */
    arr.resize(1000);
    arr16.resize(1000);
    for (i = 1000; i < 5000; i++)
    {
        arr.pushBack(i);
        arr16.pushBack((int16_t)i);
    }

    DE_TEST_ASSERT(arr.size() == 5000);
    DE_TEST_ASSERT(arr16.size() == 5000);
    for (i = 0; i < 5000; i++)
    {
        DE_TEST_ASSERT(arr[i] == i);
        DE_TEST_ASSERT(arr16[i] == i);
    }

    arr.resize(0);
    arr.resize(100, -123);
    DE_TEST_ASSERT(arr.size() == 100);
    for (i = 0; i < 100; i++)
        DE_TEST_ASSERT(arr[i] == -123);

    /* Test set() and pushBack() with reserve(). */
    PoolArray<int, 32> arr2(&pool);
    arr2.resize(1500);
    arr2.reserve(2000);
    for (i = 0; i < 1500; i++)
        arr2[i] = i;
    for (; i < 5000; i++)
        arr2.pushBack(i);

    DE_TEST_ASSERT(arr2.size() == 5000);
    for (i = 0; i < 5000; i++)
    {
        int val = arr2[i];
        DE_TEST_ASSERT(val == i);
    }
}

namespace
{

class RefCount
{
public:
    RefCount(void) : m_count(nullptr)
    {
    }

    RefCount(int *count) : m_count(count)
    {
        *m_count += 1;
    }

    RefCount(const RefCount &other) : m_count(other.m_count)
    {
        if (m_count)
            *m_count += 1;
    }

    ~RefCount(void)
    {
        if (m_count)
            *m_count -= 1;
    }

    RefCount &operator=(const RefCount &other)
    {
        if (this == &other)
            return *this;

        if (m_count)
            *m_count -= 1;

        m_count = other.m_count;

        if (m_count)
            *m_count += 1;

        return *this;
    }

private:
    int *m_count;
};

} // namespace

static void sideEffectTest(void)
{
    MemPool pool;
    PoolArray<RefCount> arr(&pool);
    int count = 0;
    RefCount counter(&count);

    DE_TEST_ASSERT(count == 1);

    for (int i = 0; i < 127; i++)
        arr.pushBack(counter);

    DE_TEST_ASSERT(count == 128);

    for (int i = 0; i < 10; i++)
        arr.popBack();

    DE_TEST_ASSERT(count == 118);

    arr.resize(150);
    DE_TEST_ASSERT(count == 118);

    arr.resize(18);
    DE_TEST_ASSERT(count == 19);

    arr.resize(19);
    DE_TEST_ASSERT(count == 19);

    arr.clear();
    DE_TEST_ASSERT(count == 1);
}

static void iteratorTest(void)
{
    MemPool pool;
    PoolArray<int> arr(&pool);

    for (int ndx = 0; ndx < 128; ndx++)
        arr.pushBack(ndx);

    // ConstIterator
    {
        const PoolArray<int> &cRef = arr;
        int ndx                    = 0;
        for (PoolArray<int>::ConstIterator iter = cRef.begin(); iter != cRef.end(); iter++, ndx++)
        {
            DE_TEST_ASSERT(*iter == ndx);
        }

        // Cast & interop with non-const array.
        ndx = 0;
        for (PoolArray<int>::ConstIterator iter = arr.begin(); iter != arr.end(); iter++, ndx++)
        {
            DE_TEST_ASSERT(*iter == ndx);
        }
    }

    // Arithmetics.
    DE_TEST_ASSERT(arr.end() - arr.begin() == 128);
    DE_TEST_ASSERT(*(arr.begin() + 3) == 3);
    DE_TEST_ASSERT(arr.begin()[4] == 4);

    // Relational
    DE_TEST_ASSERT(arr.begin() != arr.begin() + 1);
    DE_TEST_ASSERT(arr.begin() == arr.begin());
    DE_TEST_ASSERT(arr.begin() != arr.end());
    DE_TEST_ASSERT(arr.begin() < arr.end());
    DE_TEST_ASSERT(arr.begin() < arr.begin() + 1);
    DE_TEST_ASSERT(arr.begin() <= arr.begin());
    DE_TEST_ASSERT(arr.end() > arr.begin());
    DE_TEST_ASSERT(arr.begin() >= arr.begin());

    // Compatibility with stl.
    DE_TEST_ASSERT(std::distance(arr.begin(), arr.end()) == 128);

    std::vector<int> vecCopy(arr.size());
    std::copy(arr.begin(), arr.end(), vecCopy.begin());
    for (int ndx = 0; ndx < (int)vecCopy.size(); ndx++)
        DE_TEST_ASSERT(vecCopy[ndx] == ndx);

    std::fill(arr.begin(), arr.end(), -1);
    for (int ndx = 0; ndx < (int)arr.size(); ndx++)
        DE_TEST_ASSERT(arr[ndx] == -1);

    std::copy(vecCopy.begin(), vecCopy.end(), arr.begin());
    for (int ndx = 0; ndx < (int)arr.size(); ndx++)
        DE_TEST_ASSERT(arr[ndx] == ndx);

    // Iterator
    {
        int ndx = 0;
        for (PoolArray<int>::Iterator iter = arr.begin(); iter != arr.end(); iter++, ndx++)
        {
            DE_TEST_ASSERT(*iter == ndx);
            if (ndx == 4)
                *iter = 0;
            else if (ndx == 7)
                *(iter - 1) = 1;
        }
    }

    DE_TEST_ASSERT(arr[4] == 0);
    DE_TEST_ASSERT(arr[6] == 1);
}

void PoolArray_selfTest(void)
{
    intArrayTest();
    alignedIntArrayTest();
    sideEffectTest();
    iteratorTest();
}

} // namespace de
