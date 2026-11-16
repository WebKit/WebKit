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
 * \brief Cross-thread barrier.
 *//*--------------------------------------------------------------------*/

#include "deSpinBarrier.hpp"
#include "deThread.hpp"
#include "deRandom.hpp"
#include "deInt32.h"

#include <vector>

namespace de
{

SpinBarrier::SpinBarrier(int32_t numThreads)
    : m_numCores(deGetNumAvailableLogicalCores())
    , m_numThreads(numThreads)
    , m_numEntered(0)
    , m_numLeaving(0)
    , m_numRemoved(0)
{
    DE_ASSERT(numThreads > 0);
}

SpinBarrier::~SpinBarrier(void)
{
    DE_ASSERT(m_numEntered == 0 && m_numLeaving == 0);
}

void SpinBarrier::reset(uint32_t numThreads)
{
    // If last threads were removed, m_numEntered > 0 && m_numRemoved > 0
    DE_ASSERT(m_numLeaving == 0);
    DE_ASSERT(numThreads > 0);
    m_numThreads = numThreads;
    m_numEntered = 0;
    m_numLeaving = 0;
    m_numRemoved = 0;
}

inline SpinBarrier::WaitMode getWaitMode(SpinBarrier::WaitMode requested, uint32_t numCores, int32_t numThreads)
{
    if (requested == SpinBarrier::WAIT_MODE_AUTO)
        return ((uint32_t)numThreads <= numCores) ? SpinBarrier::WAIT_MODE_BUSY : SpinBarrier::WAIT_MODE_YIELD;
    else
        return requested;
}

inline void wait(SpinBarrier::WaitMode mode)
{
    DE_ASSERT(mode == SpinBarrier::WAIT_MODE_YIELD || mode == SpinBarrier::WAIT_MODE_BUSY);

    if (mode == SpinBarrier::WAIT_MODE_YIELD)
        deYield();
}

void SpinBarrier::sync(WaitMode requestedMode)
{
    const int32_t cachedNumThreads = m_numThreads;
    const WaitMode waitMode        = getWaitMode(requestedMode, m_numCores, cachedNumThreads);

    deMemoryReadWriteFence();

    // m_numEntered must not be touched until all threads have had
    // a chance to observe it being 0.
    if (m_numLeaving > 0)
    {
        for (;;)
        {
            if (m_numLeaving == 0)
                break;

            wait(waitMode);
        }
    }

    // If m_numRemoved > 0, m_numThreads will decrease. If m_numThreads is decreased
    // just after atomicOp and before comparison, the branch could be taken by multiple
    // threads. Since m_numThreads only changes if all threads are inside the spinbarrier,
    // cached value at snapshotted at the beginning of the function will be equal for
    // all threads.
    if (deAtomicIncrement32(&m_numEntered) == cachedNumThreads)
    {
        // Release all waiting threads. Since this thread has not been removed, m_numLeaving will
        // be >= 1 until m_numLeaving is decremented at the end of this function.
        m_numThreads = m_numThreads - m_numRemoved;
        m_numLeaving = m_numThreads;
        m_numRemoved = 0;

        deMemoryReadWriteFence();
        m_numEntered = 0;
    }
    else
    {
        for (;;)
        {
            if (m_numEntered == 0)
                break;

            wait(waitMode);
        }
    }

    deAtomicDecrement32(&m_numLeaving);
    deMemoryReadWriteFence();
}

void SpinBarrier::removeThread(WaitMode requestedMode)
{
    const int32_t cachedNumThreads = m_numThreads;
    const WaitMode waitMode        = getWaitMode(requestedMode, m_numCores, cachedNumThreads);

    // Wait for other threads exiting previous barrier
    if (m_numLeaving > 0)
    {
        for (;;)
        {
            if (m_numLeaving == 0)
                break;

            wait(waitMode);
        }
    }

    // Ask for last thread entering barrier to adjust thread count
    deAtomicIncrement32(&m_numRemoved);

    // See sync() - use cached value
    if (deAtomicIncrement32(&m_numEntered) == cachedNumThreads)
    {
        // Release all waiting threads.
        m_numThreads = m_numThreads - m_numRemoved;
        m_numLeaving = m_numThreads;
        m_numRemoved = 0;

        deMemoryReadWriteFence();
        m_numEntered = 0;
    }
}

namespace
{

void singleThreadTest(SpinBarrier::WaitMode mode)
{
    SpinBarrier barrier(1);

    barrier.sync(mode);
    barrier.sync(mode);
    barrier.sync(mode);
}

class TestThread : public de::Thread
{
public:
    TestThread(SpinBarrier &barrier, volatile int32_t *sharedVar, int numThreads, int threadNdx)
        : m_barrier(barrier)
        , m_sharedVar(sharedVar)
        , m_numThreads(numThreads)
        , m_threadNdx(threadNdx)
        , m_busyOk((uint32_t)m_numThreads <= deGetNumAvailableLogicalCores())
    {
    }

    void run(void)
    {
        const int numIters = 10000;
        de::Random rnd(deInt32Hash(m_numThreads) ^ deInt32Hash(m_threadNdx));

        for (int iterNdx = 0; iterNdx < numIters; iterNdx++)
        {
            // Phase 1: count up
            deAtomicIncrement32(m_sharedVar);

            // Verify
            m_barrier.sync(getWaitMode(rnd));

            DE_TEST_ASSERT(*m_sharedVar == m_numThreads);

            m_barrier.sync(getWaitMode(rnd));

            // Phase 2: count down
            deAtomicDecrement32(m_sharedVar);

            // Verify
            m_barrier.sync(getWaitMode(rnd));

            DE_TEST_ASSERT(*m_sharedVar == 0);

            m_barrier.sync(getWaitMode(rnd));
        }
    }

private:
    SpinBarrier &m_barrier;
    volatile int32_t *const m_sharedVar;
    const int m_numThreads;
    const int m_threadNdx;
    const bool m_busyOk;

    SpinBarrier::WaitMode getWaitMode(de::Random &rnd)
    {
        static const SpinBarrier::WaitMode s_allModes[] = {
            SpinBarrier::WAIT_MODE_YIELD,
            SpinBarrier::WAIT_MODE_AUTO,
            SpinBarrier::WAIT_MODE_BUSY,
        };
        const int numModes = DE_LENGTH_OF_ARRAY(s_allModes) - (m_busyOk ? 0 : 1);

        return rnd.choose<SpinBarrier::WaitMode>(DE_ARRAY_BEGIN(s_allModes), DE_ARRAY_BEGIN(s_allModes) + numModes);
    }
};

void multiThreadTest(int numThreads)
{
    SpinBarrier barrier(numThreads);
    volatile int32_t sharedVar = 0;
    std::vector<TestThread *> threads(numThreads, static_cast<TestThread *>(nullptr));

    for (int ndx = 0; ndx < numThreads; ndx++)
    {
        threads[ndx] = new TestThread(barrier, &sharedVar, numThreads, ndx);
        DE_TEST_ASSERT(threads[ndx]);
        threads[ndx]->start();
    }

    for (int ndx = 0; ndx < numThreads; ndx++)
    {
        threads[ndx]->join();
        delete threads[ndx];
    }

    DE_TEST_ASSERT(sharedVar == 0);
}

void singleThreadRemoveTest(SpinBarrier::WaitMode mode)
{
    SpinBarrier barrier(3);

    barrier.removeThread(mode);
    barrier.removeThread(mode);
    barrier.sync(mode);
    barrier.removeThread(mode);

    barrier.reset(1);
    barrier.sync(mode);

    barrier.reset(2);
    barrier.removeThread(mode);
    barrier.sync(mode);
}

class TestExitThread : public de::Thread
{
public:
    TestExitThread(SpinBarrier &barrier, int numThreads, int threadNdx, SpinBarrier::WaitMode waitMode)
        : m_barrier(barrier)
        , m_numThreads(numThreads)
        , m_threadNdx(threadNdx)
        , m_waitMode(waitMode)
    {
    }

    void run(void)
    {
        const int numIters = 10000;
        de::Random rnd(deInt32Hash(m_numThreads) ^ deInt32Hash(m_threadNdx) ^ deInt32Hash((int32_t)m_waitMode));
        const int invExitProb = 1000;

        for (int iterNdx = 0; iterNdx < numIters; iterNdx++)
        {
            if (rnd.getInt(0, invExitProb) == 0)
            {
                m_barrier.removeThread(m_waitMode);
                break;
            }
            else
                m_barrier.sync(m_waitMode);
        }
    }

private:
    SpinBarrier &m_barrier;
    const int m_numThreads;
    const int m_threadNdx;
    const SpinBarrier::WaitMode m_waitMode;
};

void multiThreadRemoveTest(int numThreads, SpinBarrier::WaitMode waitMode)
{
    SpinBarrier barrier(numThreads);
    std::vector<TestExitThread *> threads(numThreads, static_cast<TestExitThread *>(nullptr));

    for (int ndx = 0; ndx < numThreads; ndx++)
    {
        threads[ndx] = new TestExitThread(barrier, numThreads, ndx, waitMode);
        DE_TEST_ASSERT(threads[ndx]);
        threads[ndx]->start();
    }

    for (int ndx = 0; ndx < numThreads; ndx++)
    {
        threads[ndx]->join();
        delete threads[ndx];
    }
}

} // namespace

void SpinBarrier_selfTest(void)
{
    singleThreadTest(SpinBarrier::WAIT_MODE_YIELD);
    singleThreadTest(SpinBarrier::WAIT_MODE_BUSY);
    singleThreadTest(SpinBarrier::WAIT_MODE_AUTO);
    multiThreadTest(1);
    multiThreadTest(2);
    multiThreadTest(4);
    multiThreadTest(8);
    multiThreadTest(16);

    singleThreadRemoveTest(SpinBarrier::WAIT_MODE_YIELD);
    singleThreadRemoveTest(SpinBarrier::WAIT_MODE_BUSY);
    singleThreadRemoveTest(SpinBarrier::WAIT_MODE_AUTO);
    multiThreadRemoveTest(1, SpinBarrier::WAIT_MODE_BUSY);
    multiThreadRemoveTest(2, SpinBarrier::WAIT_MODE_AUTO);
    multiThreadRemoveTest(4, SpinBarrier::WAIT_MODE_AUTO);
    multiThreadRemoveTest(8, SpinBarrier::WAIT_MODE_AUTO);
    multiThreadRemoveTest(16, SpinBarrier::WAIT_MODE_AUTO);
    multiThreadRemoveTest(1, SpinBarrier::WAIT_MODE_YIELD);
    multiThreadRemoveTest(2, SpinBarrier::WAIT_MODE_YIELD);
    multiThreadRemoveTest(4, SpinBarrier::WAIT_MODE_YIELD);
    multiThreadRemoveTest(8, SpinBarrier::WAIT_MODE_YIELD);
    multiThreadRemoveTest(16, SpinBarrier::WAIT_MODE_YIELD);
}

} // namespace de
