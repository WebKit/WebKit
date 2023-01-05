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
#include "StreamClientConnection.h"
#include "StreamConnectionWorkQueue.h"
#include "StreamServerConnection.h"
#include "Test.h"
#include "Utilities.h"
#include <optional>
#include <wtf/Lock.h>
#include <wtf/threads/BinarySemaphore.h>

namespace TestWebKitAPI {

namespace {
static constexpr Seconds defaultSendTimeout = 1_s;

enum TestObjectIdentifierTag { };
using TestObjectIdentifier = ObjectIdentifier<TestObjectIdentifierTag>;

struct MessageInfo {
    IPC::MessageName messageName;
    UInt128 destinationID;
};

struct MockStreamTestMessage1 {
    static constexpr bool isSync = false;
    static constexpr bool isStreamEncodable = true;
    static constexpr bool isStreamBatched = false;
    static constexpr IPC::MessageName name()  { return static_cast<IPC::MessageName>(123); }
    std::tuple<> arguments() { return { }; }
};

struct MockStreamTestMessageWithAsyncReply1 {
    static constexpr bool isSync = false;
    static constexpr bool isStreamEncodable = true;
    static constexpr bool isStreamBatched = false;
    static constexpr IPC::MessageName name()  { return static_cast<IPC::MessageName>(124); }
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

class StreamConnectionTest : public ::testing::Test {
public:
    StreamConnectionTest()
        : m_workQueue(IPC::StreamConnectionWorkQueue::create("StreamConnectionTest work queue"))
    {
    }

    void SetUp() override
    {
        WTF::initializeMainThread();
        auto [clientConnection, serverConnectionHandle] = IPC::StreamClientConnection::create(10000);
        auto serverConnection = IPC::StreamServerConnection::create(WTFMove(serverConnectionHandle), *m_workQueue);
        m_clientConnection = WTFMove(clientConnection);
        m_serverConnection = WTFMove(serverConnection);
    }

    void TearDown() override
    {
        m_workQueue->stopAndWaitForCompletion();
        m_clientConnection->invalidate();
        m_serverConnection->invalidate();
    }

    void localReferenceBarrier()
    {
        BinarySemaphore workQueueWait;
        m_workQueue->dispatch([&] {
            workQueueWait.signal();
        });
        workQueueWait.wait();
    }

protected:
    MockMessageReceiver m_mockClientReceiver;
    RefPtr<IPC::StreamConnectionWorkQueue> m_workQueue;
    RefPtr<IPC::StreamClientConnection> m_clientConnection;
    RefPtr<IPC::StreamServerConnection> m_serverConnection;
};

TEST_F(StreamConnectionTest, OpenConnections)
{
    m_clientConnection->open(m_mockClientReceiver);
    m_serverConnection->open();
    m_serverConnection->invalidate();
    m_mockClientReceiver.waitUntilClosed();
    m_clientConnection->invalidate();
}

TEST_F(StreamConnectionTest, SendMessage)
{
    m_clientConnection->open(m_mockClientReceiver);
    m_serverConnection->open();
    RefPtr<MockStreamMessageReceiver> mockServerReceiver = adoptRef(new MockStreamMessageReceiver);
    m_serverConnection->startReceivingMessages(*mockServerReceiver, IPC::receiverName(MockStreamTestMessage1::name()), 77);
    for (uint64_t i = 0u; i < 55u; ++i)
        m_clientConnection->send(MockStreamTestMessage1 { }, makeObjectIdentifier<TestObjectIdentifier>(77), defaultSendTimeout);
    m_workQueue->dispatch([this] {
        for (uint64_t i = 100u; i < 160u; ++i)
            m_serverConnection->send(MockStreamTestMessage1 { }, makeObjectIdentifier<TestObjectIdentifier>(i));
    });
    for (uint64_t i = 100u; i < 160u; ++i) {
        auto message = m_mockClientReceiver.waitForMessage();
        EXPECT_EQ(message.messageName, MockStreamTestMessage1::name());
        EXPECT_EQ(message.destinationID, i);
    }
    for (uint64_t i = 0u; i < 55u; ++i) {
        auto message = mockServerReceiver->waitForMessage();
        EXPECT_EQ(message.messageName, MockStreamTestMessage1::name());
        EXPECT_EQ(message.destinationID, 77u);
    }
    m_serverConnection->stopReceivingMessages(IPC::receiverName(MockStreamTestMessage1::name()), 77);
}

TEST_F(StreamConnectionTest, SendAsyncReplyMessage)
{
    m_clientConnection->open(m_mockClientReceiver);
    m_serverConnection->open();
    RefPtr<MockStreamMessageReceiver> mockServerReceiver = adoptRef(new MockStreamMessageReceiver);
    mockServerReceiver->setAsyncMessageHandler([&] (IPC::Decoder& decoder) -> bool {
        using AsyncReplyID =IPC::StreamServerConnection::AsyncReplyID;
        auto contents = decoder.decode<uint64_t>();
        auto asyncReplyID = decoder.decode<AsyncReplyID>();
        ASSERT(decoder.isValid());
        m_serverConnection->sendAsyncReply<MockStreamTestMessageWithAsyncReply1>(asyncReplyID.value_or(AsyncReplyID { }), contents.value_or(0));
        return true;
    });
    m_serverConnection->startReceivingMessages(*mockServerReceiver, IPC::receiverName(MockStreamTestMessageWithAsyncReply1::name()), 77);

    HashSet<uint64_t> replies;
    for (uint64_t i = 100u; i < 155u; ++i) {
        m_clientConnection->sendWithAsyncReply(MockStreamTestMessageWithAsyncReply1 { i }, [&, j = i] (uint64_t value) {
            EXPECT_GE(value, 100u) << j;
            replies.add(value);
        }, makeObjectIdentifier<TestObjectIdentifier>(77), defaultSendTimeout);
    }
    while (replies.size() < 55u)
        RunLoop::current().cycle();
    for (uint64_t i = 100u; i < 155u; ++i)
        EXPECT_TRUE(replies.contains(i));
    m_serverConnection->stopReceivingMessages(IPC::receiverName(MockStreamTestMessageWithAsyncReply1::name()), 77);
}

TEST_F(StreamConnectionTest, SendAsyncReplyMessageCancel)
{
    m_clientConnection->open(m_mockClientReceiver);
    m_serverConnection->open();
    RefPtr<MockStreamMessageReceiver> mockServerReceiver = adoptRef(new MockStreamMessageReceiver);
    mockServerReceiver->setAsyncMessageHandler([&] (IPC::Decoder& decoder) -> bool {
        using AsyncReplyID =IPC::StreamServerConnection::AsyncReplyID;
        auto contents = decoder.decode<uint64_t>();
        auto asyncReplyID = decoder.decode<AsyncReplyID>();
        ASSERT(decoder.isValid());
        m_serverConnection->sendAsyncReply<MockStreamTestMessageWithAsyncReply1>(asyncReplyID.value_or(AsyncReplyID { }), contents.value_or(0));
        return true;
    });
    m_serverConnection->startReceivingMessages(*mockServerReceiver, IPC::receiverName(MockStreamTestMessageWithAsyncReply1::name()), 77);

    std::atomic<bool> waiting;
    BinarySemaphore workQueueWait;
    m_workQueue->dispatch([&] {
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
        }, makeObjectIdentifier<TestObjectIdentifier>(77), defaultSendTimeout);
        EXPECT_NE(0u, result.toUInt64());
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
    m_serverConnection->stopReceivingMessages(IPC::receiverName(MockStreamTestMessageWithAsyncReply1::name()), 77);
    localReferenceBarrier();
}

}
