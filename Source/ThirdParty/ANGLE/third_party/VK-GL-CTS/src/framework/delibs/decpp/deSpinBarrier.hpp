#ifndef _DESPINBARRIER_HPP
#define _DESPINBARRIER_HPP
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

#include "deDefs.hpp"
#include "deAtomic.h"

namespace de
{

/*--------------------------------------------------------------------*//*!
 * \brief Cross-thread barrier
 *
 * SpinBarrier provides barrier implementation that uses spin loop for
 * waiting for other threads. Threads may choose to wait in tight loop
 * (WAIT_MODE_BUSY) or yield between iterations (WAIT_MODE_YIELD).
 *
 * It is not recommended to use WAIT_MODE_BUSY when there are more threads
 * than number of cores participating in the barrier as it will lead to
 * priority inversion and dramatic slowdown. For that reason WAIT_MODE_AUTO
 * is provided, which selects between busy and yielding waiting based on
 * number of threads.
 *//*--------------------------------------------------------------------*/
class SpinBarrier
{
public:
    enum WaitMode
    {
        WAIT_MODE_BUSY = 0, //! Wait in tight spin loop.
        WAIT_MODE_YIELD,    //! Call deYield() between spin loop iterations.
        WAIT_MODE_AUTO,     //! Use WAIT_MODE_BUSY loop if #threads <= #cores, otherwise WAIT_MODE_YIELD.

        WAIT_MODE_LAST
    };

    SpinBarrier(int32_t numThreads);
    ~SpinBarrier(void);

    //! Reset barrier. Not thread-safe, e.g. no other thread can
    //! be calling sync() or removeThread() at the same time.
    void reset(uint32_t numThreads);

    //! Wait until all threads (determined by active thread count)
    //! have entered sync().
    void sync(WaitMode mode);

    //! Remove thread from barrier (decrements active thread count).
    //! Can be called concurrently with sync() or removeThread().
    void removeThread(WaitMode mode);

private:
    SpinBarrier(const SpinBarrier &);
    SpinBarrier operator=(const SpinBarrier &);

    const uint32_t m_numCores;

    volatile int32_t m_numThreads;
    volatile int32_t m_numEntered;
    volatile int32_t m_numLeaving;
    volatile int32_t m_numRemoved;
};

void SpinBarrier_selfTest(void);

} // namespace de

#endif // _DESPINBARRIER_HPP
