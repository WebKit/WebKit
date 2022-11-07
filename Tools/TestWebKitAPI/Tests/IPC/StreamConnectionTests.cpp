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

namespace TestWebKitAPI {

namespace {
static constexpr Seconds defaultSendTimeout = 1_s;

enum TestObjectIdentifierTag { };
using TestObjectIdentifier = ObjectIdentifier<TestObjectIdentifierTag>;

template <typename T>
std::optional<T> refViaEncoder(const T& o)
{
    IPC::Encoder encoder(static_cast<IPC::MessageName>(78), 0);
    encoder << o;
    auto decoder = IPC::Decoder::create(encoder.buffer(), encoder.bufferSize(), encoder.releaseAttachments());
    return decoder->decode<T>();
}

struct MessageInfo {
    IPC::MessageName messageName;
    uint64_t destinationID;
};

struct MockTestMessage1 {
    static constexpr bool isSync = false;
    static constexpr bool isStreamEncodable = true;
    static constexpr bool isStreamBatched = false;
    static constexpr IPC::MessageName name()  { return static_cast<IPC::MessageName>(123); }
    std::tuple<> arguments() { return { }; }
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

    void addMessage(IPC::Decoder& decoder)
    {
        Locker locker { m_lock };
        m_messages.insert(0, { decoder.messageName(), decoder.destinationID() });
        m_continueWaitForMessage = true;
    }

protected:
    Lock m_lock;
    Vector<MessageInfo> m_messages WTF_GUARDED_BY_LOCK(m_lock);
    std::atomic<bool> m_continueWaitForMessage { false };
};

class MockMessageReceiver : public IPC::MessageReceiver, public WaitForMessageMixin {
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
        auto [clientConnection, serverConnectionHandle] = IPC::StreamClientConnection::createWithDedicatedConnection(m_mockClientReceiver, 1000);
        auto serverConnection = IPC::StreamServerConnection::createWithDedicatedConnection(WTFMove(serverConnectionHandle), *refViaEncoder(clientConnection->streamBuffer()), *m_workQueue);
        m_clientConnection = WTFMove(clientConnection);
        m_serverConnection = WTFMove(serverConnection);
    }
    void TearDown() override
    {
        m_workQueue->stopAndWaitForCompletion();
        m_clientConnection->invalidate();
        m_serverConnection->invalidate();
    }
protected:
    MockMessageReceiver m_mockClientReceiver;
    RefPtr<IPC::StreamConnectionWorkQueue> m_workQueue;
    std::unique_ptr<IPC::StreamClientConnection> m_clientConnection;
    RefPtr<IPC::StreamServerConnection> m_serverConnection;
};

TEST_F(StreamConnectionTest, OpenConnections)
{
    m_clientConnection->open();
    m_serverConnection->open();
    m_clientConnection->invalidate();
    m_serverConnection->invalidate();
}

TEST_F(StreamConnectionTest, SendLocalMessage)
{
    m_clientConnection->open();
    m_serverConnection->open();
    RefPtr<MockStreamMessageReceiver> mockServerReceiver = adoptRef(new MockStreamMessageReceiver);
    m_serverConnection->startReceivingMessages(*mockServerReceiver, IPC::receiverName(MockTestMessage1::name()), 77);
    for (uint64_t i = 0u; i < 55u; ++i)
        m_clientConnection->send(MockTestMessage1 { }, makeObjectIdentifier<TestObjectIdentifier>(77), defaultSendTimeout);
    m_workQueue->dispatch([this] {
        for (uint64_t i = 100u; i < 160u; ++i)
            m_serverConnection->send(MockTestMessage1 { }, makeObjectIdentifier<TestObjectIdentifier>(i));
    });
    for (uint64_t i = 100u; i < 160u; ++i) {
        auto message = m_mockClientReceiver.waitForMessage();
        EXPECT_EQ(message.messageName, MockTestMessage1::name());
        EXPECT_EQ(message.destinationID, i);
    }
    for (uint64_t i = 0u; i < 55u; ++i) {
        auto message = mockServerReceiver->waitForMessage();
        EXPECT_EQ(message.messageName, MockTestMessage1::name());
        EXPECT_EQ(message.destinationID, 77u);
    }
    m_serverConnection->stopReceivingMessages(IPC::receiverName(MockTestMessage1::name()), 77);
}

}
