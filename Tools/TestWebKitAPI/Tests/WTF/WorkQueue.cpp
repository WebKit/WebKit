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

#include "Test.h"
#include <wtf/Condition.h>
#include <wtf/Lock.h>
#include <wtf/Vector.h>
#include <wtf/WorkQueue.h>
#include <string>
#include <thread>

namespace TestWebKitAPI {

using namespace std::literals::chrono_literals;

static const char* simpleTestLabel = "simpleTest";
static const char* longTestLabel = "longTest";
static const char* thirdTestLabel = "thirdTest";
static const char* dispatchAfterLabel = "dispatchAfter";
    
TEST(WTF_WorkQueue, Simple)
{
    Lock m_lock;
    Condition m_testCompleted;
    Vector<std::string> m_functionCallOrder;

    bool calledSimpleTest = false;
    bool calledLongTest = false;
    bool calledThirdTest = false;

    static const char* simpleTestLabel = "simpleTest";
    static const char* longTestLabel = "longTest";
    static const char* thirdTestLabel = "thirdTest";

    auto queue = WorkQueue::create("com.apple.WebKit.Test.simple");
    int initialRefCount = queue->refCount();
    EXPECT_EQ(1, initialRefCount);

    LockHolder locker(m_lock);
    queue->dispatch([&](void) {
        m_functionCallOrder.append(simpleTestLabel);
        calledSimpleTest = true;
    });

    queue->dispatch([&](void) {
        m_functionCallOrder.append(longTestLabel);
        std::this_thread::sleep_for(100ns);
        calledLongTest = true;
    });

    queue->dispatch([&](void) {
        LockHolder locker(m_lock);
        m_functionCallOrder.append(thirdTestLabel);
        calledThirdTest = true;

        EXPECT_TRUE(calledSimpleTest);
        EXPECT_TRUE(calledLongTest);
        EXPECT_TRUE(calledThirdTest);
        
        m_testCompleted.notifyOne();
    });

    EXPECT_GT(queue->refCount(), 1);

    m_testCompleted.wait(m_lock);

    EXPECT_TRUE(calledSimpleTest);
    EXPECT_TRUE(calledLongTest);
    EXPECT_TRUE(calledThirdTest);

    EXPECT_EQ(static_cast<size_t>(3), m_functionCallOrder.size());
    EXPECT_STREQ(simpleTestLabel, m_functionCallOrder[0].c_str());
    EXPECT_STREQ(longTestLabel, m_functionCallOrder[1].c_str());
    EXPECT_STREQ(thirdTestLabel, m_functionCallOrder[2].c_str());
}

TEST(WTF_WorkQueue, TwoQueues)
{
    Lock m_lock;
    Condition m_testQueue1Completed, m_testQueue2Completed;
    Vector<std::string> m_functionCallOrder;

    bool calledSimpleTest = false;
    bool calledLongTest = false;
    bool calledThirdTest = false;
    
    auto queue1 = WorkQueue::create("com.apple.WebKit.Test.twoQueues1");
    auto queue2 = WorkQueue::create("com.apple.WebKit.Test.twoQueues2");

    EXPECT_EQ(1, queue1->refCount());
    EXPECT_EQ(1, queue2->refCount());

    LockHolder locker(m_lock);
    
    queue1->dispatch([&](void) {
        m_functionCallOrder.append(simpleTestLabel);
        calledSimpleTest = true;
    });

    queue2->dispatch([&](void) {
        std::this_thread::sleep_for(50ms);

        LockHolder locker(m_lock);

        // Will fail if queue2 took the mutex before queue1.
        EXPECT_TRUE(calledThirdTest);

        m_functionCallOrder.append(longTestLabel);
        calledLongTest = true;
        m_testQueue2Completed.notifyOne();
    });

    queue1->dispatch([&](void) {
        LockHolder locker(m_lock);
        m_functionCallOrder.append(thirdTestLabel);
        calledThirdTest = true;
        
        m_testQueue1Completed.notifyOne();
    });

    m_testQueue1Completed.wait(m_lock);

    EXPECT_TRUE(calledSimpleTest);
    EXPECT_FALSE(calledLongTest);
    EXPECT_TRUE(calledThirdTest);

    m_testQueue2Completed.wait(m_lock);

    EXPECT_TRUE(calledSimpleTest);
    EXPECT_TRUE(calledLongTest);
    EXPECT_TRUE(calledThirdTest);

    EXPECT_EQ(static_cast<size_t>(3), m_functionCallOrder.size());
    EXPECT_STREQ(simpleTestLabel, m_functionCallOrder[0].c_str());
    EXPECT_STREQ(thirdTestLabel, m_functionCallOrder[1].c_str());
    EXPECT_STREQ(longTestLabel, m_functionCallOrder[2].c_str());
}

TEST(WTF_WorkQueue, DispatchAfter)
{
    Lock m_lock;
    Condition m_testCompleted, m_dispatchAfterTestCompleted;
    Vector<std::string> m_functionCallOrder;

    bool calledSimpleTest = false;
    bool calledDispatchAfterTest = false;

    auto queue = WorkQueue::create("com.apple.WebKit.Test.dispatchAfter");

    LockHolder locker(m_lock);

    queue->dispatch([&](void) {
        LockHolder locker(m_lock);
        m_functionCallOrder.append(simpleTestLabel);
        calledSimpleTest = true;
        m_testCompleted.notifyOne();
    });

    queue->dispatchAfter(500_ms, [&](void) {
        LockHolder locker(m_lock);
        m_functionCallOrder.append(dispatchAfterLabel);
        calledDispatchAfterTest = true;
        m_dispatchAfterTestCompleted.notifyOne();
    });

    m_testCompleted.wait(m_lock);

    EXPECT_TRUE(calledSimpleTest);
    EXPECT_FALSE(calledDispatchAfterTest);
    
    m_dispatchAfterTestCompleted.wait(m_lock);

    EXPECT_TRUE(calledSimpleTest);
    EXPECT_TRUE(calledDispatchAfterTest);

    EXPECT_EQ(static_cast<size_t>(2), m_functionCallOrder.size());
    EXPECT_STREQ(simpleTestLabel, m_functionCallOrder[0].c_str());
    EXPECT_STREQ(dispatchAfterLabel, m_functionCallOrder[1].c_str());
}

TEST(WTF_WorkQueue, DestroyOnSelf)
{
    Lock lock;
    Condition dispatchAfterTestStarted;
    Condition dispatchAfterTestCompleted;
    bool started = false;
    bool completed = false;

    {
        LockHolder locker(lock);
        {
            auto queue = WorkQueue::create("com.apple.WebKit.Test.dispatchAfter");
            queue->dispatchAfter(500_ms, [&](void) {
                LockHolder locker(lock);
                dispatchAfterTestStarted.wait(lock, [&] {
                    return started;
                });
                completed = true;
                dispatchAfterTestCompleted.notifyOne();
            });
        }
        started = true;
        dispatchAfterTestStarted.notifyOne();
    }
    {
        LockHolder locker(lock);
        dispatchAfterTestCompleted.wait(lock, [&] {
            return completed;
        });
        WTF::sleep(100_ms);
    }
}

} // namespace TestWebKitAPI
