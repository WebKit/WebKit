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
#include <wtf/MonotonicTime.h>
#include <wtf/Threading.h>
#include <wtf/ThreadingPrimitives.h>
#include <wtf/UniqueArray.h>
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
    std::unique_ptr<LockType[]> locks = makeUniqueWithoutFastMallocCheck<LockType[]>(numThreadGroups);
    auto words = makeUniqueArray<double>(numThreadGroups);
    auto threads = Vector<RefPtr<Thread>>(numThreadGroups * numThreadsPerGroup);

    for (unsigned threadGroupIndex = numThreadGroups; threadGroupIndex--;) {
        words[threadGroupIndex] = 0;

        for (unsigned threadIndex = numThreadsPerGroup; threadIndex--;) {
            threads[threadGroupIndex * numThreadsPerGroup + threadIndex] = Thread::create(
                "Lock test thread"_s,
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

#if !PLATFORM(IOS_SIMULATOR) || defined(NDEBUG)
TEST(WTF_Lock, ContendedShortSection)
{
    if (skipSlow())
        return;
    runLockTest<Lock>(1, 10, 1, 1000000);
}
#endif

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

namespace {
class MyValue {
public:
    void setValue(int value)
    {
        Locker holdLock { m_lock };
        m_value = value;
    }
    void maybeSetOtherValue(int value)
    {
        if (!m_otherLock.tryLock())
            return;
        Locker holdLock { AdoptLock, m_otherLock };
        m_otherValue = value;
    }
    // This function can be used to manually check that compile fails.
    template<typename T> void shouldFailCompile(T t)
    {
        m_value = t;
    }
    private:
    Lock m_lock;
    int m_value WTF_GUARDED_BY_LOCK(m_lock) { 77 };
    Lock m_otherLock;
    int m_otherValue WTF_GUARDED_BY_LOCK(m_otherLock) { 88 };
};

}

TEST(WTF_Lock, Basic)
{
    MyValue v;
    v.setValue(7);
    v.maybeSetOtherValue(34);
}

TEST(WTF_Lock, TryLockWithTimeoutZeroDoesNotSleep)
{
    // A zero timeout on a contended lock must fail promptly rather than sleeping a
    // full second (regression test for the off-by-one / sub-second-truncation bug in
    // tryLockWithTimeout, where maxRetries == 0 still slept once).
    //
    // Note: we assert on elapsed time only, not on the return value. tryLockWithTimeout
    // returns isHeld(), which reports whether the lock is held by *any* thread, so with
    // the main thread holding the lock the worker observes true regardless of whether it
    // acquired the lock. The timing is the meaningful signal for this fix.
    Lock lock;
    Locker holder { lock };

    MonotonicTime start = MonotonicTime::now();
    Thread::create("tryLockWithTimeout test"_s, [&] {
        lock.tryLockWithTimeout(0_s);
    })->waitForCompletion();
    Seconds elapsed = MonotonicTime::now() - start;

    EXPECT_LT(elapsed, 500_ms);
}

TEST(WTF_Lock, TryLockWithTimeoutHonorsTimeoutWithoutOvershoot)
{
    // With a 1s timeout on a permanently-held lock, the retry loop must sleep exactly
    // once (~1s) and then give up. The off-by-one bug slept twice (~2s), overshooting
    // the requested timeout. We bound elapsed below the pre-fix ~2s and above the single
    // ~1s sleep so the test fails for either the old off-by-one or a no-sleep regression.
    Lock lock;
    Locker holder { lock };

    MonotonicTime start = MonotonicTime::now();
    Thread::create("tryLockWithTimeout overshoot test"_s, [&] {
        lock.tryLockWithTimeout(1_s);
    })->waitForCompletion();
    Seconds elapsed = MonotonicTime::now() - start;

    EXPECT_GE(elapsed, 900_ms);
    EXPECT_LT(elapsed, 1600_ms);
}

} // namespace TestWebKitAPI
