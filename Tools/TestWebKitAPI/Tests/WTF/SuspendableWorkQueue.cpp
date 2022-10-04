/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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
#include <wtf/SuspendableWorkQueue.h>

#include "Test.h"
#include "Utilities.h"
#include <wtf/ApproximateTime.h>
#include <wtf/Lock.h>
#include <wtf/Scope.h>
#include <wtf/threads/BinarySemaphore.h>

namespace TestWebKitAPI {

TEST(WTF_SuspendableWorkQueue, Suspend)
{
    WTF::initializeMainThread();

    int taskCount = 100;
    int completedTaskCount = 0;
    std::atomic<bool> allTasksCompleted = false;
    bool suspended = false;
    bool suspendCompletionHandlerCalled = false;
    auto queue = SuspendableWorkQueue::create("com.apple.WebKit.Test.simple");
    for (int i = 0; i < taskCount; i ++) {
        queue->dispatch([&]() mutable {
            suspended = false;
            ++completedTaskCount;
            if (completedTaskCount == taskCount)
                allTasksCompleted = true;
        });
    }
    queue->suspend([&] {
        suspended = true;
    }, [&] {
        suspendCompletionHandlerCalled = true;
    });
    Util::run(&suspendCompletionHandlerCalled);
    {
        EXPECT_LE(completedTaskCount, taskCount);
        EXPECT_TRUE(suspended);
    }

    queue->resume();
    while (!allTasksCompleted)
        Util::spinRunLoop();
    EXPECT_EQ(completedTaskCount, taskCount);
    EXPECT_TRUE(suspended);
}

TEST(WTF_SuspendableWorkQueue, SuspendTwice)
{
    WTF::initializeMainThread();

    bool suspendCompletionHandlerCalled = false;
    int suspendCount = 0;
    auto queue = SuspendableWorkQueue::create("com.apple.WebKit.Test.simple");
    queue->suspend([&]() {
        ++suspendCount;
    }, [&] {
        suspendCompletionHandlerCalled = true;
    });
    Util::run(&suspendCompletionHandlerCalled);
    {
        EXPECT_EQ(1, suspendCount);
    }
    
    suspendCompletionHandlerCalled = false;
    queue->suspend([&]() {
        ++suspendCount;
    }, [&] {
        suspendCompletionHandlerCalled = true;
    });
    Util::run(&suspendCompletionHandlerCalled);
    EXPECT_EQ(2, suspendCount);
}

TEST(WTF_SuspendableWorkQueue, AllPresuspendTasksAreExecuted)
{
    WTF::initializeMainThread();

    auto queue = SuspendableWorkQueue::create("AllPresuspendTasksAreExecuted");
    int count = 0;
    bool lastDone = false;
    for (int i = 0; i < 76; ++i) {
        queue->suspend([&count] {
            count++;
        }, [] { });
    }
    queue->suspend([] { }, [&lastDone] { 
        lastDone = true; 
    });
    Util::run(&lastDone);
    EXPECT_EQ(76, count);
    queue->resume();
    queue->dispatchSync([] { });
}

TEST(WTF_SuspendableWorkQueue, AllTasksBeforeSuspendAreExecuted)
{
    auto queue = SuspendableWorkQueue::create("AllTasksBeforeSuspendAreExecuted");
    std::atomic<int> count = 0; // count is atomic because we race to it.
    for (int i = 0; i < 7600; ++i) {
        queue->dispatch([&count] {
            count++;
        });
    }
    queue->suspend();
    for (int i = 0; i < 77; ++i) {
        queue->dispatch([&count] {
            count++;
        });
    }
    Util::runFor(0.2_s);
    // Here we race to count but assume .2s is enough to run the 7600 tasks.
    EXPECT_EQ(7600, count);
    queue->resume();
    queue->dispatchSync([] { });
    EXPECT_EQ(7677, count);
}

TEST(WTF_SuspendableWorkQueue, NoDeadlockWhenSuspendCompletes)
{
    WTF::initializeMainThread();

    auto queue = SuspendableWorkQueue::create("NoDeadlockWhenSuspendCompletes");
    for (int i = 0; i < 76; ++i) {
        auto suspendQueue = makeScopeExit([queue] {
            queue->suspend([] { }, [] { });
        });
        // The test ensures that the suspend task is not destroyed with the internal lock held.
        queue->suspend([suspendQueueTask = WTFMove(suspendQueue)] { }, [] { });
    }
    queue->resume();
    queue->dispatchSync([] { });
    ASSERT_TRUE(true);
}

TEST(WTF_SuspendableWorkQueue, SuspendedSendSyncOtherThreadResumes)
{
    WTF::initializeMainThread();

    auto queue = SuspendableWorkQueue::create("SuspendedSendSyncOtherThreadResumes1");
    queue->suspend();

    auto resumeQueue = SuspendableWorkQueue::create("SuspendedSendSyncOtherThreadResumes2");
    auto start = ApproximateTime::now();
    ApproximateTime runAt;
    resumeQueue->dispatchAfter(0.2_s, [&queue, &runAt] { 
        runAt = ApproximateTime::now(); 
        queue->resume();
    });
    queue->dispatchSync([] { });
    EXPECT_NEAR((runAt - start).seconds(), 0.2, .05);
    queue->resume(); // Clean up.
}

TEST(WTF_SuspendableWorkQueue, SuspendedDispatchAfter)
{
    WTF::initializeMainThread();

    auto queue = SuspendableWorkQueue::create("SuspendedDispatchAfter");
    ApproximateTime start = ApproximateTime::now();
    ApproximateTime runAt = start;
    queue->suspend();
    BinarySemaphore semaphore;
    queue->dispatchAfter(0.1_s, [&runAt, &semaphore] { 
        runAt = ApproximateTime::now(); 
        semaphore.signal();
    });
    EXPECT_EQ(start, runAt);
    Util::runFor(.3_s);
    EXPECT_EQ(start, runAt);
    queue->resume();
    semaphore.wait();
    EXPECT_NEAR((runAt - start).seconds(), 0.3, .05);
}

} // namesapce TestWebKitAPI
