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
#include "Helpers/Utilities.h"
#include <optional>
#include <wtf/Deque.h>
#include <wtf/Forward.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/threads/BinarySemaphore.h>

#if ENABLE(IPC_SEND_DIRECTLY)
#include <pthread/qos.h>
#endif

namespace TestWebKitAPI {

template <typename T>
std::optional<T> copyViaEncoder(const T& o)
{
    IPC::Encoder encoder(IPC::MessageName::IPCTester_EmptyMessage, 0);
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
    static constexpr IPC::MessageName name()  { return IPC::MessageName::IPCTester_EmptyMessage; }
    template<typename Encoder> void encode(Encoder&) { }
};

struct MockTestMessageWithAsyncReply1 {
    static constexpr bool isSync = false;
    static constexpr bool canDispatchOutOfOrder = false;
    static constexpr bool replyCanDispatchOutOfOrder = false;
    static constexpr IPC::MessageName name()  { return IPC::MessageName::IPCTester_EmptyMessageWithReply; }
    static constexpr IPC::MessageName asyncMessageReplyName() { return IPC::MessageName::IPCTester_EmptyMessageWithReplyReply; }
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

    testing::AssertionResult checkMessages()
    {
        auto messages = takeMessages();
        if (!messages.isEmpty())
            return testing::AssertionFailure() << "Unexpected messages: " << messages.size() << " first: " << static_cast<unsigned>(messages[0].messageName) << " to " << messages[0].destinationID;
        auto invalidMessages = takeInvalidMessages();
        if (!invalidMessages.isEmpty())
            return testing::AssertionFailure() << "Unexpected invalid messages: " << invalidMessages.size() << " first:" << static_cast<unsigned>(invalidMessages[0]);
        return testing::AssertionSuccess();
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

    void ref() const final { RefCounted::ref(); }
    void deref() const final { RefCounted::deref(); }

    // Handler returns false if the message should be just recorded.
    void setAsyncMessageHandler(Function<bool(IPC::Connection&, IPC::Decoder&)>&& handler)
    {
        m_asyncMessageHandler = WTF::move(handler);
    }

    // Handler contract as IPC::MessageReceiver::didReceiveSyncMessage: false on invalid message, may adopt encoder,
    // decoder used only during the call, if encoder not adopted it will be submitted.
    void setSyncMessageHandler(Function<bool(IPC::Connection&, IPC::Decoder&, UniqueRef<IPC::Encoder>&)>&& handler)
    {
        m_syncMessageHandler = WTF::move(handler);
    }

    void setInvalidMessageHandler(Function<bool(IPC::Connection&, IPC::MessageName, const Vector<uint32_t>&)>&& handler)
    {
        m_invalidMessageHandler = WTF::move(handler);
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
    void didReceiveSyncMessage(IPC::Connection& connection, IPC::Decoder& decoder, UniqueRef<IPC::Encoder>& encoder) override
    {
        if (m_syncMessageHandler && m_syncMessageHandler(connection, decoder, encoder))
            return;
        addMessage(decoder);
        return;
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

// Whether connections may send messages on the calling thread, see IPC::Connection::setAllowsSendingDirectly(). Only
// Disabled is supported without ENABLE(IPC_SEND_DIRECTLY).
enum class ConnectionTestDirectSend : bool {
    Disabled,
    Enabled
};

void PrintTo(ConnectionTestDirectSend, ::std::ostream*);

// Allows or disallows direct sends, and makes the calling thread's QoS high enough for them if they are allowed.
// Restores both when destroyed.
class ConnectionTestDirectSendScope {
    WTF_MAKE_NONCOPYABLE(ConnectionTestDirectSendScope);
public:
    explicit ConnectionTestDirectSendScope(ConnectionTestDirectSend);
    ~ConnectionTestDirectSendScope();

private:
#if ENABLE(IPC_SEND_DIRECTLY)
    bool m_savedAllowsSendingDirectly { true };
    std::optional<qos_class_t> m_savedQOSClass;
#endif
};

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
// With ENABLE(IPC_SEND_DIRECTLY), each of these also runs with direct sends enabled, see
// IPC::Connection::setAllowsSendingDirectly().
class ConnectionTestABBA : public testing::TestWithParam<std::tuple<ConnectionTestDirection, ConnectionTestDirectSend>>, protected ConnectionTestBase {
public:
    bool serverIsA() const { return std::get<0>(GetParam()) == ConnectionTestDirection::ServerIsA; }
    ConnectionTestDirectSend directSend() const { return std::get<1>(GetParam()); }

    void SetUp() override
    {
        m_directSendScope.emplace(directSend());
        setupBase();
        if (!serverIsA())
            std::swap(m_connections[0].connection, m_connections[1].connection);
    }

    void TearDown() override
    {
        teardownBase();
        m_directSendScope = std::nullopt;
    }

private:
    std::optional<ConnectionTestDirectSendScope> m_directSendScope;
};

// The parameters to instantiate ConnectionTestABBA and the fixtures derived from it with.
inline auto connectionTestABBAParameters()
{
    return testing::Combine(testing::Values(ConnectionTestDirection::ServerIsA, ConnectionTestDirection::ClientIsA),
#if ENABLE(IPC_SEND_DIRECTLY)
        testing::Values(ConnectionTestDirectSend::Disabled, ConnectionTestDirectSend::Enabled));
#else
        testing::Values(ConnectionTestDirectSend::Disabled));
#endif
}

// Names the tests that run with direct sends disabled like the tests did before there were direct sends, for example
// "ServerIsA", and the others like "ServerIsADirectSendEnabled".
struct ConnectionTestABBAParametersToStringFormatter {
    std::string operator()(const testing::TestParamInfo<ConnectionTestABBA::ParamType>&) const;
};

class AutoWorkQueue {
public:
    class WorkQueueWithShutdown : public WorkQueue {
    public:
        static Ref<WorkQueueWithShutdown> create(ASCIILiteral name) { return adoptRef(*new WorkQueueWithShutdown(name)); }
        void beginShutdown()
        {
            dispatch([this, strong = Ref { *this }] {
                m_shutdown = true;
                m_semaphore.signal();
            });
        }
        void waitUntilShutdown()
        {
            while (!m_shutdown)
                m_semaphore.wait();
        }

    private:
        WorkQueueWithShutdown(ASCIILiteral name)
            : WorkQueue(name, QOS::Default)
        {
        }
        std::atomic<bool> m_shutdown { false };
        BinarySemaphore m_semaphore;
    };

    AutoWorkQueue()
        : m_workQueue(WorkQueueWithShutdown::create("com.apple.WebKit.Test.simple"_s))
    {
    }

    Ref<WorkQueueWithShutdown> queue() { return m_workQueue; }

    ~AutoWorkQueue()
    {
        m_workQueue->waitUntilShutdown();
    }

private:
    Ref<WorkQueueWithShutdown> m_workQueue;
};


enum class InvalidMessageTestType : uint8_t {
    DecodeError,
    ValidationError
};

void PrintTo(InvalidMessageTestType, ::std::ostream*);

}
