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

#include "IPCTestUtilities.h"
#include "MessageLog.h"
#include "StreamClientConnection.h"
#include "StreamConnectionWorkQueue.h"
#include "StreamServerConnection.h"
#include <thread>
#include <wtf/Vector.h>
#include <wtf/threads/BinarySemaphore.h>

namespace TestWebKitAPI {

static constexpr Seconds kDefaultWaitForTimeout = 1_s;

// Mock message types for testing different message names in the log
struct MockTestMessage2 {
    static constexpr bool isSync = false;
    static constexpr bool canDispatchOutOfOrder = false;
    static constexpr bool replyCanDispatchOutOfOrder = false;
    static constexpr IPC::MessageName name() { return IPC::MessageName::IPCTester_EmptyMessageWithReply; }
    template<typename Encoder> void encode(Encoder&) { }
};

struct MockTestMessage3 {
    static constexpr bool isSync = false;
    static constexpr bool canDispatchOutOfOrder = false;
    static constexpr bool replyCanDispatchOutOfOrder = false;
    static constexpr IPC::MessageName name() { return IPC::MessageName::IPCTester_SendAsyncMessageToReceiver; }
    template<typename Encoder> void encode(Encoder& encoder)
    {
        encoder << static_cast<uint32_t>(0);
    }
};

struct MockTestMessage4 {
    static constexpr bool isSync = false;
    static constexpr bool canDispatchOutOfOrder = false;
    static constexpr bool replyCanDispatchOutOfOrder = false;
    static constexpr IPC::MessageName name() { return IPC::MessageName::IPCTester_AsyncPing; }
    template<typename Encoder> void encode(Encoder&) { }
};

struct MockTestSyncMessage {
    static constexpr bool isSync = true;
    static constexpr bool canDispatchOutOfOrder = false;
    static constexpr bool replyCanDispatchOutOfOrder = false;
    static constexpr IPC::MessageName name() { return IPC::MessageName::IPCTester_SyncPing; }
    using ReplyArguments = std::tuple<>;
    template<typename Encoder> void encode(Encoder&) { }
};

class MessageLogEndToEndTest : public testing::Test, protected ConnectionTestBase {
public:
    void SetUp() override
    {
        setupBase();
        // Record the initial index to isolate this test from others
        m_initialLogIndex = IPC::messageLog().indexForTesting();
    }

    void TearDown() override
    {
        teardownBase();
    }

protected:
    size_t messagesLoggedSinceSetup() const
    {
        return IPC::messageLog().indexForTesting() - m_initialLogIndex;
    }

    bool messageLogContains(IPC::MessageName messageName, size_t startIndex = 0) const
    {
        const auto& buffer = IPC::messageLog().bufferForTesting();
        size_t currentIndex = IPC::messageLog().indexForTesting();
        size_t capacity = buffer.size();

        // Search from startIndex to currentIndex
        for (size_t i = startIndex; i < currentIndex; ++i) {
            if (buffer[i % capacity] == messageName)
                return true;
        }
        return false;
    }

    size_t countMessagesInLog(IPC::MessageName messageName) const
    {
        const auto& buffer = IPC::messageLog().bufferForTesting();
        size_t capacity = buffer.size();
        size_t count = 0;

        // Count in the entire buffer (for wrap-around tests)
        for (size_t i = 0; i < capacity; ++i) {
            if (buffer[i] == messageName)
                count++;
        }
        return count;
    }

    size_t m_initialLogIndex { 0 };
};

// Test that receiving a single message through IPC logs it
TEST_F(MessageLogEndToEndTest, SingleMessageLogged)
{
    ASSERT_TRUE(openA());
    ASSERT_TRUE(openB());

    size_t indexBeforeSend = IPC::messageLog().indexForTesting();

    // Send a message from A to B
    a()->send(MockTestMessage1 { }, 0);

    // Wait for B to receive the message
    auto message = bClient().waitForMessage(kDefaultWaitForTimeout);
    EXPECT_EQ(message.messageName, MockTestMessage1::name());

    // Verify the message was logged
    size_t indexAfterReceive = IPC::messageLog().indexForTesting();
    EXPECT_GT(indexAfterReceive, indexBeforeSend);

    // The logged message should match what we sent
    EXPECT_TRUE(messageLogContains(MockTestMessage1::name(), indexBeforeSend));
}

// Test that receiving multiple messages logs them all
TEST_F(MessageLogEndToEndTest, MultipleMessagesLogged)
{
    ASSERT_TRUE(openA());
    ASSERT_TRUE(openB());

    size_t indexBeforeSend = IPC::messageLog().indexForTesting();

    constexpr size_t messageCount = 10;

    // Send multiple messages with different types
    for (size_t i = 0; i < messageCount; ++i) {
        if (!(i % 3))
            a()->send(MockTestMessage1 { }, i);
        else if (i % 3 == 1)
            a()->send(MockTestMessage2 { }, i);
        else
            a()->send(MockTestMessage3 { }, i);
    }

    // Wait for all messages to be received
    for (size_t i = 0; i < messageCount; ++i) {
        auto message = bClient().waitForMessage(kDefaultWaitForTimeout);
        EXPECT_EQ(message.destinationID, i);
    }

    // Verify all messages were logged
    size_t indexAfterReceive = IPC::messageLog().indexForTesting();
    EXPECT_GE(indexAfterReceive - indexBeforeSend, messageCount);

    // Verify different message types are in the log
    EXPECT_TRUE(messageLogContains(MockTestMessage1::name(), indexBeforeSend));
    EXPECT_TRUE(messageLogContains(MockTestMessage2::name(), indexBeforeSend));
    EXPECT_TRUE(messageLogContains(MockTestMessage3::name(), indexBeforeSend));
}

// Test that bidirectional messaging logs messages on both sides
TEST_F(MessageLogEndToEndTest, BidirectionalMessagesLogged)
{
    ASSERT_TRUE(openA());
    ASSERT_TRUE(openB());

    size_t indexBeforeSend = IPC::messageLog().indexForTesting();

    // Send messages in both directions
    a()->send(MockTestMessage1 { }, 1);
    b()->send(MockTestMessage2 { }, 2);

    // Wait for A to receive B's message
    auto messageAtA = aClient().waitForMessage(kDefaultWaitForTimeout);
    EXPECT_EQ(messageAtA.messageName, MockTestMessage2::name());

    // Wait for B to receive A's message
    auto messageAtB = bClient().waitForMessage(kDefaultWaitForTimeout);
    EXPECT_EQ(messageAtB.messageName, MockTestMessage1::name());

    // Both messages should be logged (since this is a single-process test,
    // all receives go to the same global log)
    size_t indexAfterReceive = IPC::messageLog().indexForTesting();
    EXPECT_GE(indexAfterReceive - indexBeforeSend, 2u);

    EXPECT_TRUE(messageLogContains(MockTestMessage1::name(), indexBeforeSend));
    EXPECT_TRUE(messageLogContains(MockTestMessage2::name(), indexBeforeSend));
}

// Test that a high volume of messages properly wraps around the log buffer
TEST_F(MessageLogEndToEndTest, HighVolumeWrapsBuffer)
{
    ASSERT_TRUE(openA());
    ASSERT_TRUE(openB());

    // Send more messages than the buffer can hold to test wrap-around
    constexpr size_t messageCount = IPC::messageLogCapacity + 50;

    for (size_t i = 0; i < messageCount; ++i)
        a()->send(MockTestMessage1 { }, i);

    // Wait for all messages to be received
    for (size_t i = 0; i < messageCount; ++i) {
        auto message = bClient().waitForMessage(kDefaultWaitForTimeout);
        EXPECT_EQ(message.destinationID, i);
    }

    // The index should have advanced by at least messageCount
    EXPECT_GE(messagesLoggedSinceSetup(), messageCount);

    // After wrap-around, the buffer should contain mostly our test messages
    // (though earlier ones will have been overwritten)
    size_t testMessageCount = countMessagesInLog(MockTestMessage1::name());
    EXPECT_GT(testMessageCount, 0u);
}

// Test that messages from multiple sending threads are all logged
TEST_F(MessageLogEndToEndTest, ConcurrentSendersLogged)
{
    ASSERT_TRUE(openA());
    ASSERT_TRUE(openB());

    size_t indexBeforeSend = IPC::messageLog().indexForTesting();

    constexpr size_t messagesPerThread = 20;
    std::atomic<size_t> messagesReceived { 0 };

    // Set up handler to count received messages
    bClient().setAsyncMessageHandler([&messagesReceived](IPC::Connection&, IPC::Decoder&) -> bool {
        messagesReceived++;
        return true; // Message handled, don't queue it
    });

    // Create threads that send messages concurrently
    std::thread thread1([this]() {
        for (size_t i = 0; i < messagesPerThread; ++i)
            a()->send(MockTestMessage1 { }, i);
    });

    std::thread thread2([this]() {
        for (size_t i = 0; i < messagesPerThread; ++i)
            a()->send(MockTestMessage2 { }, i + 100);
    });

    thread1.join();
    thread2.join();

    // Wait for all messages to be received
    while (messagesReceived < messagesPerThread * 2)
        Util::spinRunLoop();

    // Give a bit more time for any pending message processing
    Util::spinRunLoop(10);

    // Verify messages were logged
    size_t indexAfterReceive = IPC::messageLog().indexForTesting();
    EXPECT_GE(indexAfterReceive - indexBeforeSend, messagesPerThread * 2);

    // Both message types should be in the log
    EXPECT_TRUE(messageLogContains(MockTestMessage1::name(), indexBeforeSend));
    EXPECT_TRUE(messageLogContains(MockTestMessage2::name(), indexBeforeSend));
}

// Test message logging with different message types interleaved
TEST_F(MessageLogEndToEndTest, InterleavedMessageTypes)
{
    ASSERT_TRUE(openA());
    ASSERT_TRUE(openB());

    size_t indexBeforeSend = IPC::messageLog().indexForTesting();

    // Send interleaved message types
    a()->send(MockTestMessage1 { }, 0);
    a()->send(MockTestMessage2 { }, 1);
    a()->send(MockTestMessage3 { }, 2);
    a()->send(MockTestMessage4 { }, 3);
    a()->send(MockTestMessage1 { }, 4);
    a()->send(MockTestMessage2 { }, 5);

    // Wait for all messages
    for (size_t i = 0; i < 6; ++i) {
        auto message = bClient().waitForMessage(kDefaultWaitForTimeout);
        EXPECT_EQ(message.destinationID, i);
    }

    size_t indexAfterReceive = IPC::messageLog().indexForTesting();
    EXPECT_GE(indexAfterReceive - indexBeforeSend, 6u);

    // All message types should be logged
    EXPECT_TRUE(messageLogContains(MockTestMessage1::name(), indexBeforeSend));
    EXPECT_TRUE(messageLogContains(MockTestMessage2::name(), indexBeforeSend));
    EXPECT_TRUE(messageLogContains(MockTestMessage3::name(), indexBeforeSend));
    EXPECT_TRUE(messageLogContains(MockTestMessage4::name(), indexBeforeSend));
}

// Test that message logging works with async replies
TEST_F(MessageLogEndToEndTest, AsyncReplyMessagesLogged)
{
    ASSERT_TRUE(openA());
    ASSERT_TRUE(openB());

    size_t indexBeforeSend = IPC::messageLog().indexForTesting();

    // Set up A to respond to async reply messages
    aClient().setAsyncMessageHandler([this](IPC::Connection&, IPC::Decoder& decoder) -> bool {
        auto listenerID = decoder.decode<uint64_t>();
        auto encoder = makeUniqueRef<IPC::Encoder>(MockTestMessageWithAsyncReply1::asyncMessageReplyName(), *listenerID);
        encoder.get() << decoder.destinationID();
        a()->sendSyncReply(WTF::move(encoder));
        return true;
    });

    bool gotReply = false;
    uint64_t replyValue = 0;

    b()->sendWithAsyncReply(MockTestMessageWithAsyncReply1 { }, [&gotReply, &replyValue](uint64_t value) {
        replyValue = value;
        gotReply = true;
    }, 42);

    // Wait for reply
    Util::runFor(&gotReply, kDefaultWaitForTimeout);
    EXPECT_TRUE(gotReply);
    EXPECT_EQ(replyValue, 42u);

    // The async message should have been logged when received by A
    size_t indexAfterReceive = IPC::messageLog().indexForTesting();
    EXPECT_GT(indexAfterReceive, indexBeforeSend);
    EXPECT_TRUE(messageLogContains(MockTestMessageWithAsyncReply1::name(), indexBeforeSend));
}

// Test that sync messages are logged on receive
TEST_F(MessageLogEndToEndTest, SyncMessageLogged)
{
    ASSERT_TRUE(openA());
    ASSERT_TRUE(openB());

    size_t indexBeforeSend = IPC::messageLog().indexForTesting();

    // Set up B to handle sync messages
    bClient().setSyncMessageHandler([](IPC::Connection& connection, IPC::Decoder&, UniqueRef<IPC::Encoder>& encoder) -> bool {
        connection.sendSyncReply(WTF::move(encoder));
        return true;
    });

    auto result = a()->sendSync(MockTestSyncMessage { }, 0, kDefaultWaitForTimeout);
    EXPECT_TRUE(result.succeeded());

    // The sync message should have been logged when received by B
    size_t indexAfterReceive = IPC::messageLog().indexForTesting();
    EXPECT_GT(indexAfterReceive, indexBeforeSend);
    EXPECT_TRUE(messageLogContains(MockTestSyncMessage::name(), indexBeforeSend));
}

// Test message logging across run loops
class MessageLogRunLoopTest : public MessageLogEndToEndTest {
public:
    void TearDown() override
    {
        // By convention the b() connection is the one that gets opened on various runloops.
        ASSERT(!b() || !b()->client());
        MessageLogEndToEndTest::TearDown();
        EXPECT_EQ(m_runLoops.size(), 0u);
    }

    Ref<RunLoop> createRunLoop(ASCIILiteral name)
    {
        auto runLoop = RunLoop::create(name, ThreadType::Unknown);
        m_runLoops.append(runLoop);
        return runLoop;
    }

    void localReferenceBarrier()
    {
        Vector<Ref<Thread>> threadsToWait;
        for (auto& runLoop : std::exchange(m_runLoops, { })) {
            BinarySemaphore semaphore;
            runLoop->dispatch([&semaphore, &threadsToWait] {
                threadsToWait.append(Thread::currentSingleton());
                RunLoop::currentSingleton().stop();
                semaphore.signal();
            });
            semaphore.wait();
        }
        while (true) {
            sleep(0.1_s);
            bool allExited = true;
            for (auto& thread : threadsToWait) {
                if (Thread::allThreads().contains(thread.get())) {
                    allExited = false;
                    break;
                }
            }
            if (allExited)
                break;
        }
    }

protected:
    Vector<Ref<RunLoop>> m_runLoops;
};

#define LOCAL_STRINGIFY(x) #x
#define RUN_LOOP_NAME "MessageLogRunLoopTest at MessageLogEndToEndTests.cpp:" LOCAL_STRINGIFY(__LINE__) ""_s

TEST_F(MessageLogRunLoopTest, MessagesLoggedAcrossRunLoops)
{
    ASSERT_TRUE(openA());

    size_t indexBeforeSend = IPC::messageLog().indexForTesting();

    auto runLoop = createRunLoop(RUN_LOOP_NAME);
    BinarySemaphore receivedSemaphore;

    runLoop->dispatch([this, &receivedSemaphore] {
        ASSERT_TRUE(openB());

        // Wait for messages on this run loop
        for (size_t i = 0; i < 5; ++i) {
            auto message = bClient().waitForMessage(kDefaultWaitForTimeout);
            EXPECT_EQ(message.destinationID, i);
        }
        receivedSemaphore.signal();
    });

    // Give B time to open
    sleep(0.1_s);

    // Send messages from main thread
    for (size_t i = 0; i < 5; ++i)
        a()->send(MockTestMessage1 { }, i);

    receivedSemaphore.wait();

    // Verify messages were logged
    size_t indexAfterReceive = IPC::messageLog().indexForTesting();
    EXPECT_GE(indexAfterReceive - indexBeforeSend, 5u);
    EXPECT_TRUE(messageLogContains(MockTestMessage1::name(), indexBeforeSend));

    runLoop->dispatch([this] {
        b()->invalidate();
    });
    localReferenceBarrier();
}

#undef RUN_LOOP_NAME
#undef LOCAL_STRINGIFY

// --- StreamServerConnection message log tests ---

namespace {

enum MessageLogTestObjectIdentifierTag { };
using MessageLogTestObjectIdentifier = ObjectIdentifier<MessageLogTestObjectIdentifierTag>;

struct MockStreamMessage {
    static constexpr bool isSync = false;
    static constexpr bool isStreamEncodable = true;
    static constexpr bool isStreamBatched = false;
    static constexpr IPC::MessageName name() { return IPC::MessageName::IPCStreamTester_EmptyMessage; }
    template<typename Encoder> void encode(Encoder&) { }
};

class MockStreamServerReceiver final : public IPC::StreamServerConnection::Client, public WaitForMessageMixin {
    WTF_MAKE_TZONE_ALLOCATED(MockStreamServerReceiver);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(MockStreamServerReceiver);

public:
    static Ref<MockStreamServerReceiver> create() { return adoptRef(*new MockStreamServerReceiver()); }

private:
    MockStreamServerReceiver() = default;

    void didReceiveStreamMessage(IPC::StreamServerConnection&, IPC::Decoder& decoder) final
    {
        addMessage(decoder);
    }

    void didReceiveInvalidMessage(IPC::StreamServerConnection&, IPC::MessageName messageName, const Vector<uint32_t>& failIndices) final
    {
        addInvalidMessage(messageName, failIndices);
    }
};

WTF_MAKE_TZONE_ALLOCATED_IMPL(MockStreamServerReceiver);

} // anonymous namespace

class MessageLogStreamTest : public testing::Test {
public:
    void SetUp() override
    {
        WTF::initializeMainThread();
        m_serverQueue = IPC::StreamConnectionWorkQueue::create("MessageLogStreamTest work queue"_s);

        auto connectionPair = IPC::StreamClientConnection::create(defaultBufferSizeLog2, defaultTimeout);
        ASSERT_TRUE(!!connectionPair);
        auto [clientConnection, serverConnectionHandle] = WTF::move(*connectionPair);
        auto serverConnection = IPC::StreamServerConnection::tryCreate(WTF::move(serverConnectionHandle), { }).releaseNonNull();

        m_clientConnection = WTF::move(clientConnection);
        m_clientConnection->setSemaphores(copyViaEncoder(m_serverQueue->wakeUpSemaphore()).value(), copyViaEncoder(serverConnection->clientWaitSemaphore()).value());

        m_clientConnection->open(m_mockClientReceiver);

        m_mockServerReceiver = MockStreamServerReceiver::create();
        m_serverQueue->dispatch([this, serverConnection = WTF::move(serverConnection)]() mutable {
            m_serverConnection = WTF::move(serverConnection);
            m_serverConnection->open(*m_mockServerReceiver, *m_serverQueue);
            m_serverConnection->startReceivingMessages(*m_mockServerReceiver, IPC::receiverName(MockStreamMessage::name()), defaultDestinationID().toUInt64());
        });
        {
            BinarySemaphore semaphore;
            m_serverQueue->dispatch([&semaphore] {
                semaphore.signal();
            });
            semaphore.wait();
        }

        m_initialLogIndex = IPC::messageLog().indexForTesting();
    }

    void TearDown() override
    {
        m_clientConnection->invalidate();
        {
            m_serverQueue->dispatch([this] {
                m_serverConnection->stopReceivingMessages(IPC::receiverName(MockStreamMessage::name()), defaultDestinationID().toUInt64());
                m_serverConnection->invalidate();
            });
            BinarySemaphore semaphore;
            m_serverQueue->dispatch([&semaphore] {
                semaphore.signal();
            });
            semaphore.wait();
        }
        m_serverQueue->stopAndWaitForCompletion();
    }

protected:
    static MessageLogTestObjectIdentifier defaultDestinationID()
    {
        return ObjectIdentifier<MessageLogTestObjectIdentifierTag>(77);
    }

    bool messageLogContains(IPC::MessageName messageName) const
    {
        const auto& buffer = IPC::messageLog().bufferForTesting();
        size_t currentIndex = IPC::messageLog().indexForTesting();
        size_t capacity = buffer.size();
        for (size_t i = m_initialLogIndex; i < currentIndex; ++i) {
            if (buffer[i % capacity] == messageName)
                return true;
        }
        return false;
    }

    static constexpr Seconds defaultTimeout = 100_s;
    static constexpr unsigned defaultBufferSizeLog2 = 8;

    RefPtr<IPC::StreamConnectionWorkQueue> m_serverQueue;
    RefPtr<IPC::StreamClientConnection> m_clientConnection;
    RefPtr<IPC::StreamServerConnection> m_serverConnection;
    Ref<MockConnectionClient> m_mockClientReceiver { MockConnectionClient::create() };
    RefPtr<MockStreamServerReceiver> m_mockServerReceiver;
    size_t m_initialLogIndex { 0 };
};

TEST_F(MessageLogStreamTest, StreamMessageLogged)
{
    constexpr size_t messageCount = 5;

    for (size_t i = 0; i < messageCount; ++i) {
        auto result = m_clientConnection->send(MockStreamMessage { }, defaultDestinationID());
        EXPECT_EQ(result, IPC::Error::NoError);
    }

    // Wait for all messages to be received on the server side
    for (size_t i = 0; i < messageCount; ++i) {
        auto message = m_mockServerReceiver->waitForMessage(defaultTimeout);
        EXPECT_EQ(message.messageName, MockStreamMessage::name());
    }

    // The stream messages should have been logged via StreamServerConnection::dispatchStreamMessage
    EXPECT_TRUE(messageLogContains(MockStreamMessage::name()));
}

} // namespace TestWebKitAPI
