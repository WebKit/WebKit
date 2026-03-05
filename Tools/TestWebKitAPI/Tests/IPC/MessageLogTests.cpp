/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#include "MessageLog.h"
#include "Test.h"
#include <thread>
#include <wtf/Vector.h>

namespace TestWebKitAPI {

TEST(MessageLogTests, InitialState)
{
    IPC::MessageLog<8> buffer;

    // Verify buffer is initialized with Invalid message names
    EXPECT_EQ(buffer.indexForTesting(), 0u);
    for (size_t i = 0; i < 8; ++i)
        EXPECT_EQ(buffer.bufferForTesting()[i], IPC::MessageName::Invalid);
}

TEST(MessageLogTests, AddSingleMessage)
{
    IPC::MessageLog<8> buffer;

    buffer.add(IPC::MessageName::IPCTester_EmptyMessage);

    EXPECT_EQ(buffer.indexForTesting(), 1u);
    EXPECT_EQ(buffer.bufferForTesting()[0], IPC::MessageName::IPCTester_EmptyMessage);

    // Rest should still be Invalid
    for (size_t i = 1; i < 8; ++i)
        EXPECT_EQ(buffer.bufferForTesting()[i], IPC::MessageName::Invalid);
}

TEST(MessageLogTests, AddMultipleMessages)
{
    IPC::MessageLog<8> buffer;

    buffer.add(IPC::MessageName::IPCTester_EmptyMessage);
    buffer.add(IPC::MessageName::IPCStreamTester_AsyncPing);
    buffer.add(IPC::MessageName::IPCTester_AsyncPing);

    EXPECT_EQ(buffer.indexForTesting(), 3u);
    EXPECT_EQ(buffer.bufferForTesting()[0], IPC::MessageName::IPCTester_EmptyMessage);
    EXPECT_EQ(buffer.bufferForTesting()[1], IPC::MessageName::IPCStreamTester_AsyncPing);
    EXPECT_EQ(buffer.bufferForTesting()[2], IPC::MessageName::IPCTester_AsyncPing);
}

TEST(MessageLogTests, WrapAroundAtCapacity)
{
    IPC::MessageLog<4> buffer;

    // Fill the buffer
    buffer.add(IPC::MessageName::IPCTester_EmptyMessage);
    buffer.add(IPC::MessageName::IPCStreamTester_AsyncPing);
    buffer.add(IPC::MessageName::IPCTester_AsyncPing);
    buffer.add(IPC::MessageName::IPCStreamTester_EmptyMessage);

    EXPECT_EQ(buffer.indexForTesting(), 4u); // Free-running index
    EXPECT_EQ(buffer.bufferForTesting()[0], IPC::MessageName::IPCTester_EmptyMessage);
    EXPECT_EQ(buffer.bufferForTesting()[1], IPC::MessageName::IPCStreamTester_AsyncPing);
    EXPECT_EQ(buffer.bufferForTesting()[2], IPC::MessageName::IPCTester_AsyncPing);
    EXPECT_EQ(buffer.bufferForTesting()[3], IPC::MessageName::IPCStreamTester_EmptyMessage);

    // Add one more, should overwrite index 0
    buffer.add(IPC::MessageName::IPCTester_CreateStreamTester);

    EXPECT_EQ(buffer.indexForTesting(), 5u); // Free-running index
    EXPECT_EQ(buffer.bufferForTesting()[0], IPC::MessageName::IPCTester_CreateStreamTester);
    EXPECT_EQ(buffer.bufferForTesting()[1], IPC::MessageName::IPCStreamTester_AsyncPing);
}

TEST(MessageLogTests, MultipleWraps)
{
    IPC::MessageLog<4> buffer;

    // Add 10 messages to a buffer of size 4
    for (size_t i = 0; i < 10; ++i)
        buffer.add(IPC::MessageName::IPCTester_EmptyMessage);

    // Free-running index should be 10
    EXPECT_EQ(buffer.indexForTesting(), 10u);

    // All entries should have the test message
    for (size_t i = 0; i < 4; ++i)
        EXPECT_EQ(buffer.bufferForTesting()[i], IPC::MessageName::IPCTester_EmptyMessage);
}

TEST(MessageLogTests, ConcurrentAddFromTwoThreads)
{
    constexpr size_t bufferSize = 256; // Power of two
    constexpr size_t messagesPerThread = bufferSize / 2;

    IPC::MessageLog<bufferSize> buffer;

    auto thread1Message = IPC::MessageName::IPCTester_EmptyMessage;
    auto thread2Message = IPC::MessageName::IPCStreamTester_AsyncPing;

    std::thread thread1([&buffer, thread1Message]() {
        for (size_t i = 0; i < messagesPerThread; ++i)
            buffer.add(thread1Message);
    });

    std::thread thread2([&buffer, thread2Message]() {
        for (size_t i = 0; i < messagesPerThread; ++i)
            buffer.add(thread2Message);
    });

    thread1.join();
    thread2.join();

    // Total messages added: 256
    // Buffer size: 256, so no wrapping
    // Expected final index: 256 (free-running)
    EXPECT_EQ(buffer.indexForTesting(), bufferSize);

    // Count occurrences of each message type in the buffer
    size_t thread1Count = 0;
    size_t thread2Count = 0;

    for (size_t i = 0; i < bufferSize; ++i) {
        if (buffer.bufferForTesting()[i] == thread1Message)
            thread1Count++;
        else if (buffer.bufferForTesting()[i] == thread2Message)
            thread2Count++;
        else
            FAIL() << "Unexpected message name at index " << i << ": expected either thread1 or thread2 message";
    }

    // Each thread should have written exactly messagesPerThread entries
    EXPECT_EQ(thread1Count, messagesPerThread);
    EXPECT_EQ(thread2Count, messagesPerThread);

    // Total should equal buffer size
    EXPECT_EQ(thread1Count + thread2Count, bufferSize);
}

TEST(MessageLogTests, ConcurrentAddFromMultipleThreads)
{
    constexpr size_t bufferSize = 512; // Power of two
    constexpr size_t numThreads = 8;
    constexpr size_t messagesPerThread = bufferSize / numThreads;

    IPC::MessageLog<bufferSize> buffer;

    Vector<std::thread> threads;
    Vector<IPC::MessageName> messageNames = {
        IPC::MessageName::IPCTester_EmptyMessage,
        IPC::MessageName::IPCStreamTester_AsyncPing,
        IPC::MessageName::IPCTester_AsyncPing,
        IPC::MessageName::IPCStreamTester_EmptyMessage,
        IPC::MessageName::IPCTester_CreateStreamTester,
        IPC::MessageName::IPCTester_CreateConnectionTester,
        IPC::MessageName::IPCTester_StartMessageTesting,
        IPC::MessageName::IPCTester_CheckTestParameter
    };

    for (size_t t = 0; t < numThreads; ++t) {
        threads.append(std::thread([&buffer, messageName = messageNames[t]]() {
            for (size_t i = 0; i < messagesPerThread; ++i)
                buffer.add(messageName);
        }));
    }

    for (auto& thread : threads)
        thread.join();

    // Total messages: 8 * 64 = 512
    // Buffer size: 512, no wrapping
    // Expected index: 512 (free-running)
    EXPECT_EQ(buffer.indexForTesting(), bufferSize);

    // Count messages from each thread
    Vector<size_t> messageCounts(numThreads, 0);

    for (size_t i = 0; i < bufferSize; ++i) {
        bool found = false;
        for (size_t t = 0; t < numThreads; ++t) {
            if (buffer.bufferForTesting()[i] == messageNames[t]) {
                messageCounts[t]++;
                found = true;
                break;
            }
        }
        EXPECT_TRUE(found) << "Unexpected message at index " << i;
    }

    // Each thread should have written exactly messagesPerThread entries
    for (size_t t = 0; t < numThreads; ++t) {
        EXPECT_EQ(messageCounts[t], messagesPerThread)
            << "Thread " << t << " wrote " << messageCounts[t]
            << " messages, expected " << messagesPerThread;
    }
}

TEST(MessageLogTests, ConcurrentAddWithWrapping)
{
    constexpr size_t bufferSize = 64;
    constexpr size_t numThreads = 4;
    constexpr size_t messagesPerThread = 100; // Total = 400, wraps multiple times

    IPC::MessageLog<bufferSize> buffer;

    Vector<std::thread> threads;
    Vector<IPC::MessageName> messageNames;

    // Each thread uses a unique message name
    for (size_t t = 0; t < numThreads; ++t)
        messageNames.append(static_cast<IPC::MessageName>(static_cast<uint16_t>(IPC::MessageName::IPCTester_EmptyMessage) + t));

    for (size_t t = 0; t < numThreads; ++t) {
        threads.append(std::thread([&buffer, messageName = messageNames[t]]() {
            for (size_t i = 0; i < messagesPerThread; ++i)
                buffer.add(messageName);
        }));
    }

    for (auto& thread : threads)
        thread.join();

    // Free-running index should reflect all messages
    EXPECT_EQ(buffer.indexForTesting(), numThreads * messagesPerThread);

    // Every slot should contain one of the valid message names (no corruption from concurrent wrapping)
    for (size_t i = 0; i < bufferSize; ++i) {
        bool found = false;
        for (size_t t = 0; t < numThreads; ++t) {
            if (buffer.bufferForTesting()[i] == messageNames[t]) {
                found = true;
                break;
            }
        }
        EXPECT_TRUE(found) << "Unexpected message at index " << i;
    }
}

} // namespace TestWebKitAPI
