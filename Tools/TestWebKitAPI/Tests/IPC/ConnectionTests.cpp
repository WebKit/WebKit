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

    MessageInfo waitForMessage(Seconds timeout)
    {
        if (m_messages.isEmpty()) {
            m_continueWaitForMessage = false;
            Util::runFor(&m_continueWaitForMessage, timeout);
        }
        ASSERT(m_messages.size() >= 1);
        return m_messages.takeLast();
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

    // IPC::Connection::Client overrides.
    void didReceiveMessage(IPC::Connection&, IPC::Decoder& decoder) override
    {
        m_messages.insert(0, { decoder.messageName(), decoder.destinationID() });
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
    Vector<MessageInfo> m_messages;
    bool m_continueWaitForMessage { false };
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

TEST_F(ConnectionTest, CreateConnectionsAndInvalidateNoAssert)
{
    auto connections = IPC::Connection::createConnectionPair();
    ASSERT_NE(connections, std::nullopt);
    {
        Ref<IPC::Connection> server = WTFMove(connections->server);
        server->invalidate();
    }
    {
        Ref<IPC::Connection> connection = IPC::Connection::createClientConnection(WTFMove(connections->client));
        connection->invalidate();
    }
}

TEST_F(ConnectionTest, ConnectLocalConnection)
{
    auto connections = IPC::Connection::createConnectionPair();
    ASSERT_NE(connections, std::nullopt);
    Ref<IPC::Connection> serverConnection = WTFMove(connections->server);
    Ref<IPC::Connection> clientConnection = IPC::Connection::createClientConnection(WTFMove(connections->client));
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
        createBoth();
    }

    void TearDown()
    {
        for (auto& c : m_connections) {
            if (c.connection)
                c.connection->invalidate();
            c.connection = nullptr;
        }
    }
    void createBoth()
    {
        auto connections = IPC::Connection::createConnectionPair();
        if (!connections) {
            FAIL();
            return;
        }
        m_connections[serverIsA() ? 0 : 1].connection = WTFMove(connections->server);
        m_connections[serverIsA() ? 1 : 0].connection = IPC::Connection::createClientConnection(WTFMove(connections->client));
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

    void deleteBoth()
    {
        deleteA();
        deleteB();
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

TEST_P(OpenedConnectionTest, UnopenedAAndInvalidateDoesDeliverBDidClose)
{
    ASSERT_TRUE(openB());
    a()->invalidate();
    EXPECT_TRUE(bClient().waitForDidClose(kDefaultWaitForTimeout));
}

TEST_P(OpenedConnectionTest, DeleteUnopenedADoesDeliverBDidClose)
{
    ASSERT_TRUE(openB());
    deleteA();
    EXPECT_TRUE(bClient().waitForDidClose(kDefaultWaitForTimeout));
}

TEST_P(OpenedConnectionTest, DeleteUnopenedAABeforeBOpenDoesDeliverBDidClose)
{
    deleteA();
    ASSERT_TRUE(openB());
    EXPECT_TRUE(bClient().waitForDidClose(kDefaultWaitForTimeout));
}

TEST_P(OpenedConnectionTest, DeleteOpenedABeforeBOpenDoesDeliverBDidClose)
{
    ASSERT_TRUE(openA());
    a()->invalidate();
    deleteA();
    ASSERT_TRUE(openB());
    EXPECT_TRUE(bClient().waitForDidClose(kDefaultWaitForTimeout));
}

TEST_P(OpenedConnectionTest, AAndBInvalidateDoesNotDeliverDidClose)
{
    ASSERT_TRUE(openBoth());
    a()->invalidate();
    b()->invalidate();
    EXPECT_FALSE(aClient().waitForDidClose(kWaitForAbsenceTimeout));
    EXPECT_FALSE(bClient().waitForDidClose(kWaitForAbsenceTimeout));
}

TEST_P(OpenedConnectionTest, CreateManyConnectionsNoAssert)
{
    deleteBoth();
    for (int i = 0; i < 10000; ++i) {
        createBoth();
        ASSERT_TRUE(openBoth());
        a()->invalidate();
        b()->invalidate();
        deleteBoth();
    }
}

INSTANTIATE_TEST_SUITE_P(ConnectionTest,
    OpenedConnectionTest,
    testing::Values(ConnectionTestDirection::ServerIsA, ConnectionTestDirection::ClientIsA),
    TestParametersToStringFormatter());

}
