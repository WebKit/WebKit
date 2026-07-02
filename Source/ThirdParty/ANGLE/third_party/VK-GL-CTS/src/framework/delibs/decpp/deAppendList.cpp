/*-------------------------------------------------------------------------
 * drawElements C++ Base Library
 * -----------------------------
 *
 * Copyright 2015 The Android Open Source Project
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
 * \brief Fast ordered append-only container
 *//*--------------------------------------------------------------------*/

#include "deAppendList.hpp"
#include "deThread.hpp"
#include "deSpinBarrier.hpp"
#include "deSharedPtr.hpp"

#include <vector>
#include <algorithm>

namespace de
{

namespace
{

using std::vector;

struct TestElem
{
    uint32_t threadNdx;
    uint32_t elemNdx;

    TestElem(uint32_t threadNdx_, uint32_t elemNdx_) : threadNdx(threadNdx_), elemNdx(elemNdx_)
    {
    }

    TestElem(void) : threadNdx(0), elemNdx(0)
    {
    }
};

struct SharedState
{
    uint32_t numElements;
    SpinBarrier barrier;
    AppendList<TestElem> testList;

    SharedState(uint32_t numThreads, uint32_t numElements_, uint32_t numElementsHint)
        : numElements(numElements_)
        , barrier(numThreads)
        , testList(numElementsHint)
    {
    }
};

class TestThread : public Thread
{
public:
    TestThread(SharedState *shared, uint32_t threadNdx) : m_shared(shared), m_threadNdx(threadNdx)
    {
    }

    void run(void)
    {
        const uint32_t syncPerElems = 10000;

        for (uint32_t elemNdx = 0; elemNdx < m_shared->numElements; elemNdx++)
        {
            if (elemNdx % syncPerElems == 0)
                m_shared->barrier.sync(SpinBarrier::WAIT_MODE_AUTO);

            m_shared->testList.append(TestElem(m_threadNdx, elemNdx));
        }
    }

private:
    SharedState *const m_shared;
    const uint32_t m_threadNdx;
};

typedef SharedPtr<TestThread> TestThreadSp;

void runAppendListTest(uint32_t numThreads, uint32_t numElements, uint32_t numElementsHint)
{
    SharedState sharedState(numThreads, numElements, numElementsHint);
    vector<TestThreadSp> threads(numThreads);

    for (uint32_t threadNdx = 0; threadNdx < numThreads; ++threadNdx)
    {
        threads[threadNdx] = TestThreadSp(new TestThread(&sharedState, threadNdx));
        threads[threadNdx]->start();
    }

    for (uint32_t threadNdx = 0; threadNdx < numThreads; ++threadNdx)
        threads[threadNdx]->join();

    DE_TEST_ASSERT(sharedState.testList.size() == (size_t)numElements * (size_t)numThreads);

    {
        vector<uint32_t> countByThread(numThreads);

        std::fill(countByThread.begin(), countByThread.end(), 0);

        for (AppendList<TestElem>::const_iterator elemIter = sharedState.testList.begin();
             elemIter != sharedState.testList.end(); ++elemIter)
        {
            const TestElem &elem = *elemIter;

            DE_TEST_ASSERT(de::inBounds(elem.threadNdx, 0u, numThreads));
            DE_TEST_ASSERT(countByThread[elem.threadNdx] == elem.elemNdx);

            countByThread[elem.threadNdx] += 1;
        }

        for (uint32_t threadNdx = 0; threadNdx < numThreads; ++threadNdx)
            DE_TEST_ASSERT(countByThread[threadNdx] == numElements);
    }
}

class ObjCountElem
{
public:
    ObjCountElem(int *liveCount) : m_liveCount(liveCount)
    {
        *m_liveCount += 1;
    }

    ~ObjCountElem(void)
    {
        *m_liveCount -= 1;
    }

    ObjCountElem(const ObjCountElem &other) : m_liveCount(other.m_liveCount)
    {
        *m_liveCount += 1;
    }

    ObjCountElem &operator=(const ObjCountElem &other)
    {
        m_liveCount = other.m_liveCount;
        *m_liveCount += 1;
        return *this;
    }

private:
    int *m_liveCount;
};

void runClearTest(uint32_t numElements1, uint32_t numElements2, uint32_t numElementsHint)
{
    int liveCount = 0;

    {
        de::AppendList<ObjCountElem> testList(numElementsHint);

        for (uint32_t ndx = 0; ndx < numElements1; ++ndx)
            testList.append(ObjCountElem(&liveCount));

        DE_TEST_ASSERT(liveCount == (int)numElements1);

        testList.clear();

        DE_TEST_ASSERT(liveCount == 0);

        for (uint32_t ndx = 0; ndx < numElements2; ++ndx)
            testList.append(ObjCountElem(&liveCount));

        DE_TEST_ASSERT(liveCount == (int)numElements2);
    }

    DE_TEST_ASSERT(liveCount == 0);
}

} // namespace

void AppendList_selfTest(void)
{
    // Single-threaded
    runAppendListTest(1, 1000, 500);
    runAppendListTest(1, 1000, 2000);
    runAppendListTest(1, 35, 1);

    // Multi-threaded
    runAppendListTest(2, 10000, 500);
    runAppendListTest(2, 100, 10);

    if (deGetNumAvailableLogicalCores() >= 4)
    {
        runAppendListTest(4, 10000, 500);
        runAppendListTest(4, 100, 10);
    }

    // Dtor + clear()
    runClearTest(1, 1, 1);
    runClearTest(1, 2, 10);
    runClearTest(50, 25, 10);
    runClearTest(9, 50, 10);
    runClearTest(10, 50, 10);
    runClearTest(50, 9, 10);
    runClearTest(50, 10, 10);
}

} // namespace de
