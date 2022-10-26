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

#pragma once

#include "ArgumentCoders.h"
#include "Connection.h"
#include "Utilities.h"
#include <optional>

namespace TestWebKitAPI {

struct MessageInfo {
    IPC::MessageName messageName;
    uint64_t destinationID;
};

struct MockTestMessage1 {
    static constexpr bool isSync = false;
    static constexpr IPC::MessageName name()  { return static_cast<IPC::MessageName>(123); }
    std::tuple<> arguments() { return { }; }
};

struct MockTestMessageWithAsyncReply1 {
    static constexpr bool isSync = false;
    static constexpr IPC::MessageName name()  { return static_cast<IPC::MessageName>(124); }
    // Just using WebPage_GetBytecodeProfileReply as something that is async message name.
    // If WebPage_GetBytecodeProfileReply is removed, just use another one.
    static constexpr IPC::MessageName asyncMessageReplyName() { return IPC::MessageName::WebPage_GetBytecodeProfileReply; }
    std::tuple<> arguments() { return { }; }
    static void callReply(IPC::Decoder& decoder, CompletionHandler<void(uint64_t)>&& completionHandler)
    {
        auto value = decoder.decode<uint64_t>();
        completionHandler(*value);
    }
    static void cancelReply(CompletionHandler<void(uint64_t)>&& completionHandler)
    {
        completionHandler(0);
    }
};

class MockConnectionClient final : public IPC::Connection::Client {
public:
    ~MockConnectionClient()
    {
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
    void setAsyncMessageHandler(Function<bool(IPC::Decoder&)>&& handler)
    {
        m_asyncMessageHandler = WTFMove(handler);
    }

    // IPC::Connection::Client overrides.
    void didReceiveMessage(IPC::Connection&, IPC::Decoder& decoder) override
    {
        if (m_asyncMessageHandler && m_asyncMessageHandler(decoder))
            return;
        m_messages.append({ decoder.messageName(), decoder.destinationID() });
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
    Function<bool(IPC::Decoder&)> m_asyncMessageHandler;
};

enum class ConnectionTestDirection {
    ServerIsA,
    ClientIsA
};

void PrintTo(ConnectionTestDirection, ::std::ostream*);

class ConnectionTestBase {
public:
    void setupBase();
    void teardownBase();

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
    static void ensureConnectionWorkQueueEmpty(IPC::Connection&);

    struct {
        RefPtr<IPC::Connection> connection;
        MockConnectionClient client;
    } m_connections[2];
};

// Test fixture for tests that are run two times:
//  - Server as a(), and client as b()
//  - Server as b() and client as a()
// The setup and teardown of the Connection is not symmetric, so this fixture is useful to test various scenarios
// around these.
class ConnectionTestABBA : public testing::TestWithParam<std::tuple<ConnectionTestDirection>>, protected ConnectionTestBase {
public:
    bool serverIsA() const { return std::get<0>(GetParam()) == ConnectionTestDirection::ServerIsA; }

    void SetUp() override
    {
        setupBase();
        if (!serverIsA())
            std::swap(m_connections[0].connection, m_connections[1].connection);
    }

    void TearDown() override
    {
        teardownBase();
    }
};

}
