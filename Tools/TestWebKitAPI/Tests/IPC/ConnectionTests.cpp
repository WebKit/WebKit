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

    MessageInfo waitForMessage()
    {
        if (m_messages.isEmpty()) {
            m_continueWaitForMessage = false;
            Util::run(&m_continueWaitForMessage);
        }
        ASSERT(m_messages.size() >= 1);
        return m_messages.takeLast();
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

TEST_F(ConnectionTest, CreateServerConnection)
{
    auto identifiers = IPC::Connection::createConnectionIdentifierPair();
    ASSERT_NE(identifiers, std::nullopt);
    Ref<IPC::Connection> connection = IPC::Connection::createServerConnection(WTFMove(identifiers->server), m_mockServerClient);
    connection->invalidate();
}

TEST_F(ConnectionTest, CreateClientConnection)
{
    auto identifiers = IPC::Connection::createConnectionIdentifierPair();
    ASSERT_NE(identifiers, std::nullopt);
    Ref<IPC::Connection> connection = IPC::Connection::createClientConnection(IPC::Connection::Identifier { identifiers->client.leakSendRight() }, m_mockClientClient);
    connection->invalidate();
}

TEST_F(ConnectionTest, ConnectLocalConnection)
{
    auto identifiers = IPC::Connection::createConnectionIdentifierPair();
    ASSERT_NE(identifiers, std::nullopt);
    Ref<IPC::Connection> serverConnection = IPC::Connection::createServerConnection(WTFMove(identifiers->server), m_mockServerClient);
    Ref<IPC::Connection> clientConnection = IPC::Connection::createClientConnection(IPC::Connection::Identifier { identifiers->client.leakSendRight() }, m_mockClientClient);
    serverConnection->open();
    clientConnection->open();
    serverConnection->invalidate();
    clientConnection->invalidate();
}

TEST_F(ConnectionTest, SendLocalMessage)
{
    auto identifiers = IPC::Connection::createConnectionIdentifierPair();
    ASSERT_NE(identifiers, std::nullopt);
    Ref<IPC::Connection> serverConnection = IPC::Connection::createServerConnection(WTFMove(identifiers->server), m_mockServerClient);
    Ref<IPC::Connection> clientConnection = IPC::Connection::createClientConnection(IPC::Connection::Identifier { identifiers->client.leakSendRight() }, m_mockClientClient);
    serverConnection->open();
    clientConnection->open();

    for (uint64_t i = 0u; i < 55u; ++i)
        serverConnection->send(MockTestMessage1 { }, i);
    for (uint64_t i = 100u; i < 160u; ++i)
        clientConnection->send(MockTestMessage1 { }, i);
    for (uint64_t i = 0u; i < 55u; ++i) {
        auto message = m_mockClientClient.waitForMessage();
        EXPECT_EQ(message.messageName, MockTestMessage1::name());
        EXPECT_EQ(message.destinationID, i);
    }
    for (uint64_t i = 100u; i < 160u; ++i) {
        auto message = m_mockServerClient.waitForMessage();
        EXPECT_EQ(message.messageName, MockTestMessage1::name());
        EXPECT_EQ(message.destinationID, i);
    }
    serverConnection->invalidate();
    clientConnection->invalidate();
}

}
