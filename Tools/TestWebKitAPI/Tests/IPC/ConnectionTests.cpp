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
#include "Connection.h"

#include "ArgumentCoders.h"
#include "Test.h"
#include "Utilities.h"
#include <optional>

namespace TestWebKitAPI {

static constexpr Seconds kDefaultWaitForTimeout = 1_s;

static constexpr Seconds kWaitForAbsenceTimeout = 300_ms;
namespace {
struct MessageInfo {
    IPC::MessageName messageName;
    uint64_t destinationID;
};

struct MockTestMessage1 {
    static constexpr bool isSync = false;
    static constexpr IPC::MessageName name()  { return static_cast<IPC::MessageName>(123); }
    std::tuple<> arguments() { return { }; }
};

class MockConnectionClient final : public IPC::Connection::Client {
public:
    ~MockConnectionClient()
    {
        ASSERT(m_messages.isEmpty()); // Received unexpected messages.
    }

    Vector<MessageInfo> takeMessages()
    {
        Vector<MessageInfo> result;
        result.appendRange(m_messages.begin(), m_messages.end());
        m_messages.clear();
        return result;
    }

    MessageInfo waitForMessage(Seconds timeout)
    {
        if (m_messages.isEmpty()) {
            m_continueWaitForMessage = false;
            Util::runFor(&m_continueWaitForMessage, timeout);
        }
        ASSERT(m_messages.size() >= 1);
        return m_messages.takeFirst();
    }

    bool gotDidClose() const
    {
        return m_didClose;
    }

    bool waitForDidClose(Seconds timeout)
    {
        ASSERT(!m_didClose); // Caller checks this.
        Util::runFor(&m_didClose, timeout);
        return m_didClose;
    }

    // Handler returns false if the message should be just recorded.
    void setAsyncMessageHandler(Function<bool(MessageInfo&)>&& handler)
    {
        m_asyncMessageHandler = WTFMove(handler);
    }

    // IPC::Connection::Client overrides.
    void didReceiveMessage(IPC::Connection&, IPC::Decoder& decoder) override
    {
        MessageInfo messageInfo { decoder.messageName(), decoder.destinationID() };
        if (m_asyncMessageHandler && m_asyncMessageHandler(messageInfo))
            return;
        m_messages.append(WTFMove(messageInfo));
        m_continueWaitForMessage = true;
    }

    bool didReceiveSyncMessage(IPC::Connection&, IPC::Decoder&, UniqueRef<IPC::Encoder>&) override
    {
        return false;
    }

    void didClose(IPC::Connection&) override
    {
        m_didClose = true;
    }

    void didReceiveInvalidMessage(IPC::Connection&, IPC::MessageName message) override
    {
        m_didReceiveInvalidMessage = message;
    }

private:
    bool m_didClose { false };
    std::optional<IPC::MessageName> m_didReceiveInvalidMessage;
    Deque<MessageInfo> m_messages;
    bool m_continueWaitForMessage { false };
    Function<bool(MessageInfo&)> m_asyncMessageHandler;
};

}

class ConnectionTest : public testing::Test {
public:
    void SetUp() override
    {
        WTF::initializeMainThread();
    }
protected:
    MockConnectionClient m_mockServerClient;
    MockConnectionClient m_mockClientClient;
};

TEST_F(ConnectionTest, CreateServerConnection)
{
    auto identifiers = IPC::Connection::createConnectionIdentifierPair();
    ASSERT_NE(identifiers, std::nullopt);
    Ref<IPC::Connection> connection = IPC::Connection::createServerConnection(WTFMove(identifiers->server));
    connection->invalidate();
}

TEST_F(ConnectionTest, CreateClientConnection)
{
    auto identifiers = IPC::Connection::createConnectionIdentifierPair();
    ASSERT_NE(identifiers, std::nullopt);
    Ref<IPC::Connection> connection = IPC::Connection::createClientConnection(IPC::Connection::Identifier { identifiers->client.leakSendRight() });
    connection->invalidate();
}

TEST_F(ConnectionTest, ConnectLocalConnection)
{
    auto identifiers = IPC::Connection::createConnectionIdentifierPair();
    ASSERT_NE(identifiers, std::nullopt);
    Ref<IPC::Connection> serverConnection = IPC::Connection::createServerConnection(WTFMove(identifiers->server));
    Ref<IPC::Connection> clientConnection = IPC::Connection::createClientConnection(IPC::Connection::Identifier { identifiers->client.leakSendRight() });
    serverConnection->open(m_mockServerClient);
    clientConnection->open(m_mockClientClient);
    serverConnection->invalidate();
    clientConnection->invalidate();
}

enum class ConnectionTestDirection {
    ServerIsA,
    ClientIsA
};

void PrintTo(ConnectionTestDirection value, ::std::ostream* o)
{
    if (value == ConnectionTestDirection::ServerIsA)
        *o << "ServerIsA";
    else if (value == ConnectionTestDirection::ClientIsA)
        *o << "ClientIsA";
    else
        *o << "Unknown";
}

class OpenedConnectionTest : public testing::TestWithParam<std::tuple<ConnectionTestDirection>> {
public:
    bool serverIsA() const { return std::get<0>(GetParam()) == ConnectionTestDirection::ServerIsA; }

    void SetUp()
    {
        WTF::initializeMainThread();
        auto identifiers = IPC::Connection::createConnectionIdentifierPair();
        if (!identifiers) {
            FAIL();
            return;
        }
        m_connections[serverIsA() ? 0 : 1].connection = IPC::Connection::createServerConnection(WTFMove(identifiers->server));
        m_connections[serverIsA() ? 1 : 0].connection = IPC::Connection::createClientConnection(IPC::Connection::Identifier { identifiers->client.leakSendRight() });
    }

    void TearDown()
    {
        for (auto& c : m_connections) {
            if (c.connection)
                c.connection->invalidate();
            c.connection = nullptr;
        }
    }

    ::testing::AssertionResult openA()
    {
        if (!a())
            return ::testing::AssertionFailure() << "No A.";
        if (!a()->open(aClient()))
            return ::testing::AssertionFailure() << "Failed to open A";
        return ::testing::AssertionSuccess();
    }

    ::testing::AssertionResult openB()
    {
        if (!b())
            return ::testing::AssertionFailure() << "No b.";
        if (!b()->open(bClient()))
            return ::testing::AssertionFailure() << "Failed to open B";

        return ::testing::AssertionSuccess();
    }

    ::testing::AssertionResult openBoth()
    {
        auto result = openA();
        if (result)
            result = openB();
        return result;
    }

    IPC::Connection* a()
    {
        return m_connections[0].connection.get();
    }

    MockConnectionClient& aClient()
    {
        return m_connections[0].client;
    }

    IPC::Connection* b()
    {
        return m_connections[1].connection.get();
    }

    MockConnectionClient& bClient()
    {
        return m_connections[1].client;
    }

    void deleteA()
    {
        m_connections[0].connection = nullptr;
    }

    void deleteB()
    {
        m_connections[1].connection = nullptr;
    }

protected:
    struct {
        RefPtr<IPC::Connection> connection;
        MockConnectionClient client;
    } m_connections[2];
};

TEST_P(OpenedConnectionTest, SendLocalMessage)
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
        EXPECT_EQ(message.messageName, MockTestMessage1::name());
        EXPECT_EQ(message.destinationID, i);
    }
}

TEST_P(OpenedConnectionTest, AInvalidateDeliversBDidClose)
{
    ASSERT_TRUE(openBoth());
    a()->invalidate();
    ASSERT_FALSE(bClient().gotDidClose());
    EXPECT_TRUE(bClient().waitForDidClose(kDefaultWaitForTimeout));
    EXPECT_FALSE(aClient().gotDidClose());
}

TEST_P(OpenedConnectionTest, AAndBInvalidateDoesNotDeliverDidClose)
{
    ASSERT_TRUE(openBoth());
    a()->invalidate();
    b()->invalidate();
    EXPECT_FALSE(aClient().waitForDidClose(kWaitForAbsenceTimeout));
    EXPECT_FALSE(bClient().waitForDidClose(kWaitForAbsenceTimeout));
}

TEST_P(OpenedConnectionTest, UnopenedAAndInvalidateDoesNotDeliverBDidClose)
{
    ASSERT_TRUE(openB());
    a()->invalidate();
    deleteA();
    EXPECT_FALSE(bClient().waitForDidClose(kWaitForAbsenceTimeout));
}

TEST_P(OpenedConnectionTest, IncomingMessageThrottlingWorks)
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
TEST_P(OpenedConnectionTest, IncomingMessageThrottlingNestedRunLoopDispatches)
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
    aClient().setAsyncMessageHandler([&] (MessageInfo& message) -> bool {
        if (message.destinationID == 888 || message.destinationID == 1299) {
            isProcessing = true;
            Util::spinRunLoop();
            isProcessing = false;
            return true; // Skiping the message is the proof that the message was processed.
        }
        if (message.destinationID == 889 || message.destinationID == 1300) {
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

INSTANTIATE_TEST_SUITE_P(ConnectionTest,
    OpenedConnectionTest,
    testing::Values(ConnectionTestDirection::ServerIsA, ConnectionTestDirection::ClientIsA),
    TestParametersToStringFormatter());

}
