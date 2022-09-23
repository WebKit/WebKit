/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

#include "Utilities.h"
#include <wtf/LockAssertion.h> // NOLINT: check-webkit-style has problems with files that do not have primary header.
#include <wtf/RunLoop.h>
#include <wtf/ThreadAssertions.h>
#include <wtf/WorkQueue.h>

namespace TestWebKitAPI {
namespace {
// Example of how to use LockAssertion for a common WebKit locking use-case:
//  The owner thread reads and writes a shared variable, and a work queue just reads it.
class MyClass {
public:
    void doInitialTask(int n)
    {
        assertIsHeld(valueLock());
        m_value = n;
        m_workQueue = WorkQueue::create("read queue");
        m_workQueue->dispatch([this] { readValueFromQueue(); }); // NOLINT
    }

    void doSecondTask(int n)
    {
        Locker lock { m_lock };
        assertIsHeld(valueLock());
        m_value = n;
        m_workQueue->dispatch([this] { readValueFromQueue(); }); // NOLINT
    }

    void doInvalidRead(int n)
    {
        assertIsHeld(valueLock());
        m_value = n;
        m_workQueue = WorkQueue::create("read queue");
        m_workQueue->dispatch([this] { readValueFromQueueWithoutLock(); }); // NOLINT
    }

    int value() const
    {
        assertIsShared(valueLock());
        return m_value;
    }

    void waitForWorkQueue() const
    {
        assertIsShared(valueLock());
        while (m_workQueueValue != m_value) { }
    }

private:
    void readValueFromQueue()
    {
        Locker lock { m_lock };
        assertIsShared(valueLock());
        m_workQueueValue = m_value;
    }

    void readValueFromQueueWithoutLock()
    {
        // No lock causes an ASSERT.
        assertIsShared(valueLock());
        m_workQueueValue = m_value;
    }

    // Function describing m_value access.
    const LockAssertion& valueLock() const
    {
        if constexpr(ASSERT_ENABLED) {
            if (isCurrent(m_ownerThread)) {
                // When the work queue does not exist, writes are can be done without locks from owner thread.
                // After work queue starts, writes can be done with m_lock held.
                if (!m_workQueue || m_lock.isLocked())
                    return LockAssertion::exclusive;
                // Since owner thread is the only one writing, it can read without locks.
                return LockAssertion::shared;
            }
            assertIsCurrent(m_workQueue);
            // Work queue can read with m_lock held.
            return m_lock.isLocked() ? LockAssertion::shared : LockAssertion::unlocked;
        }
        return LockAssertion::unused;
    }
    Lock m_lock;
    int m_value WTF_GUARDED_BY_LOCK(valueLock()) { 0 };
    std::atomic<int> m_workQueueValue { 0 };
    RefPtr<WorkQueue> m_workQueue;
    NO_UNIQUE_ADDRESS ThreadAssertion m_ownerThread;
};
}

TEST(WTF_LockAssertionTests, TestLockAssertions)
{
    WTF::initializeMainThread();

    MyClass instance;
    instance.doInitialTask(77);
    EXPECT_EQ(77, instance.value());
    instance.waitForWorkQueue();

    instance.doSecondTask(88);
    EXPECT_EQ(88, instance.value());
    instance.waitForWorkQueue();
}

#if ASSERT_ENABLED
#define MAYBE_TestLockAssertionsNegativeDeathTest TestLockAssertionsNegativeDeathTest
#else
#define MAYBE_TestLockAssertionsNegativeDeathTest DISABLED_TestLockAssertionsNegativeDeathTest
#endif

TEST(WTF_LockAssertionTests, MAYBE_TestLockAssertionsNegativeDeathTest)
{
    ::testing::FLAGS_gtest_death_test_style = "threadsafe";
    ASSERT_DEATH_IF_SUPPORTED({
        WTF::initializeMainThread();
        MyClass instance;
        instance.doInvalidRead(88);
        EXPECT_EQ(88, instance.value());
        instance.waitForWorkQueue();
    }, "ASSERTION FAILED: lockIsShared");
}

} // namespace TestWebKitAPI
