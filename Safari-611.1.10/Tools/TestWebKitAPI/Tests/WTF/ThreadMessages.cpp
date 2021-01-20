/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#include <wtf/HashSet.h>
#include <wtf/Ref.h>
#include <wtf/ThreadMessage.h>
#include <wtf/Vector.h>

static void runThreadMessageTest(unsigned numSenders, unsigned numMessages)
{
    Atomic<bool> receiverShouldKeepRunning(true);
    RefPtr<Thread> receiverThread = Thread::create("ThreadMessage receiver", [&receiverShouldKeepRunning] () {
        while (receiverShouldKeepRunning.load()) { }
    });
    ASSERT_TRUE(receiverThread);

    Vector<RefPtr<Thread>> senderThreads(numSenders);
    Vector<unsigned> messagesRun(numSenders);
    Vector<unsigned> handlersRun(numSenders);
    messagesRun.fill(0);
    handlersRun.fill(0);

    for (unsigned senderID = 0; senderID < numSenders; ++senderID) {
        senderThreads[senderID] = Thread::create("ThreadMessage sender", [senderID, numMessages, receiverThread, &messagesRun, &handlersRun] () {
            for (unsigned i = 0; i < numMessages; ++i) {
                auto result = sendMessage(*receiverThread.get(), [senderID, &handlersRun] (PlatformRegisters&) {
                    handlersRun[senderID]++;
                });
                EXPECT_TRUE(result == WTF::MessageStatus::MessageRan);
                messagesRun[senderID]++;
            }
        });
        ASSERT_TRUE(senderThreads[senderID]);
    }

    for (unsigned i = 0; i < numSenders; ++i)
        senderThreads[i]->waitForCompletion();

    receiverShouldKeepRunning.store(false);
    receiverThread->waitForCompletion();

    for (unsigned i = 0; i < numSenders; ++i) {
        EXPECT_EQ(numMessages, messagesRun[i]);
        EXPECT_EQ(numMessages, handlersRun[i]);
    }
}

TEST(ThreadMessage, Basic)
{
    runThreadMessageTest(1, 1);
    runThreadMessageTest(1, 100);
}

TEST(ThreadMessage, MultipleSenders)
{
    runThreadMessageTest(10, 1);
    runThreadMessageTest(10, 100);
    runThreadMessageTest(10, 10000);
}
