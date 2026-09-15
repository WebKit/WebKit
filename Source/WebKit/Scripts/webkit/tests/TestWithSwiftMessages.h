/*
 * Copyright (C) 2021-2023 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "ArgumentCoders.h"
#include "Connection.h"
#include "MessageNames.h"
#include "SessionState.h"
#include <wtf/Forward.h>
#include <wtf/Ref.h>
#include <wtf/RuntimeApplicationChecks.h>
#include <wtf/ThreadSafeRefCounted.h>

namespace WebCore {
struct FrameIdentifierType;
using FrameIdentifier = ObjectIdentifier<FrameIdentifierType>;
}
namespace WebKit {
class TestWithSwift;
class TestWithSwiftMessageForwarder;
class TestWithSwiftWeakRef;
}

namespace WebKit {

class TestWithSwiftMessageForwarder: public RefCounted<TestWithSwiftMessageForwarder>, public IPC::MessageReceiver {
public:
    static Ref<TestWithSwiftMessageForwarder> createFromWeak(WebKit::TestWithSwiftWeakRef* _Nonnull handler)
    {
        return adoptRef(*new TestWithSwiftMessageForwarder(handler));
    }
    ~TestWithSwiftMessageForwarder();
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&);
    void didReceiveSyncMessage(IPC::Connection&, IPC::Decoder&, UniqueRef<IPC::Encoder>&);
    void ref() const final { RefCounted::ref(); }
    void deref() const final { RefCounted::deref(); }
private:
    TestWithSwiftMessageForwarder(WebKit::TestWithSwiftWeakRef* _Nonnull);
    std::unique_ptr<WebKit::TestWithSwift> getMessageTarget();
    std::unique_ptr<WebKit::TestWithSwiftWeakRef> m_handler;
} SWIFT_SHARED_REFERENCE(.ref, .deref);

}

using RefTestWithSwiftMessageForwarder = Ref<WebKit::TestWithSwiftMessageForwarder>;

namespace WebKit {
using RefFrameState = WTF::Ref<WebKit::FrameState>;
}

namespace Messages {
namespace TestWithSwift {

static inline IPC::ReceiverName messageReceiverName()
{
    return IPC::ReceiverName::TestWithSwift;
}

class TestAsyncMessage {
public:
    using Arguments = std::tuple<uint32_t>;

    static IPC::MessageName name() { return IPC::MessageName::TestWithSwift_TestAsyncMessage; }
    static constexpr bool isSync = false;
    static constexpr bool canDispatchOutOfOrder = false;
    static constexpr bool replyCanDispatchOutOfOrder = false;
    static constexpr bool deferSendingIfSuspended = false;

    static IPC::MessageName asyncMessageReplyName() { return IPC::MessageName::TestWithSwift_TestAsyncMessageReply; }
    static constexpr auto callbackThread = WTF::CompletionHandlerCallThread::ConstructionThread;
    using ReplyArguments = std::tuple<uint8_t>;
    using Reply = CompletionHandler<void(uint8_t)>;
    using Promise = WTF::NativePromise<uint8_t, IPC::Error>;
    explicit TestAsyncMessage(uint32_t param)
        : m_param(param)
    {
    }

    template<typename Encoder>
    void encode(Encoder& encoder)
    {
        encoder << m_param;
    }

private:
    uint32_t m_param;
};

class TestSyncMessage {
public:
    using Arguments = std::tuple<uint32_t>;

    static IPC::MessageName name() { return IPC::MessageName::TestWithSwift_TestSyncMessage; }
    static constexpr bool isSync = true;
    static constexpr bool canDispatchOutOfOrder = false;
    static constexpr bool replyCanDispatchOutOfOrder = false;
    static constexpr bool deferSendingIfSuspended = false;

    static constexpr auto callbackThread = WTF::CompletionHandlerCallThread::ConstructionThread;
    using ReplyArguments = std::tuple<uint8_t>;
    using Reply = CompletionHandler<void(uint8_t)>;
    explicit TestSyncMessage(uint32_t param)
        : m_param(param)
    {
    }

    template<typename Encoder>
    void encode(Encoder& encoder)
    {
        encoder << m_param;
    }

private:
    uint32_t m_param;
};

class TestMessageWithAliasedParameter {
public:
    using Arguments = std::tuple<Ref<WebKit::FrameState>>;

    static IPC::MessageName name() { return IPC::MessageName::TestWithSwift_TestMessageWithAliasedParameter; }
    static constexpr bool isSync = false;
    static constexpr bool canDispatchOutOfOrder = false;
    static constexpr bool replyCanDispatchOutOfOrder = false;
    static constexpr bool deferSendingIfSuspended = false;

    explicit TestMessageWithAliasedParameter(const Ref<WebKit::FrameState>& frameState)
        : m_frameState(frameState)
    {
    }

    template<typename Encoder>
    void encode(Encoder& encoder)
    {
        SUPPRESS_FORWARD_DECL_ARG encoder << m_frameState;
    }

private:
    SUPPRESS_FORWARD_DECL_MEMBER const Ref<WebKit::FrameState>& m_frameState;
};

class TestThrowingMessageWithReply {
public:
    using Arguments = std::tuple<uint32_t>;

    static IPC::MessageName name() { return IPC::MessageName::TestWithSwift_TestThrowingMessageWithReply; }
    static constexpr bool isSync = true;
    static constexpr bool canDispatchOutOfOrder = false;
    static constexpr bool replyCanDispatchOutOfOrder = false;
    static constexpr bool deferSendingIfSuspended = false;

    static constexpr auto callbackThread = WTF::CompletionHandlerCallThread::ConstructionThread;
    using ReplyArguments = std::tuple<uint8_t>;
    using Reply = CompletionHandler<void(uint8_t)>;
    explicit TestThrowingMessageWithReply(uint32_t param)
        : m_param(param)
    {
    }

    template<typename Encoder>
    void encode(Encoder& encoder)
    {
        encoder << m_param;
    }

private:
    uint32_t m_param;
};

class TestThrowingMessageWithoutReply {
public:
    using Arguments = std::tuple<Ref<WebKit::FrameState>, WebCore::FrameIdentifier>;

    static IPC::MessageName name() { return IPC::MessageName::TestWithSwift_TestThrowingMessageWithoutReply; }
    static constexpr bool isSync = false;
    static constexpr bool canDispatchOutOfOrder = false;
    static constexpr bool replyCanDispatchOutOfOrder = false;
    static constexpr bool deferSendingIfSuspended = false;

    TestThrowingMessageWithoutReply(const Ref<WebKit::FrameState>& frameState, const WebCore::FrameIdentifier& frameID)
        : m_frameState(frameState)
        , m_frameID(frameID)
    {
    }

    template<typename Encoder>
    void encode(Encoder& encoder)
    {
        SUPPRESS_FORWARD_DECL_ARG encoder << m_frameState;
        SUPPRESS_FORWARD_DECL_ARG encoder << m_frameID;
    }

private:
    SUPPRESS_FORWARD_DECL_MEMBER const Ref<WebKit::FrameState>& m_frameState;
    SUPPRESS_FORWARD_DECL_MEMBER const WebCore::FrameIdentifier& m_frameID;
};

class TestAsyncMessageReply {
public:
    using Arguments = std::tuple<uint8_t>;

    static IPC::MessageName name() { return IPC::MessageName::TestWithSwift_TestAsyncMessageReply; }
    static constexpr bool isSync = false;
    static constexpr bool canDispatchOutOfOrder = false;
    static constexpr bool replyCanDispatchOutOfOrder = false;
    static constexpr bool deferSendingIfSuspended = false;

    explicit TestAsyncMessageReply(uint8_t reply)
        : m_reply(reply)
    {
    }

    template<typename Encoder>
    void encode(Encoder& encoder)
    {
        encoder << m_reply;
    }

private:
    uint8_t m_reply;
};

} // namespace TestWithSwift
} // namespace Messages

namespace CompletionHandlers {
namespace TestWithSwift {
using TestAsyncMessageCompletionHandler = WTF::RefCountable<Messages::TestWithSwift::TestAsyncMessage::Reply>;
using TestSyncMessageCompletionHandler = WTF::RefCountable<Messages::TestWithSwift::TestSyncMessage::Reply>;
using TestThrowingMessageWithReplyCompletionHandler = WTF::RefCountable<Messages::TestWithSwift::TestThrowingMessageWithReply::Reply>;

void completeWithDefaultReply(TestAsyncMessageCompletionHandler&);

void completeWithDefaultReply(TestSyncMessageCompletionHandler&);

void completeWithDefaultReply(TestThrowingMessageWithReplyCompletionHandler&);
} // namespace TestWithSwift
} // namespace CompletionHandlers

