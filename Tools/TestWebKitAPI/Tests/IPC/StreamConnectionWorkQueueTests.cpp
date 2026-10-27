/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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

#include "IPCTestUtilities.h"
#include "StreamConnectionWorkQueue.h"
#include "Helpers/Test.h"
#include <wtf/Threading.h>
#include <wtf/threads/BinarySemaphore.h>

namespace TestWebKitAPI {

namespace {

// Called on the work queue as the only work dispatched so far. Returns once stopAndWaitForCompletion()
// has been called: it wakes up the work queue only after it has marked the work queue as stopping.
void waitUntilStopping(IPC::StreamConnectionWorkQueue& queue)
{
    EXPECT_TRUE(queue.wakeUpSemaphore().wait());
}

}

class StreamConnectionWorkQueueTest : public ::testing::Test {
public:
    void SetUp() override
    {
        WTF::initializeMainThread();
    }
};

TEST_F(StreamConnectionWorkQueueTest, IsCurrentAtStopNoCrash)
{
    auto queue = IPC::StreamConnectionWorkQueue::create("StreamConnectionWorkQueueTest work queue"_s);
    for (int i = 0; i < 10000; ++i) {
        queue->dispatch([&] {
            assertIsCurrent(queue);
        });
    }
    queue->stopAndWaitForCompletion();
}

TEST_F(StreamConnectionWorkQueueTest, WorkDispatchedWhileStoppingRuns)
{
    auto queue = IPC::StreamConnectionWorkQueue::create("StreamConnectionWorkQueueTest work queue"_s);
    Vector<int> order;
    queue->dispatch([&] {
        waitUntilStopping(queue);
        order.append(1);
        queue->dispatch([&] {
            order.append(2);
            queue->dispatch([&] {
                order.append(3);
            });
        });
    });
    queue->stopAndWaitForCompletion([&] {
        assertIsCurrent(queue);
        order.append(4);
    });
    EXPECT_EQ(order, (Vector<int> { 1, 2, 3, 4 }));
}

TEST_F(StreamConnectionWorkQueueTest, WorkDispatchedFromOtherThreadWhileStoppingRuns)
{
    auto queue = IPC::StreamConnectionWorkQueue::create("StreamConnectionWorkQueueTest work queue"_s);
    Vector<int> order;
    RefPtr<Thread> otherThread;
    queue->dispatch([&] {
        waitUntilStopping(queue);
        order.append(1);
        BinarySemaphore didDispatch;
        otherThread = Thread::create("StreamConnectionWorkQueueTest other thread"_s, [&] {
            queue->dispatch([&] {
                order.append(2);
            });
            didDispatch.signal();
        });
        didDispatch.wait();
    });
    queue->stopAndWaitForCompletion([&] {
        order.append(3);
    });
    otherThread->waitForCompletion();
    EXPECT_EQ(order, (Vector<int> { 1, 2, 3 }));
}

}
