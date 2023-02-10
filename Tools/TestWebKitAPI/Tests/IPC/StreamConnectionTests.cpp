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

#include "ArgumentCoders.h"
#include "IPCTestUtilities.h"
#include "StreamClientConnection.h"
#include "StreamConnectionWorkQueue.h"
#include "StreamServerConnection.h"
#include "Test.h"
#include "Utilities.h"
#include <optional>
#include <wtf/Lock.h>
#include <wtf/Scope.h>
#include <wtf/threads/BinarySemaphore.h>

namespace TestWebKitAPI {

namespace {
static constexpr Seconds defaultSendTimeout = 1_s;

enum TestObjectIdentifierTag { };
using TestObjectIdentifier = ObjectIdentifier<TestObjectIdentifierTag>;

struct MessageInfo {
    IPC::MessageName messageName;
    uint64_t destinationID;
};

struct MockStreamTestMessage1 {
    static constexpr bool isSync = false;
    static constexpr bool isStreamEncodable = true;
    static constexpr bool isStreamBatched = false;
    static constexpr IPC::MessageName name()  { return IPC::MessageName::RemoteRenderingBackend_ReleaseAllResources; }
    std::tuple<> arguments() { return { }; }
};

struct MockStreamTestMessageWithAsyncReply1 {
    static constexpr bool isSync = false;
    static constexpr bool isStreamEncodable = true;
    static constexpr bool isStreamBatched = false;
    static constexpr IPC::MessageName name()  { return IPC::MessageName::RemoteRenderingBackend_ReleaseResource; }
    // Just using WebPage_GetBytecodeProfileReply as something that is async message name.
    // If WebPage_GetBytecodeProfileReply is removed, just use another one.
    static constexpr IPC::MessageName asyncMessageReplyName() { return IPC::MessageName::WebPage_GetBytecodeProfileReply; }
    std::tuple<uint64_t> arguments() { return { contents }; }
    using ReplyArguments = std::tuple<uint64_t>;
    MockStreamTestMessageWithAsyncReply1(uint64_t contents)
        : contents(contents)
    {
    }
    uint64_t contents;
};


class WaitForMessageMixin {
public:
    ~WaitForMessageMixin()
    {
        ASSERT(m_messages.isEmpty()); // Received unexpected messages.
    }
    MessageInfo waitForMessage()
    {
        Locker locker { m_lock };
        if (m_messages.isEmpty()) {
            m_continueWaitForMessage = false;
            DropLockForScope unlocker { locker };
            while (!m_continueWaitForMessage)
                Util::spinRunLoop(1);
        }
        ASSERT(m_messages.size() >= 1);
        return m_messages.takeLast();
    }

    void waitUntilClosed()
    {
        while (!m_closed)
            Util::spinRunLoop(1);

    }

    void addMessage(IPC::Decoder& decoder)
    {
        ASSERT(!m_closed);
        if (m_asyncMessageHandler && m_asyncMessageHandler(decoder))
            return;
        Locker locker { m_lock };
        m_messages.insert(0, { decoder.messageName(), decoder.destinationID() });
        m_continueWaitForMessage = true;
    }

    void markClosed()
    {
        m_closed = true;
    }

    // Handler returns false if the message should be just recorded.
    void setAsyncMessageHandler(Function<bool(IPC::Decoder&)>&& handler)
    {
        m_asyncMessageHandler = WTFMove(handler);
    }

protected:
    Lock m_lock;
    Vector<MessageInfo> m_messages WTF_GUARDED_BY_LOCK(m_lock);
    std::atomic<bool> m_continueWaitForMessage { false };
    std::atomic<bool> m_closed { false };
    Function<bool(IPC::Decoder&)> m_asyncMessageHandler;
};

class MockMessageReceiver : public IPC::Connection::Client, public WaitForMessageMixin {
public:
    // IPC::Connection::MessageReceiver overrides.
    void didReceiveMessage(IPC::Connection&, IPC::Decoder& decoder) override
    {
        addMessage(decoder);
    }

    bool didReceiveSyncMessage(IPC::Connection&, IPC::Decoder&, UniqueRef<IPC::Encoder>&) override
    {
        return false;
    }

    void didClose(IPC::Connection&) final
    {
        markClosed();
    }

    void didReceiveInvalidMessage(IPC::Connection&, IPC::MessageName) final { ASSERT_NOT_REACHED(); }
};

class MockStreamMessageReceiver : public IPC::StreamMessageReceiver, public WaitForMessageMixin {
public:
    // IPC::StreamMessageReceiver overrides.
    void didReceiveStreamMessage(IPC::StreamServerConnection&, IPC::Decoder& decoder) override
    {
        addMessage(decoder);
    }
};

}

class StreamConnectionTestBase {
public:
    void setupBase()
    {
        WTF::initializeMainThread();
        m_serverQueue = IPC::StreamConnectionWorkQueue::create("StreamConnectionTestBase work queue");
    }

    void teardownBase()
    {
        m_serverQueue->stopAndWaitForCompletion();
    }

    auto localReferenceBarrier()
    {
        return makeScopeExit([this] {
            BinarySemaphore workQueueWait;
            serverQueue().dispatch([&] {
                workQueueWait.signal();
            });
            workQueueWait.wait();
        });
    }

    IPC::StreamConnectionWorkQueue& serverQueue()
    {
        return *m_serverQueue;
    }

protected:
    static constexpr unsigned defaultBufferSizeLog2 = 8;
    RefPtr<IPC::StreamConnectionWorkQueue> m_serverQueue;
};

class StreamConnectionTest : public ::testing::Test, public StreamConnectionTestBase {
public:
    void SetUp() override
    {
        setupBase();
    }

    void TearDown() override
    {
        teardownBase();
    }
};

TEST_F(StreamConnectionTest, OpenConnections)
{
    auto cleanup = localReferenceBarrier();
    auto [clientConnection, serverConnectionHandle] = IPC::StreamClientConnection::create(defaultBufferSizeLog2);
    auto serverConnection = IPC::StreamServerConnection::create(WTFMove(serverConnectionHandle), serverQueue());
    MockMessageReceiver mockClientReceiver;
    clientConnection->open(mockClientReceiver);
    serverQueue().dispatch([this, serverConnection] {
        assertIsCurrent(serverQueue());
        serverConnection->open();
        serverConnection->invalidate();
    });
    mockClientReceiver.waitUntilClosed();
    clientConnection->invalidate();
}

TEST_F(StreamConnectionTest, InvalidateUnopened)
{
    auto cleanup = localReferenceBarrier();
    auto [clientConnection, serverConnectionHandle] = IPC::StreamClientConnection::create(defaultBufferSizeLog2);
    auto serverConnection = IPC::StreamServerConnection::create(WTFMove(serverConnectionHandle), serverQueue());
    serverQueue().dispatch([this, serverConnection] {
        assertIsCurrent(serverQueue());
        serverConnection->invalidate();
    });
    clientConnection->invalidate();
}

class StreamMessageTest : public ::testing::TestWithParam<std::tuple<unsigned>>, public StreamConnectionTestBase {
public:
    unsigned bufferSizeLog2() const
    {
        return std::get<0>(GetParam());
    }

    void SetUp() override
    {
        setupBase();
        auto [clientConnection, serverConnectionHandle] = IPC::StreamClientConnection::create(bufferSizeLog2());
        auto serverConnection = IPC::StreamServerConnection::create(WTFMove(serverConnectionHandle), serverQueue());
        m_clientConnection = WTFMove(clientConnection);
        m_clientConnection->setSemaphores(copyViaEncoder(serverQueue().wakeUpSemaphore()).value(), copyViaEncoder(serverConnection->clientWaitSemaphore()).value());
        m_clientConnection->open(m_mockClientReceiver);
        m_mockServerReceiver = adoptRef(new MockStreamMessageReceiver);
        m_mockServerReceiver->setAsyncMessageHandler([this] (IPC::Decoder& decoder) -> bool {
            assertIsCurrent(serverQueue());
            if (decoder.messageName() != MockStreamTestMessageWithAsyncReply1::name())
                return false;
            using AsyncReplyID =IPC::StreamServerConnection::AsyncReplyID;
            auto contents = decoder.decode<uint64_t>();
            auto asyncReplyID = decoder.decode<AsyncReplyID>();
            ASSERT(decoder.isValid());
            m_serverConnection->sendAsyncReply<MockStreamTestMessageWithAsyncReply1>(asyncReplyID.value_or(AsyncReplyID { }), contents.value_or(0));
            return true;
        });
        serverQueue().dispatch([this, serverConnection = WTFMove(serverConnection)] () mutable {
            assertIsCurrent(serverQueue());
            m_serverConnection = WTFMove(serverConnection);
            m_serverConnection->open();
            m_serverConnection->startReceivingMessages(*m_mockServerReceiver, IPC::receiverName(MockStreamTestMessage1::name()), defaultDestinationID().toUInt64());
        });
        localReferenceBarrier();
    }

    void TearDown() override
    {
        m_clientConnection->invalidate();
        serverQueue().dispatch([&] {
            assertIsCurrent(serverQueue());
            m_serverConnection->stopReceivingMessages(IPC::receiverName(MockStreamTestMessage1::name()), defaultDestinationID().toUInt64());
            m_serverConnection->invalidate();
        });
        teardownBase();
    }

protected:
    static TestObjectIdentifier defaultDestinationID()
    {
        return makeObjectIdentifier<TestObjectIdentifierTag>(77);
    }

    MockMessageReceiver m_mockClientReceiver;
    RefPtr<IPC::StreamClientConnection> m_clientConnection;
    RefPtr<IPC::StreamConnectionWorkQueue> m_serverQueue;
    RefPtr<IPC::StreamServerConnection> m_serverConnection WTF_GUARDED_BY_CAPABILITY(serverQueue());
    RefPtr<MockStreamMessageReceiver> m_mockServerReceiver;
};

TEST_P(StreamMessageTest, Send)
{
    auto cleanup = localReferenceBarrier();
    for (uint64_t i = 0u; i < 55u; ++i) {
        auto success = m_clientConnection->send(MockStreamTestMessage1 { }, defaultDestinationID(), defaultSendTimeout);
        EXPECT_TRUE(success);
    }
    serverQueue().dispatch([&] {
        assertIsCurrent(serverQueue());
        for (uint64_t i = 100u; i < 160u; ++i) {
            auto success = m_serverConnection->send(MockTestMessage1 { }, makeObjectIdentifier<TestObjectIdentifierTag>(i));
            EXPECT_TRUE(success);
        }
    });
    for (uint64_t i = 100u; i < 160u; ++i) {
        auto message = m_mockClientReceiver.waitForMessage();
        EXPECT_EQ(message.messageName, MockTestMessage1::name());
        EXPECT_EQ(message.destinationID, i);
    }
    for (uint64_t i = 0u; i < 55u; ++i) {
        auto message = m_mockServerReceiver->waitForMessage();
        EXPECT_EQ(message.messageName, MockStreamTestMessage1::name());
        EXPECT_EQ(message.destinationID, defaultDestinationID().toUInt64());
    }
}

TEST_P(StreamMessageTest, SendWithSwitchingDestinationIDs)
{
    auto other = makeObjectIdentifier<TestObjectIdentifierTag>(0x1234567891234);
    {
        serverQueue().dispatch([&] {
            assertIsCurrent(serverQueue());
            m_serverConnection->startReceivingMessages(*m_mockServerReceiver, IPC::receiverName(MockStreamTestMessage1::name()), other.toUInt64());
        });
        localReferenceBarrier();
    }
    auto cleanup = makeScopeExit([&] {
        serverQueue().dispatch([&] {
            assertIsCurrent(serverQueue());
            m_serverConnection->stopReceivingMessages(IPC::receiverName(MockStreamTestMessage1::name()), other.toUInt64());
        });
        localReferenceBarrier();
    });

    for (uint64_t i = 0u; i < 777u; ++i) {
        auto success = m_clientConnection->send(MockStreamTestMessage1 { }, defaultDestinationID(), defaultSendTimeout);
        EXPECT_TRUE(success);
        if (i % 77) {
            success = m_clientConnection->send(MockStreamTestMessage1 { }, other, defaultSendTimeout);
            EXPECT_TRUE(success);
        }
    }
    for (uint64_t i = 0u; i < 777u; ++i) {
        auto message = m_mockServerReceiver->waitForMessage();
        EXPECT_EQ(message.messageName, MockStreamTestMessage1::name());
        EXPECT_EQ(message.destinationID, defaultDestinationID().toUInt64());
        if (i % 77) {
            auto message2 = m_mockServerReceiver->waitForMessage();
            EXPECT_EQ(message2.messageName, MockStreamTestMessage1::name());
            EXPECT_EQ(message2.destinationID, other.toUInt64());
        }
    }
}

TEST_P(StreamMessageTest, SendAsyncReply)
{
    auto cleanup = localReferenceBarrier();
    HashSet<uint64_t> replies;
    for (uint64_t i = 100u; i < 155u; ++i) {
        auto result = m_clientConnection->sendWithAsyncReply(MockStreamTestMessageWithAsyncReply1 { i }, [&, j = i] (uint64_t value) {
            EXPECT_GE(value, 100u) << j;
            replies.add(value);
        }, defaultDestinationID(), defaultSendTimeout);
        EXPECT_TRUE(result.isValid());
    }
    while (replies.size() < 55u)
        RunLoop::current().cycle();
    for (uint64_t i = 100u; i < 155u; ++i)
        EXPECT_TRUE(replies.contains(i));
}

TEST_P(StreamMessageTest, SendAsyncReplyCancel)
{
    if (bufferSizeLog2() < 10) {
        // The test sends N messages and expects to cancel them all. Thus it will halt the processing
        // of the messages in the receiving side.
        // Skip if not all messages fit to the buffer.
        return;
    }
    auto cleanup = localReferenceBarrier();

    static std::atomic<bool> waiting;
    static BinarySemaphore workQueueWait;
    serverQueue().dispatch([&] {
        waiting = true;
        workQueueWait.wait();
    });
    while (!waiting)
        RunLoop::current().cycle();

    HashSet<uint64_t> replies;
    for (uint64_t i = 100u; i < 155u; ++i) {
        auto result = m_clientConnection->sendWithAsyncReply(MockStreamTestMessageWithAsyncReply1 { i }, [&, j = i] (uint64_t value) {
            EXPECT_EQ(value, 0u) << j; // Cancel handler returns 0 for uint64_t.
            replies.add(j);
        }, defaultDestinationID(), defaultSendTimeout);
        ASSERT_TRUE(result.isValid());
    }
    m_clientConnection->invalidate();
    workQueueWait.signal();
    // FIXME: this should be more consistent: the async replies are asynchronous, and they cannot be invoked at the
    // point of invalidate as that is not always guaranteed to be in safe call stack.
    // They should be scheduled during invalidate() and ran from the event loop.
    // EXPECT_EQ(0u, replies.size());

    while (replies.size() < 55u)
        RunLoop::current().cycle();
    for (uint64_t i = 100u; i < 155u; ++i)
        EXPECT_TRUE(replies.contains(i));
}

INSTANTIATE_TEST_SUITE_P(StreamConnectionSizedBuffer,
    StreamMessageTest,
    testing::Values(6, 7, 8, 9, 14),
    TestParametersToStringFormatter());

}
