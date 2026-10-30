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

#include "IPCTestUtilities.h"
#include "Helpers/Test.h"
#include "WorkQueueMessageReceiver.h"
#include <wtf/Scope.h>
#include <wtf/Threading.h>
#include <wtf/threads/BinarySemaphore.h>

// libproc is private on iOS, so the send benchmarks are macOS only.
#if PLATFORM(MAC)
#include <libproc.h>
#include <pthread.h>
#include <time.h>
#endif


namespace TestWebKitAPI {

static constexpr Seconds kDefaultWaitForTimeout = 1_s;

static constexpr Seconds kWaitForAbsenceTimeout = 300_ms;

struct MockTestMessageWithConnection {
    static constexpr bool isSync = false;
    static constexpr bool canDispatchOutOfOrder = false;
    static constexpr bool replyCanDispatchOutOfOrder = false;
    static constexpr IPC::MessageName name()  { return IPC::MessageName::IPCTester_EmptyMessage; }
    MockTestMessageWithConnection(IPC::Connection::Handle&& handle)
        : m_handle(WTF::move(handle))
    {
    }

    template<typename Encoder>
    void encode(Encoder& encoder)
    {
        encoder << WTF::move(m_handle);
    }

private:
    IPC::Connection::Handle&& m_handle;
};

struct MockTestSyncMessage {
    static constexpr bool isSync = true;
    static constexpr bool canDispatchOutOfOrder = false;
    static constexpr bool replyCanDispatchOutOfOrder = false;
    static constexpr IPC::MessageName name()  { return IPC::MessageName::IPCTester_SyncPing; }
    using ReplyArguments = std::tuple<>;

    template<typename Encoder> void encode(Encoder&) { }
};

struct MockTestSyncMessageWithDataReply {
    static constexpr bool isSync = true;
    static constexpr bool canDispatchOutOfOrder = false;
    static constexpr bool replyCanDispatchOutOfOrder = false;
    static constexpr IPC::MessageName name()  { return IPC::MessageName::IPCTester_SyncPing; } // Needs to be sync.
    using ReplyArguments = std::tuple<std::span<const uint8_t>>;

    template<typename Encoder> void encode(Encoder&) { }
};

struct MockTestMessageWithAsyncReplyCanDispatchOutOfOrder {
    static constexpr bool isSync = false;
    static constexpr bool canDispatchOutOfOrder = true;
    static constexpr bool replyCanDispatchOutOfOrder = true;
    static constexpr IPC::MessageName name()  { return IPC::MessageName::IPCTester_EmptyMessageWithReply; } // Needs to be async with reply.
    static constexpr IPC::MessageName asyncMessageReplyName() { return IPC::MessageName::IPCTester_EmptyMessageWithReplyReply; }
    template<typename Encoder> void encode(Encoder&) { }

    using ReplyArguments = std::tuple<uint64_t>;
};

#if PLATFORM(MAC)
struct MockTestMessageWithDataAndAttachments {
    static constexpr bool isSync = false;
    static constexpr bool canDispatchOutOfOrder = false;
    static constexpr bool replyCanDispatchOutOfOrder = false;
    static constexpr IPC::MessageName name()  { return IPC::MessageName::IPCTester_EmptyMessage; }

    template<typename Encoder>
    void encode(Encoder& encoder)
    {
        encoder << data;
        encoder << static_cast<uint64_t>(attachments.size());
        for (auto& attachment : attachments)
            encoder << WTF::move(attachment);
    }

    std::span<const uint8_t> data;
    Vector<IPC::Attachment> attachments;
};

static Vector<uint8_t> testDataOfSize(size_t size)
{
    return Vector<uint8_t>(size, [size](size_t i) {
        return static_cast<uint8_t>(i * 31 + size);
    });
}
#endif // PLATFORM(MAC)

namespace {
class SimpleConnectionTest : public testing::Test {
public:
    SimpleConnectionTest()
        : m_mockServerClient(MockConnectionClient::create())
        , m_mockClientClient(MockConnectionClient::create())
    {
    }

    void SetUp() override
    {
        WTF::initializeMainThread();
    }
protected:
    Ref<MockConnectionClient> m_mockServerClient;
    Ref<MockConnectionClient> m_mockClientClient;
};
}

TEST_F(SimpleConnectionTest, CreateServerConnection)
{
    auto identifiers = IPC::Connection::createConnectionIdentifierPair();
    ASSERT_NE(identifiers, std::nullopt);
    Ref<IPC::Connection> connection = IPC::Connection::createServerConnection(WTF::move(identifiers->server));
    connection->invalidate();
}

TEST_F(SimpleConnectionTest, CreateClientConnection)
{
    auto identifiers = IPC::Connection::createConnectionIdentifierPair();
    ASSERT_NE(identifiers, std::nullopt);
    Ref<IPC::Connection> connection = IPC::Connection::createClientConnection(IPC::Connection::Identifier { WTF::move(identifiers->client) });
    connection->invalidate();
}

TEST_F(SimpleConnectionTest, ConnectLocalConnection)
{
    auto identifiers = IPC::Connection::createConnectionIdentifierPair();
    ASSERT_NE(identifiers, std::nullopt);
    Ref<IPC::Connection> serverConnection = IPC::Connection::createServerConnection(WTF::move(identifiers->server));
    Ref<IPC::Connection> clientConnection = IPC::Connection::createClientConnection(IPC::Connection::Identifier { WTF::move(identifiers->client) });
    serverConnection->open(m_mockServerClient);
    clientConnection->open(m_mockClientClient);
    serverConnection->invalidate();
    clientConnection->invalidate();
}

TEST_F(SimpleConnectionTest, ClearOutgoingMessages)
{
    // Create a connection, but leave the client
    // handle pending.
    auto firstIdentifiers = IPC::Connection::createConnectionIdentifierPair();
    ASSERT_NE(firstIdentifiers, std::nullopt);
    Ref<IPC::Connection> firstServerConnection = IPC::Connection::createServerConnection(WTF::move(firstIdentifiers->server));
    firstServerConnection->open(m_mockServerClient);

    // Create a second connection, and send the client
    // handle in a message over the first connection (such
    // that it will be stored as a pending message).
    auto secondIdentifiers = IPC::Connection::createConnectionIdentifierPair();
    ASSERT_NE(secondIdentifiers, std::nullopt);
    Ref<IPC::Connection> secondServerConnection = IPC::Connection::createServerConnection(WTF::move(secondIdentifiers->server));
    Ref mockSecondServerClient = MockConnectionClient::create();
    secondServerConnection->open(mockSecondServerClient);

    firstServerConnection->send(MockTestMessageWithConnection { WTF::move(secondIdentifiers->client) }, 0);

    // Invalidate the first connection's client handle,
    // which should clear pending messages and also invalidate
    // the second connection.
    firstIdentifiers->client = IPC::Connection::Handle();

    // Try a sync send over the second connection, which should
    // fail immediately if the client handle has been released.
    secondServerConnection->sendSync(MockTestSyncMessage(), 0, IPC::Timeout::infinity(), IPC::SendSyncOption::UseFullySynchronousModeForTesting);
    firstServerConnection->invalidate();
    secondServerConnection->invalidate();
}

class ConnectionTest : public testing::Test, protected ConnectionTestBase {
public:
    void SetUp() override
    {
        setupBase();
    }
    void TearDown() override
    {
        teardownBase();
    }
    auto openServer() { return openA(); }
    auto openClient() { return openB(); }
    auto* server() { return a(); }
    auto* client() { return b(); }
    auto& serverClient() { return aClient(); }
    auto& clientClient() { return bClient(); }
    void deleteServer() { deleteA(); }
    void deleteClient() { deleteB(); }
};

// Explicit version of AInvalidateDeliversBDidClose that was flaky on Cocoa in scenario to
//  1. Both connections open
//  2. Client sends the initialization message with the mach port to use as server's send port
//  3. Client is cancelled and the mach port destroyed
//  4. Server receives the initialization message
TEST_F(ConnectionTest, ClientInvalidateBeforeServerHandlesInitializationDeliversDidClose)
{
    ASSERT_TRUE(openServer());
    // Simulation for scheduling for step 4: insert a wait after receive source has been
    // resumed.
    BinarySemaphore semaphore;
    bool captureGuard = false;
    server()->dispatchOnReceiveQueueForTesting([&semaphore, &captureGuard] {
        semaphore.wait();
        captureGuard = true;
    });
    ASSERT_TRUE(openClient());
    Util::runFor(0.2_s); // Simulation for step 2. Give client time to send the initialization message.
    client()->invalidate(); // Step 3.
    semaphore.signal(); // Step 4.
    ASSERT_FALSE(serverClient().gotDidClose());

    // Test for the contract that did not work: invalidated on client causes didClose on server.
    EXPECT_TRUE(serverClient().waitForDidClose(kDefaultWaitForTimeout));

    // End of test. Ensure clean up for buggy cases.
    EXPECT_FALSE(clientClient().gotDidClose());
    Util::run(&captureGuard);
}

TEST_P(ConnectionTestABBA, SendLocalMessage)
{
    ASSERT_TRUE(openBoth());

    for (uint64_t i = 0u; i < 55u; ++i)
        a()->send(MockTestMessage1 { }, i);
    for (uint64_t i = 100u; i < 160u; ++i)
        b()->send(MockTestMessage1 { }, i);
    for (uint64_t i = 0u; i < 55u; ++i) {
        auto message = bClient().waitForMessage(kDefaultWaitForTimeout);
        EXPECT_EQ(message.messageName, MockTestMessage1::name());
        EXPECT_EQ(message.destinationID, i);
    }
    for (uint64_t i = 100u; i < 160u; ++i) {
        auto message = aClient().waitForMessage(kDefaultWaitForTimeout);
        EXPECT_EQ(message.messageName, MockTestMessage1::name()) << " i:" << i;
        EXPECT_EQ(message.destinationID, i) << " i:" << i;
    }
}

TEST_P(ConnectionTestABBA, AInvalidateDeliversBDidClose)
{
    ASSERT_TRUE(openBoth());
    a()->invalidate();
    ASSERT_FALSE(bClient().gotDidClose());
    EXPECT_TRUE(bClient().waitForDidClose(kDefaultWaitForTimeout));
    EXPECT_FALSE(aClient().gotDidClose());
}

TEST_P(ConnectionTestABBA, AAndBInvalidateDoesNotDeliverDidClose)
{
    ASSERT_TRUE(openBoth());
    a()->invalidate();
    b()->invalidate();
    EXPECT_FALSE(aClient().waitForDidClose(kWaitForAbsenceTimeout));
    EXPECT_FALSE(bClient().waitForDidClose(kWaitForAbsenceTimeout));
}

// A flush with nothing left to send returns NoError without waiting, so a zero timeout is enough.
TEST_P(ConnectionTestABBA, FlushSentMessagesWithNothingToSendIsNoOp)
{
    EXPECT_EQ(a()->flushSentMessages(0_s), IPC::Error::NoError);
    EXPECT_EQ(b()->flushSentMessages(0_s), IPC::Error::NoError);

    ASSERT_TRUE(openBoth());

    // Opening the client side may send a message, so the first flush may have to wait.
    EXPECT_EQ(a()->flushSentMessages(kDefaultWaitForTimeout), IPC::Error::NoError);
    EXPECT_EQ(b()->flushSentMessages(kDefaultWaitForTimeout), IPC::Error::NoError);
    for (int i = 0; i < 3; ++i) {
        EXPECT_EQ(a()->flushSentMessages(0_s), IPC::Error::NoError);
        EXPECT_EQ(b()->flushSentMessages(0_s), IPC::Error::NoError);
    }

    for (uint64_t i = 0u; i < 55u; ++i)
        a()->send(MockTestMessage1 { }, i);
    EXPECT_EQ(a()->flushSentMessages(kDefaultWaitForTimeout), IPC::Error::NoError);
    for (int i = 0; i < 3; ++i)
        EXPECT_EQ(a()->flushSentMessages(0_s), IPC::Error::NoError);

    for (uint64_t i = 0u; i < 55u; ++i) {
        auto message = bClient().waitForMessage(kDefaultWaitForTimeout);
        EXPECT_EQ(message.messageName, MockTestMessage1::name());
        EXPECT_EQ(message.destinationID, i);
    }
    EXPECT_EQ(a()->flushSentMessages(0_s), IPC::Error::NoError);
    EXPECT_EQ(b()->flushSentMessages(0_s), IPC::Error::NoError);

    a()->invalidate();
    b()->invalidate();
    EXPECT_EQ(a()->flushSentMessages(0_s), IPC::Error::InvalidConnection);
    EXPECT_EQ(b()->flushSentMessages(0_s), IPC::Error::InvalidConnection);
}

#if PLATFORM(COCOA)

// Invalidating an unopened connection closes its socket, so a peer using Unix domain sockets sees
// the end of file and reports didClose. A Mach based peer is never told.

TEST_P(ConnectionTestABBA, UnopenedAAndInvalidateDoesNotDeliverBDidClose)
{
    ASSERT_TRUE(openB());
    a()->invalidate();
    deleteA();
    EXPECT_FALSE(bClient().waitForDidClose(kWaitForAbsenceTimeout));
}

struct MockTestMessageWithDataAndAttachments {
    static constexpr bool isSync = false;
    static constexpr bool canDispatchOutOfOrder = false;
    static constexpr bool replyCanDispatchOutOfOrder = false;
    static constexpr IPC::MessageName name()  { return IPC::MessageName::IPCTester_EmptyMessage; }

    template<typename Encoder>
    void encode(Encoder& encoder)
    {
        encoder << data;
        encoder << static_cast<uint64_t>(attachments.size());
        for (auto& attachment : attachments)
            encoder << WTF::move(attachment);
    }

    std::span<const uint8_t> data;
    Vector<IPC::Attachment> attachments;
};

static Vector<uint8_t> testDataOfSize(size_t size)
{
    return Vector<uint8_t>(size, [size](size_t i) {
        return static_cast<uint8_t>(i * 31 + size);
    });
}

// Sends one message at a time while B doesn't receive, until a message can't be sent because B's port is full. That
// message is parked while the outgoing queue is empty. flushSentMessages() must keep waiting until it has been sent.
TEST_P(ConnectionTestABBA, FlushSentMessagesWaitsForParkedMessage)
{
    ASSERT_TRUE(openBoth());

    // Make sure A can send before B stops receiving.
    a()->send(MockTestMessage1 { }, 0);
    EXPECT_EQ(bClient().waitForMessage(kDefaultWaitForTimeout).destinationID, 0u);

    BinarySemaphore unblockB;
    b()->dispatchOnReceiveQueueForTesting([&] {
        unblockB.wait();
    });

    uint64_t nextDestinationID = 1;
    bool flushTimedOut = false;
    while (!flushTimedOut && nextDestinationID < 1000) {
        a()->send(MockTestMessage1 { }, nextDestinationID++);
        flushTimedOut = a()->flushSentMessages(200_ms) == IPC::Error::Timeout;
    }
    EXPECT_TRUE(flushTimedOut);
    // B's port holds 128 messages.
    EXPECT_GT(nextDestinationID, 100u);
    // A message is parked now. Before flushSentMessages() checked for parked messages, this flush returned NoError
    // as soon as the outgoing queue was empty.
    EXPECT_EQ(a()->flushSentMessages(50_ms), IPC::Error::Timeout);

    unblockB.signal();
    EXPECT_EQ(a()->flushSentMessages(kDefaultWaitForTimeout), IPC::Error::NoError);
    for (uint64_t i = 1; i < nextDestinationID; ++i)
        EXPECT_EQ(bClient().waitForMessage(kDefaultWaitForTimeout).destinationID, i);
}

// A message sent after B is gone fails with MACH_SEND_INVALID_DEST and is dropped. A can't send anything after that,
// so flushSentMessages() keeps waiting until A handles the close instead of returning NoError for the dropped message.
TEST_P(ConnectionTestABBA, FlushSentMessagesAfterPeerIsGoneWaitsForClose)
{
    ASSERT_TRUE(openBoth());

    // Make sure A can send before B goes away.
    a()->send(MockTestMessage1 { }, 0);
    EXPECT_EQ(bClient().waitForMessage(kDefaultWaitForTimeout).destinationID, 0u);
    EXPECT_EQ(a()->flushSentMessages(kDefaultWaitForTimeout), IPC::Error::NoError);

    // Hold A's connection queue while B goes away, so that the next message fails to send. Then hold it again, so that
    // A handles the close only after the flush below has had a chance to return early.
    BinarySemaphore holdA;
    a()->dispatchOnReceiveQueueForTesting([&] {
        holdA.waitFor(kWaitForAbsenceTimeout);
    });

    // Send a message that is large enough that it is guaranteed to be sent on the connection queue
    // rather than directly on the calling thread.
    auto largeData = testDataOfSize(8000);
    a()->send(MockTestMessageWithDataAndAttachments { largeData.span(), { } }, 1);
    BinarySemaphore releaseA;
    a()->dispatchOnReceiveQueueForTesting([&] {
        releaseA.waitFor(10_s);
    });
    b()->invalidate();

    EXPECT_EQ(a()->flushSentMessages(kDefaultWaitForTimeout), IPC::Error::Timeout);
    releaseA.signal();
    EXPECT_EQ(a()->flushSentMessages(kDefaultWaitForTimeout), IPC::Error::InvalidConnection);
    EXPECT_TRUE(aClient().waitForDidClose(kDefaultWaitForTimeout));
}

#if ENABLE(IPC_SEND_DIRECTLY)

// Blocks a connection's queue, so that it neither receives nor sends messages, until unblock() is called or it is
// destroyed.
class ConnectionQueueBlocker {
    WTF_MAKE_NONCOPYABLE(ConnectionQueueBlocker);
public:
    explicit ConnectionQueueBlocker(IPC::Connection& connection)
    {
        connection.dispatchOnReceiveQueueForTesting([semaphore = m_semaphore] {
            semaphore->wait();
        });
    }

    ~ConnectionQueueBlocker()
    {
        unblock();
    }

    // May be called on any thread.
    void unblock()
    {
        if (m_isBlocking.exchange(false))
            m_semaphore->signal();
    }

private:
    // The connection queue may still be waking up when this is destroyed.
    Box<BinarySemaphore> m_semaphore { Box<BinarySemaphore>::create() };
    std::atomic<bool> m_isBlocking { true };
};

// Tests for sending messages on the calling thread, see IPC::Connection::setAllowsSendingDirectly(). They run with
// direct sends allowed, and the main thread's QoS is higher than that of the connection queues. While A's connection
// queue is blocked, only a direct send can deliver a message to B. So B receiving a message then shows that it was sent
// directly, and B not receiving it shows that it was queued.
class ConnectionDirectSendTest : public ConnectionTestABBA {
protected:
    static constexpr uint64_t probeDestinationID = 0xffffff;

    // The destination IDs of the messages that B receives within the timeout, until there are at least `count`.
    Vector<uint64_t> receiveFromA(size_t count, Seconds timeout)
    {
        Vector<uint64_t> destinationIDs;
        auto deadline = MonotonicTime::now() + timeout;
        while (true) {
            for (auto& message : bClient().takeMessages())
                destinationIDs.append(message.destinationID);
            if (destinationIDs.size() >= count || MonotonicTime::now() >= deadline)
                return destinationIDs;
            Util::runFor(10_ms);
        }
    }

    // A message can only be sent directly once a() is connected and its send source has been registered, which
    // happens on its connection queue some time after it connected. Send messages until one is sent directly.
    ::testing::AssertionResult waitUntilASendsDirectly()
    {
        for (unsigned attempt = 0; attempt < 100; ++attempt) {
            Vector<uint64_t> received;
            {
                ConnectionQueueBlocker blocker { *a() };
                a()->send(MockTestMessage1 { }, probeDestinationID);
                received = receiveFromA(1, 100_ms);
            }
            if (!received.isEmpty()) {
                if (received.size() != 1 || received[0] != probeDestinationID)
                    return ::testing::AssertionFailure() << "Unexpected messages: " << received.size() << " first to " << received[0];
                return ::testing::AssertionSuccess();
            }
            // The probe was queued, and A's connection queue sends it now.
            received = receiveFromA(1, kDefaultWaitForTimeout);
            if (received.size() != 1 || received[0] != probeDestinationID)
                return ::testing::AssertionFailure() << "The queued probe didn't arrive, " << received.size() << " messages";
            sleep(10_ms);
        }
        return ::testing::AssertionFailure() << "No message was sent directly";
    }

    enum class ToggleSendingDirectly : bool { No, Yes };

    // Sends from several threads at once while B doesn't receive, with message sizes that can be sent directly and
    // a size that is too large to be. Every thread's messages must arrive in order.
    void sendFromThreadsWhileBIsBlocked(ToggleSendingDirectly toggleSendingDirectly)
    {
        constexpr uint64_t threadCount = 4;
        constexpr uint64_t messagesPerThread = 500;
        Vector<Vector<uint8_t>> data { testDataOfSize(16), testDataOfSize(2000), testDataOfSize(6000) };
        auto sendMessages = [&](uint64_t thread) {
            for (uint64_t i = 0; i < messagesPerThread; ++i)
                a()->send(MockTestMessageWithDataAndAttachments { data[i % data.size()].span(), { } }, (thread << 32) | i);
        };

        ConnectionQueueBlocker blocker { *b() };

        std::atomic<bool> doneSending { false };
        RefPtr<Thread> toggler;
        if (toggleSendingDirectly == ToggleSendingDirectly::Yes) {
            toggler = Thread::create("ConnectionDirectSendTest toggler"_s, [&] {
                for (unsigned i = 0; !doneSending; ++i) {
                    IPC::Connection::setAllowsSendingDirectly(!(i % 2));
                    sleep(100_us);
                }
                IPC::Connection::setAllowsSendingDirectly(true);
            });
        }

        Vector<Ref<Thread>> senders;
        for (uint64_t thread = 1; thread < threadCount; ++thread) {
            senders.append(Thread::create("ConnectionDirectSendTest sender"_s, [&sendMessages, thread] {
                sendMessages(thread);
            }, ThreadType::Unknown, Thread::QOS::UserInteractive));
        }
        sendMessages(0);
        for (auto& sender : senders)
            sender->waitForCompletion();
        doneSending = true;
        if (toggler)
            toggler->waitForCompletion();

        blocker.unblock();
        std::array<uint64_t, threadCount> nextSequenceNumbers { };
        for (uint64_t i = 0; i < threadCount * messagesPerThread; ++i) {
            auto destinationID = bClient().waitForMessage(kDefaultWaitForTimeout).destinationID;
            auto thread = destinationID >> 32;
            auto sequenceNumber = destinationID & 0xffffffff;
            ASSERT_LT(thread, threadCount);
            EXPECT_EQ(sequenceNumber, nextSequenceNumbers[thread]) << " thread: " << thread;
            nextSequenceNumbers[thread] = sequenceNumber + 1;
        }
    }
};

TEST_P(ConnectionDirectSendTest, SmallMessagesAreSentDirectly)
{
    ASSERT_TRUE(openBoth());
    ASSERT_TRUE(waitUntilASendsDirectly());

    // Fewer messages than B's port holds, so that none of them has to wait for B.
    constexpr uint64_t messageCount = 100;
    ConnectionQueueBlocker blocker { *a() };
    for (uint64_t i = 0; i < messageCount; ++i)
        a()->send(MockTestMessage1 { }, i);
    auto received = receiveFromA(messageCount, kDefaultWaitForTimeout);
    ASSERT_EQ(received.size(), messageCount);
    for (uint64_t i = 0; i < messageCount; ++i)
        EXPECT_EQ(received[i], i);
}

TEST_P(ConnectionDirectSendTest, MessagesWithAttachmentsAreSentDirectly)
{
    ASSERT_TRUE(openBoth());
    ASSERT_TRUE(waitUntilASendsDirectly());

    mach_port_t port = MACH_PORT_NULL;
    ASSERT_EQ(mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &port), KERN_SUCCESS);
    auto sendRight = MachSendRight::createFromReceiveRight(port);
    ASSERT_TRUE(!!sendRight);

    constexpr uint64_t messageCount = 10;
    auto data = testDataOfSize(100);
    bClient().setAsyncMessageHandler([&](IPC::Connection&, IPC::Decoder& decoder) -> bool {
        if (decoder.destinationID() >= messageCount)
            return false;
        auto receivedData = decoder.decode<std::span<const uint8_t>>();
        auto attachmentCount = decoder.decode<uint64_t>();
        EXPECT_TRUE(receivedData && equalSpans(*receivedData, data.span()));
        EXPECT_TRUE(attachmentCount && *attachmentCount == 2u);
        for (unsigned i = 0; i < 2; ++i) {
            auto attachment = decoder.decode<IPC::Attachment>();
            EXPECT_TRUE(attachment && attachment->sendRight() == port);
        }
        return false; // Record the message.
    });

    {
        ConnectionQueueBlocker blocker { *a() };
        for (uint64_t i = 0; i < messageCount; ++i) {
            Vector<IPC::Attachment> attachments(2, [&](size_t) {
                return MachSendRight { sendRight };
            });
            a()->send(MockTestMessageWithDataAndAttachments { data.span(), WTF::move(attachments) }, i);
        }
        auto received = receiveFromA(messageCount, kDefaultWaitForTimeout);
        ASSERT_EQ(received.size(), messageCount);
        for (uint64_t i = 0; i < messageCount; ++i)
            EXPECT_EQ(received[i], i);
    }

    sendRight = { };
    mach_port_mod_refs(mach_task_self(), port, MACH_PORT_RIGHT_RECEIVE, -1);
}

// A can't send directly before it is connected, which for a server is when B has opened, and before its send source has
// been registered. The messages sent before then must still arrive first.
TEST_P(ConnectionDirectSendTest, MessagesSentBeforeConnectingAreQueued)
{
    ASSERT_TRUE(openA());
    for (uint64_t i = 0; i < 10; ++i)
        a()->send(MockTestMessage1 { }, i);

    ASSERT_TRUE(openB());
    for (uint64_t i = 10; i < 20; ++i)
        a()->send(MockTestMessage1 { }, i);
    for (uint64_t i = 0; i < 20; ++i)
        EXPECT_EQ(bClient().waitForMessage(kDefaultWaitForTimeout).destinationID, i);
}

// A message that isn't sent directly is queued, and the messages sent after it are queued behind it until it has been
// sent, even if they could be sent directly. Only threads whose QoS is at least that of A's connection queue send
// directly.
TEST_P(ConnectionDirectSendTest, MessagesAreNotSentDirectlyAheadOfQueuedMessage)
{
    ASSERT_TRUE(openBoth());
    ASSERT_TRUE(waitUntilASendsDirectly());

    auto checkMessageIsQueuedWithMessageBehindIt = [&](uint64_t destinationID, auto&& sendMessage) {
        SCOPED_TRACE(destinationID);
        {
            // Keep A's connection queue from sending the queued message.
            ConnectionQueueBlocker blocker { *a() };
            sendMessage();
            a()->send(MockTestMessage1 { }, destinationID + 1);
            EXPECT_TRUE(receiveFromA(1, kWaitForAbsenceTimeout).isEmpty());
        }
        auto received = receiveFromA(2, kDefaultWaitForTimeout);
        ASSERT_EQ(received.size(), 2u);
        EXPECT_EQ(received[0], destinationID);
        EXPECT_EQ(received[1], destinationID + 1);
    };

    // A message sent while direct sends aren't allowed.
    checkMessageIsQueuedWithMessageBehindIt(100, [&] {
        IPC::Connection::setAllowsSendingDirectly(false);
        a()->send(MockTestMessage1 { }, 100);
        IPC::Connection::setAllowsSendingDirectly(true);
    });

    // A message too large to be sent inline.
    auto largeData = testDataOfSize(8000);
    checkMessageIsQueuedWithMessageBehindIt(200, [&] {
        a()->send(MockTestMessageWithDataAndAttachments { largeData.span(), { } }, 200);
    });

    // Messages sent from threads with a lower QoS than A's connection queue.
    checkMessageIsQueuedWithMessageBehindIt(300, [&] {
        Thread::create("ConnectionDirectSendTest utility"_s, [&] {
            a()->send(MockTestMessage1 { }, 300);
        }, ThreadType::Unknown, Thread::QOS::Utility)->waitForCompletion();
    });
    checkMessageIsQueuedWithMessageBehindIt(400, [&] {
        Thread::create("ConnectionDirectSendTest background"_s, [&] {
            a()->send(MockTestMessage1 { }, 400);
        }, ThreadType::Unknown, Thread::QOS::Background)->waitForCompletion();
    });

    // A message sent from another thread with a high enough QoS is sent directly.
    ConnectionQueueBlocker blocker { *a() };
    Thread::create("ConnectionDirectSendTest user interactive"_s, [&] {
        a()->send(MockTestMessage1 { }, 500);
    }, ThreadType::Unknown, Thread::QOS::UserInteractive)->waitForCompletion();
    auto received = receiveFromA(1, kDefaultWaitForTimeout);
    ASSERT_EQ(received.size(), 1u);
    EXPECT_EQ(received[0], 500u);
}

// When B's port is full, the direct send that finds it full parks its message, and the messages sent after it are
// queued behind it.
TEST_P(ConnectionDirectSendTest, MessagesAreNotSentDirectlyAheadOfParkedMessage)
{
    ASSERT_TRUE(openBoth());
    ASSERT_TRUE(waitUntilASendsDirectly());

    // B's port holds 128 messages.
    constexpr uint64_t messageCount = 200;
    ConnectionQueueBlocker blockerA { *a() };
    {
        ConnectionQueueBlocker blockerB { *b() };
        for (uint64_t i = 0; i < messageCount; ++i)
            a()->send(MockTestMessage1 { }, i);
    }

    // B now receives the messages that were sent directly before its port was full. The parked message and the ones
    // behind it wait for A's connection queue.
    auto sentDirectly = receiveFromA(messageCount, kDefaultWaitForTimeout);
    EXPECT_GT(sentDirectly.size(), 100u);
    EXPECT_LT(sentDirectly.size(), messageCount);
    for (uint64_t i = 0; i < sentDirectly.size(); ++i)
        EXPECT_EQ(sentDirectly[i], i);

    blockerA.unblock();
    auto sentLater = receiveFromA(messageCount - sentDirectly.size(), kDefaultWaitForTimeout);
    ASSERT_EQ(sentDirectly.size() + sentLater.size(), messageCount);
    for (uint64_t i = 0; i < sentLater.size(); ++i)
        EXPECT_EQ(sentLater[i], sentDirectly.size() + i);
}

TEST_P(ConnectionDirectSendTest, SendingFromThreadsKeepsOrder)
{
    ASSERT_TRUE(openBoth());
    ASSERT_TRUE(waitUntilASendsDirectly());
    sendFromThreadsWhileBIsBlocked(ToggleSendingDirectly::No);
}

TEST_P(ConnectionDirectSendTest, TogglingSendingDirectlyWhileSendingFromThreadsKeepsOrder)
{
    ASSERT_TRUE(openBoth());
    ASSERT_TRUE(waitUntilASendsDirectly());
    sendFromThreadsWhileBIsBlocked(ToggleSendingDirectly::Yes);
}

// Messages from A's connection queue can be sent directly too. That must not deadlock or reorder messages.
TEST_P(ConnectionDirectSendTest, SendFromConnectionQueue)
{
    ASSERT_TRUE(openBoth());
    ASSERT_TRUE(waitUntilASendsDirectly());

    a()->send(MockTestMessage1 { }, 1);
    BinarySemaphore didSend;
    BinarySemaphore didReceive;
    a()->dispatchOnReceiveQueueForTesting([&] {
        // The queue runs at its own QoS, it can't be draining or handling a send source event at the same time, and a
        // message 1 that it had to send has been sent by now. It stays busy until B has received message 2, so message
        // 2 can only have been sent directly.
        a()->send(MockTestMessage1 { }, 2);
        didSend.signal();
        didReceive.wait();
    });
    didSend.wait();
    auto received = receiveFromA(2, kDefaultWaitForTimeout);
    didReceive.signal();
    ASSERT_EQ(received.size(), 2u);
    EXPECT_EQ(received[0], 1u);
    EXPECT_EQ(received[1], 2u);

    a()->send(MockTestMessage1 { }, 3);
    EXPECT_EQ(bClient().waitForMessage(kDefaultWaitForTimeout).destinationID, 3u);
}

// B going away while threads send directly to it. Depending on whether A first finds B's port dead or gets the
// no-senders notification, direct sends fail with MACH_SEND_INVALID_DEST, which cancels A's send source on the sending
// thread, or stop because A is no longer connected. DirectSendToReceiverThatHasGoneAway checks the former reliably.
TEST_P(ConnectionDirectSendTest, ReceiverInvalidatesWhileSendingDirectly)
{
    ASSERT_TRUE(openBoth());
    ASSERT_TRUE(waitUntilASendsDirectly());
    bClient().setAsyncMessageHandler([](IPC::Connection&, IPC::Decoder&) {
        return true; // Drop the message.
    });

    std::atomic<bool> stop { false };
    Vector<Ref<Thread>> senders;
    for (unsigned i = 0; i < 4; ++i) {
        senders.append(Thread::create("ConnectionDirectSendTest sender"_s, [&] {
            for (uint64_t j = 0; !stop && a()->send(MockTestMessage1 { }, j) == IPC::Error::NoError; ++j) { }
        }, ThreadType::Unknown, Thread::QOS::UserInteractive));
    }
    sleep(10_ms);
    b()->invalidate();
    EXPECT_TRUE(aClient().waitForDidClose(5_s));
    stop = true;
    for (auto& sender : senders)
        sender->waitForCompletion();
}

// A direct send that finds B's port dead drops the message and cancels A's send source on the sending thread. Later
// messages are queued, and A still closes once it gets the no-senders notification.
TEST_P(ConnectionDirectSendTest, DirectSendToReceiverThatHasGoneAway)
{
    ASSERT_TRUE(openBoth());
    ASSERT_TRUE(waitUntilASendsDirectly());

    {
        // Keep A from handling the no-senders notification, so that it is still connected when B's port dies. Only
        // direct sends can reach B's port while A's connection queue is blocked.
        ConnectionQueueBlocker blocker { *a() };
        b()->invalidate();
        // B's port dies once B's receive source has been cancelled on B's connection queue. After a direct send fails
        // because the port is dead, flushSentMessages() waits until A closes. A direct send that parks its message
        // because B's port filled up first does the same, but B's port usually dies long before it holds 128 messages.
        bool directSendFailed = false;
        for (unsigned attempt = 0; attempt < 5000 && !directSendFailed; ++attempt) {
            a()->send(MockTestMessage1 { }, attempt);
            directSendFailed = a()->flushSentMessages(0_s) == IPC::Error::Timeout;
            if (!directSendFailed)
                sleep(1_ms);
        }
        ASSERT_TRUE(directSendFailed);

        // The send source has been cancelled, so this one is queued.
        a()->send(MockTestMessage1 { }, 5000);
        EXPECT_EQ(a()->flushSentMessages(0_s), IPC::Error::Timeout);
    }
    EXPECT_TRUE(aClient().waitForDidClose(5_s));
}

TEST_P(ConnectionDirectSendTest, SenderInvalidatesWhileOtherThreadsSendDirectly)
{
    ASSERT_TRUE(openBoth());
    ASSERT_TRUE(waitUntilASendsDirectly());
    bClient().setAsyncMessageHandler([](IPC::Connection&, IPC::Decoder&) {
        return true; // Drop the message.
    });

    Vector<Ref<Thread>> senders;
    for (unsigned i = 0; i < 4; ++i) {
        senders.append(Thread::create("ConnectionDirectSendTest sender"_s, [&] {
            // Stops once A has been invalidated.
            for (uint64_t j = 0; a()->send(MockTestMessage1 { }, j) == IPC::Error::NoError; ++j) { }
        }, ThreadType::Unknown, Thread::QOS::UserInteractive));
    }
    sleep(10_ms);
    a()->invalidate();
    for (auto& sender : senders)
        sender->waitForCompletion();
    EXPECT_TRUE(bClient().waitForDidClose(5_s));
}

INSTANTIATE_TEST_SUITE_P(ConnectionTest,
    ConnectionDirectSendTest,
    testing::Combine(testing::Values(ConnectionTestDirection::ServerIsA, ConnectionTestDirection::ClientIsA), testing::Values(ConnectionTestDirectSend::Enabled)),
    ConnectionTestABBAParametersToStringFormatter());

#endif // ENABLE(IPC_SEND_DIRECTLY)

#endif // PLATFORM(COCOA)

TEST_P(ConnectionTestABBA, IncomingMessageThrottlingWorks)
{
    const size_t testedCount = 2300;
    a()->enableIncomingMessagesThrottling();
    ASSERT_TRUE(openBoth());
    size_t otherRunLoopTasksRun = 0u;

    for (uint64_t i = 0u; i < testedCount; ++i)
        b()->send(MockTestMessage1 { }, i);
    while (a()->pendingMessageCountForTesting() < testedCount)
        sleep(0.1_s);

    Vector<MessageInfo> messages;
    std::array<size_t, 18> messageCounts { 600, 300, 200, 150, 120, 100, 85, 75, 66, 60, 60, 66, 75, 85, 100, 120, 37, 1 };
    for (size_t i = 0; i < messageCounts.size(); ++i) {
        SCOPED_TRACE(i);
        RunLoop::currentSingleton().dispatch([&otherRunLoopTasksRun] {
            otherRunLoopTasksRun++;
        });
        Util::spinRunLoop();
        EXPECT_EQ(otherRunLoopTasksRun, i + 1u);
        auto messages1 = aClient().takeMessages();
        EXPECT_EQ(messageCounts[i], messages1.size());
        messages.appendVector(WTF::move(messages1));
    }
    EXPECT_EQ(testedCount, messages.size());
    for (uint64_t i = 0u; i < messages.size(); ++i) {
        auto& message = messages[i];
        EXPECT_EQ(message.messageName, MockTestMessage1::name());
        EXPECT_EQ(message.destinationID, i);
    }
}

// Tests the case where a throttled connection dispatches a message that
// spins the run loop in the message handler. A naive throttled connection
// would only schedule one work dispatch function, which would then fail
// in this scenario. Thus test the non-naive implementation where the throttled
// connection schedules another dispatch function that ensures that nested
// runloops will dispatch the throttled connection messages.
TEST_P(ConnectionTestABBA, IncomingMessageThrottlingNestedRunLoopDispatches)
{
    const size_t testedCount = 2300;
    a()->enableIncomingMessagesThrottling();
    ASSERT_TRUE(openBoth());
    size_t otherRunLoopTasksRun = 0u;

    for (uint64_t i = 0u; i < testedCount; ++i)
        b()->send(MockTestMessage1 { }, i);
    while (a()->pendingMessageCountForTesting() < testedCount)
        sleep(0.1_s);

    // Two messages invoke nested run loop. The handler skips total 4 messages for the
    // proofs of logic that the test was ran.
    bool isProcessing = false;
    aClient().setAsyncMessageHandler([&] (IPC::Connection&, IPC::Decoder& decoder) -> bool {
        auto destinationID = decoder.destinationID();
        if (destinationID == 888 || destinationID == 1299) {
            isProcessing = true;
            Util::spinRunLoop();
            isProcessing = false;
            return true; // Skiping the message is the proof that the message was processed.
        }
        if (destinationID == 889 || destinationID == 1300) {
            EXPECT_TRUE(isProcessing); // Passing the EXPECT is the proof that we ran the message in a nested event loop.
            return true; // Skipping the message is the proof that above EXPECT was ran.
        }
        return false;
    });

    Vector<MessageInfo> messages;
    std::array<size_t, 16> messageCounts { 600, 498, 150, 218, 85, 75, 66, 60, 60, 66, 75, 85, 100, 120, 37, 1 };
    for (size_t i = 0; i < messageCounts.size(); ++i) {
        SCOPED_TRACE(i);
        RunLoop::currentSingleton().dispatch([&otherRunLoopTasksRun] {
            otherRunLoopTasksRun++;
        });
        Util::spinRunLoop();
        EXPECT_EQ(otherRunLoopTasksRun, i + 1u);
        auto messages1 = aClient().takeMessages();
        EXPECT_EQ(messageCounts[i], messages1.size());
        messages.appendVector(WTF::move(messages1));
    }
    EXPECT_EQ(testedCount - 4, messages.size());
    for (uint64_t i = 0u, j = 0u; i < messages.size(); ++i, ++j) {
        if (j == 888 || j == 1299)
            j += 2;
        auto& message = messages[i];
        EXPECT_EQ(message.messageName, MockTestMessage1::name());
        EXPECT_EQ(message.destinationID, j);
    }
}

// Sends a connection that is already closed (invalidated). We still expect to receive the connection
// and receive didClose on the connection.
TEST_P(ConnectionTestABBA, ReceiveAlreadyInvalidatedClientNoAssert)
{
    ASSERT_TRUE(openBoth());
    constexpr size_t iterations = 800;
    HashSet<uint64_t> done;
    struct {
        RefPtr<IPC::Connection> clientConnection;
        Ref<MockConnectionClient> mockClientClient { MockConnectionClient::create() };
    } connections[iterations];

    bClient().setAsyncMessageHandler([&] (IPC::Connection&, IPC::Decoder& decoder) -> bool {
        auto i = decoder.destinationID();
        auto handle = decoder.decode<IPC::Connection::Handle>();
        if (!handle)
            return false;
        Ref<IPC::Connection> clientConnection = IPC::Connection::createClientConnection(IPC::Connection::Identifier { WTF::move(*handle) });
        clientConnection->open(connections[i].mockClientClient);
        connections[i].clientConnection = WTF::move(clientConnection);
        // The connection starts as not closed in order for the system to deliver didClose().
        EXPECT_FALSE(connections[i].mockClientClient->gotDidClose()) << i;
        done.add(i);
        return true;
    });
    for (uint64_t i = 1; i < iterations; ++i) {
        auto identifiers = IPC::Connection::createConnectionIdentifierPair();
        ASSERT_NE(identifiers, std::nullopt);
        Ref<IPC::Connection> serverConnection = IPC::Connection::createServerConnection(WTF::move(identifiers->server));
        Ref mockServerClient = MockConnectionClient::create();
        serverConnection->open(mockServerClient);
        a()->send(MockTestMessageWithConnection { WTF::move(identifiers->client) }, i);
        serverConnection->invalidate();
    }
    while (done.size() < iterations - 1)
        RunLoop::currentSingleton().cycle();

    for (uint64_t i = 1; i < iterations; ++i) {
        auto& connection = connections[i];
        EXPECT_TRUE(connection.mockClientClient->gotDidClose() || connection.mockClientClient->waitForDidClose(kDefaultWaitForTimeout)) << i;
        connection.clientConnection->invalidate();
    }
}

// DISABLED: currently cannot test that wait on unopened connection causes InvalidConnection,
// since that will crash. The semantics are that isValid() == true for unopened connection,
// which doesn't make much sense.
TEST_P(ConnectionTestABBA, DISABLED_UnopenedWaitForAndDispatchImmediatelyIsInvalidConnection)
{
    IPC::Error error = a()->waitForAndDispatchImmediately<MockTestMessage1>(0, kWaitForAbsenceTimeout);
    EXPECT_EQ(IPC::Error::InvalidConnection, error);
}

TEST_P(ConnectionTestABBA, InvalidatedWaitForAndDispatchImmediatelyIsInvalidConnection)
{
    ASSERT_TRUE(openA());
    a()->invalidate();
    IPC::Error error = a()->waitForAndDispatchImmediately<MockTestMessage1>(0, kWaitForAbsenceTimeout);
    EXPECT_EQ(IPC::Error::InvalidConnection, error);
}

TEST_P(ConnectionTestABBA, UnsentWaitForAndDispatchImmediatelyIsTimeout)
{
    ASSERT_TRUE(openA());
    IPC::Error error = a()->waitForAndDispatchImmediately<MockTestMessage1>(0, kWaitForAbsenceTimeout);
    EXPECT_EQ(IPC::Error::Timeout, error);
}

template<typename C>
static void dispatchSync(RunLoop& runLoop, C&& function)
{
    BinarySemaphore semaphore;
    runLoop.dispatch([&] () mutable {
        function();
        semaphore.signal();
    });
    semaphore.wait();
}

template<typename C>
static void dispatchAndWait(RunLoop& runLoop, C&& function)
{
    std::atomic<bool> done = false;
    runLoop.dispatch([&] () mutable {
        function();
        done = true;
    });
    while (!done)
        RunLoop::currentSingleton().cycle();
}

class ConnectionRunLoopTest : public ConnectionTestABBA {
public:
    void TearDown() override
    {
        // By convention the b() connection is the one that gets openend on various runloops.
        // The API contract of Connection is that invalidate() is called on the dispatcher
        // that called open(). The test should invalidate on that runloop.
        ASSERT(!b() || !b()->client());

        ConnectionTestABBA::TearDown();
        // Remember to call localReferenceBarrier() in test scope.
        // Otherwise run loops might be executing code that uses variables
        // that went out of scope.
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
        // Since we need to send sync to create a barrier to run loops,
        // we might as well destroy the run loops in this function.
        Vector<Ref<Thread>> threadsToWait;
        // FIXME: Cannot wait for RunLoop to really exit.
        for (auto& runLoop : std::exchange(m_runLoops, { })) {
            dispatchSync(runLoop, [&] {
                threadsToWait.append(Thread::currentSingleton());
                RunLoop::currentSingleton().stop();
            });
        }
        while (true) {
            sleep(0.1_s);
            for (auto& thread : threadsToWait) {
                if (Thread::allThreads().contains(thread.get()))
                    continue;
            }
            break;
        }
    }

protected:
    Vector<Ref<RunLoop>> m_runLoops;
};

#define LOCAL_STRINGIFY(x) #x
#define RUN_LOOP_NAME "RunLoop at ConnectionTests.cpp:" LOCAL_STRINGIFY(__LINE__) ""_s

TEST_P(ConnectionRunLoopTest, RunLoopOpen)
{
    ASSERT_TRUE(openA());
    auto runLoop = createRunLoop(RUN_LOOP_NAME);
    BinarySemaphore semaphore;
    runLoop->dispatch([&] {
        ASSERT_TRUE(openB());
        bClient().waitForDidClose(kDefaultWaitForTimeout);
        semaphore.signal();
    });
    a()->invalidate();
    semaphore.wait();

    runLoop->dispatch([&] {
        b()->invalidate();
    });
    localReferenceBarrier();
}

TEST_P(ConnectionRunLoopTest, RunLoopInvalidate)
{
    ASSERT_TRUE(openA());
    auto runLoop = createRunLoop(RUN_LOOP_NAME);
    runLoop->dispatch([&] {
        ASSERT_TRUE(openB());
        b()->invalidate();
    });
    aClient().waitForDidClose(kDefaultWaitForTimeout);
    localReferenceBarrier();
}

TEST_P(ConnectionRunLoopTest, RunLoopSend)
{
    ASSERT_TRUE(openA());
    for (uint64_t i = 0u; i < 55u; ++i)
        a()->send(MockTestMessage1 { }, i);

    auto runLoop = createRunLoop(RUN_LOOP_NAME);
    BinarySemaphore semaphore;
    runLoop->dispatch([&] {
        ASSERT_TRUE(openB());
        for (uint64_t i = 100u; i < 160u; ++i)
            b()->send(MockTestMessage1 { }, i);
        for (uint64_t i = 0u; i < 55u; ++i) {
            auto message = bClient().waitForMessage(kDefaultWaitForTimeout);
            EXPECT_EQ(message.messageName, MockTestMessage1::name());
            EXPECT_EQ(message.destinationID, i);
        }
        auto flushResult = b()->flushSentMessages(kDefaultWaitForTimeout);
        EXPECT_EQ(flushResult, IPC::Error::NoError);
        b()->invalidate();
    });
    for (uint64_t i = 100u; i < 160u; ++i) {
        auto message = aClient().waitForMessage(kDefaultWaitForTimeout);
        EXPECT_EQ(message.messageName, MockTestMessage1::name());
        EXPECT_EQ(message.destinationID, i);
    }
    semaphore.signal();

    localReferenceBarrier();
}

TEST_P(ConnectionRunLoopTest, RunLoopSendAsync)
{
    ASSERT_TRUE(openA());
    aClient().setAsyncMessageHandler([&] (IPC::Connection&, IPC::Decoder& decoder) -> bool {
        auto listenerID = decoder.asyncReplyID().toUInt64();
        decoder.markHandled();
        auto encoder = makeUniqueRef<IPC::Encoder>(MockTestMessageWithAsyncReply1::asyncMessageReplyName(), listenerID);
        encoder.get() << decoder.destinationID();
        a()->sendSyncReply(WTF::move(encoder));
        return true;
    });
    HashSet<uint64_t> replies;

    auto runLoop = createRunLoop(RUN_LOOP_NAME);
    dispatchAndWait(runLoop, [&] {
        ASSERT_TRUE(openB());
        for (uint64_t i = 100u; i < 160u; ++i) {
            b()->sendWithAsyncReply(MockTestMessageWithAsyncReply1 { }, [&, j = i] (uint64_t value) {
                if (!value)
                    WTFLogAlways("GOT: %" PRIu64, j);
                EXPECT_GE(value, 100u);
                replies.add(value);
            }, i);
        }
        while (replies.size() < 60u)
            RunLoop::currentSingleton().cycle();
        b()->invalidate();
    });

    for (uint64_t i = 100u; i < 160u; ++i)
        EXPECT_TRUE(replies.contains(i));
    localReferenceBarrier();
}

// Test for ensuring that async messages are not reordered when sync message is sent with SendSyncOption::MaintainOrderingWithAsyncMessages.
// At the time message sequence of "A, Sync, B" would see "B, A, Sync" because A would be moved to sync message specific list, but
// then the scheduled async message dispatch would dispatch B.
// The order is tested by increasing the destination id.
TEST_P(ConnectionRunLoopTest, SendSyncMaintainOrderingWithAsyncMessagesOrder)
{
    ASSERT_TRUE(openA());

    auto runLoop = createRunLoop(RUN_LOOP_NAME);
    runLoop->dispatch([&] {
        // Sync message handler only to respond with "message was handled".
        bClient().setSyncMessageHandler([&](IPC::Connection& connection, IPC::Decoder& decoder, UniqueRef<IPC::Encoder>& encoder) -> bool {
            bClient().addMessage(decoder);
            connection.sendSyncReply(WTF::move(encoder));
            return true;
        });
        ASSERT_TRUE(openB());
    });
    for (uint64_t i = 0u; i < 300u; i += 3) {
        a()->send(MockTestMessage1 { }, i);
        auto result = a()->sendSync(MockTestSyncMessage(), i + 1, kDefaultWaitForTimeout, { IPC::SendSyncOption::MaintainOrderingWithAsyncMessages });
        EXPECT_TRUE(result.succeeded());
        a()->send(MockTestMessage1 { }, i + 2);
    }
    auto result = a()->sendSync(MockTestSyncMessage(), 300, kDefaultWaitForTimeout, { IPC::SendSyncOption::MaintainOrderingWithAsyncMessages });
    // The last message sent is sync, so when we are here we know all messages have been added to the message list.
    runLoop->dispatch([&] {
        b()->invalidate();
    });
    auto messages = bClient().takeMessages();
    ASSERT_EQ(messages.size(), 301u);
    for (uint64_t i = 0u; i < 300u; i += 3) {
        EXPECT_EQ(messages[i].messageName, MockTestMessage1::name()) << i;
        EXPECT_EQ(messages[i].destinationID, i);
        EXPECT_EQ(messages[i + 1].messageName, MockTestSyncMessage::name()) << (i + 1);
        EXPECT_EQ(messages[i + 1].destinationID, i + 1u);
        EXPECT_EQ(messages[i + 2].messageName, MockTestMessage1::name()) << (i + 2);
        EXPECT_EQ(messages[i + 2].destinationID, i + 2u);
    }
    EXPECT_EQ(messages[300].messageName, MockTestSyncMessage::name());
    EXPECT_EQ(messages[300].destinationID, 300u);
    localReferenceBarrier();
}

TEST_P(ConnectionRunLoopTest, SendSyncMaintainOrderingWithAsyncMessagesOrderMultipleSendingThreads)
{
    ASSERT_TRUE(openA());

    auto runLoop = createRunLoop(RUN_LOOP_NAME);
    runLoop->dispatch([&] {
        // Sync message handler only to respond with "message was handled".
        bClient().setSyncMessageHandler([&](IPC::Connection& connection, IPC::Decoder& decoder, UniqueRef<IPC::Encoder>& encoder) -> bool {
            bClient().addMessage(decoder);
            connection.sendSyncReply(WTF::move(encoder));
            sleep(0.1_s);
            return true;
        });
        bClient().setAsyncMessageHandler([](IPC::Connection&, IPC::Decoder&) -> bool {
            sleep(0.1_s);
            return false;
        });
        ASSERT_TRUE(openB());
        sleep(0.1_s);
    });

    auto sendingRunLoop = createRunLoop(RUN_LOOP_NAME);
    sendingRunLoop->dispatch([&] {
        for (uint64_t i = 0u; i < 30u; i++) {
            a()->send(MockTestMessage1 { }, i);
            sleep(0.1_s);
        }
    });

    for (uint64_t i = 0u; i < 30u; i++) {
        auto result = a()->sendSync(MockTestSyncMessage(), i, kDefaultWaitForTimeout, { IPC::SendSyncOption::MaintainOrderingWithAsyncMessages });
        EXPECT_TRUE(result.succeeded());
        sleep(0.1_s);
    }

    dispatchAndWait(sendingRunLoop, [] {
    });

    auto result = a()->sendSync(MockTestSyncMessage(), 60, kDefaultWaitForTimeout, { IPC::SendSyncOption::MaintainOrderingWithAsyncMessages });
    // The last message sent is sync, so when we are here we know all messages have been added to the message list.
    runLoop->dispatch([&] {
        b()->invalidate();
    });
    auto messages = bClient().takeMessages();
    ASSERT_EQ(messages.size(), 61u);
    std::optional<uint64_t> seenAsyncMessage;
    std::optional<uint64_t> seenSyncMessage;
    for (uint64_t i = 0u; i < 60; i++) {
        if (messages[i].messageName == MockTestMessage1::name()) {
            if (seenAsyncMessage)
                EXPECT_EQ(messages[i].destinationID, *seenAsyncMessage + 1);
            else
                EXPECT_EQ(messages[i].destinationID, 0u);
            seenAsyncMessage = messages[i].destinationID;
        } else {
            EXPECT_EQ(messages[i].messageName, MockTestSyncMessage::name());
            if (seenSyncMessage)
                EXPECT_EQ(messages[i].destinationID, *seenSyncMessage + 1);
            else
                EXPECT_EQ(messages[i].destinationID, 0u);
            seenSyncMessage = messages[i].destinationID;
        }
    }
    EXPECT_EQ(messages[60].messageName, MockTestSyncMessage::name());
    EXPECT_EQ(messages[60].destinationID, 60u);
    localReferenceBarrier();
}

// Test for ensuring that async messages are not reordered when sync message is sent with SendSyncOption::MaintainOrderingWithAsyncMessages
// and the async messages are waited with waitForAndDispatchImmediately.
TEST_P(ConnectionRunLoopTest, SendSyncMaintainOrderingWithAsyncMessagesWaitForOrderMultipleSendingThreads)
{
    ASSERT_TRUE(openA());

    auto runLoop = createRunLoop(RUN_LOOP_NAME);
    runLoop->dispatch([&] {
        // Sync message handler only to respond with "message was handled".
        bClient().setSyncMessageHandler([&](IPC::Connection& connection, IPC::Decoder& decoder, UniqueRef<IPC::Encoder>& encoder) -> bool {
            bClient().addMessage(decoder);
            connection.sendSyncReply(WTF::move(encoder));
            sleep(0.1_s);
            return true;
        });
        ASSERT_TRUE(openB());

        // The messages the test sends are never dispatched from run loop, rather from this wait.
        // Since we never drop to run loop after open, we catch even the first message.
        for (uint64_t i = 0u; i < 30u; i += 3) {
            auto result = b()->waitForAndDispatchImmediately<MockTestMessage1>(i, kDefaultWaitForTimeout);
            EXPECT_TRUE(result == IPC::Error::NoError);
        }
    });

    auto sendingRunLoop = createRunLoop(RUN_LOOP_NAME);
    sendingRunLoop->dispatch([&] {
        for (uint64_t i = 0u; i < 30u; i++) {
            a()->send(MockTestMessage1 { }, i);
            sleep(0.1_s);
        }
    });

    for (uint64_t i = 0u; i < 30u; i++) {
        auto result = a()->sendSync(MockTestSyncMessage(), i, kDefaultWaitForTimeout, { IPC::SendSyncOption::MaintainOrderingWithAsyncMessages });
        EXPECT_TRUE(result.succeeded());
        sleep(0.1_s);
    }

    dispatchAndWait(sendingRunLoop, [] {
    });

    auto result = a()->sendSync(MockTestSyncMessage(), 60, kDefaultWaitForTimeout, { IPC::SendSyncOption::MaintainOrderingWithAsyncMessages });
    // The last message sent is sync, so when we are here we know all messages have been added to the message list.
    runLoop->dispatch([&] {
        b()->invalidate();
    });
    auto messages = bClient().takeMessages();
    ASSERT_EQ(messages.size(), 61u);
    std::optional<uint64_t> seenAsyncMessage;
    std::optional<uint64_t> seenSyncMessage;
    for (uint64_t i = 0u; i < 60; i++) {
        if (messages[i].messageName == MockTestMessage1::name()) {
            if (seenAsyncMessage)
                EXPECT_EQ(messages[i].destinationID, *seenAsyncMessage + 1);
            else
                EXPECT_EQ(messages[i].destinationID, 0u);
            seenAsyncMessage = messages[i].destinationID;
        } else {
            EXPECT_EQ(messages[i].messageName, MockTestSyncMessage::name());
            if (seenSyncMessage)
                EXPECT_EQ(messages[i].destinationID, *seenSyncMessage + 1);
            else
                EXPECT_EQ(messages[i].destinationID, 0u);
            seenSyncMessage = messages[i].destinationID;
        }
    }
    EXPECT_EQ(messages[60].messageName, MockTestSyncMessage::name());
    EXPECT_EQ(messages[60].destinationID, 60u);
    localReferenceBarrier();
}

TEST_P(ConnectionRunLoopTest, RunLoopSendAsyncOnTarget)
{
    HashSet<uint64_t> replies;
    {
        AutoWorkQueue awq;

        ASSERT_TRUE(openA());
        aClient().setAsyncMessageHandler([&] (IPC::Connection&, IPC::Decoder& decoder) -> bool {
            auto listenerID = decoder.asyncReplyID().toUInt64();
            decoder.markHandled();
            auto encoder = makeUniqueRef<IPC::Encoder>(MockTestMessageWithAsyncReply1::asyncMessageReplyName(), listenerID);
            encoder.get() << decoder.destinationID();
            a()->sendSyncReply(WTF::move(encoder));
            return true;
        });

        auto runLoop = createRunLoop(RUN_LOOP_NAME);
        dispatchAndWait(runLoop, [&] {
            ASSERT_TRUE(openB());
            for (uint64_t i = 100u; i < 160u; ++i) {
                b()->sendWithAsyncReplyOnDispatcher(MockTestMessageWithAsyncReply1 { }, awq.queue(), [&, j = i, queue = awq.queue()] (uint64_t value) {
                    assertIsCurrent(queue);
                    if (!value)
                        WTFLogAlways("GOT: %" PRIu64, j);
                    EXPECT_GE(value, 100u);
                    replies.add(value);
                }, i);
            }
            while (replies.size() < 60u)
                RunLoop::currentSingleton().cycle();
            b()->invalidate();
        });
        awq.queue()->beginShutdown();
    }
    for (uint64_t i = 100u; i < 160u; ++i)
        EXPECT_TRUE(replies.contains(i));
    localReferenceBarrier();
}

TEST_P(ConnectionRunLoopTest, RunLoopSendWithPromisedReply)
{
    ASSERT_TRUE(openA());
    aClient().setAsyncMessageHandler([&] (IPC::Connection&, IPC::Decoder& decoder) -> bool {
        auto listenerID = decoder.asyncReplyID().toUInt64();
        decoder.markHandled();
        auto encoder = makeUniqueRef<IPC::Encoder>(MockTestMessageWithAsyncReply1::asyncMessageReplyName(), listenerID);
        encoder.get() << decoder.destinationID();
        a()->sendSyncReply(WTF::move(encoder));
        return true;
    });
    HashSet<uint64_t> replies;

    auto runLoop = createRunLoop(RUN_LOOP_NAME);
    dispatchAndWait(runLoop, [&] {
        ASSERT_TRUE(openB());
        for (uint64_t i = 100u; i < 160u; ++i) {
            b()->sendWithPromisedReply(MockTestMessageWithAsyncReply1 { }, i)->then(runLoop,
                [&, j = i] (uint64_t value) {
                    if (!value)
                        WTFLogAlways("GOT: %" PRIu64, j);
                    EXPECT_GE(value, 100u);
                    replies.add(value);
                },
                [] {
                    // There should never be a connection failure in this case.
                    EXPECT_TRUE(false);
                });
        }
        while (replies.size() < 60u)
            RunLoop::currentSingleton().cycle();
        b()->invalidate();
    });

    for (uint64_t i = 100u; i < 160u; ++i)
        EXPECT_TRUE(replies.contains(i));
    localReferenceBarrier();
}

struct PromiseConverter {
    static auto convertError(IPC::Error)
    {
        return makeUnexpected(String { "2"_s });
    }
};

TEST_P(ConnectionRunLoopTest, SendWithConvertedPromisedReply)
{
    ASSERT_TRUE(openA());
    aClient().setAsyncMessageHandler([&] (IPC::Connection&, IPC::Decoder& decoder) -> bool {
        auto listenerID = decoder.asyncReplyID().toUInt64();
        decoder.markHandled();
        auto encoder = makeUniqueRef<IPC::Encoder>(MockTestMessageWithAsyncReply1::asyncMessageReplyName(), listenerID);
        encoder.get() << decoder.destinationID();
        a()->sendSyncReply(WTF::move(encoder));
        return true;
    });
    std::atomic<bool> isFinished = false;

    auto runLoop = createRunLoop(RUN_LOOP_NAME);
    dispatchAndWait(runLoop, [&] {
        ASSERT_TRUE(openB());
        b()->sendWithPromisedReply<PromiseConverter>(MockTestMessageWithAsyncReply1 { }, 1)->then(runLoop, [&] (uint64_t value) {
            EXPECT_EQ(value, 1u);
            isFinished = true;
        }, [&] (String&& error) {
            EXPECT_EQ(error, "2"_s);
            isFinished = true;
        });
        while (!isFinished)
            RunLoop::currentSingleton().cycle();
        b()->invalidate();
    });

    localReferenceBarrier();
}

TEST_P(ConnectionRunLoopTest, RunLoopSendWithPromisedReplyOnMixAndMatchDispatcher)
{
    HashSet<uint64_t> replies;

    {
        AutoWorkQueue awq;
        ASSERT_TRUE(openA());
        aClient().setAsyncMessageHandler([&] (IPC::Connection&, IPC::Decoder& decoder) -> bool {
            auto listenerID = decoder.asyncReplyID().toUInt64();
            decoder.markHandled();
            auto encoder = makeUniqueRef<IPC::Encoder>(MockTestMessageWithAsyncReply1::asyncMessageReplyName(), listenerID);
            encoder.get() << decoder.destinationID();
            a()->sendSyncReply(WTF::move(encoder));
            return true;
        });

        auto runLoop = createRunLoop(RUN_LOOP_NAME);
        dispatchAndWait(runLoop, [&] {
            ASSERT_TRUE(openB());
            for (uint64_t i = 100u; i < 160u; ++i) {
                b()->sendWithPromisedReply(MockTestMessageWithAsyncReply1 { }, i)->whenSettled(runLoop, [&, j = i] (auto&& result) {
                    EXPECT_TRUE(result);
                    auto value = *result;
                    if (!value)
                        WTFLogAlways("GOT: %" PRIu64, j);
                    EXPECT_GE(value, 100u);
                    replies.add(value);
                });
            }
            while (replies.size() < 60u)
                RunLoop::currentSingleton().cycle();
            b()->invalidate();
        });
        awq.queue()->beginShutdown();
    }
    for (uint64_t i = 100u; i < 160u; ++i)
        EXPECT_TRUE(replies.contains(i));
    localReferenceBarrier();
}

// Tests that all sent messages with async replies are received, even if sender invalidates
// without synchronizing with the receiver. The async reply callbacks are always run, either
// with the reply or the cancel value and always on the provided dispatcher
TEST_P(ConnectionRunLoopTest, SendAsyncAndInvalidateOnDispatcher)
{
    HashSet<uint64_t> messages;
    HashSet<uint64_t> replies;
    constexpr uint64_t messageCount = 1536u;

    {
        AutoWorkQueue awq;

        ASSERT_TRUE(openA());
        auto runLoop = createRunLoop(RUN_LOOP_NAME);
        BinarySemaphore semaphore;
        runLoop->dispatch([&] {
            bClient().setAsyncMessageHandler([&] (IPC::Connection&, IPC::Decoder& decoder) -> bool {
                auto listenerID = decoder.asyncReplyID().toUInt64();
                decoder.markHandled();
                auto encoder = makeUniqueRef<IPC::Encoder>(MockTestMessageWithAsyncReply1::asyncMessageReplyName(), listenerID);
                encoder.get() << decoder.destinationID();
                b()->sendSyncReply(WTF::move(encoder));
                messages.add(decoder.destinationID());
                return true;
            });
            ASSERT_TRUE(openB());
            while (messages.size() < messageCount - 1)
                RunLoop::currentSingleton().cycle();
            semaphore.signal();
        });
        for (uint64_t i = 1u; i < messageCount; ++i) {
            a()->sendWithAsyncReplyOnDispatcher(MockTestMessageWithAsyncReply1 { }, awq.queue(), [&, j = i] (uint64_t) {
                assertIsCurrent(awq.queue());
                // The reply value might be the reply value (destinationID) or zero in case the
                // reply was resolved as invalid. Either of these are expected valid results.
                // Use the `j` to prove that reply callback was run as expected.
                replies.add(j);
            }, i);
        }
        auto flushResult = a()->flushSentMessages(kDefaultWaitForTimeout);
        EXPECT_EQ(flushResult, IPC::Error::NoError);
        a()->invalidate();
        semaphore.wait();

        // Sending a message on an invalidated Connection should still deliver the message on the dispatcher.
        a()->sendWithAsyncReplyOnDispatcher(MockTestMessageWithAsyncReply1 { }, awq.queue(), [queue = awq.queue()] (uint64_t) {
            assertIsCurrent(queue);
            queue->beginShutdown();
        }, 0);
        runLoop->dispatch([&] {
            b()->invalidate();
        });
    }

    for (uint64_t i = 1u; i < messageCount; ++i) {
        EXPECT_TRUE(replies.contains(i)) << i;
        EXPECT_TRUE(messages.contains(i)) << i;
    }
    localReferenceBarrier();
}

// Tests that all sent messages are received, even if sender invalidates
// without synchronizing with the receiver.
// FIXME when rdar://159131152 is resolved
#if PLATFORM(IOS)
TEST_P(ConnectionRunLoopTest, DISABLED_SendAndInvalidate)
#else
TEST_P(ConnectionRunLoopTest, SendAndInvalidate)
#endif
{
    constexpr uint64_t messageCount = 1777;
    ASSERT_TRUE(openA());
    auto runLoop = createRunLoop(RUN_LOOP_NAME);
    BinarySemaphore semaphore;
    runLoop->dispatch([&] {
        ASSERT_TRUE(openB());
        for (uint64_t i = 1u; i < messageCount; ++i) {
            auto message = bClient().waitForMessage(kDefaultWaitForTimeout);
            EXPECT_EQ(message.messageName, MockTestMessage1::name());
            EXPECT_EQ(message.destinationID, i);
        }
        semaphore.signal();
    });
    for (uint64_t i = 1u; i < messageCount; ++i)
        a()->send(MockTestMessage1 { }, i);
    auto flushResult = a()->flushSentMessages(kDefaultWaitForTimeout);
    EXPECT_EQ(flushResult, IPC::Error::NoError);
    a()->invalidate();
    semaphore.wait();
    runLoop->dispatch([&] {
        b()->invalidate();
    });
    localReferenceBarrier();
}

// Tests that all sent messages with async replies are received, even if sender invalidates
// without synchronizing with the receiver. The async reply callbacks are always run, either
// with the reply or the cancel value.
TEST_P(ConnectionRunLoopTest, SendAsyncAndInvalidate)
{
    constexpr uint64_t messageCount = 1536u;
    ASSERT_TRUE(openA());
    auto runLoop = createRunLoop(RUN_LOOP_NAME);
    HashSet<uint64_t> messages;
    HashSet<uint64_t> replies;
    BinarySemaphore semaphore;
    runLoop->dispatch([&] {
        bClient().setAsyncMessageHandler([&] (IPC::Connection&, IPC::Decoder& decoder) -> bool {
            auto listenerID = decoder.asyncReplyID().toUInt64();
            decoder.markHandled();
            auto encoder = makeUniqueRef<IPC::Encoder>(MockTestMessageWithAsyncReply1::asyncMessageReplyName(), listenerID);
            encoder.get() << decoder.destinationID();
            b()->sendSyncReply(WTF::move(encoder));
            messages.add(decoder.destinationID());
            return true;
        });
        ASSERT_TRUE(openB());
        while (messages.size() < messageCount - 1)
            RunLoop::currentSingleton().cycle();
        semaphore.signal();
    });
    for (uint64_t i = 1u; i < messageCount; ++i) {
        a()->sendWithAsyncReply(MockTestMessageWithAsyncReply1 { }, [&, j = i] (uint64_t) {
            // The reply value might be the reply value (destinationID) or zero in case the
            // reply was resolved as invalid. Either of these are expected valid results.
            // Use the `j` to prove that reply callback was run as expected.
            replies.add(j);
        }, i);
    }
    auto flushResult = a()->flushSentMessages(kDefaultWaitForTimeout);
    EXPECT_EQ(flushResult, IPC::Error::NoError);
    a()->invalidate();
    semaphore.wait();
    for (uint64_t i = 1u; i < messageCount; ++i) {
        EXPECT_TRUE(replies.contains(i)) << i;
        EXPECT_TRUE(messages.contains(i)) << i;
    }
    runLoop->dispatch([&] {
        b()->invalidate();
    });
    localReferenceBarrier();
}

// Ensure that replies are received in the right order.
TEST_P(ConnectionRunLoopTest, RunLoopSendWithPromisedReplyOrder)
{
    using Promise = MockTestMessageWithAsyncReply1::Promise;

    ASSERT_TRUE(openA());
    uint64_t replyID = 0;
    aClient().setAsyncMessageHandler([&] (IPC::Connection&, IPC::Decoder& decoder) -> bool {
        auto listenerID = decoder.asyncReplyID().toUInt64();
        decoder.markHandled();
        auto encoder = makeUniqueRef<IPC::Encoder>(MockTestMessageWithAsyncReply1::asyncMessageReplyName(), listenerID);
        encoder.get() << replyID++;
        a()->sendSyncReply(WTF::move(encoder));
        return true;
    });
    Vector<uint64_t> replies;
    constexpr size_t counter = 50;
    replies.reserveInitialCapacity(counter);

    auto runLoop = createRunLoop(RUN_LOOP_NAME);
    dispatchAndWait(runLoop, [&] {
        ASSERT_TRUE(openB());
        for (uint64_t i = 0; i < counter; ++i) {
            if (!(i % 2)) {
                b()->sendWithPromisedReply(MockTestMessageWithAsyncReply1 { }, 100)->whenSettled(runLoop, [&, i] (Promise::Result result) {
                    EXPECT_TRUE(result.has_value());
                    EXPECT_EQ(result.value(), i);
                    replies.append(i);
                });
            } else {
                b()->sendWithAsyncReply(MockTestMessageWithAsyncReply1 { }, [&, i] (uint64_t value) {
                    EXPECT_EQ(value, i);
                    replies.append(i);
                }, 100);
            }
        }
        while (replies.size() < counter)
            RunLoop::currentSingleton().cycle();
        b()->invalidate();
    });

    for (uint64_t i = 0u; i < counter; ++i)
        EXPECT_EQ(replies[i], i);
    runLoop->dispatch([&] {
        b()->invalidate();
    });
    localReferenceBarrier();
}

// This API contract does not make sense. Not only that, but there is no good way currently
// to capture this in a thread-safe way (construct completion handler in a thread-safe way
// so that it would assert that it would execute in the run loop thread). This is disabled
// until the API contract is changed.
TEST_P(ConnectionRunLoopTest, DISABLED_RunLoopSendAsyncOnAnotherRunLoopDispatchesOnConnectionRunLoop)
{
    ASSERT_TRUE(openA());
    aClient().setAsyncMessageHandler([&] (IPC::Connection&, IPC::Decoder& decoder) -> bool {
        auto listenerID = decoder.asyncReplyID().toUInt64();
        decoder.markHandled();
        auto encoder = makeUniqueRef<IPC::Encoder>(MockTestMessageWithAsyncReply1::asyncMessageReplyName(), listenerID);
        encoder.get() << decoder.destinationID();
        a()->sendSyncReply(WTF::move(encoder));
        return true;
    });
    HashSet<uint64_t> replies;

    auto runLoop = createRunLoop(RUN_LOOP_NAME);
    dispatchSync(runLoop, [&] {
        ASSERT_TRUE(openB());
    });

    BinarySemaphore semaphore;
    auto otherRunLoop = createRunLoop(RUN_LOOP_NAME);
    otherRunLoop->dispatch([&] {
        for (uint64_t i = 100u; i < 160u; ++i) {
            b()->sendWithAsyncReply(MockTestMessageWithAsyncReply1 { }, [&] (uint64_t value) {
                EXPECT_GE(value, 100u);
                // These should be dispatched on `runLoop` above, which does not make much sense.
                replies.add(value);
            }, i);
        }
        // Halt the runloop for a proof that the async replies are not processed on
        // this run loop.
        semaphore.wait();
    });
    dispatchAndWait(runLoop, [&] {
        while (replies.size() < 60u)
            RunLoop::currentSingleton().cycle();
    });

    for (uint64_t i = 100u; i < 160u; ++i)
        EXPECT_TRUE(replies.contains(i));
    semaphore.signal();
    runLoop->dispatch([&] {
        b()->invalidate();
    });
    localReferenceBarrier();
}

// This makes no sense:
//  - async reply handlers are dispatched on the connection run loop
//  - async reply handlers are dispatched as cancelled on connection run loop during invalidate
//  - async reply handlers that are sent to already invalid connection are dispatched on main run loop
// We have to make the discrepancy as the Connection is not bound to any run loop if it is invalid, e.g.
// prior to open() and after invalidate().
// Previously Connection was bound only to main run loop. In that scenario also the invalid send could cancel the reply handler
// on main run loop, as that is guaranteed to exist. After Connection could be bound to an arbitrary run loop, we cannot
// cancel the reply handler on a run loop we do not know about.
// Will be fixed later. Likely this needs an API contract change, where async reply handlers are dispatched on the
// calling run loop.
TEST_P(ConnectionRunLoopTest, InvalidSendWithAsyncReplyDispatchesCancelHandlerOnMainThread)
{
    ASSERT_TRUE(openA());
    auto runLoop = createRunLoop(RUN_LOOP_NAME);
    uint64_t reply = 1u;
    BinarySemaphore semaphore;
    runLoop->dispatch([&] {
        ASSERT_TRUE(openB());
        b()->invalidate();
        b()->sendWithAsyncReply(MockTestMessageWithAsyncReply1 { }, [&] (uint64_t value) {
            reply = value;
            }, 77);
        // Halt the runloop for a proof that the async replies are not processed on
        // this run loop.
        semaphore.wait();
    });
    EXPECT_EQ(reply, 1u);
    while (reply == 1u)
        RunLoop::currentSingleton().cycle();
    EXPECT_EQ(reply, 0u);
    semaphore.signal();
    runLoop->dispatch([&] {
        b()->invalidate();
    });
    localReferenceBarrier();
}

TEST_P(ConnectionRunLoopTest, RunLoopWaitForAndDispatchImmediately)
{
    ASSERT_TRUE(openA());

    for (uint64_t i = 0u; i < 55u; ++i)
        a()->send(MockTestMessage1 { }, i);

    auto runLoop = createRunLoop(RUN_LOOP_NAME);
    runLoop->dispatch([&] {
        ASSERT_TRUE(openB());
        for (uint64_t i = 100u; i < 160u; ++i)
            b()->send(MockTestMessage1 { }, i);

        for (uint64_t i = 0u; i < 55u; ++i) {
            IPC::Error error = b()->waitForAndDispatchImmediately<MockTestMessage1>(i, kDefaultWaitForTimeout);
            ASSERT_EQ(IPC::Error::NoError, error);

            auto message = bClient().waitForMessage(0_s);
            EXPECT_EQ(message.messageName, MockTestMessage1::name());
            EXPECT_EQ(message.destinationID, i);
        }
    });
    for (uint64_t i = 100u; i < 160u; ++i) {
        IPC::Error error = a()->waitForAndDispatchImmediately<MockTestMessage1>(i, kDefaultWaitForTimeout);
        ASSERT_EQ(IPC::Error::NoError, error);

        auto message = aClient().waitForMessage(0_s);
        EXPECT_EQ(message.messageName, MockTestMessage1::name());
        EXPECT_EQ(message.destinationID, i);
    }
    runLoop->dispatch([&] {
        b()->invalidate();
    });
    localReferenceBarrier();
}

#if PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 260000
TEST_P(ConnectionRunLoopTest, DISABLED_SendLocalSyncMessageWithDataReply)
#else
TEST_P(ConnectionRunLoopTest, SendLocalSyncMessageWithDataReply)
#endif
{
    constexpr int iterations = 5;
    constexpr size_t dataSize = 1e8; // 100 MB.
    ASSERT_TRUE(openA());
    auto runLoop = createRunLoop(RUN_LOOP_NAME);
    runLoop->dispatch([&] {
        bClient().setSyncMessageHandler([&](IPC::Connection&, IPC::Decoder& decoder, UniqueRef<IPC::Encoder>& encoder) -> bool {
            Vector<uint8_t> data(dataSize);
            for (size_t i = 0; i < dataSize; ++i)
                data[i] = static_cast<uint8_t>(i);
            encoder.get() << data;
            b()->sendSyncReply(WTF::move(encoder));
            return true;
        });
        ASSERT_TRUE(openB());
    });
    for (int i = 0; i < iterations; ++i) {
        auto sendResult = a()->sendSync(MockTestSyncMessageWithDataReply { }, i, kDefaultWaitForTimeout);
        ASSERT_TRUE(sendResult.succeeded());
        auto& [replyData] = sendResult.reply();
        for (size_t i = 0; i < replyData.size(); ++i) {
            auto expected = static_cast<uint8_t>(i);
            if (expected != replyData[i])
                ASSERT_EQ(expected, replyData[i]);
        }
        ASSERT_EQ(dataSize, replyData.size());
    }
    runLoop->dispatch([&] {
        b()->invalidate();
    });
    localReferenceBarrier();
}

// Tests that unhandled sync message is cancelled. IPC::Connection receiving unhandled messages.
TEST_P(ConnectionRunLoopTest, SyncMessageNotHandledIsCancelled)
{
    constexpr size_t iterations = 10;
    ASSERT_TRUE(openA());
    auto runLoop = createRunLoop(RUN_LOOP_NAME);
    uint64_t gotDestination = 0;
    runLoop->dispatch([&] {
        bClient().setSyncMessageHandler([&](IPC::Connection&, IPC::Decoder& decoder, UniqueRef<IPC::Encoder>& encoder) -> bool {
            gotDestination = decoder.destinationID();
            // Unhandled message.
            if (decoder.destinationID() == 77)
                return true; // Message destination was unknown, unhandled message.
            if (decoder.destinationID() == 99) {
                b()->sendSyncReply(WTF::move(encoder));
                return true;
            }
            EXPECT_TRUE(false);
            return false;
        });
        ASSERT_TRUE(openB());
    });
    for (size_t i = 0; i < iterations; ++i) {
        {
            gotDestination = 0;
            auto result = a()->sendSync(MockTestSyncMessage(), 77, kDefaultWaitForTimeout);
            ASSERT_FALSE(result.succeeded());
            EXPECT_EQ(IPC::Error::SyncMessageCancelled, result.error());
            EXPECT_EQ(77u, gotDestination);
        }
        {
            gotDestination = 0;
            auto result = a()->sendSync(MockTestSyncMessage(), 99, kDefaultWaitForTimeout);
            EXPECT_TRUE(result.succeeded());
            EXPECT_EQ(99u, gotDestination);
        }
    }
    runLoop->dispatch([&] {
        b()->invalidate();
    });
    localReferenceBarrier();
}

#if ENABLE(IPC_TESTING_API)
// Tests that sync message with decode failure is cancelled. IPC::Connection does not allow these,
// but JS IPC Testing API uses these.
TEST_P(ConnectionRunLoopTest, SyncMessageDecodeFailureIsCancelled)
{
    constexpr size_t iterations = 10;
    ASSERT_TRUE(openA());
    auto runLoop = createRunLoop(RUN_LOOP_NAME);
    uint64_t gotDestination = 0;
    runLoop->dispatch([&] {
        b()->setIgnoreInvalidMessageForTesting();
        bClient().setSyncMessageHandler([&](IPC::Connection&, IPC::Decoder& decoder, UniqueRef<IPC::Encoder>& encoder) -> bool {
            gotDestination = decoder.destinationID();
            // Decode failure.
            if (decoder.destinationID() == 88) {
                EXPECT_FALSE(decoder.decode<uint64_t>());
                return true; // Message was handled, but decode failed.
            }
            if (decoder.destinationID() == 99) {
                b()->sendSyncReply(WTF::move(encoder));
                return true;
            }
            EXPECT_TRUE(false);
            return false;
        });
        ASSERT_TRUE(openB());
    });
    for (size_t i = 0; i < iterations; ++i) {
        {
            gotDestination = 0;
            auto result = a()->sendSync(MockTestSyncMessage(), 88, IPC::Timeout::infinity());
            ASSERT_FALSE(result.succeeded());
            EXPECT_EQ(IPC::Error::SyncMessageCancelled, result.error());
            EXPECT_EQ(88u, gotDestination);
        }
        {
            gotDestination = 0;
            auto result = a()->sendSync(MockTestSyncMessage(), 99, IPC::Timeout::infinity());
            EXPECT_TRUE(result.succeeded());
            EXPECT_EQ(99u, gotDestination);
        }
    }
    runLoop->dispatch([&] {
        b()->invalidate();
    });
    localReferenceBarrier();
}
#endif

// Async message handler that replies with the destination ID, except for messages to destination 77 which it
// leaves unhandled.
static bool replyUnlessDestination77(IPC::Connection& connection, IPC::Decoder& decoder)
{
    if (decoder.destinationID() == 77)
        return true; // Message destination was unknown, unhandled message.
    if (decoder.destinationID() == 99) {
        auto listenerID = decoder.asyncReplyID().toUInt64();
        decoder.markHandled();
        auto encoder = makeUniqueRef<IPC::Encoder>(MockTestMessageWithAsyncReply1::asyncMessageReplyName(), listenerID);
        encoder.get() << decoder.destinationID();
        connection.sendSyncReply(WTF::move(encoder));
        return true;
    }
    EXPECT_TRUE(false);
    return false;
}

// Tests that unhandled async message with reply is cancelled. IPC::Connection receiving unhandled messages.
TEST_P(ConnectionRunLoopTest, AsyncWithReplyNotHandledIsCancelled)
{
    constexpr size_t iterations = 10;
    ASSERT_TRUE(openA());
    aClient().setAsyncMessageHandler(replyUnlessDestination77);
    Vector<std::pair<uint64_t, uint64_t>> replies;

    auto runLoop = createRunLoop(RUN_LOOP_NAME);
    dispatchAndWait(runLoop, [&] {
        ASSERT_TRUE(openB());
        auto send = [&](uint64_t destinationID) {
            b()->sendWithAsyncReply(MockTestMessageWithAsyncReply1 { }, [&, destinationID](uint64_t value) {
                replies.append({ destinationID, value });
            }, destinationID);
        };
        for (size_t i = 0; i < iterations; ++i) {
            send(77);
            send(99);
        }
        while (replies.size() < 2 * iterations)
            RunLoop::currentSingleton().cycle();
        b()->invalidate();
    });

    // Cancellations and replies are delivered in the send order.
    ASSERT_EQ(replies.size(), 2 * iterations);
    for (size_t i = 0; i < replies.size(); i += 2) {
        EXPECT_EQ(replies[i], (std::pair<uint64_t, uint64_t> { 77, 0 })); // Cancelled.
        EXPECT_EQ(replies[i + 1], (std::pair<uint64_t, uint64_t> { 99, 99 }));
    }
    localReferenceBarrier();
}

// Tests that unhandled async message with reply is cancelled when the reply handler is run on a dispatcher.
TEST_P(ConnectionRunLoopTest, AsyncWithReplyOnDispatcherNotHandledIsCancelled)
{
    constexpr uint64_t iterations = 10;
    std::atomic<uint64_t> cancelCount = 0;
    std::atomic<uint64_t> replyCount = 0;
    {
        AutoWorkQueue awq;

        ASSERT_TRUE(openA());
        aClient().setAsyncMessageHandler(replyUnlessDestination77);

        auto runLoop = createRunLoop(RUN_LOOP_NAME);
        dispatchAndWait(runLoop, [&] {
            ASSERT_TRUE(openB());
            auto send = [&](uint64_t destinationID) {
                b()->sendWithAsyncReplyOnDispatcher(MockTestMessageWithAsyncReply1 { }, awq.queue(), [&, destinationID, queue = awq.queue()](uint64_t value) {
                    assertIsCurrent(queue);
                    if (destinationID == 77) {
                        EXPECT_EQ(value, 0u); // Cancelled.
                        ++cancelCount;
                    } else {
                        EXPECT_EQ(value, 99u);
                        ++replyCount;
                    }
                }, destinationID);
            };
            for (uint64_t i = 0; i < iterations; ++i) {
                send(77);
                send(99);
            }
            while (cancelCount < iterations || replyCount < iterations)
                RunLoop::currentSingleton().cycle();
            b()->invalidate();
        });
        awq.queue()->beginShutdown();
    }
    EXPECT_EQ(cancelCount, iterations);
    EXPECT_EQ(replyCount, iterations);
    localReferenceBarrier();
}

// Tests that unhandled async message with promised reply is rejected.
TEST_P(ConnectionRunLoopTest, PromisedReplyNotHandledIsRejected)
{
    constexpr uint64_t iterations = 10;
    ASSERT_TRUE(openA());
    aClient().setAsyncMessageHandler(replyUnlessDestination77);
    uint64_t rejectCount = 0;
    uint64_t replyCount = 0;

    auto runLoop = createRunLoop(RUN_LOOP_NAME);
    dispatchAndWait(runLoop, [&] {
        ASSERT_TRUE(openB());
        auto send = [&](uint64_t destinationID) {
            b()->sendWithPromisedReply(MockTestMessageWithAsyncReply1 { }, destinationID)->whenSettled(runLoop, [&, destinationID](MockTestMessageWithAsyncReply1::Promise::Result result) {
                if (destinationID == 77) {
                    EXPECT_FALSE(result.has_value());
                    if (!result.has_value())
                        EXPECT_EQ(result.error(), IPC::Error::AsyncReplyCancelled);
                    ++rejectCount;
                } else {
                    EXPECT_TRUE(result.has_value());
                    if (result.has_value())
                        EXPECT_EQ(*result, 99u);
                    ++replyCount;
                }
            });
        };
        for (uint64_t i = 0; i < iterations; ++i) {
            send(77);
            send(99);
        }
        while (rejectCount < iterations || replyCount < iterations)
            RunLoop::currentSingleton().cycle();
        b()->invalidate();
    });

    EXPECT_EQ(rejectCount, iterations);
    EXPECT_EQ(replyCount, iterations);
    localReferenceBarrier();
}

// Tests that waiting for an async reply completes when the reply is cancelled.
TEST_P(ConnectionRunLoopTest, WaitForAsyncReplyAndDispatchImmediatelyNotHandledIsCancelled)
{
    ASSERT_TRUE(openA());
    aClient().setAsyncMessageHandler(replyUnlessDestination77);

    auto runLoop = createRunLoop(RUN_LOOP_NAME);
    dispatchAndWait(runLoop, [&] {
        ASSERT_TRUE(openB());
        for (uint64_t destinationID : std::array<uint64_t, 2> { 77, 99 }) {
            std::optional<uint64_t> reply;
            auto replyID = b()->sendWithAsyncReply(MockTestMessageWithAsyncReplyCanDispatchOutOfOrder { }, [&](uint64_t value) {
                reply = value;
            }, destinationID);
            ASSERT_TRUE(replyID);
            auto error = b()->waitForAsyncReplyAndDispatchImmediately<MockTestMessageWithAsyncReplyCanDispatchOutOfOrder>(*replyID, kDefaultWaitForTimeout);
            if (destinationID == 77) {
                EXPECT_EQ(error, IPC::Error::AsyncReplyCancelled);
                EXPECT_EQ(reply.value_or(1), 0u); // Cancelled.
            } else {
                EXPECT_EQ(error, IPC::Error::NoError);
                EXPECT_EQ(reply.value_or(1), 99u);
            }
        }
        b()->invalidate();
    });
    localReferenceBarrier();
}

// Tests that unhandled async message with reply that is dispatched with waitForAndDispatchImmediately is cancelled.
TEST_P(ConnectionRunLoopTest, WaitForAndDispatchImmediatelyAsyncWithReplyNotHandledIsCancelled)
{
    ASSERT_TRUE(openA());
    aClient().setAsyncMessageHandler([](IPC::Connection&, IPC::Decoder&) {
        return true; // Message destination was unknown, unhandled message.
    });
    uint64_t reply = 1;
    std::atomic<bool> gotReply = false;

    auto runLoop = createRunLoop(RUN_LOOP_NAME);
    runLoop->dispatch([&] {
        ASSERT_TRUE(openB());
        b()->sendWithAsyncReply(MockTestMessageWithAsyncReplyCanDispatchOutOfOrder { }, [&](uint64_t value) {
            reply = value;
            gotReply = true;
        }, 77);
    });
    auto error = a()->waitForAndDispatchImmediately<MockTestMessageWithAsyncReplyCanDispatchOutOfOrder>(77, kDefaultWaitForTimeout);
    EXPECT_EQ(error, IPC::Error::NoError);
    while (!gotReply)
        RunLoop::currentSingleton().cycle();
    EXPECT_EQ(reply, 0u); // Cancelled.

    runLoop->dispatch([&] {
        b()->invalidate();
    });
    localReferenceBarrier();
}

#undef RUN_LOOP_NAME
#undef LOCAL_STRINGIFY

// A message check failing in a receiver that is dispatched on a work queue rather than on the
// connection's client run loop must still be reported to the client, so that the sender can be
// terminated.
class MockMessageCheckingWorkQueueMessageReceiver final : public IPC::WorkQueueMessageReceiver<WTF::DestructionThread::Any> {
public:
    static Ref<MockMessageCheckingWorkQueueMessageReceiver> create() { return adoptRef(*new MockMessageCheckingWorkQueueMessageReceiver); }

private:
    MockMessageCheckingWorkQueueMessageReceiver() = default;

    void didReceiveMessage(IPC::Connection& connection, IPC::Decoder&) final
    {
        connection.markCurrentlyDispatchedMessageAsInvalid("work queue message check"_s);
    }
};

TEST_F(ConnectionTest, WorkQueueMessageReceiverMessageCheckIsReported)
{
    ASSERT_TRUE(openBoth());

    auto receiverName = IPC::receiverName(MockTestMessage1::name());
    Ref workQueue = WorkQueue::create("MockMessageCheckingWorkQueueMessageReceiver"_s);
    Ref receiver = MockMessageCheckingWorkQueueMessageReceiver::create();
    server()->addWorkQueueMessageReceiver(receiverName, workQueue, receiver);

    for (uint64_t i = 100u; i < 160u; ++i)
        client()->send(MockTestMessage1 { }, i);

    for (uint64_t i = 100u; i < 160u; ++i) {
        auto invalidMessage = serverClient().waitForInvalidMessage(kDefaultWaitForTimeout);
        EXPECT_EQ(invalidMessage, MockTestMessage1::name());
    }

    server()->removeWorkQueueMessageReceiver(receiverName);
}

class MockIgnoringWorkQueueMessageReceiver final : public IPC::WorkQueueMessageReceiver<WTF::DestructionThread::Any> {
public:
    static Ref<MockIgnoringWorkQueueMessageReceiver> create() { return adoptRef(*new MockIgnoringWorkQueueMessageReceiver); }

private:
    MockIgnoringWorkQueueMessageReceiver() = default;

    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) final { }
};

// Tests that async message with reply that a work queue message receiver does not handle is cancelled.
TEST_F(ConnectionTest, WorkQueueMessageReceiverAsyncWithReplyNotHandledIsCancelled)
{
    ASSERT_TRUE(openBoth());

    auto receiverName = IPC::receiverName(MockTestMessageWithAsyncReply1::name());
    Ref workQueue = WorkQueue::create("MockIgnoringWorkQueueMessageReceiver"_s);
    Ref receiver = MockIgnoringWorkQueueMessageReceiver::create();
    server()->addWorkQueueMessageReceiver(receiverName, workQueue, receiver);

    HashMap<uint64_t, uint64_t> replies;
    for (uint64_t i = 100u; i < 160u; ++i) {
        client()->sendWithAsyncReply(MockTestMessageWithAsyncReply1 { }, [&, i](uint64_t value) {
            replies.add(i, value);
        }, i);
    }
    while (replies.size() < 60u)
        RunLoop::currentSingleton().cycle();

    for (uint64_t i = 100u; i < 160u; ++i) {
        auto reply = replies.find(i);
        ASSERT_NE(reply, replies.end());
        EXPECT_EQ(reply->value, 0u); // Cancelled.
    }

    server()->removeWorkQueueMessageReceiver(receiverName);
}

// Tests that CancelAsyncReply with an invalid async reply ID is an invalid message and that CancelAsyncReply
// with an unknown async reply ID is ignored.
TEST_F(ConnectionTest, CancelAsyncReplyWithInvalidDestinationIsInvalidMessage)
{
    ASSERT_TRUE(openBoth());

    client()->sendSyncReply(makeUniqueRef<IPC::Encoder>(IPC::MessageName::CancelAsyncReply, 0));
    EXPECT_EQ(serverClient().waitForInvalidMessage(kDefaultWaitForTimeout), IPC::MessageName::CancelAsyncReply);

    client()->sendSyncReply(makeUniqueRef<IPC::Encoder>(IPC::MessageName::CancelAsyncReply, 1234567));

    // The server records but does not handle the message, so the reply is cancelled. Receiving the
    // cancellation proves that the server has processed the preceding CancelAsyncReply.
    std::optional<uint64_t> reply;
    client()->sendWithAsyncReply(MockTestMessageWithAsyncReply1 { }, [&](uint64_t value) {
        reply = value;
    }, 77);
    while (!reply)
        RunLoop::currentSingleton().cycle();
    EXPECT_EQ(*reply, 0u);

    auto message = serverClient().waitForMessage(0_s);
    EXPECT_EQ(message.messageName, MockTestMessageWithAsyncReply1::name());
    EXPECT_EQ(message.destinationID, 77u);
    EXPECT_TRUE(serverClient().checkMessages());
}

class ConnectionDidReceiveInvalidMessageTest : public testing::TestWithParam<std::tuple<ConnectionTestDirection, InvalidMessageTestType>>, protected ConnectionTestBase {
public:
    bool serverIsA() const { return std::get<0>(GetParam()) == ConnectionTestDirection::ServerIsA; }
    InvalidMessageTestType testType()
    {
        return std::get<1>(GetParam());
    }

    void SetUp() override
    {
        setupBase();
        if (!serverIsA())
            std::swap(m_connections[0].connection, m_connections[1].connection);

        if (testType() == InvalidMessageTestType::DecodeError) {
            // Cause a decode error by decoding too much.
            serverClient().setAsyncMessageHandler([] (IPC::Connection&, IPC::Decoder& decoder) {
                while (std::optional contents = decoder.decode<uint64_t>()) {
                }
                return true;
            });
            serverClient().setSyncMessageHandler([] (IPC::Connection&, IPC::Decoder& decoder, UniqueRef<IPC::Encoder>&) {
                while (std::optional contents = decoder.decode<uint64_t>()) {
                }
                return true;
            });
        } else {
            // Cause a validation error, MESSAGE_CHECK.
            serverClient().setAsyncMessageHandler([] (IPC::Connection& connection, IPC::Decoder&) {
                connection.markCurrentlyDispatchedMessageAsInvalid("async message check"_s);
                return true;
            });
            serverClient().setSyncMessageHandler([] (IPC::Connection& connection, IPC::Decoder&, UniqueRef<IPC::Encoder>&) {
                connection.markCurrentlyDispatchedMessageAsInvalid("sync message check"_s);
                return true;
            });
        }

    }

    void TearDown() override
    {
        teardownBase();
    }

protected:
    ::testing::AssertionResult openServer() { return openA(); }
    ::testing::AssertionResult openClient() { return openB(); }
    IPC::Connection* server() { return a(); }
    IPC::Connection* client() { return b(); }
    MockConnectionClient& serverClient() { return aClient(); }
    MockConnectionClient& clientClient() { return bClient(); }
    void deleteServer() { deleteA(); }
    void deleteClient() { deleteB(); }
};

TEST_P(ConnectionDidReceiveInvalidMessageTest, Async)
{
    ASSERT_TRUE(openBoth());
    for (uint64_t i = 100u; i < 160u; ++i)
        client()->send(MockTestMessage1 { }, i);

    for (uint64_t i = 100u; i < 160u; ++i) {
        auto invalidMessage = serverClient().waitForInvalidMessage(kDefaultWaitForTimeout);
        EXPECT_EQ(invalidMessage, MockTestMessage1::name());
    }
}

TEST_P(ConnectionDidReceiveInvalidMessageTest, AsyncWithReply)
{
    ASSERT_TRUE(openBoth());
    HashMap<uint64_t, uint64_t> replies;
    for (uint64_t i = 100u; i < 160u; ++i) {
        client()->sendWithAsyncReply(MockTestMessageWithAsyncReply1 { }, [&, i](uint64_t value) {
            replies.add(i, value);
        }, i);
    }

    for (uint64_t i = 100u; i < 160u; ++i) {
        auto invalidMessage = serverClient().waitForInvalidMessage(kDefaultWaitForTimeout);
        EXPECT_EQ(invalidMessage, MockTestMessageWithAsyncReply1::name());
    }

    // The invalid messages are not handled, so the async replies are cancelled.
    while (replies.size() < 60u)
        RunLoop::currentSingleton().cycle();

    for (uint64_t i = 100u; i < 160u; ++i) {
        auto reply = replies.find(i);
        ASSERT_NE(reply, replies.end());
        EXPECT_EQ(reply->value, 0u); // Cancelled.
    }
}

TEST_P(ConnectionDidReceiveInvalidMessageTest, Sync)
{
    ASSERT_TRUE(openBoth());
    for (uint64_t i = 100u; i < 160u; ++i) {
        auto result = client()->sendSync(MockTestSyncMessage(), 99, kDefaultWaitForTimeout);
        EXPECT_FALSE(result.succeeded());
    }

    for (uint64_t i = 100u; i < 160u; ++i) {
        auto invalidMessage = serverClient().waitForInvalidMessage(kDefaultWaitForTimeout);
        EXPECT_EQ(invalidMessage, MockTestSyncMessage::name());
    }
}

#if PLATFORM(MAC)
// Microbenchmarks for sending messages with IPC::Connection, for comparing changes to the send path. They are
// disabled because they take a while, and their numbers only mean something next to other runs on the same machine.
// Run them in a Release build with:
//
//     TestIPC --force --filter '*ConnectionSendBenchmark*'
//
// a() sends from the main thread, which runs at the user-interactive QoS of a UI process's main thread, and b()
// receives on a run loop of its own. Both ends are in this process, so the instruction and cycle counts include the
// receiving side. The sender's CPU time is the main thread's.
class ConnectionSendBenchmark : public ConnectionRunLoopTest {
public:
    void SetUp() override
    {
        ConnectionRunLoopTest::SetUp();
        m_savedQOSClass = qos_class_self();
        pthread_set_qos_class_self_np(QOS_CLASS_USER_INTERACTIVE, 0);
    }

    void TearDown() override
    {
        if (m_receiveRunLoop) {
            dispatchSync(*m_receiveRunLoop, [&] {
                b()->invalidate();
            });
            m_receiveRunLoop = nullptr;
            localReferenceBarrier();
        }
        // A thread can't go back to having no QoS class, so restore the QoS the main thread started with instead, or
        // the default QoS.
        auto qosClass = m_savedQOSClass;
        if (qosClass == QOS_CLASS_UNSPECIFIED)
            qosClass = qos_class_main();
        if (qosClass == QOS_CLASS_UNSPECIFIED)
            qosClass = QOS_CLASS_DEFAULT;
        pthread_set_qos_class_self_np(qosClass, 0);
        ConnectionRunLoopTest::TearDown();
    }

protected:
    struct Counters {
        MonotonicTime time;
        Seconds threadCPUTime;
        uint64_t instructions { 0 };
        uint64_t cycles { 0 };
    };

    static Counters readCounters()
    {
        Counters counters { MonotonicTime::now(), { } };
        struct timespec threadCPUTime { };
        if (!clock_gettime(CLOCK_THREAD_CPUTIME_ID, &threadCPUTime))
            counters.threadCPUTime = Seconds(threadCPUTime.tv_sec) + Seconds::fromNanoseconds(threadCPUTime.tv_nsec);
        rusage_info_v4 usage { };
        if (!proc_pid_rusage(getpid(), RUSAGE_INFO_V4, reinterpret_cast<rusage_info_t*>(&usage))) {
            counters.instructions = usage.ri_instructions;
            counters.cycles = usage.ri_cycles;
        }
        return counters;
    }

    // Opens both connections, with b() handling messages on its own run loop, and waits until a message from a() has
    // arrived, so that connecting isn't measured.
    void openForBenchmark()
    {
        bClient().setAsyncMessageHandler([this](IPC::Connection&, IPC::Decoder& decoder) {
            didReceiveMessage(decoder.destinationID());
            return true;
        });
        bClient().setSyncMessageHandler([](IPC::Connection& connection, IPC::Decoder&, UniqueRef<IPC::Encoder>& encoder) {
            connection.sendSyncReply(WTF::move(encoder));
            return true;
        });
        ASSERT_TRUE(openA());
        m_receiveRunLoop = createRunLoop("ConnectionSendBenchmark receiver"_s);
        bool didOpenB = false;
        dispatchSync(*m_receiveRunLoop, [&] {
            didOpenB = openB();
        });
        ASSERT_TRUE(didOpenB);
        expectMessages(1);
        a()->send(MockTestMessage1 { }, 0);
        waitForMessages();
    }

    // Starts expecting `count` messages with destination IDs from 0 to count - 1, which are sent in sequences of
    // `sequenceLength`, each in order. For example, each thread that sends can send a sequence of its own.
    void expectMessages(size_t count, std::optional<size_t> sequenceLength = std::nullopt)
    {
        m_signalEveryMessage = false;
        m_expectedMessageCount = count;
        m_sequenceLength = sequenceLength.value_or(count);
        m_receivedMessageCount = 0;
        m_outOfOrderMessageCount = 0;
        m_sendTimes = std::unique_ptr<std::atomic<double>[]>(new std::atomic<double>[count]);
        for (size_t i = 0; i < count; ++i)
            m_sendTimes[i].store(0, std::memory_order_relaxed);
        m_nextSequenceNumbers = Vector<uint64_t>(count / m_sequenceLength, [](size_t) -> uint64_t { return 0; });
        m_latencies.clear();
    }

    // Like expectMessages(), but waitForMessage() waits for each message in turn.
    void expectMessagesOneAtATime(size_t count)
    {
        expectMessages(count);
        m_signalEveryMessage = true;
    }

    void waitForMessage()
    {
        EXPECT_TRUE(m_didReceiveAllMessages.waitFor(10_s)) << "Received " << m_receivedMessageCount << " of " << m_expectedMessageCount << " messages";
    }

    void waitForMessages()
    {
        // Generous, since a Debug build is a lot slower.
        EXPECT_TRUE(m_didReceiveAllMessages.waitFor(120_s)) << "Received " << m_receivedMessageCount << " of " << m_expectedMessageCount << " messages";
        EXPECT_EQ(m_outOfOrderMessageCount, 0u);
    }

    // Sends a message made by makeMessage() to each of the `count` destination IDs from `firstDestinationID` on the
    // calling thread. Returns how long each send() took.
    template<typename MakeMessage>
    Vector<double> sendMessages(uint64_t firstDestinationID, uint64_t count, NOESCAPE const MakeMessage& makeMessage)
    {
        Vector<double> sendDurations;
        sendDurations.reserveInitialCapacity(count);
        for (uint64_t destinationID = firstDestinationID; destinationID < firstDestinationID + count; ++destinationID) {
            auto message = makeMessage();
            auto start = MonotonicTime::now();
            m_sendTimes[destinationID].store(start.secondsSinceEpoch().value(), std::memory_order_release);
            a()->send(WTF::move(message), destinationID);
            sendDurations.append((MonotonicTime::now() - start).value());
        }
        return sendDurations;
    }

    void report(ASCIILiteral scenario, size_t messageCount, const Counters& start, const Counters& end, Vector<double>&& sendDurations)
    {
        auto wallTime = end.time - start.time;
        auto latencies = std::exchange(m_latencies, { });
        SAFE_PRINTF("[ BENCHMARK ] %-34s messages=%7zu wall=%9.2fms rate=%9.0f/s sender_cpu=%9.2fms instructions=%13llu cycles=%13llu send_p50=%7.2fus send_p99=%8.2fus latency_p50=%8.2fus latency_p99=%9.2fus\n",
            scenario, messageCount, wallTime.milliseconds(), messageCount / wallTime.seconds(), (end.threadCPUTime - start.threadCPUTime).milliseconds(),
            static_cast<unsigned long long>(end.instructions - start.instructions), static_cast<unsigned long long>(end.cycles - start.cycles),
            percentile(sendDurations, 0.5) * 1e6, percentile(sendDurations, 0.99) * 1e6, percentile(latencies, 0.5) * 1e6, percentile(latencies, 0.99) * 1e6);
        fflush(stdout);
    }

    static auto smallMessage() { return MockTestMessage1 { }; }

private:
    static double percentile(Vector<double>& values, double fraction)
    {
        if (values.isEmpty())
            return 0;
        std::sort(values.begin(), values.end());
        return values[std::min<size_t>(values.size() - 1, values.size() * fraction)];
    }

    // Called on b()'s run loop.
    void didReceiveMessage(uint64_t destinationID)
    {
        if (destinationID < m_expectedMessageCount) {
            if (auto sendTime = m_sendTimes[destinationID].load(std::memory_order_acquire))
                m_latencies.append(MonotonicTime::now().secondsSinceEpoch().value() - sendTime);
            auto& nextSequenceNumber = m_nextSequenceNumbers[destinationID / m_sequenceLength];
            if (destinationID % m_sequenceLength != nextSequenceNumber)
                ++m_outOfOrderMessageCount;
            nextSequenceNumber = destinationID % m_sequenceLength + 1;
        }
        if (++m_receivedMessageCount == m_expectedMessageCount || m_signalEveryMessage)
            m_didReceiveAllMessages.signal();
    }

    RefPtr<RunLoop> m_receiveRunLoop;
    size_t m_expectedMessageCount { 0 };
    bool m_signalEveryMessage { false };
    size_t m_sequenceLength { 1 };
    std::atomic<size_t> m_receivedMessageCount { 0 };
    std::atomic<size_t> m_outOfOrderMessageCount { 0 };
    std::unique_ptr<std::atomic<double>[]> m_sendTimes;
    Vector<uint64_t> m_nextSequenceNumbers;
    Vector<double> m_latencies;
    BinarySemaphore m_didReceiveAllMessages;
    qos_class_t m_savedQOSClass { QOS_CLASS_UNSPECIFIED };
};

TEST_P(ConnectionSendBenchmark, DISABLED_SmallMessages)
{
    ASSERT_NO_FATAL_FAILURE(openForBenchmark());
    constexpr uint64_t messageCount = 100000;
    expectMessages(messageCount);
    auto start = readCounters();
    auto sendDurations = sendMessages(0, messageCount, smallMessage);
    waitForMessages();
    report("SmallMessages"_s, messageCount, start, readCounters(), WTF::move(sendDurations));
}

// Sends each message only once the one before it has arrived, so that the latencies don't include waiting behind
// other messages.
TEST_P(ConnectionSendBenchmark, DISABLED_SmallMessagesOneAtATime)
{
    ASSERT_NO_FATAL_FAILURE(openForBenchmark());
    constexpr uint64_t messageCount = 10000;
    expectMessagesOneAtATime(messageCount);
    Vector<double> sendDurations;
    sendDurations.reserveInitialCapacity(messageCount);
    auto start = readCounters();
    for (uint64_t i = 0; i < messageCount; ++i) {
        sendDurations.appendVector(sendMessages(i, 1, smallMessage));
        waitForMessage();
    }
    report("SmallMessagesOneAtATime"_s, messageCount, start, readCounters(), WTF::move(sendDurations));
}

TEST_P(ConnectionSendBenchmark, DISABLED_MessagesWithAttachment)
{
    ASSERT_NO_FATAL_FAILURE(openForBenchmark());
    mach_port_t port = MACH_PORT_NULL;
    ASSERT_EQ(mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &port), KERN_SUCCESS);
    auto sendRight = MachSendRight::createFromReceiveRight(port);

    constexpr uint64_t messageCount = 20000;
    expectMessages(messageCount);
    auto start = readCounters();
    auto sendDurations = sendMessages(0, messageCount, [&] {
        Vector<IPC::Attachment> attachments;
        attachments.append(MachSendRight { sendRight });
        return MockTestMessageWithDataAndAttachments { { }, WTF::move(attachments) };
    });
    waitForMessages();
    report("MessagesWithAttachment"_s, messageCount, start, readCounters(), WTF::move(sendDurations));

    sendRight = { };
    mach_port_mod_refs(mach_task_self(), port, MACH_PORT_RIGHT_RECEIVE, -1);
}

// Messages large enough that their bodies are sent out of line.
TEST_P(ConnectionSendBenchmark, DISABLED_LargeMessages)
{
    ASSERT_NO_FATAL_FAILURE(openForBenchmark());
    auto data = testDataOfSize(64 * 1024);
    constexpr uint64_t messageCount = 5000;
    expectMessages(messageCount);
    auto start = readCounters();
    auto sendDurations = sendMessages(0, messageCount, [&] {
        return MockTestMessageWithDataAndAttachments { data.span(), { } };
    });
    waitForMessages();
    report("LargeMessages"_s, messageCount, start, readCounters(), WTF::move(sendDurations));
}

// b() stops taking messages off its port, which fills up after 128 messages, so the rest wait in a()'s outgoing queue.
// Measures sending while b() is blocked, and then how long the backlog takes to arrive once b() isn't.
TEST_P(ConnectionSendBenchmark, DISABLED_SlowReceiver)
{
    ASSERT_NO_FATAL_FAILURE(openForBenchmark());
    constexpr uint64_t messageCount = 5000;
    expectMessages(messageCount);
    BinarySemaphore unblockB;
    b()->dispatchOnReceiveQueueForTesting([&] {
        unblockB.wait();
    });
    auto start = readCounters();
    auto sendDurations = sendMessages(0, messageCount, smallMessage);
    auto sent = readCounters();
    unblockB.signal();
    waitForMessages();
    auto end = readCounters();
    report("SlowReceiver (sending)"_s, messageCount, start, sent, WTF::move(sendDurations));
    report("SlowReceiver (delivering backlog)"_s, messageCount, sent, end, { });
}

TEST_P(ConnectionSendBenchmark, DISABLED_SendersOnFourThreads)
{
    ASSERT_NO_FATAL_FAILURE(openForBenchmark());
    constexpr uint64_t threadCount = 4;
    constexpr uint64_t messagesPerThread = 25000;
    expectMessages(threadCount * messagesPerThread, messagesPerThread);
    auto start = readCounters();
    Vector<Ref<Thread>> senders;
    for (uint64_t thread = 1; thread < threadCount; ++thread) {
        senders.append(Thread::create("ConnectionSendBenchmark sender"_s, [this, thread] {
            sendMessages(thread * messagesPerThread, messagesPerThread, smallMessage);
        }, ThreadType::Unknown, Thread::QOS::UserInteractive));
    }
    auto sendDurations = sendMessages(0, messagesPerThread, smallMessage);
    for (auto& sender : senders)
        sender->waitForCompletion();
    waitForMessages();
    report("SendersOnFourThreads"_s, threadCount * messagesPerThread, start, readCounters(), WTF::move(sendDurations));
}

// A background thread keeps sending while the main thread does. Measures the main thread's sends.
TEST_P(ConnectionSendBenchmark, DISABLED_MainThreadWhileBackgroundThreadSends)
{
    ASSERT_NO_FATAL_FAILURE(openForBenchmark());
    constexpr uint64_t messagesPerThread = 50000;
    expectMessages(2 * messagesPerThread, messagesPerThread);
    auto start = readCounters();
    auto background = Thread::create("ConnectionSendBenchmark background sender"_s, [this] {
        sendMessages(messagesPerThread, messagesPerThread, smallMessage);
    }, ThreadType::Unknown, Thread::QOS::Background);
    auto sendDurations = sendMessages(0, messagesPerThread, smallMessage);
    background->waitForCompletion();
    waitForMessages();
    report("MainThreadWhileBackgroundSends"_s, 2 * messagesPerThread, start, readCounters(), WTF::move(sendDurations));
}

// The send durations are round trips.
TEST_P(ConnectionSendBenchmark, DISABLED_SyncRoundTrips)
{
    ASSERT_NO_FATAL_FAILURE(openForBenchmark());
    constexpr uint64_t messageCount = 10000;
    Vector<double> roundTripDurations;
    roundTripDurations.reserveInitialCapacity(messageCount);
    auto start = readCounters();
    for (uint64_t i = 0; i < messageCount; ++i) {
        auto roundTripStart = MonotonicTime::now();
        auto result = a()->sendSync(MockTestSyncMessage { }, i, IPC::Timeout::infinity());
        roundTripDurations.append((MonotonicTime::now() - roundTripStart).value());
        ASSERT_TRUE(result.succeeded());
    }
    report("SyncRoundTrips"_s, messageCount, start, readCounters(), WTF::move(roundTripDurations));
}

INSTANTIATE_TEST_SUITE_P(ConnectionTest,
    ConnectionSendBenchmark,
    testing::Values(ConnectionTestDirection::ServerIsA),
    TestParametersToStringFormatter());
#endif // PLATFORM(MAC)

INSTANTIATE_TEST_SUITE_P(ConnectionTest,
    ConnectionTestABBA,
    connectionTestABBAParameters(),
    ConnectionTestABBAParametersToStringFormatter());

INSTANTIATE_TEST_SUITE_P(ConnectionTest,
    ConnectionRunLoopTest,
    connectionTestABBAParameters(),
    ConnectionTestABBAParametersToStringFormatter());

INSTANTIATE_TEST_SUITE_P(ConnectionTest,
    ConnectionDidReceiveInvalidMessageTest,
    testing::Combine(testing::Values(ConnectionTestDirection::ServerIsA, ConnectionTestDirection::ClientIsA), testing::Values(InvalidMessageTestType::DecodeError, InvalidMessageTestType::ValidationError)),
    TestParametersToStringFormatter());

}
