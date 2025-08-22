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
#include <wtf/Deque.h>
#include <wtf/Forward.h>
#include <wtf/TZoneMalloc.h>

namespace TestWebKitAPI {

template <typename T>
std::optional<T> copyViaEncoder(const T& o)
{
    IPC::Encoder encoder(static_cast<IPC::MessageName>(78), 0);
    encoder << o;
    auto decoder = IPC::Decoder::create(encoder.span(), encoder.releaseAttachments());
    return decoder->decode<T>();
}

struct MessageInfo {
    IPC::MessageName messageName;
    uint64_t destinationID;
};

struct MockTestMessage1 {
    static constexpr bool isSync = false;
    static constexpr bool canDispatchOutOfOrder = true;
    static constexpr bool replyCanDispatchOutOfOrder = false;
    static constexpr IPC::MessageName name()  { return static_cast<IPC::MessageName>(123); }
    template<typename Encoder> void encode(Encoder&) { }
};

struct MockTestMessageWithAsyncReply1 {
    static constexpr bool isSync = false;
    static constexpr bool canDispatchOutOfOrder = false;
    static constexpr bool replyCanDispatchOutOfOrder = false;
    static constexpr IPC::MessageName name()  { return static_cast<IPC::MessageName>(124); }
    // Just using WebPage_GetBytecodeProfileReply as something that is async message name.
    // If WebPage_GetBytecodeProfileReply is removed, just use another one.
    static constexpr IPC::MessageName asyncMessageReplyName() { return IPC::MessageName::WebPage_GetBytecodeProfileReply; }
    template<typename Encoder> void encode(Encoder&) { }

    using ReplyArguments = std::tuple<uint64_t>;
    using Promise = WTF::NativePromise<uint64_t, IPC::Error>;
};

class WaitForMessageMixin {
public:
    ~WaitForMessageMixin()
    {
        ASSERT(m_messages.isEmpty()); // Received unexpected messages.
        ASSERT(m_invalidMessages.isEmpty()); // Received unexpected invalid message.
    }

    Vector<MessageInfo> takeMessages()
    {
        Locker locker { m_lock };
        Vector<MessageInfo> result;
        result.appendRange(m_messages.begin(), m_messages.end());
        m_messages.clear();
        return result;
    }

    Vector<IPC::MessageName> takeInvalidMessages()
    {
        Locker locker { m_lock };
        Vector<IPC::MessageName> result;
        result.appendRange(m_invalidMessages.begin(), m_invalidMessages.end());
        m_invalidMessages.clear();
        return result;
    }

    MessageInfo waitForMessage(Seconds timeout)
    {
        Locker locker { m_lock };
        if (m_messages.isEmpty()) {
            m_continueWaitForMessage = false;
            DropLockForScope unlocker { locker };
            Util::runFor(&m_continueWaitForMessage, timeout);
        }
        ASSERT(m_messages.size() >= 1);
        return m_messages.takeFirst();
    }

    bool waitForDidClose(Seconds timeout)
    {
        Locker locker { m_lock };
        ASSERT(!m_didClose); // Caller checks this.
        {
            DropLockForScope unlocker { locker }; // FIXME: makes no sense.
            Util::runFor(&m_didClose, timeout);
        }
        return m_didClose;
    }

    bool gotDidClose() const
    {
        Locker locker { m_lock };
        return m_didClose;
    }

    IPC::MessageName waitForInvalidMessage(Seconds timeout)
    {
        Locker locker { m_lock };
        if (m_invalidMessages.isEmpty()) {
            m_continueWaitForMessage = false;
            DropLockForScope unlocker { locker }; // FIXME: makes no sense.
            Util::runFor(&m_continueWaitForMessage, timeout);
        }
        ASSERT(m_invalidMessages.size() >= 1);
        return m_invalidMessages.takeFirst();
    }

    void addMessage(IPC::Decoder& decoder)
    {
        Locker locker { m_lock };
        ASSERT(!m_didClose);
        m_messages.append({ decoder.messageName(), decoder.destinationID() });
        m_continueWaitForMessage = true;
    }

    void addInvalidMessage(IPC::MessageName messageName, const Vector<uint32_t>&)
    {
        Locker locker { m_lock };
        ASSERT(!m_didClose);
        m_invalidMessages.append(messageName);
        m_continueWaitForMessage = true;
    }

    void markDidClose()
    {
        Locker locker { m_lock };
        ASSERT(!m_didClose);
        m_didClose = true;
    }

protected:
    mutable Lock m_lock;
    Deque<MessageInfo> m_messages WTF_GUARDED_BY_LOCK(m_lock);
    Deque<IPC::MessageName> m_invalidMessages WTF_GUARDED_BY_LOCK(m_lock);
    bool m_continueWaitForMessage WTF_GUARDED_BY_LOCK(m_lock) { false };
    bool m_didClose WTF_GUARDED_BY_LOCK(m_lock) { false };
};

class MockConnectionClient final : public IPC::Connection::Client, public RefCounted<MockConnectionClient>, public WaitForMessageMixin {
    WTF_MAKE_TZONE_ALLOCATED(MockConnectionClient);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(MockConnectionClient);
public:
    static Ref<MockConnectionClient> create()
    {
        return adoptRef(*new MockConnectionClient);
    }

    ~MockConnectionClient() = default;

    void ref() const final { RefCounted::ref(); }
    void deref() const final { RefCounted::deref(); }

    // Handler returns false if the message should be just recorded.
    void setAsyncMessageHandler(Function<bool(IPC::Connection&, IPC::Decoder&)>&& handler)
    {
        m_asyncMessageHandler = WTFMove(handler);
    }

    // Handler contract as IPC::MessageReceiver::didReceiveSyncMessage: false on invalid message, may adopt encoder,
    // decoder used only during the call, if encoder not adopted it will be submitted.
    void setSyncMessageHandler(Function<bool(IPC::Connection&, IPC::Decoder&, UniqueRef<IPC::Encoder>&)>&& handler)
    {
        m_syncMessageHandler = WTFMove(handler);
    }

    void setInvalidMessageHandler(Function<bool(IPC::Connection&, IPC::MessageName, const Vector<uint32_t>&)>&& handler)
    {
        m_invalidMessageHandler = WTFMove(handler);
    }

private:
    MockConnectionClient() = default;

    // IPC::Connection::Client overrides.
    void didReceiveMessage(IPC::Connection& connection, IPC::Decoder& decoder) override
    {
        if (m_asyncMessageHandler && m_asyncMessageHandler(connection, decoder))
            return;
        addMessage(decoder);
    }
    bool didReceiveSyncMessage(IPC::Connection& connection, IPC::Decoder& decoder, UniqueRef<IPC::Encoder>& encoder) override
    {
        if (m_syncMessageHandler)
            return m_syncMessageHandler(connection, decoder, encoder);
        addMessage(decoder);
        return false;
    }

    void didClose(IPC::Connection&) override
    {
        markDidClose();
    }

    void didReceiveInvalidMessage(IPC::Connection& connection, IPC::MessageName messageName, const Vector<uint32_t>& failIndices) override
    {
        if (m_invalidMessageHandler && m_invalidMessageHandler(connection, messageName, failIndices))
            return;
        addInvalidMessage(messageName, failIndices);
    }

    Function<bool(IPC::Connection&, IPC::Decoder&)> m_asyncMessageHandler;
    Function<bool(IPC::Connection&, IPC::Decoder&, UniqueRef<IPC::Encoder>&)> m_syncMessageHandler;
    Function<bool(IPC::Connection&, IPC::MessageName, const Vector<uint32_t>&)> m_invalidMessageHandler;
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
        Ref<MockConnectionClient> client = MockConnectionClient::create();
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


enum class InvalidMessageTestType : uint8_t {
    DecodeError,
    ValidationError
};

void PrintTo(InvalidMessageTestType, ::std::ostream*);

}
