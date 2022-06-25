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
#include <wtf/Lock.h>

namespace TestWebKitAPI {

TEST(WTF_SuspendableWorkQueue, Suspend)
{
    Lock lock;
    int taskCount = 100;
    int completedTaskCount = 0;
    bool allTasksCompleted = false;
    bool suspended = false;
    bool suspendCompletionHandlerCalled = false;
    auto queue = SuspendableWorkQueue::create("com.apple.WebKit.Test.simple");
    for (int i = 0; i < taskCount; i ++) {
        queue->dispatch([&]() mutable {
            Locker locker { lock };
            suspended = false;
            ++completedTaskCount;
            if (completedTaskCount == taskCount)
                allTasksCompleted = true;
        });
    }
    queue->suspend([&] {
        Locker locker { lock };
        suspended = true;
    }, [&] {
        suspendCompletionHandlerCalled = true;
    });
    Util::run(&suspendCompletionHandlerCalled);
    {
        Locker locker { lock };
        EXPECT_LE(completedTaskCount, taskCount);
        EXPECT_TRUE(suspended);
    }

    queue->resume();
    Util::run(&allTasksCompleted);
    Locker locker { lock };
    EXPECT_EQ(completedTaskCount, taskCount);
    EXPECT_FALSE(suspended);
}

TEST(WTF_SuspendableWorkQueue, SuspendTwice)
{
    Lock lock;
    bool suspendCompletionHandlerCalled = false;
    int suspendCount = 0;
    auto queue = SuspendableWorkQueue::create("com.apple.WebKit.Test.simple");
    queue->suspend([&]() {
        Locker locker { lock };
        ++suspendCount;
    }, [&] {
        suspendCompletionHandlerCalled = true;
    });
    Util::run(&suspendCompletionHandlerCalled);
    {
        Locker locker { lock };
        EXPECT_EQ(1, suspendCount);
    }
    
    suspendCompletionHandlerCalled = false;
    queue->suspend([&]() {
        Locker locker { lock };
        ++suspendCount;
    }, [&] {
        suspendCompletionHandlerCalled = true;
    });
    Util::run(&suspendCompletionHandlerCalled);
    Locker locker { lock };
    EXPECT_EQ(1, suspendCount);
}

} // namesapce TestWebKitAPI
