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
#include "Test.h"
#include <wtf/threads/BinarySemaphore.h>


namespace TestWebKitAPI {

static constexpr Seconds kDefaultWaitForTimeout = 1_s;

static constexpr Seconds kWaitForAbsenceTimeout = 300_ms;

struct MockTestMessageWithConnection {
    static constexpr bool isSync = false;
    static constexpr bool canDispatchOutOfOrder = false;
    static constexpr bool replyCanDispatchOutOfOrder = false;
    static constexpr IPC::MessageName name()  { return static_cast<IPC::MessageName>(123); }
    auto&& arguments() { return WTFMove(m_arguments); }
    MockTestMessageWithConnection(IPC::Connection::Handle&& handle)
        : m_arguments(WTFMove(handle))
    {
    }
    std::tuple<IPC::Connection::Handle&&> m_arguments;
};

struct MockTestSyncMessage {
    static constexpr bool isSync = true;
    static constexpr bool canDispatchOutOfOrder = false;
    static constexpr bool replyCanDispatchOutOfOrder = false;
    static constexpr IPC::MessageName name()  { return static_cast<IPC::MessageName>(124); }
    using ReplyArguments = std::tuple<>;
    auto&& arguments()
    {
        return WTFMove(m_arguments);
    }

    MockTestSyncMessage()
    {
    }

    std::tuple<> m_arguments;
};

struct MockTestSyncMessageWithDataReply {
    static constexpr bool isSync = true;
    static constexpr bool canDispatchOutOfOrder = false;
    static constexpr bool replyCanDispatchOutOfOrder = false;
    static constexpr IPC::MessageName name()  { return  IPC::MessageName::IPCTester_SyncPing; } // Needs to be sync.
    using ReplyArguments = std::tuple<std::span<const uint8_t>>;
    auto&& arguments()
    {
        return WTFMove(m_arguments);
    }

    MockTestSyncMessageWithDataReply()
    {
    }

    std::tuple<> m_arguments;
};

namespace {
class SimpleConnectionTest : public testing::Test {
public:
    void SetUp() override
    {
        WTF::initializeMainThread();
    }
protected:
    MockConnectionClient m_mockServerClient;
    MockConnectionClient m_mockClientClient;
};
}

TEST_F(SimpleConnectionTest, CreateServerConnection)
{
    auto identifiers = IPC::Connection::createConnectionIdentifierPair();
    ASSERT_NE(identifiers, std::nullopt);
    Ref<IPC::Connection> connection = IPC::Connection::createServerConnection(WTFMove(identifiers->server));
    connection->invalidate();
}

TEST_F(SimpleConnectionTest, CreateClientConnection)
{
    auto identifiers = IPC::Connection::createConnectionIdentifierPair();
    ASSERT_NE(identifiers, std::nullopt);
    Ref<IPC::Connection> connection = IPC::Connection::createClientConnection(IPC::Connection::Identifier { WTFMove(identifiers->client) });
    connection->invalidate();
}

TEST_F(SimpleConnectionTest, ConnectLocalConnection)
{
    auto identifiers = IPC::Connection::createConnectionIdentifierPair();
    ASSERT_NE(identifiers, std::nullopt);
    Ref<IPC::Connection> serverConnection = IPC::Connection::createServerConnection(WTFMove(identifiers->server));
    Ref<IPC::Connection> clientConnection = IPC::Connection::createClientConnection(IPC::Connection::Identifier { WTFMove(identifiers->client) });
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
    Ref<IPC::Connection> firstServerConnection = IPC::Connection::createServerConnection(WTFMove(firstIdentifiers->server));
    firstServerConnection->open(m_mockServerClient);

    // Create a second connection, and send the client
    // handle in a message over the first connection (such
    // that it will be stored as a pending message).
    auto secondIdentifiers = IPC::Connection::createConnectionIdentifierPair();
    ASSERT_NE(secondIdentifiers, std::nullopt);
    Ref<IPC::Connection> secondServerConnection = IPC::Connection::createServerConnection(WTFMove(secondIdentifiers->server));
    MockConnectionClient mockSecondServerClient;
    secondServerConnection->open(mockSecondServerClient);

    firstServerConnection->send(MockTestMessageWithConnection { WTFMove(secondIdentifiers->client) }, 0);

    // Invalidate the first connection's client handle,
    // which should clear pending messages and also invalidate
    // the second connection.
    firstIdentifiers->client = IPC::Connection::Handle();

    // Try a sync send over the second connection, which should
    // fail immediately if the client handle has been released.
    secondServerConnection->sendSync(MockTestSyncMessage(), 0, IPC::Timeout::infinity(), IPC::SendSyncOption::UseFullySynchronousModeForTesting);
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

TEST_P(ConnectionTestABBA, UnopenedAAndInvalidateDoesNotDeliverBDidClose)
{
    ASSERT_TRUE(openB());
    a()->invalidate();
    deleteA();
    EXPECT_FALSE(bClient().waitForDidClose(kWaitForAbsenceTimeout));
}

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
        RunLoop::current().dispatch([&otherRunLoopTasksRun] {
            otherRunLoopTasksRun++;
        });
        Util::spinRunLoop();
        EXPECT_EQ(otherRunLoopTasksRun, i + 1u);
        auto messages1 = aClient().takeMessages();
        EXPECT_EQ(messageCounts[i], messages1.size());
        messages.appendVector(WTFMove(messages1));
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
    aClient().setAsyncMessageHandler([&] (IPC::Decoder& decoder) -> bool {
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
        RunLoop::current().dispatch([&otherRunLoopTasksRun] {
            otherRunLoopTasksRun++;
        });
        Util::spinRunLoop();
        EXPECT_EQ(otherRunLoopTasksRun, i + 1u);
        auto messages1 = aClient().takeMessages();
        EXPECT_EQ(messageCounts[i], messages1.size());
        messages.appendVector(WTFMove(messages1));
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

TEST_P(ConnectionTestABBA, ReceiveAlreadyInvalidatedClientNoAssert)
{
    ASSERT_TRUE(openBoth());
    HashSet<uint64_t> done;
    bClient().setAsyncMessageHandler([&] (IPC::Decoder& decoder) -> bool {
        auto destinationID = decoder.destinationID();
        auto handle = decoder.decode<IPC::Connection::Handle>();
        if (!handle)
            return false;
        Ref<IPC::Connection> clientConnection = IPC::Connection::createClientConnection(IPC::Connection::Identifier { WTFMove(*handle) });
        MockConnectionClient mockClientClient;
        clientConnection->open(mockClientClient);
        EXPECT_TRUE(mockClientClient.waitForDidClose(kDefaultWaitForTimeout)) << destinationID;
        clientConnection->invalidate();
        done.add(destinationID);
        return true;
    });
    for (uint64_t i = 1; i < 801; ++i) {
        auto identifiers = IPC::Connection::createConnectionIdentifierPair();
        ASSERT_NE(identifiers, std::nullopt);
        Ref<IPC::Connection> serverConnection = IPC::Connection::createServerConnection(WTFMove(identifiers->server));
        MockConnectionClient mockServerClient;
        serverConnection->open(mockServerClient);
        a()->send(MockTestMessageWithConnection { WTFMove(identifiers->client) }, i);
        serverConnection->invalidate();
    }
    while (done.size() < 800)
        RunLoop::current().cycle();
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
        RunLoop::current().cycle();
}

class ConnectionRunLoopTest : public ConnectionTestABBA {
public:
    void TearDown() override
    {
        ConnectionTestABBA::TearDown();
        // Remember to call localReferenceBarrier() in test scope.
        // Otherwise run loops might be executing code that uses variables
        // that went out of scope.
        EXPECT_EQ(m_runLoops.size(), 0u);
    }

    Ref<RunLoop> createRunLoop(const char* name)
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
                threadsToWait.append(Thread::current());
                RunLoop::current().stop();
            });
        }
        while (true) {
            sleep(0.1_s);
            Locker lock { Thread::allThreadsLock() };
            for (auto& thread : threadsToWait) {
                if (Thread::allThreads().contains(thread.ptr()))
                    continue;
            }
            break;
        }
    }

protected:
    Vector<Ref<RunLoop>> m_runLoops;
};

#define LOCAL_STRINGIFY(x) #x
#define RUN_LOOP_NAME "RunLoop at ConnectionTests.cpp:" LOCAL_STRINGIFY(__LINE__)

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
    aClient().setAsyncMessageHandler([&] (IPC::Decoder& decoder) -> bool {
        auto listenerID = decoder.decode<uint64_t>();
        auto encoder = makeUniqueRef<IPC::Encoder>(MockTestMessageWithAsyncReply1::asyncMessageReplyName(), *listenerID);
        encoder.get() << decoder.destinationID();
        a()->sendSyncReply(WTFMove(encoder));
        return true;
    });
    HashSet<uint64_t> replies;

    auto runLoop = createRunLoop(RUN_LOOP_NAME);
    dispatchAndWait(runLoop, [&] {
        ASSERT_TRUE(openB());
        for (uint64_t i = 100u; i < 160u; ++i) {
            b()->sendWithAsyncReply(MockTestMessageWithAsyncReply1 { }, [&, j = i] (uint64_t value) {
                if (!value)
                    WTFLogAlways("GOT: %llu", j);
                EXPECT_GE(value, 100u);
                replies.add(value);
            }, i);
        }
        while (replies.size() < 60u)
            RunLoop::current().cycle();
        b()->invalidate();
    });

    for (uint64_t i = 100u; i < 160u; ++i)
        EXPECT_TRUE(replies.contains(i));
    localReferenceBarrier();
}

TEST_P(ConnectionRunLoopTest, RunLoopSendWithPromisedReply)
{
    ASSERT_TRUE(openA());
    aClient().setAsyncMessageHandler([&] (IPC::Decoder& decoder) -> bool {
        auto listenerID = decoder.decode<uint64_t>();
        auto encoder = makeUniqueRef<IPC::Encoder>(MockTestMessageWithAsyncReply1::asyncMessageReplyName(), *listenerID);
        encoder.get() << decoder.destinationID();
        a()->sendSyncReply(WTFMove(encoder));
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
                        WTFLogAlways("GOT: %llu", j);
                    EXPECT_GE(value, 100u);
                    replies.add(value);
                },
                [] {
                    // There should never be a connection failure in this case.
                    EXPECT_TRUE(false);
                });
        }
        while (replies.size() < 60u)
            RunLoop::current().cycle();
        b()->invalidate();
    });

    for (uint64_t i = 100u; i < 160u; ++i)
        EXPECT_TRUE(replies.contains(i));
    localReferenceBarrier();
}

// Tests that all sent messages are received, even if sender invalidates
// without synchronizing with the receiver.
TEST_P(ConnectionRunLoopTest, SendAndInvalidate)
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
        bClient().setAsyncMessageHandler([&] (IPC::Decoder& decoder) -> bool {
            auto listenerID = decoder.decode<uint64_t>();
            auto encoder = makeUniqueRef<IPC::Encoder>(MockTestMessageWithAsyncReply1::asyncMessageReplyName(), *listenerID);
            encoder.get() << decoder.destinationID();
            b()->sendSyncReply(WTFMove(encoder));
            messages.add(decoder.destinationID());
            return true;
        });
        ASSERT_TRUE(openB());
        while (messages.size() < messageCount - 1)
            RunLoop::current().cycle();
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
    localReferenceBarrier();
}

// Ensure that replies are received in the right order.
TEST_P(ConnectionRunLoopTest, RunLoopSendWithPromisedReplyOrder)
{
    using Promise = MockTestMessageWithAsyncReply1::Promise;

    ASSERT_TRUE(openA());
    uint64_t replyID = 0;
    aClient().setAsyncMessageHandler([&] (IPC::Decoder& decoder) -> bool {
        auto listenerID = decoder.decode<uint64_t>();
        auto encoder = makeUniqueRef<IPC::Encoder>(MockTestMessageWithAsyncReply1::asyncMessageReplyName(), *listenerID);
        encoder.get() << replyID++;
        a()->sendSyncReply(WTFMove(encoder));
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
            RunLoop::current().cycle();
        b()->invalidate();
    });

    for (uint64_t i = 0u; i < counter; ++i)
        EXPECT_EQ(replies[i], i);
    localReferenceBarrier();
}

// This API contract does not make sense. Not only that, but there is no good way currently
// to capture this in a thread-safe way (construct completion handler in a thread-safe way
// so that it would assert that it would execute in the run loop thread). This is disabled
// until the API contract is changed.
TEST_P(ConnectionRunLoopTest, DISABLED_RunLoopSendAsyncOnAnotherRunLoopDispatchesOnConnectionRunLoop)
{
    ASSERT_TRUE(openA());
    aClient().setAsyncMessageHandler([&] (IPC::Decoder& decoder) -> bool {
        auto listenerID = decoder.decode<uint64_t>();
        auto encoder = makeUniqueRef<IPC::Encoder>(MockTestMessageWithAsyncReply1::asyncMessageReplyName(), *listenerID);
        encoder.get() << decoder.destinationID();
        a()->sendSyncReply(WTFMove(encoder));
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
            RunLoop::current().cycle();
    });

    for (uint64_t i = 100u; i < 160u; ++i)
        EXPECT_TRUE(replies.contains(i));
    semaphore.signal();
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
        RunLoop::current().cycle();
    EXPECT_EQ(reply, 0u);
    semaphore.signal();
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

TEST_P(ConnectionRunLoopTest, SendLocalSyncMessageWithDataReply)
{
    constexpr int iterations = 10;
    constexpr size_t dataSize = 1e8; // 100 MB.
    ASSERT_TRUE(openA());
    auto runLoop = createRunLoop(RUN_LOOP_NAME);
    runLoop->dispatch([&] {
        bClient().setSyncMessageHandler([&](IPC::Decoder& decoder, UniqueRef<IPC::Encoder>& encoder) -> bool {
            Vector<uint8_t> data(dataSize);
            for (size_t i = 0; i < dataSize; ++i)
                data[i] = static_cast<uint8_t>(i);
            encoder.get() << data;
            return false;
        });
        ASSERT_TRUE(openB());
    });
    for (int i = 0; i < iterations; ++i) {
        auto sendResult = a()->sendSync(MockTestSyncMessageWithDataReply { }, i, IPC::Timeout::infinity());
        ASSERT_TRUE(sendResult.succeeded());
        auto& [replyData] = sendResult.reply();
        for (size_t i = 0; i < replyData.size(); ++i)
            ASSERT_EQ(static_cast<uint8_t>(i), replyData[i]);
    }
    runLoop->dispatch([&] {
        b()->invalidate();
    });
    localReferenceBarrier();
}

#undef RUN_LOOP_NAME
#undef LOCAL_STRINGIFY

INSTANTIATE_TEST_SUITE_P(ConnectionTest,
    ConnectionTestABBA,
    testing::Values(ConnectionTestDirection::ServerIsA, ConnectionTestDirection::ClientIsA),
    TestParametersToStringFormatter());

INSTANTIATE_TEST_SUITE_P(ConnectionTest,
    ConnectionRunLoopTest,
    testing::Values(ConnectionTestDirection::ServerIsA, ConnectionTestDirection::ClientIsA),
    TestParametersToStringFormatter());

}
