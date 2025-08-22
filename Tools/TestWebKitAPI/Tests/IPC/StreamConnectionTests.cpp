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
static constexpr Seconds defaultTimeout = 1_s;
static constexpr unsigned defaultBufferSizeLog2 = 8;


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
    static constexpr IPC::MessageName name()  { return IPC::MessageName::IPCStreamTester_EmptyMessage; }
    template<typename Encoder> void encode(Encoder&) { }
};

struct MockStreamTestMessageNotStreamEncodable {
    static constexpr bool isSync = false;
    static constexpr bool isStreamEncodable = false;
    static constexpr IPC::MessageName name()  { return IPC::MessageName::IPCStreamTester_EmptyMessage; }
    explicit MockStreamTestMessageNotStreamEncodable(IPC::Semaphore&& s)
        : semaphore(WTFMove(s))
    {
    }
    template<typename Encoder>
    void encode(Encoder& encoder)
    {
        encoder << WTFMove(semaphore);
    }

    IPC::Semaphore semaphore;
};

struct MockStreamTestMessageWithAsyncReply1 {
    static constexpr bool isSync = false;
    static constexpr bool isStreamEncodable = true;
    static constexpr bool isStreamBatched = false;
    static constexpr IPC::MessageName name()  { return IPC::MessageName::IPCStreamTester_AsyncPing; }
    // Just using IPCStreamTester_AsyncPingReply as something that is async message name.
    static constexpr IPC::MessageName asyncMessageReplyName() { return IPC::MessageName::IPCStreamTester_AsyncPingReply; }

    template<typename Encoder>
    void encode(Encoder& encoder)
    {
        encoder << contents;
    }

    using ReplyArguments = std::tuple<uint64_t>;
    MockStreamTestMessageWithAsyncReply1(uint64_t contents)
        : contents(contents)
    {
    }
    uint64_t contents;
};

class MockSyncMessage {
public:
    using Arguments = std::tuple<uint32_t>;
    static IPC::MessageName name() { return IPC::MessageName::IPCStreamTester_SyncMessage; }
    static constexpr bool isSync = true;
    static constexpr bool isStreamEncodable = true;
    static constexpr bool isReplyStreamEncodable = true;
    using ReplyArguments = std::tuple<uint32_t>;
    using Reply = CompletionHandler<void(uint32_t)>;
    explicit MockSyncMessage(uint32_t value)
        : m_arguments(value)
    {
    }
    template<typename Encoder>
    void encode(Encoder& encoder)
    {
        encoder << m_arguments;
    }
private:
    std::tuple<uint32_t> m_arguments;
};

#if ENABLE(IPC_TESTING_API)
class MockSyncMessageNotStreamEncodableBoth {
public:
    using Arguments = std::tuple<uint32_t>;
    static IPC::MessageName name() { return IPC::MessageName::IPCStreamTester_SyncMessageNotStreamEncodableBoth; }
    static constexpr bool isSync = true;
    static constexpr bool isStreamEncodable = false;
    static constexpr bool isReplyStreamEncodable = false;
    using ReplyArguments = std::tuple<uint32_t>;
    using Reply = CompletionHandler<void(uint32_t)>;
    explicit MockSyncMessageNotStreamEncodableBoth(uint32_t value)
        : m_arguments(value)
    {
    }
    template<typename Encoder>
    void encode(Encoder& encoder)
    {
        encoder << m_arguments;
    }
private:
    std::tuple<uint32_t> m_arguments;
};
#endif

class MockSyncMessageNotStreamEncodableReply {
public:
    using Arguments = std::tuple<uint32_t>;
    static IPC::MessageName name() { return IPC::MessageName::IPCStreamTester_SyncMessageNotStreamEncodableReply; }
    static constexpr bool isSync = true;
    static constexpr bool isStreamEncodable = true;
    static constexpr bool isReplyStreamEncodable = false;
    using ReplyArguments = std::tuple<uint32_t>;
    using Reply = CompletionHandler<void(uint32_t)>;
    explicit MockSyncMessageNotStreamEncodableReply(uint32_t value)
        : m_arguments(value)
    {
    }
    template<typename Encoder>
    void encode(Encoder& encoder)
    {
        encoder << m_arguments;
    }
private:
    std::tuple<uint32_t> m_arguments;
};

using MockStreamClientConnectionClient = MockConnectionClient;

class MockStreamServerConnectionClient final : public IPC::StreamServerConnection::Client, public WaitForMessageMixin {
    WTF_MAKE_TZONE_ALLOCATED(MockStreamServerConnectionClient);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(MockStreamServerConnectionClient);

public:
    static Ref<MockStreamServerConnectionClient> create() { return adoptRef(*new MockStreamServerConnectionClient()); }

    // Handler returns false if the message should be just recorded.
    void setAsyncMessageHandler(Function<bool(IPC::StreamServerConnection&, IPC::Decoder&)>&& handler)
    {
        m_asyncMessageHandler = WTFMove(handler);
    }

    // Handler returns false if the message should be just recorded.
    void setSyncMessageHandler(Function<bool(IPC::StreamServerConnection&, IPC::Decoder&)>&& handler)
    {
        m_syncMessageHandler = WTFMove(handler);
    }
    // Handler returns false if the message should be just recorded.
    void setInvalidMessageHandler(Function<bool(IPC::StreamServerConnection&, IPC::MessageName, const Vector<uint32_t>&)>&& handler)
    {
        m_invalidMessageHandler = WTFMove(handler);
    }

private:
    MockStreamServerConnectionClient() = default;

    // IPC::StreamServerConnection::Client overrides.
    void didReceiveStreamMessage(IPC::StreamServerConnection& connection, IPC::Decoder& decoder) final
    {
        if (decoder.isSyncMessage()) {
            if (m_syncMessageHandler && m_syncMessageHandler(connection, decoder))
                return;
            return;
        }
        if (m_asyncMessageHandler && m_asyncMessageHandler(connection, decoder))
            return;
        addMessage(decoder);
    }

    void didReceiveInvalidMessage(IPC::StreamServerConnection& connection, IPC::MessageName messageName, const Vector<uint32_t>& indicesOfObjectsFailingDecoding) final
    {
        if (m_invalidMessageHandler && m_invalidMessageHandler(connection, messageName, indicesOfObjectsFailingDecoding))
            return;
        addInvalidMessage(messageName, indicesOfObjectsFailingDecoding);
    }

    Function<bool(IPC::StreamServerConnection&, IPC::Decoder&)> m_asyncMessageHandler;
    Function<bool(IPC::StreamServerConnection&, IPC::Decoder&)> m_syncMessageHandler;
    Function<bool(IPC::StreamServerConnection&, IPC::MessageName, const Vector<uint32_t>&)> m_invalidMessageHandler;
};

WTF_MAKE_TZONE_ALLOCATED_IMPL(MockStreamServerConnectionClient);

class StreamConnectionTestBase {
public:
    void setupBase()
    {
        WTF::initializeMainThread();
        m_serverQueue = IPC::StreamConnectionWorkQueue::create("StreamConnectionTestBase work queue"_s);
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
    auto connectionPair = IPC::StreamClientConnection::create(defaultBufferSizeLog2, defaultTimeout);
    ASSERT_TRUE(!!connectionPair);
    auto [clientConnection, serverConnectionHandle] = WTFMove(*connectionPair);
    auto serverConnection = IPC::StreamServerConnection::tryCreate(WTFMove(serverConnectionHandle), { }).releaseNonNull();
    auto cleanup = localReferenceBarrier();
    Ref mockClientReceiver = MockStreamClientConnectionClient::create();
    clientConnection->open(mockClientReceiver);
    serverQueue().dispatch([this, serverConnection, mockClientReceiver] {
        assertIsCurrent(serverQueue());
        Ref<MockStreamServerConnectionClient> mockServerReceiver = MockStreamServerConnectionClient::create();
        serverConnection->open(mockServerReceiver, serverQueue());
        serverConnection->invalidate();
    });
    mockClientReceiver->waitForDidClose(defaultTimeout);
    clientConnection->invalidate();
}

TEST_F(StreamConnectionTest, InvalidateUnopened)
{
    auto connectionPair = IPC::StreamClientConnection::create(defaultBufferSizeLog2, defaultTimeout);
    ASSERT_TRUE(!!connectionPair);
    auto [clientConnection, serverConnectionHandle] = WTFMove(*connectionPair);
    auto serverConnection = IPC::StreamServerConnection::tryCreate(WTFMove(serverConnectionHandle), { }).releaseNonNull();
    auto cleanup = localReferenceBarrier();
    serverQueue().dispatch([this, serverConnection] {
        assertIsCurrent(serverQueue());
        serverConnection->invalidate();
    });
    clientConnection->invalidate();
}

class StreamMessageTest : public ::testing::TestWithParam<std::tuple<unsigned>>, public StreamConnectionTestBase {
public:
    StreamMessageTest()
        : m_mockClientReceiver(MockStreamClientConnectionClient::create())
    {
    }

    unsigned bufferSizeLog2() const
    {
        return std::get<0>(GetParam());
    }

    void SetUp() override
    {
        setupBase();
        auto connectionPair = IPC::StreamClientConnection::create(bufferSizeLog2(), defaultTimeout);
        ASSERT(!!connectionPair);
        auto [clientConnection, serverConnectionHandle] = WTFMove(*connectionPair);
        auto serverConnection = IPC::StreamServerConnection::tryCreate(WTFMove(serverConnectionHandle), { }).releaseNonNull();
        m_clientConnection = WTFMove(clientConnection);
        m_clientConnection->setSemaphores(copyViaEncoder(serverQueue().wakeUpSemaphore()).value(), copyViaEncoder(serverConnection->clientWaitSemaphore()).value());
        m_clientConnection->open(m_mockClientReceiver);
        m_mockServerReceiver = MockStreamServerConnectionClient::create();
        m_mockServerReceiver->setAsyncMessageHandler([] (IPC::StreamServerConnection& connection, IPC::Decoder& decoder) {
            if (decoder.messageName() != MockStreamTestMessageWithAsyncReply1::name())
                return false;
            using AsyncReplyID = IPC::StreamServerConnection::AsyncReplyID;
            auto contents = decoder.decode<uint64_t>();
            ASSERT(contents);
            auto asyncReplyID = decoder.decode<AsyncReplyID>();
            ASSERT(asyncReplyID);
            ASSERT(decoder.isValid());
            connection.sendAsyncReply<MockStreamTestMessageWithAsyncReply1>(*asyncReplyID, *contents);
            return true;
        });
        serverQueue().dispatch([this, serverConnection = WTFMove(serverConnection)] () mutable {
            assertIsCurrent(serverQueue());
            m_serverConnection = WTFMove(serverConnection);
            m_serverConnection->open(*m_mockServerReceiver, serverQueue());
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
        return ObjectIdentifier<TestObjectIdentifierTag>(77);
    }

    Ref<MockStreamClientConnectionClient> m_mockClientReceiver;
    RefPtr<IPC::StreamClientConnection> m_clientConnection;
    RefPtr<IPC::StreamConnectionWorkQueue> m_serverQueue;
    RefPtr<IPC::StreamServerConnection> m_serverConnection WTF_GUARDED_BY_CAPABILITY(serverQueue());
    RefPtr<MockStreamServerConnectionClient> m_mockServerReceiver;
};

TEST_P(StreamMessageTest, Send)
{
    auto cleanup = localReferenceBarrier();
    for (uint64_t i = 0u; i < 55u; ++i) {
        auto result = m_clientConnection->send(MockStreamTestMessage1 { }, defaultDestinationID());
        EXPECT_EQ(result, IPC::Error::NoError);
    }
    serverQueue().dispatch([&] {
        assertIsCurrent(serverQueue());
        for (uint64_t i = 100u; i < 160u; ++i) {
            auto result = m_serverConnection->send(MockTestMessage1 { }, ObjectIdentifier<TestObjectIdentifierTag>(i));
            EXPECT_EQ(result, IPC::Error::NoError);
        }
    });
    for (uint64_t i = 100u; i < 160u; ++i) {
        auto message = m_mockClientReceiver->waitForMessage(defaultTimeout);
        EXPECT_EQ(message.messageName, MockTestMessage1::name());
        EXPECT_EQ(message.destinationID, i);
    }
    for (uint64_t i = 0u; i < 55u; ++i) {
        auto message = m_mockServerReceiver->waitForMessage(defaultTimeout);
        EXPECT_EQ(message.messageName, MockStreamTestMessage1::name());
        EXPECT_EQ(message.destinationID, defaultDestinationID().toUInt64());
    }
}

TEST_P(StreamMessageTest, SendWithSwitchingDestinationIDs)
{
    auto other = ObjectIdentifier<TestObjectIdentifierTag>(0x1234567891234);
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
        auto result = m_clientConnection->send(MockStreamTestMessage1 { }, defaultDestinationID());
        EXPECT_EQ(result, IPC::Error::NoError);
        if (i % 77) {
            result = m_clientConnection->send(MockStreamTestMessage1 { }, other);
            EXPECT_EQ(result, IPC::Error::NoError);
        }
    }
    for (uint64_t i = 0u; i < 777u; ++i) {
        auto message = m_mockServerReceiver->waitForMessage(defaultTimeout);
        EXPECT_EQ(message.messageName, MockStreamTestMessage1::name());
        EXPECT_EQ(message.destinationID, defaultDestinationID().toUInt64());
        if (i % 77) {
            auto message2 = m_mockServerReceiver->waitForMessage(defaultTimeout);
            EXPECT_EQ(message2.messageName, MockStreamTestMessage1::name());
            EXPECT_EQ(message2.destinationID, other.toUInt64());
        }
    }
}

TEST_P(StreamMessageTest, SendAndInvalidate)
{
    const uint64_t messageCount = 2004;
    auto cleanup = localReferenceBarrier();

    for (uint64_t i = 0u; i < messageCount; ++i) {
        auto result = m_clientConnection->send(MockStreamTestMessageNotStreamEncodable { IPC::Semaphore { } }, defaultDestinationID());
        EXPECT_EQ(result, IPC::Error::NoError);
    }
    auto flushResult = m_clientConnection->flushSentMessages();
    EXPECT_EQ(flushResult, IPC::Error::NoError);
    m_clientConnection->invalidate();

    for (uint64_t i = 0u; i < messageCount; ++i) {
        auto message = m_mockServerReceiver->waitForMessage(defaultTimeout);
        EXPECT_EQ(message.messageName, MockStreamTestMessageNotStreamEncodable::name());
        EXPECT_EQ(message.destinationID, defaultDestinationID().toUInt64());
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
        }, defaultDestinationID());
        EXPECT_TRUE(!!result);
    }
    while (replies.size() < 55u)
        RunLoop::currentSingleton().cycle();
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
    std::atomic<bool> waiting = false;
    BinarySemaphore workQueueWait;
    auto cleanup = localReferenceBarrier();
    serverQueue().dispatch([&] {
        waiting = true;
        workQueueWait.wait();
    });
    while (!waiting)
        RunLoop::currentSingleton().cycle();

    HashSet<uint64_t> replies;
    for (uint64_t i = 100u; i < 155u; ++i) {
        auto result = m_clientConnection->sendWithAsyncReply(MockStreamTestMessageWithAsyncReply1 { i }, [&, j = i] (uint64_t value) {
            EXPECT_EQ(value, 0u) << j; // Cancel handler returns 0 for uint64_t.
            replies.add(j);
        }, defaultDestinationID());
        EXPECT_TRUE(!!result);
    }
    m_clientConnection->invalidate();
    workQueueWait.signal();
    // FIXME: this should be more consistent: the async replies are asynchronous, and they cannot be invoked at the
    // point of invalidate as that is not always guaranteed to be in safe call stack.
    // They should be scheduled during invalidate() and ran from the event loop.
    // EXPECT_EQ(0u, replies.size());

    while (replies.size() < 55u)
        RunLoop::currentSingleton().cycle();
    for (uint64_t i = 100u; i < 155u; ++i)
        EXPECT_TRUE(replies.contains(i));
}

TEST_P(StreamMessageTest, SendSyncMessage)
{
    const uint32_t messageCount = 2004u;
    auto cleanup = localReferenceBarrier();
    m_mockServerReceiver->setSyncMessageHandler([] (IPC::StreamServerConnection& connection, IPC::Decoder& decoder) {
        auto value = decoder.decode<uint32_t>();
        connection.sendSyncReply<MockSyncMessage>(decoder.syncRequestID(), *value);
        return true;
    });
    for (uint32_t i = 0u; i < messageCount; ++i) {
        auto result = m_clientConnection->sendSync(MockSyncMessage { i }, defaultDestinationID());
        EXPECT_TRUE(result.succeeded());
        if (result.succeeded()) {
            auto [sameValue] = result.reply();
            EXPECT_EQ(i, sameValue);
        }
    }
    m_clientConnection->invalidate();
}

TEST_P(StreamMessageTest, ASendSyncMessageNotStreamEncodableReply)
{
    const uint32_t messageCount = 2004u;
    auto cleanup = localReferenceBarrier();
    m_mockServerReceiver->setSyncMessageHandler([] (IPC::StreamServerConnection& connection, IPC::Decoder& decoder) {
        auto value = decoder.decode<uint32_t>();
        connection.sendSyncReply<MockSyncMessageNotStreamEncodableReply>(decoder.syncRequestID(), *value);
        return true;
    });
    for (uint32_t i = 0u; i < messageCount; ++i) {
        auto result = m_clientConnection->sendSync(MockSyncMessageNotStreamEncodableReply { i }, defaultDestinationID());
        EXPECT_TRUE(result.succeeded());
        if (result.succeeded()) {
            auto [sameValue] = result.reply();
            EXPECT_EQ(i, sameValue);
        }
    }
    m_clientConnection->invalidate();
}

#if ENABLE(IPC_TESTING_API)
// Tests the case where we send a sync reply cancel message for a decoding failure. This is
// for the purposes of JS IPC Testing API to detect when a sync message was not handled.
TEST_P(StreamMessageTest, SyncMessageDecodeFailureCancelled)
{
    const uint32_t messageCount = 20u;
    auto cleanup = localReferenceBarrier();
    serverQueue().dispatch([&] {
        assertIsCurrent(serverQueue());
        m_serverConnection->setIgnoreInvalidMessageForTesting();
    });
    m_mockServerReceiver->setSyncMessageHandler([] (IPC::StreamServerConnection& connection, IPC::Decoder& decoder) {
        auto value = decoder.decode<uint32_t>();
        ASSERT(value);
        if (*value % 2) {
            connection.sendSyncReply<MockSyncMessageNotStreamEncodableBoth>(decoder.syncRequestID(), *value);
            return true;
        }
        // Cause decode error.
        EXPECT_FALSE(decoder.decode<uint64_t>());
        return false;
    });
    for (uint32_t i = 0u; i < messageCount; ++i) {
        auto result = m_clientConnection->sendSync(MockSyncMessageNotStreamEncodableBoth { i }, defaultDestinationID());
        if  (i % 2) {
            EXPECT_TRUE(result.succeeded());
            if (result.succeeded()) {
                auto [sameValue] = result.reply();
                EXPECT_EQ(i, sameValue);
            }
        } else {
            EXPECT_FALSE(result.succeeded());
            EXPECT_EQ(IPC::Error::SyncMessageCancelled, result.error());
        }
    }
    m_clientConnection->invalidate();
}
#endif

INSTANTIATE_TEST_SUITE_P(StreamConnectionSizedBuffer,
    StreamMessageTest,
    testing::Values(6, 7, 8, 9, 14),
    TestParametersToStringFormatter());


class StreamServerDidReceiveInvalidMessageTest : public ::testing::TestWithParam<std::tuple<InvalidMessageTestType>>, public StreamConnectionTestBase {
public:
    StreamServerDidReceiveInvalidMessageTest()
        : m_mockClientReceiver(MockStreamClientConnectionClient::create())
    {
    }

    unsigned bufferSizeLog2() const
    {
        return 8;
    }

    InvalidMessageTestType testType()
    {
        return std::get<0>(GetParam());
    }

    void SetUp() override
    {
        setupBase();
        auto connectionPair = IPC::StreamClientConnection::create(defaultBufferSizeLog2, defaultTimeout);
        ASSERT(connectionPair.has_value());
        auto [clientConnection, serverConnectionHandle] = WTFMove(*connectionPair);
        auto serverConnection = IPC::StreamServerConnection::tryCreate(WTFMove(serverConnectionHandle), { }).releaseNonNull();
        m_clientConnection = WTFMove(clientConnection);
        m_clientConnection->setSemaphores(copyViaEncoder(serverQueue().wakeUpSemaphore()).value(), copyViaEncoder(serverConnection->clientWaitSemaphore()).value());
        m_clientConnection->open(m_mockClientReceiver);
        m_mockServerReceiver = MockStreamServerConnectionClient::create();
        if (testType() == InvalidMessageTestType::DecodeError) {
            // Cause a decode error by decoding too much.
            m_mockServerReceiver->setAsyncMessageHandler([] (IPC::StreamServerConnection&, IPC::Decoder& decoder) {
                while (std::optional contents = decoder.decode<uint64_t>()) {
                }
                return true;
            });
            m_mockServerReceiver->setSyncMessageHandler([] (IPC::StreamServerConnection&, IPC::Decoder& decoder) {
                while (std::optional contents = decoder.decode<uint64_t>()) {
                }
                return true;
            });
        } else {
            // Cause a validation error, MESSAGE_CHECK.
            m_mockServerReceiver->setAsyncMessageHandler([] (IPC::StreamServerConnection& connection, IPC::Decoder&) {
                connection.markCurrentlyDispatchedMessageAsInvalid();
                return true;
            });
            m_mockServerReceiver->setSyncMessageHandler([] (IPC::StreamServerConnection& connection, IPC::Decoder&) {
                connection.markCurrentlyDispatchedMessageAsInvalid();
                return true;
            });
        }
        serverQueue().dispatch([this, serverConnection = WTFMove(serverConnection)] () mutable {
            assertIsCurrent(serverQueue());
            m_serverConnection = WTFMove(serverConnection);
            m_serverConnection->open(*m_mockServerReceiver, serverQueue());
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
        return ObjectIdentifier<TestObjectIdentifierTag>(77);
    }

    Ref<MockStreamClientConnectionClient> m_mockClientReceiver;
    RefPtr<IPC::StreamClientConnection> m_clientConnection;
    RefPtr<IPC::StreamConnectionWorkQueue> m_serverQueue;
    RefPtr<IPC::StreamServerConnection> m_serverConnection WTF_GUARDED_BY_CAPABILITY(serverQueue());
    RefPtr<MockStreamServerConnectionClient> m_mockServerReceiver;
};

TEST_P(StreamServerDidReceiveInvalidMessageTest, Async)
{
    constexpr uint64_t messageCount = 2u;
    for (uint64_t i = 0u; i < messageCount; ++i) {
        auto result = m_clientConnection->send(MockStreamTestMessage1 { }, defaultDestinationID());
        ASSERT_EQ(result, IPC::Error::NoError);
    }
    auto flushResult = m_clientConnection->flushSentMessages();
    EXPECT_EQ(flushResult, IPC::Error::NoError);

    std::optional invalidMessageName = m_mockServerReceiver->waitForInvalidMessage(defaultTimeout);
    ASSERT_TRUE(invalidMessageName.has_value());
    EXPECT_EQ(*invalidMessageName, MockStreamTestMessage1::name());
}

TEST_P(StreamServerDidReceiveInvalidMessageTest, AsyncNotStreamEncodable)
{
    constexpr uint64_t messageCount = 2u;
    for (uint64_t i = 0u; i < messageCount; ++i) {
        auto result = m_clientConnection->send(MockStreamTestMessageNotStreamEncodable { IPC::Semaphore { } }, defaultDestinationID());
        ASSERT_EQ(result, IPC::Error::NoError);
    }
    auto flushResult = m_clientConnection->flushSentMessages();
    EXPECT_EQ(flushResult, IPC::Error::NoError);

    std::optional invalidMessageName = m_mockServerReceiver->waitForInvalidMessage(defaultTimeout);
    ASSERT_TRUE(invalidMessageName.has_value());
    EXPECT_EQ(*invalidMessageName, MockStreamTestMessageNotStreamEncodable::name());
}

TEST_P(StreamServerDidReceiveInvalidMessageTest, AsyncWithReply)
{
    auto cleanup = localReferenceBarrier();

    HashSet<uint64_t> replies;
    for (uint64_t i = 10u; i < 15u; ++i) {
        auto result = m_clientConnection->sendWithAsyncReply(MockStreamTestMessageWithAsyncReply1 { i }, [&, j = i] (uint64_t value) {
            EXPECT_EQ(value, 0u) << j; // Cancel handler returns 0 for uint64_t.
            replies.add(j);
        }, defaultDestinationID());
        EXPECT_TRUE(!!result);
    }
    auto flushResult = m_clientConnection->flushSentMessages();
    EXPECT_EQ(flushResult, IPC::Error::NoError);

    std::optional invalidMessageName = m_mockServerReceiver->waitForInvalidMessage(defaultTimeout);
    ASSERT_TRUE(invalidMessageName.has_value());
    EXPECT_EQ(*invalidMessageName, MockStreamTestMessageWithAsyncReply1::name());

    while (replies.size() < 5u)
        RunLoop::currentSingleton().cycle();
    for (uint64_t i = 10u; i < 15u; ++i)
        EXPECT_TRUE(replies.contains(i));
}

INSTANTIATE_TEST_SUITE_P(StreamServerConnectionTests,
    StreamServerDidReceiveInvalidMessageTest,
    testing::Values(InvalidMessageTestType::DecodeError, InvalidMessageTestType::ValidationError),
    TestParametersToStringFormatter());

}

}
