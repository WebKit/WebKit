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
#include <wtf/Forward.h>
#include <wtf/RuntimeApplicationChecks.h>
#include <wtf/ThreadSafeRefCounted.h>

#if ENABLE(SWIFT_TEST_CONDITION)
namespace WebKit {
class TestWithSwiftConditionally;
class TestWithSwiftConditionallyMessageForwarder;
class TestWithSwiftConditionallyWeakRef;
}
#endif // ENABLE(SWIFT_TEST_CONDITION)

#if ENABLE(SWIFT_TEST_CONDITION)
namespace WebKit {

class TestWithSwiftConditionallyMessageForwarder: public RefCounted<TestWithSwiftConditionallyMessageForwarder>, public IPC::MessageReceiver {
public:
    static Ref<TestWithSwiftConditionallyMessageForwarder> createFromWeak(WebKit::TestWithSwiftConditionallyWeakRef* _Nonnull handler)
    {
        return adoptRef(*new TestWithSwiftConditionallyMessageForwarder(handler));
    }
    ~TestWithSwiftConditionallyMessageForwarder();
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&);
    void didReceiveSyncMessage(IPC::Connection&, IPC::Decoder&, UniqueRef<IPC::Encoder>&);
    void ref() const final { RefCounted::ref(); }
    void deref() const final { RefCounted::deref(); }
private:
    TestWithSwiftConditionallyMessageForwarder(WebKit::TestWithSwiftConditionallyWeakRef* _Nonnull);
    std::unique_ptr<WebKit::TestWithSwiftConditionally> getMessageTarget();
    std::unique_ptr<WebKit::TestWithSwiftConditionallyWeakRef> m_handler;
} SWIFT_SHARED_REFERENCE(.ref, .deref);

}

using RefTestWithSwiftConditionallyMessageForwarder = Ref<WebKit::TestWithSwiftConditionallyMessageForwarder>;

#endif // ENABLE(SWIFT_TEST_CONDITION)
namespace Messages {
namespace TestWithSwiftConditionally {

static inline IPC::ReceiverName messageReceiverName()
{
    return IPC::ReceiverName::TestWithSwiftConditionally;
}

class TestAsyncMessage {
public:
    using Arguments = std::tuple<uint32_t>;

    static IPC::MessageName name() { return IPC::MessageName::TestWithSwiftConditionally_TestAsyncMessage; }
    static constexpr bool isSync = false;
    static constexpr bool canDispatchOutOfOrder = false;
    static constexpr bool replyCanDispatchOutOfOrder = false;
    static constexpr bool deferSendingIfSuspended = false;

    static IPC::MessageName asyncMessageReplyName() { return IPC::MessageName::TestWithSwiftConditionally_TestAsyncMessageReply; }
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

    static IPC::MessageName name() { return IPC::MessageName::TestWithSwiftConditionally_TestSyncMessage; }
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

class TestAsyncMessageReply {
public:
    using Arguments = std::tuple<uint8_t>;

    static IPC::MessageName name() { return IPC::MessageName::TestWithSwiftConditionally_TestAsyncMessageReply; }
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

} // namespace TestWithSwiftConditionally
} // namespace Messages
#if ENABLE(SWIFT_TEST_CONDITION)

namespace CompletionHandlers {
namespace TestWithSwiftConditionally {
using TestAsyncMessageCompletionHandler = WTF::RefCountable<Messages::TestWithSwiftConditionally::TestAsyncMessage::Reply>;
using TestSyncMessageCompletionHandler = WTF::RefCountable<Messages::TestWithSwiftConditionally::TestSyncMessage::Reply>;
} // namespace TestWithSwiftConditionally
} // namespace CompletionHandlers

#endif // ENABLE(SWIFT_TEST_CONDITION)
