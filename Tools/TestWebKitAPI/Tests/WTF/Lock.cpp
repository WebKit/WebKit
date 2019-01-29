/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include <wtf/Lock.h>
#include <wtf/Threading.h>
#include <wtf/ThreadingPrimitives.h>
#include <wtf/WordLock.h>

namespace TestWebKitAPI {

struct LockInspector {
    template<typename LockType>
    static bool isFullyReset(LockType& lock)
    {
        return lock.isFullyReset();
    }
};

template<typename LockType>
void runLockTest(unsigned numThreadGroups, unsigned numThreadsPerGroup, unsigned workPerCriticalSection, unsigned numIterations)
{
    std::unique_ptr<LockType[]> locks = std::make_unique<LockType[]>(numThreadGroups);
    std::unique_ptr<double[]> words = std::make_unique<double[]>(numThreadGroups);
    std::unique_ptr<RefPtr<Thread>[]> threads = std::make_unique<RefPtr<Thread>[]>(numThreadGroups * numThreadsPerGroup);

    for (unsigned threadGroupIndex = numThreadGroups; threadGroupIndex--;) {
        words[threadGroupIndex] = 0;

        for (unsigned threadIndex = numThreadsPerGroup; threadIndex--;) {
            threads[threadGroupIndex * numThreadsPerGroup + threadIndex] = Thread::create(
                "Lock test thread",
                [threadGroupIndex, &locks, &words, numIterations, workPerCriticalSection] () {
                    for (unsigned i = numIterations; i--;) {
                        locks[threadGroupIndex].lock();
                        for (unsigned j = workPerCriticalSection; j--;)
                            words[threadGroupIndex]++;
                        locks[threadGroupIndex].unlock();
                    }
                });
        }
    }

    for (unsigned threadIndex = numThreadGroups * numThreadsPerGroup; threadIndex--;)
        threads[threadIndex]->waitForCompletion();

    double expected = 0;
    for (uint64_t i = static_cast<uint64_t>(numIterations) * workPerCriticalSection * numThreadsPerGroup; i--;)
        expected++;

    for (unsigned threadGroupIndex = numThreadGroups; threadGroupIndex--;)
        EXPECT_EQ(expected, words[threadGroupIndex]);

    // Now test that the locks correctly reset themselves. We expect that if a single thread locks
    // each of the locks twice in a row, then the lock should be in a pristine state.
    for (unsigned threadGroupIndex = numThreadGroups; threadGroupIndex--;) {
        for (unsigned i = 2; i--;) {
            locks[threadGroupIndex].lock();
            locks[threadGroupIndex].unlock();
        }

        EXPECT_EQ(true, LockInspector::isFullyReset(locks[threadGroupIndex]));
    }
}

bool skipSlow()
{
#if PLATFORM(WIN) && !defined(NDEBUG)
    return true;
#else
    return false;
#endif
}

TEST(WTF_WordLock, UncontendedShortSection)
{
    runLockTest<WordLock>(1, 1, 1, 10000000);
}

TEST(WTF_WordLock, UncontendedLongSection)
{
    runLockTest<WordLock>(1, 1, 10000, 1000);
}

TEST(WTF_WordLock, ContendedShortSection)
{
    if (skipSlow())
        return;
    runLockTest<WordLock>(1, 10, 1, 5000000);
}

TEST(WTF_WordLock, ContendedLongSection)
{
    if (skipSlow())
        return;
    runLockTest<WordLock>(1, 10, 10000, 10000);
}

TEST(WTF_WordLock, ManyContendedShortSections)
{
    if (skipSlow())
        return;
    runLockTest<WordLock>(10, 10, 1, 500000);
}

TEST(WTF_WordLock, ManyContendedLongSections)
{
    if (skipSlow())
        return;
    runLockTest<WordLock>(10, 10, 10000, 500);
}

TEST(WTF_Lock, UncontendedShortSection)
{
    runLockTest<Lock>(1, 1, 1, 10000000);
}

TEST(WTF_Lock, UncontendedLongSection)
{
    runLockTest<Lock>(1, 1, 10000, 1000);
}

TEST(WTF_Lock, ContendedShortSection)
{
    if (skipSlow())
        return;
    runLockTest<Lock>(1, 10, 1, 10000000);
}

TEST(WTF_Lock, ContendedLongSection)
{
    if (skipSlow())
        return;
    runLockTest<Lock>(1, 10, 10000, 10000);
}

TEST(WTF_Lock, ManyContendedShortSections)
{
    if (skipSlow())
        return;
    runLockTest<Lock>(10, 10, 1, 500000);
}

TEST(WTF_Lock, ManyContendedLongSections)
{
    if (skipSlow())
        return;
    runLockTest<Lock>(10, 10, 10000, 1000);
}

TEST(WTF_Lock, ManyContendedLongerSections)
{
    if (skipSlow())
        return;
    runLockTest<Lock>(10, 10, 100000, 1);
}

TEST(WTF_Lock, SectionAddressCollision)
{
    if (skipSlow())
        return;
    runLockTest<Lock>(4, 2, 10000, 2000);
}

} // namespace TestWebKitAPI
