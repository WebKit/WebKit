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
#include "Helpers/Test.h"
#include "Helpers/Utilities.h"
#include "ThreadSafeObjectHeap.h"
#include <atomic>
#include <optional>
#include <wtf/Lock.h>
#include <wtf/Scope.h>
#include <wtf/Threading.h>
#include <wtf/threads/BinarySemaphore.h>

namespace TestWebKitAPI {

namespace {
// Default timeout must be long enough that taxed system still is likely to not get false negatives.
static constexpr Seconds defaultTimeout = 100_s;
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

struct MockStreamTestMessageBatched {
    static constexpr bool isSync = false;
    static constexpr bool isStreamEncodable = true;
    static constexpr bool isStreamBatched = true;
    static constexpr IPC::MessageName name()  { return IPC::MessageName::IPCStreamTester_EmptyMessage; }
    template<typename Encoder> void encode(Encoder&) { }
};

struct MockStreamTestMessageNotStreamEncodable {
    static constexpr bool isSync = false;
    static constexpr bool isStreamEncodable = false;
    static constexpr IPC::MessageName name()  { return IPC::MessageName::IPCStreamTester_EmptyMessage; }
    explicit MockStreamTestMessageNotStreamEncodable(IPC::Semaphore&& s)
        : semaphore(WTF::move(s))
    {
    }
    template<typename Encoder>
    void encode(Encoder& encoder)
    {
        encoder << WTF::move(semaphore);
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

struct MockStreamTestMessageWithAsyncReplyNotStreamEncodable {
    static constexpr bool isSync = false;
    static constexpr bool isStreamEncodable = false;
    static constexpr IPC::MessageName name()  { return IPC::MessageName::IPCStreamTester_AsyncPing; }
    static constexpr IPC::MessageName asyncMessageReplyName() { return IPC::MessageName::IPCStreamTester_AsyncPingReply; }

    template<typename Encoder>
    void encode(Encoder& encoder)
    {
        encoder << contents;
    }

    using ReplyArguments = std::tuple<uint64_t>;
    MockStreamTestMessageWithAsyncReplyNotStreamEncodable(uint64_t contents)
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
        m_asyncMessageHandler = WTF::move(handler);
    }

    // Handler returns false if the message should be just recorded.
    void setSyncMessageHandler(Function<bool(IPC::StreamServerConnection&, IPC::Decoder&)>&& handler)
    {
        m_syncMessageHandler = WTF::move(handler);
    }
    // Handler returns false if the message should be just recorded.
    void setInvalidMessageHandler(Function<bool(IPC::StreamServerConnection&, IPC::MessageName, const Vector<uint32_t>&)>&& handler)
    {
        m_invalidMessageHandler = WTF::move(handler);
    }

    unsigned didRunOutOfMessagesCount() const { return m_didRunOutOfMessagesCount; }

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

    void didRunOutOfMessages() final
    {
        ++m_didRunOutOfMessagesCount;
    }

    std::atomic<unsigned> m_didRunOutOfMessagesCount { 0 };
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
    auto [clientConnection, serverConnectionHandle] = WTF::move(*connectionPair);
    auto serverConnection = IPC::StreamServerConnection::tryCreate(WTF::move(serverConnectionHandle), { }).releaseNonNull();
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
    auto [clientConnection, serverConnectionHandle] = WTF::move(*connectionPair);
    auto serverConnection = IPC::StreamServerConnection::tryCreate(WTF::move(serverConnectionHandle), { }).releaseNonNull();
    auto cleanup = localReferenceBarrier();
    serverQueue().dispatch([this, serverConnection] {
        assertIsCurrent(serverQueue());
        serverConnection->invalidate();
    });
    clientConnection->invalidate();
}

struct MockStreamTestMessageBlockingEncode {
    static constexpr bool isSync = false;
    static constexpr bool isStreamEncodable = true;
    static constexpr bool isStreamBatched = false;
    static constexpr IPC::MessageName name()  { return IPC::MessageName::IPCStreamTester_EmptyMessage; }
    template<typename Encoder> void encode(Encoder&)
    {
        didStartEncoding.signal();
        finishEncoding.wait();
    }
    BinarySemaphore& didStartEncoding;
    BinarySemaphore& finishEncoding;
};

TEST_F(StreamConnectionTest, DidRunOutOfMessagesNotSentForLoneDestinationIDOnInvalidate)
{
    auto connectionPair = IPC::StreamClientConnection::create(17, defaultTimeout);
    ASSERT_TRUE(!!connectionPair);
    auto [clientConnection, serverConnectionHandle] = WTF::move(*connectionPair);
    RefPtr serverConnection = IPC::StreamServerConnection::tryCreate(WTF::move(serverConnectionHandle), { });
    Ref mockClientReceiver = MockStreamClientConnectionClient::create();
    clientConnection->open(mockClientReceiver);
    Ref mockServerReceiver = MockStreamServerConnectionClient::create();
    auto destination = ObjectIdentifier<TestObjectIdentifierTag>(77);
    auto otherDestination = ObjectIdentifier<TestObjectIdentifierTag>(78);
    serverQueue().dispatch([&] {
        serverConnection->open(mockServerReceiver, serverQueue());
        serverConnection->startReceivingMessages(mockServerReceiver, IPC::receiverName(MockStreamTestMessage1::name()), destination.toUInt64());
    });
    localReferenceBarrier();
    EXPECT_EQ(clientConnection->send(MockStreamTestMessage1 { }, destination), IPC::Error::NoError);
    mockServerReceiver->waitForMessage(defaultTimeout);
    EXPECT_TRUE(Util::waitFor([&] {
        return mockServerReceiver->didRunOutOfMessagesCount() == 1;
    }));

    BinarySemaphore destinationIDWritten;
    BinarySemaphore didInvalidate;
    serverQueue().dispatch([&] {
        destinationIDWritten.wait();
        serverConnection->stopReceivingMessages(IPC::receiverName(MockStreamTestMessage1::name()), destination.toUInt64());
        serverConnection->invalidate();
        didInvalidate.signal();
    });
    BinarySemaphore didStartEncoding;
    BinarySemaphore finishEncoding;
    RefPtr sender = Thread::create("sender"_s, [&] {
        clientConnection->send(MockStreamTestMessageBlockingEncode { didStartEncoding, finishEncoding }, otherDestination);
    });
    didStartEncoding.wait();
    destinationIDWritten.signal();
    didInvalidate.wait();
    localReferenceBarrier();
    EXPECT_EQ(mockServerReceiver->didRunOutOfMessagesCount(), 1u);
    finishEncoding.signal();
    sender->waitForCompletion();
    clientConnection->invalidate();
}

TEST_F(StreamConnectionTest, DidRunOutOfMessagesSentOnceWhenInvalidatedAfterFullPass)
{
    static constexpr unsigned count = 999;
    auto connectionPair = IPC::StreamClientConnection::create(17, defaultTimeout);
    ASSERT_TRUE(!!connectionPair);
    auto [clientConnection, serverConnectionHandle] = WTF::move(*connectionPair);
    RefPtr serverConnection = IPC::StreamServerConnection::tryCreate(WTF::move(serverConnectionHandle), { });
    Ref mockClientReceiver = MockStreamClientConnectionClient::create();
    clientConnection->open(mockClientReceiver);
    Ref mockServerReceiver = MockStreamServerConnectionClient::create();
    auto destination = ObjectIdentifier<TestObjectIdentifierTag>(77);
    unsigned received = 0;
    bool invalidated = false;
    mockServerReceiver->setAsyncMessageHandler([&](IPC::StreamServerConnection& connection, IPC::Decoder&) {
        if (++received == count) {
            serverQueue().dispatch([&, connection = Ref { connection }] {
                connection->stopReceivingMessages(IPC::receiverName(MockStreamTestMessage1::name()), destination.toUInt64());
                connection->invalidate();
                invalidated = true;
            });
        }
        return true;
    });
    serverQueue().dispatch([&] {
        serverConnection->open(mockServerReceiver, serverQueue());
        serverConnection->startReceivingMessages(mockServerReceiver, IPC::receiverName(MockStreamTestMessage1::name()), destination.toUInt64());
    });
    localReferenceBarrier();
    BinarySemaphore didBlock;
    BinarySemaphore unblock;
    serverQueue().dispatch([&] {
        didBlock.signal();
        unblock.wait();
    });
    didBlock.wait();
    for (unsigned i = 0; i < count; ++i)
        EXPECT_EQ(clientConnection->send(MockStreamTestMessage1 { }, destination), IPC::Error::NoError);
    unblock.signal();
    EXPECT_TRUE(Util::waitFor([&] {
        return invalidated;
    }));
    localReferenceBarrier();
    EXPECT_EQ(received, count);
    EXPECT_EQ(mockServerReceiver->didRunOutOfMessagesCount(), 1u);
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
        auto [clientConnection, serverConnectionHandle] = WTF::move(*connectionPair);
        auto serverConnection = IPC::StreamServerConnection::tryCreate(WTF::move(serverConnectionHandle), { }).releaseNonNull();
        m_clientConnection = WTF::move(clientConnection);
        m_clientConnection->open(m_mockClientReceiver);
        m_mockServerReceiver = MockStreamServerConnectionClient::create();
        m_mockServerReceiver->setAsyncMessageHandler([] (IPC::StreamServerConnection& connection, IPC::Decoder& decoder) {
            if (decoder.messageName() != MockStreamTestMessageWithAsyncReply1::name())
                return false;
            auto contents = decoder.decode<uint64_t>();
            ASSERT(contents);
            ASSERT(decoder.isValid());
            ASSERT(decoder.isAsyncWithReplyMessage());
            auto asyncReplyID = decoder.asyncReplyID();
            decoder.markHandled();
            connection.sendAsyncReply<MockStreamTestMessageWithAsyncReply1>(asyncReplyID, *contents);
            return true;
        });
        serverQueue().dispatch([this, serverConnection = WTF::move(serverConnection)] () mutable {
            assertIsCurrent(serverQueue());
            m_serverConnection = WTF::move(serverConnection);
            m_serverConnection->open(*m_mockServerReceiver, serverQueue());
            m_serverConnection->startReceivingMessages(*m_mockServerReceiver, IPC::receiverName(MockStreamTestMessage1::name()), defaultDestinationID().toUInt64());
        });
        localReferenceBarrier();
    }

    void TearDown() override
    {
        m_clientConnection->invalidate();
        {
            serverQueue().dispatch([&] {
                assertIsCurrent(serverQueue());
                m_serverConnection->stopReceivingMessages(IPC::receiverName(MockStreamTestMessage1::name()), defaultDestinationID().toUInt64());
                m_serverConnection->invalidate();
            });
            localReferenceBarrier();
        }
        EXPECT_TRUE(m_mockClientReceiver->checkMessages());
        EXPECT_TRUE(m_mockServerReceiver->checkMessages());
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

// The server is asleep after setup, so the first batched message starts a batch and nothing wakes the server until the
// batch is flushed. Sending must not wait for buffer space that only the sleeping server can free.
TEST_P(StreamMessageTest, SendBatchedFillsBufferWhileServerIsAsleep)
{
    auto cleanup = localReferenceBarrier();
    m_clientConnection->setMaxBatchSize(std::numeric_limits<unsigned>::max());
    // Each message takes the minimum message size of 16 bytes, so this fills the buffer several times.
    const unsigned messageCount = 4u << (bufferSizeLog2() - 4);
    for (unsigned i = 0u; i < messageCount; ++i)
        ASSERT_EQ(m_clientConnection->send(MockStreamTestMessageBatched { }, defaultDestinationID()), IPC::Error::NoError);
    m_clientConnection->flushBatch();
    for (unsigned i = 0u; i < messageCount; ++i) {
        auto message = m_mockServerReceiver->waitForMessage(defaultTimeout);
        EXPECT_EQ(message.messageName, MockStreamTestMessageBatched::name());
        EXPECT_EQ(message.destinationID, defaultDestinationID().toUInt64());
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

// Tests that unhandled async messages with reply are cancelled, both for messages sent through the stream
// and for messages sent out of stream.
TEST_P(StreamMessageTest, SendAsyncReplyNotHandledIsCancelled)
{
    auto cleanup = localReferenceBarrier();
    m_mockServerReceiver->setAsyncMessageHandler([](IPC::StreamServerConnection& connection, IPC::Decoder& decoder) {
        auto contents = decoder.decode<uint64_t>();
        ASSERT(contents);
        if (*contents % 2)
            return true; // Unhandled message.
        auto asyncReplyID = decoder.asyncReplyID();
        decoder.markHandled();
        connection.sendAsyncReply<MockStreamTestMessageWithAsyncReply1>(asyncReplyID, *contents);
        return true;
    });
    HashMap<uint64_t, uint64_t> replies;
    for (uint64_t i = 100u; i < 160u; ++i) {
        auto completionHandler = [&, i](uint64_t value) {
            replies.add(i, value);
        };
        std::optional<IPC::StreamClientConnection::AsyncReplyID> result;
        if (i % 4 < 2)
            result = m_clientConnection->sendWithAsyncReply(MockStreamTestMessageWithAsyncReply1 { i }, WTF::move(completionHandler), defaultDestinationID());
        else
            result = m_clientConnection->sendWithAsyncReply(MockStreamTestMessageWithAsyncReplyNotStreamEncodable { i }, WTF::move(completionHandler), defaultDestinationID());
        EXPECT_TRUE(!!result);
    }
    while (replies.size() < 60u)
        RunLoop::currentSingleton().cycle();
    for (uint64_t i = 100u; i < 160u; ++i) {
        auto reply = replies.find(i);
        ASSERT_NE(reply, replies.end());
        EXPECT_EQ(reply->value, i % 2 ? uint64_t { 0 } : i); // Odd messages are cancelled.
    }
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

TEST_P(StreamMessageTest, SendWithAsyncReplyOnDispatcher)
{
    auto cleanup = localReferenceBarrier();
    HashSet<uint64_t> replies;
    {
        AutoWorkQueue awq;

        for (uint64_t i = 100u; i < 160u; ++i) {
            m_clientConnection->sendWithAsyncReplyOnDispatcher(MockStreamTestMessageWithAsyncReply1 { i }, awq.queue(), [&, j = i, queue = awq.queue()] (uint64_t value) {
                assertIsCurrent(queue);
                EXPECT_GE(value, 100u) << j;
                replies.add(value);
            }, defaultDestinationID());
        }
        while (replies.size() < 60u)
            RunLoop::currentSingleton().cycle();
        m_clientConnection->invalidate();

        awq.queue()->beginShutdown();
    }
    for (uint64_t i = 100u; i < 160u; ++i)
        EXPECT_TRUE(replies.contains(i));
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
        auto [clientConnection, serverConnectionHandle] = WTF::move(*connectionPair);
        auto serverConnection = IPC::StreamServerConnection::tryCreate(WTF::move(serverConnectionHandle), { }).releaseNonNull();
        m_clientConnection = WTF::move(clientConnection);
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
                connection.markCurrentlyDispatchedMessageAsInvalid("async stream message check"_s);
                return true;
            });
            m_mockServerReceiver->setSyncMessageHandler([] (IPC::StreamServerConnection& connection, IPC::Decoder&) {
                connection.markCurrentlyDispatchedMessageAsInvalid("sync stream message check"_s);
                return true;
            });
        }
        serverQueue().dispatch([this, serverConnection = WTF::move(serverConnection)] () mutable {
            assertIsCurrent(serverQueue());
            m_serverConnection = WTF::move(serverConnection);
            m_serverConnection->open(*m_mockServerReceiver, serverQueue());
            m_serverConnection->startReceivingMessages(*m_mockServerReceiver, IPC::receiverName(MockStreamTestMessage1::name()), defaultDestinationID().toUInt64());
        });
        localReferenceBarrier();
    }

    void TearDown() override
    {
        m_clientConnection->invalidate();
        {
            serverQueue().dispatch([&] {
                assertIsCurrent(serverQueue());
                m_serverConnection->stopReceivingMessages(IPC::receiverName(MockStreamTestMessage1::name()), defaultDestinationID().toUInt64());
                m_serverConnection->invalidate();
            });
            localReferenceBarrier();
        }
        EXPECT_TRUE(m_mockClientReceiver->checkMessages());
        EXPECT_TRUE(m_mockServerReceiver->checkMessages());
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
    // Note: here we do not check for errors, since the receiver might already have closed the connection and as such
    // we would get a InvalidConnection.
    for (uint64_t i = 0u; i < messageCount; ++i)
        m_clientConnection->send(MockStreamTestMessage1 { }, defaultDestinationID());
    m_clientConnection->flushSentMessages();

    std::optional invalidMessageName = m_mockServerReceiver->waitForInvalidMessage(defaultTimeout);
    ASSERT_TRUE(invalidMessageName.has_value());
    EXPECT_EQ(*invalidMessageName, MockStreamTestMessage1::name());
}

TEST_P(StreamServerDidReceiveInvalidMessageTest, AsyncNotStreamEncodable)
{
    constexpr uint64_t messageCount = 2u;
    // Note: here we do not check for send errors, since the receiver might already have closed the connection and as such
    // we would get a InvalidConnection.
    for (uint64_t i = 0u; i < messageCount; ++i)
        m_clientConnection->send(MockStreamTestMessageNotStreamEncodable { IPC::Semaphore { } }, defaultDestinationID());
    m_clientConnection->flushSentMessages();

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
    m_clientConnection->flushSentMessages();

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

class StreamServerDidRunOutOfMessagesTest : public ::testing::TestWithParam<std::tuple<unsigned>>, public StreamConnectionTestBase {
public:
    unsigned messageCount() const
    {
        return std::get<0>(GetParam());
    }

    void SetUp() override
    {
        setupBase();
        m_connectionA = openConnection();
        m_connectionB = openConnection();
    }

    void TearDown() override
    {
        closeConnection(m_connectionA);
        closeConnection(m_connectionB);
        teardownBase();
    }

protected:
    struct ConnectionPair {
        RefPtr<IPC::StreamClientConnection> clientConnection;
        RefPtr<MockStreamClientConnectionClient> mockClientReceiver;
        RefPtr<IPC::StreamServerConnection> serverConnection;
        RefPtr<MockStreamServerConnectionClient> mockServerReceiver;
    };

    static constexpr unsigned bufferSizeLog2 = 17;

    static TestObjectIdentifier defaultDestinationID()
    {
        return ObjectIdentifier<TestObjectIdentifierTag>(77);
    }

    ConnectionPair openConnection()
    {
        auto connectionPair = IPC::StreamClientConnection::create(bufferSizeLog2, defaultTimeout);
        ASSERT(!!connectionPair);
        auto [clientConnection, serverConnectionHandle] = WTF::move(*connectionPair);
        ConnectionPair connection;
        connection.clientConnection = WTF::move(clientConnection);
        connection.mockClientReceiver = MockStreamClientConnectionClient::create();
        connection.clientConnection->open(*connection.mockClientReceiver);
        connection.serverConnection = IPC::StreamServerConnection::tryCreate(WTF::move(serverConnectionHandle), { });
        connection.mockServerReceiver = MockStreamServerConnectionClient::create();
        serverQueue().dispatch([this, serverConnection = connection.serverConnection, mockServerReceiver = connection.mockServerReceiver] {
            assertIsCurrent(serverQueue());
            serverConnection->open(*mockServerReceiver, serverQueue());
            serverConnection->startReceivingMessages(*mockServerReceiver, IPC::receiverName(MockStreamTestMessage1::name()), defaultDestinationID().toUInt64());
        });
        localReferenceBarrier();
        return connection;
    }

    void closeConnection(ConnectionPair& connection)
    {
        connection.clientConnection->invalidate();
        serverQueue().dispatch([&] {
            assertIsCurrent(serverQueue());
            connection.serverConnection->stopReceivingMessages(IPC::receiverName(MockStreamTestMessage1::name()), defaultDestinationID().toUInt64());
            connection.serverConnection->invalidate();
        });
        localReferenceBarrier();
        EXPECT_TRUE(connection.mockClientReceiver->checkMessages());
        EXPECT_TRUE(connection.mockServerReceiver->checkMessages());
    }

    void send(ConnectionPair& connection, unsigned count)
    {
        for (unsigned i = 0; i < count; ++i) {
            auto result = connection.clientConnection->send(MockStreamTestMessage1 { }, defaultDestinationID());
            EXPECT_EQ(result, IPC::Error::NoError);
        }
    }

    void sendWhileWorkQueueIsBlocked(ConnectionPair& connection, unsigned count)
    {
        serverQueue().dispatch([this] {
            m_workQueueDidBlock.signal();
            m_unblockWorkQueue.wait();
        });
        m_workQueueDidBlock.wait();
        send(connection, count);
        m_unblockWorkQueue.signal();
    }

    void receive(ConnectionPair& connection, unsigned count)
    {
        for (unsigned i = 0; i < count; ++i) {
            auto message = connection.mockServerReceiver->waitForMessage(defaultTimeout);
            EXPECT_EQ(message.messageName, MockStreamTestMessage1::name());
            EXPECT_EQ(message.destinationID, defaultDestinationID().toUInt64());
        }
    }

    static bool waitForDidRunOutOfMessages(ConnectionPair& connection, unsigned count)
    {
        return Util::waitFor([&] {
            return connection.mockServerReceiver->didRunOutOfMessagesCount() >= count;
        });
    }

    ConnectionPair m_connectionA;
    ConnectionPair m_connectionB;
    BinarySemaphore m_workQueueDidBlock;
    BinarySemaphore m_unblockWorkQueue;
};

TEST_P(StreamServerDidRunOutOfMessagesTest, NotSentWithoutMessages)
{
    localReferenceBarrier();
    EXPECT_EQ(m_connectionA.mockServerReceiver->didRunOutOfMessagesCount(), 0u);
    EXPECT_EQ(m_connectionB.mockServerReceiver->didRunOutOfMessagesCount(), 0u);
}

TEST_P(StreamServerDidRunOutOfMessagesTest, SentOnceAfterBurst)
{
    sendWhileWorkQueueIsBlocked(m_connectionA, messageCount());
    receive(m_connectionA, messageCount());
    EXPECT_TRUE(waitForDidRunOutOfMessages(m_connectionA, 1));
    localReferenceBarrier();
    EXPECT_EQ(m_connectionA.mockServerReceiver->didRunOutOfMessagesCount(), 1u);

    sendWhileWorkQueueIsBlocked(m_connectionA, messageCount());
    receive(m_connectionA, messageCount());
    EXPECT_TRUE(waitForDidRunOutOfMessages(m_connectionA, 2));
    localReferenceBarrier();
    EXPECT_EQ(m_connectionA.mockServerReceiver->didRunOutOfMessagesCount(), 2u);
}

TEST_P(StreamServerDidRunOutOfMessagesTest, SentAfterEveryUnpacedSend)
{
    for (unsigned expected = 1; expected <= 3; ++expected) {
        auto before = m_connectionA.mockServerReceiver->didRunOutOfMessagesCount();
        send(m_connectionA, messageCount());
        receive(m_connectionA, messageCount());
        EXPECT_TRUE(waitForDidRunOutOfMessages(m_connectionA, before + 1));
        localReferenceBarrier();
        auto after = m_connectionA.mockServerReceiver->didRunOutOfMessagesCount();
        EXPECT_GE(after, before + 1);
        EXPECT_LE(after, before + messageCount());
    }
}

TEST_P(StreamServerDidRunOutOfMessagesTest, OnlySentToConnectionThatReceivedMessages)
{
    sendWhileWorkQueueIsBlocked(m_connectionA, messageCount());
    receive(m_connectionA, messageCount());
    EXPECT_TRUE(waitForDidRunOutOfMessages(m_connectionA, 1));
    localReferenceBarrier();
    EXPECT_EQ(m_connectionA.mockServerReceiver->didRunOutOfMessagesCount(), 1u);
    EXPECT_EQ(m_connectionB.mockServerReceiver->didRunOutOfMessagesCount(), 0u);

    sendWhileWorkQueueIsBlocked(m_connectionB, messageCount());
    receive(m_connectionB, messageCount());
    EXPECT_TRUE(waitForDidRunOutOfMessages(m_connectionB, 1));
    localReferenceBarrier();
    EXPECT_EQ(m_connectionA.mockServerReceiver->didRunOutOfMessagesCount(), 1u);
    EXPECT_EQ(m_connectionB.mockServerReceiver->didRunOutOfMessagesCount(), 1u);
}

INSTANTIATE_TEST_SUITE_P(StreamServerConnectionTests,
    StreamServerDidRunOutOfMessagesTest,
    testing::Values(1, 10, 999, 1000, 1001, 2000),
    TestParametersToStringFormatter());

struct MockStreamTestMessageWithValue {
    using Arguments = std::tuple<uint64_t>;
    static constexpr bool isSync = false;
    static constexpr bool isStreamEncodable = true;
    static constexpr bool isStreamBatched = false;
    static constexpr IPC::MessageName name()  { return IPC::MessageName::IPCStreamTester_EmptyMessage; }
    template<typename Encoder> void encode(Encoder& encoder) { encoder << value; }
    uint64_t value;
};

// Sent out of stream.
struct MockStreamTestMessageWithValueNotStreamEncodable {
    using Arguments = std::tuple<uint64_t>;
    static constexpr bool isSync = false;
    static constexpr bool isStreamEncodable = false;
    static constexpr IPC::MessageName name()  { return IPC::MessageName::IPCStreamTester_EmptyMessage; }
    template<typename Encoder> void encode(Encoder& encoder) { encoder << value; }
    uint64_t value;
};

// Sent out of stream, with an attachment after the value.
struct MockStreamTestMessageWithValueAndAttachment {
    static constexpr bool isSync = false;
    static constexpr bool isStreamEncodable = false;
    static constexpr IPC::MessageName name()  { return IPC::MessageName::IPCStreamTester_EmptyMessage; }
    template<typename Encoder>
    void encode(Encoder& encoder)
    {
        encoder << value;
        encoder << WTF::move(semaphore);
    }
    uint64_t value;
    IPC::Semaphore semaphore;
};

enum class WaitedObjectIdentifierType { };
using WaitedObjectIdentifier = AtomicObjectIdentifier<WaitedObjectIdentifierType>;
class WaitedObject : public ThreadSafeRefCounted<WaitedObject> {
public:
    static Ref<WaitedObject> create(uint64_t value) { return adoptRef(*new WaitedObject(value)); }
    uint64_t value() const { return m_value; }

private:
    explicit WaitedObject(uint64_t value)
        : m_value(value)
    {
    }
    const uint64_t m_value;
};

using WaitedObjectHeap = IPC::ThreadSafeObjectHeap<WaitedObjectIdentifier, RefPtr<WaitedObject>>;

// Stream connections whose message handlers wait for each other without blocking their work queue. The
// handled messages of all connections are recorded in one log, in the order they are handled.
class StreamWaitingMessageTest : public ::testing::Test, public StreamConnectionTestBase {
public:
    void SetUp() override
    {
        setupBase();
        m_otherServerQueue = IPC::StreamConnectionWorkQueue::create("StreamWaitingMessageTest other work queue"_s);
    }

    void TearDown() override
    {
        for (auto& connection : m_connections)
            closeConnection(*connection);
        m_otherServerQueue->stopAndWaitForCompletion();
        teardownBase();
    }

protected:
    // Returns false if the message is deferred.
    using MessageHandler = Function<bool(IPC::StreamServerConnection&, uint64_t value)>;

    struct TestConnection {
        WTF_DEPRECATED_MAKE_STRUCT_FAST_ALLOCATED(TestConnection);
        uint64_t tag { 0 };
        RefPtr<IPC::StreamConnectionWorkQueue> queue;
        RefPtr<IPC::StreamClientConnection> clientConnection;
        RefPtr<MockStreamClientConnectionClient> mockClientReceiver;
        RefPtr<IPC::StreamServerConnection> serverConnection;
        RefPtr<MockStreamServerConnectionClient> mockServerReceiver;
    };

    static TestObjectIdentifier defaultDestinationID()
    {
        return ObjectIdentifier<TestObjectIdentifierTag>(77);
    }

    static uint64_t entry(uint64_t tag, uint64_t index)
    {
        return (tag << 32) | index;
    }

    IPC::StreamConnectionWorkQueue& otherServerQueue()
    {
        return *m_otherServerQueue;
    }

    // The connections are run in the order they are opened.
    TestConnection& openConnection(uint64_t tag, IPC::StreamConnectionWorkQueue& queue, MessageHandler&& handler = nullptr)
    {
        auto connectionPair = IPC::StreamClientConnection::create(17, defaultTimeout);
        RELEASE_ASSERT(!!connectionPair);
        auto [clientConnection, serverConnectionHandle] = WTF::move(*connectionPair);
        auto connection = makeUnique<TestConnection>();
        connection->tag = tag;
        connection->queue = &queue;
        connection->clientConnection = WTF::move(clientConnection);
        connection->mockClientReceiver = MockStreamClientConnectionClient::create();
        connection->clientConnection->open(*connection->mockClientReceiver);
        connection->serverConnection = IPC::StreamServerConnection::tryCreate(WTF::move(serverConnectionHandle), { });
        RELEASE_ASSERT(connection->serverConnection);
        connection->mockServerReceiver = MockStreamServerConnectionClient::create();
        connection->mockServerReceiver->setAsyncMessageHandler([this, handler = WTF::move(handler)](IPC::StreamServerConnection& connection, IPC::Decoder& decoder) {
            auto value = decoder.decode<uint64_t>().value_or(0);
            if (handler && !handler(connection, value)) {
                connection.markCurrentlyDispatchedMessageAsDeferred();
                return true;
            }
            Locker locker { m_logLock };
            m_log.append(value);
            return true;
        });
        queue.dispatch([serverConnection = connection->serverConnection, mockServerReceiver = connection->mockServerReceiver, protectedQueue = Ref { queue }] {
            serverConnection->open(*mockServerReceiver, protectedQueue);
            serverConnection->startReceivingMessages(*mockServerReceiver, IPC::receiverName(MockStreamTestMessage1::name()), defaultDestinationID().toUInt64());
        });
        barrier(queue);
        m_connections.append(WTF::move(connection));
        return *m_connections.last();
    }

    void closeConnection(TestConnection& connection)
    {
        connection.clientConnection->invalidate();
        connection.queue->dispatch([&] {
            connection.serverConnection->stopReceivingMessages(IPC::receiverName(MockStreamTestMessage1::name()), defaultDestinationID().toUInt64());
            connection.serverConnection->invalidate();
        });
        barrier(*connection.queue);
        EXPECT_TRUE(connection.mockClientReceiver->checkMessages());
        EXPECT_TRUE(connection.mockServerReceiver->checkMessages());
    }

    static void barrier(IPC::StreamConnectionWorkQueue& queue)
    {
        BinarySemaphore semaphore;
        queue.dispatch([&] {
            semaphore.signal();
        });
        semaphore.wait();
    }

    void block(IPC::StreamConnectionWorkQueue& queue)
    {
        BinarySemaphore didBlock;
        queue.dispatch([&] {
            didBlock.signal();
            m_unblock.wait();
        });
        didBlock.wait();
    }

    void unblock()
    {
        m_unblock.signal();
    }

    static void send(TestConnection& connection, uint64_t index)
    {
        EXPECT_EQ(connection.clientConnection->send(MockStreamTestMessageWithValue { entry(connection.tag, index) }, defaultDestinationID()), IPC::Error::NoError);
    }

    static void sendOutOfStream(TestConnection& connection, uint64_t index)
    {
        EXPECT_EQ(connection.clientConnection->send(MockStreamTestMessageWithValueNotStreamEncodable { entry(connection.tag, index) }, defaultDestinationID()), IPC::Error::NoError);
    }

    Vector<uint64_t> log()
    {
        Locker locker { m_logLock };
        return m_log;
    }

    bool waitForLogSize(size_t size)
    {
        return Util::waitFor([&] {
            Locker locker { m_logLock };
            return m_log.size() >= size;
        });
    }

    // A handler that reads the object of `readReference` from `m_heap` for the message `index`, and records what it read.
    MessageHandler readObjectHandler(uint64_t tag, uint64_t index, IPC::ObjectIdentifierReadReference<WaitedObjectIdentifier> readReference)
    {
        return [this, readMessage = entry(tag, index), readReference](IPC::StreamServerConnection& connection, uint64_t value) {
            if (value != readMessage)
                return true;
            auto read = readReference;
            auto object = m_heap.read(connection, WTF::move(read));
            if (!object)
                return false;
            m_readObject = *object ? (*object)->value() : 0;
            return true;
        };
    }

    // A handler that adds `object` to `m_heap` for the message `index`.
    MessageHandler addObjectHandler(uint64_t tag, uint64_t index, IPC::ObjectIdentifierReference<WaitedObjectIdentifier> reference, uint64_t object)
    {
        return [this, addMessage = entry(tag, index), reference, object](IPC::StreamServerConnection&, uint64_t value) {
            if (value == addMessage)
                EXPECT_TRUE(m_heap.add(reference, WaitedObject::create(object)));
            return true;
        };
    }

    WaitedObjectHeap m_heap;
    IPC::ObjectIdentifierReferenceTracker<WaitedObjectIdentifier> m_referenceTracker { WaitedObjectIdentifier::generate() };
    std::atomic<uint64_t> m_readObject { 0 };
    RefPtr<IPC::StreamConnectionWorkQueue> m_otherServerQueue;
    Vector<std::unique_ptr<TestConnection>> m_connections;
    BinarySemaphore m_unblock;
    Lock m_logLock;
    Vector<uint64_t> m_log WTF_GUARDED_BY_LOCK(m_logLock);
};

TEST_F(StreamWaitingMessageTest, DeferredMessageIsDispatchedAgainWhenWokenUp)
{
    std::atomic<bool> canHandle { false };
    std::atomic<unsigned> dispatchCount { 0 };
    auto& waiting = openConnection(1, serverQueue(), [&](IPC::StreamServerConnection& connection, uint64_t value) {
        if (value != entry(1, 0))
            return true;
        ++dispatchCount;
        EXPECT_TRUE(connection.canCurrentlyDispatchedMessageWait());
        return canHandle.load();
    });
    auto& other = openConnection(2, serverQueue());
    send(waiting, 0);
    send(waiting, 1);
    send(other, 0);
    EXPECT_TRUE(waitForLogSize(1));
    barrier(serverQueue());
    barrier(serverQueue());
    // The deferred message holds back the messages after it, but not the other connection.
    EXPECT_EQ(log(), Vector<uint64_t>({ entry(2, 0) }));
    EXPECT_GE(dispatchCount.load(), 1u);
    canHandle = true;
    serverQueue().wakeUp();
    EXPECT_TRUE(waitForLogSize(3));
    EXPECT_EQ(log(), Vector<uint64_t>({ entry(2, 0), entry(1, 0), entry(1, 1) }));
}

TEST_F(StreamWaitingMessageTest, DeferredOutOfStreamMessageIsDecodedAgainWhenWokenUp)
{
    std::atomic<bool> canHandle { false };
    std::atomic<unsigned> dispatchCount { 0 };
    auto& waiting = openConnection(1, serverQueue(), [&](IPC::StreamServerConnection& connection, uint64_t value) {
        if (value != entry(1, 1))
            return true;
        ++dispatchCount;
        EXPECT_TRUE(connection.canCurrentlyDispatchedMessageWait());
        return canHandle.load();
    });
    auto& other = openConnection(2, serverQueue());
    send(waiting, 0);
    sendOutOfStream(waiting, 1);
    send(waiting, 2);
    sendOutOfStream(waiting, 3);
    send(other, 0);
    EXPECT_TRUE(waitForLogSize(2));
    barrier(serverQueue());
    barrier(serverQueue());
    // The deferred message holds back the messages after it, also the other out-of-stream messages.
    EXPECT_EQ(log(), Vector<uint64_t>({ entry(1, 0), entry(2, 0) }));
    EXPECT_GE(dispatchCount.load(), 1u);
    unsigned dispatchCountBeforeWakeUp = dispatchCount;
    canHandle = true;
    serverQueue().wakeUp();
    EXPECT_TRUE(waitForLogSize(5));
    // The message is decoded again from the start of its arguments.
    EXPECT_EQ(log(), Vector<uint64_t>({ entry(1, 0), entry(2, 0), entry(1, 1), entry(1, 2), entry(1, 3) }));
    EXPECT_GT(dispatchCount.load(), dispatchCountBeforeWakeUp);
}

TEST_F(StreamWaitingMessageTest, OutOfStreamMessageWithAttachmentsCannotWait)
{
    std::atomic<bool> didDispatch { false };
    std::atomic<bool> canWait { true };
    auto& connection = openConnection(1, serverQueue(), [&](IPC::StreamServerConnection& connection, uint64_t value) {
        if (value == entry(1, 0)) {
            canWait = connection.canCurrentlyDispatchedMessageWait();
            didDispatch = true;
        }
        return true;
    });
    EXPECT_EQ(connection.clientConnection->send(MockStreamTestMessageWithValueAndAttachment { entry(1, 0), IPC::Semaphore { } }, defaultDestinationID()), IPC::Error::NoError);
    EXPECT_TRUE(waitForLogSize(1));
    EXPECT_TRUE(didDispatch.load());
    // Decoding the message again would need the attachments that the arguments took.
    EXPECT_FALSE(canWait.load());
}

TEST_F(StreamWaitingMessageTest, HeapReadWaitsForObjectAddedOnSameWorkQueue)
{
    auto readReference = m_referenceTracker.read();
    auto& consumer = openConnection(1, serverQueue(), readObjectHandler(1, 0, readReference));
    auto& producer = openConnection(2, serverQueue(), addObjectHandler(2, 0, m_referenceTracker.add(), 42));
    block(serverQueue());
    send(consumer, 0);
    send(consumer, 1);
    send(producer, 0);
    unblock();
    EXPECT_TRUE(waitForLogSize(3));
    // The consumer runs first, so it reads before the producer adds the object.
    EXPECT_EQ(log(), Vector<uint64_t>({ entry(2, 0), entry(1, 0), entry(1, 1) }));
    EXPECT_EQ(m_readObject.load(), 42u);
}

TEST_F(StreamWaitingMessageTest, HeapReadWaitsForObjectAddedOnOtherWorkQueue)
{
    auto readReference = m_referenceTracker.read();
    auto& consumer = openConnection(1, serverQueue(), readObjectHandler(1, 0, readReference));
    auto& producer = openConnection(2, otherServerQueue(), addObjectHandler(2, 0, m_referenceTracker.add(), 42));
    block(otherServerQueue());
    send(consumer, 0);
    send(consumer, 1);
    send(producer, 0);
    barrier(serverQueue());
    barrier(serverQueue());
    EXPECT_TRUE(log().isEmpty());
    unblock();
    EXPECT_TRUE(waitForLogSize(3));
    EXPECT_EQ(log(), Vector<uint64_t>({ entry(2, 0), entry(1, 0), entry(1, 1) }));
    EXPECT_EQ(m_readObject.load(), 42u);
}

TEST_F(StreamWaitingMessageTest, HeapReadOfObjectThatIsNeverAddedKeepsWaiting)
{
    auto readReference = m_referenceTracker.read();
    auto& consumer = openConnection(1, serverQueue(), readObjectHandler(1, 0, readReference));
    auto& other = openConnection(2, serverQueue());
    send(consumer, 0);
    send(consumer, 1);
    // The other connection keeps running while the consumer waits.
    send(other, 0);
    EXPECT_TRUE(waitForLogSize(1));
    // Clearing the heap wakes up the work queue, but the object is still not added.
    m_heap.clear();
    send(other, 1);
    EXPECT_TRUE(waitForLogSize(2));
    barrier(serverQueue());
    barrier(serverQueue());
    EXPECT_EQ(log(), Vector<uint64_t>({ entry(2, 0), entry(2, 1) }));
}

TEST_F(StreamWaitingMessageTest, HeapDoesNotKeepWorkQueueOfTornDownConnectionAlive)
{
    RefPtr queue = IPC::StreamConnectionWorkQueue::create("StreamWaitingMessageTest torn down work queue"_s);
    ThreadSafeWeakPtr<IPC::StreamConnectionWorkQueue> weakQueue { *queue };
    auto reference = m_referenceTracker.add();
    auto readReference = m_referenceTracker.read();
    std::atomic<unsigned> dispatchCount { 0 };
    auto& consumer = openConnection(1, *queue, [&](IPC::StreamServerConnection& connection, uint64_t) {
        ++dispatchCount;
        auto read = readReference;
        return !!m_heap.read(connection, WTF::move(read));
    });
    send(consumer, 0);
    EXPECT_TRUE(Util::waitFor([&] {
        return dispatchCount.load() >= 1;
    }));
    closeConnection(consumer);
    m_connections.removeFirstMatching([&](auto& connection) {
        return connection.get() == &consumer;
    });
    queue->stopAndWaitForCompletion();
    queue = nullptr;
    // The message is still deferred, waiting for the object.
    EXPECT_TRUE(log().isEmpty());
    EXPECT_FALSE(weakQueue.get());
    // Adding the object does not wake up the destroyed work queue.
    EXPECT_TRUE(m_heap.add(reference, WaitedObject::create(42)));
}

TEST_F(StreamWaitingMessageTest, HeapReadInSyncMessageDoesNotWait)
{
    auto& consumer = openConnection(1, serverQueue());
    auto readReference = m_referenceTracker.read();
    consumer.mockServerReceiver->setSyncMessageHandler([&](IPC::StreamServerConnection& connection, IPC::Decoder& decoder) {
        EXPECT_TRUE(!!decoder.decode<uint32_t>());
        EXPECT_FALSE(connection.canCurrentlyDispatchedMessageWait());
        auto read = readReference;
        auto object = m_heap.read(connection, WTF::move(read));
        // The handler of a sync message cannot defer it, so the read does not wait.
        EXPECT_TRUE(!!object);
        connection.sendSyncReply<MockSyncMessage>(decoder.syncRequestID(), object && *object ? static_cast<uint32_t>((*object)->value()) : 0u);
        return true;
    });
    auto result = consumer.clientConnection->sendSync(MockSyncMessage { 7 }, defaultDestinationID());
    ASSERT_TRUE(result.succeeded());
    auto [reply] = result.reply();
    EXPECT_EQ(reply, 0u);
}

}

}
