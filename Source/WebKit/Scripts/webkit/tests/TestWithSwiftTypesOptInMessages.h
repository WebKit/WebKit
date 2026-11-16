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
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace WebKit {
class TestSwiftStruct;
}
#if ENABLE(SWIFT_TEST_CONDITION)
namespace WebKit {
class TestWithSwiftTypesOptIn;
class TestWithSwiftTypesOptInMessageForwarder;
class TestWithSwiftTypesOptInWeakRef;
}
#endif // ENABLE(SWIFT_TEST_CONDITION)

#if ENABLE(SWIFT_TEST_CONDITION)
namespace WebKit {

class TestWithSwiftTypesOptInMessageForwarder: public RefCounted<TestWithSwiftTypesOptInMessageForwarder>, public IPC::MessageReceiver {
public:
    static Ref<TestWithSwiftTypesOptInMessageForwarder> createFromWeak(WebKit::TestWithSwiftTypesOptInWeakRef* WTF_NONNULL handler)
    {
        return adoptRef(*new TestWithSwiftTypesOptInMessageForwarder(handler));
    }
    ~TestWithSwiftTypesOptInMessageForwarder();
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&);
    void didReceiveSyncMessage(IPC::Connection&, IPC::Decoder&, UniqueRef<IPC::Encoder>&);
    void ref() const final { RefCounted::ref(); }
    void deref() const final { RefCounted::deref(); }
private:
    TestWithSwiftTypesOptInMessageForwarder(WebKit::TestWithSwiftTypesOptInWeakRef* WTF_NONNULL);
    std::unique_ptr<WebKit::TestWithSwiftTypesOptIn> getMessageTarget();
    std::unique_ptr<WebKit::TestWithSwiftTypesOptInWeakRef> m_handler;
} DERIVED_CLASS_SWIFT_SHARED_REFERENCE(.ref, .deref);

}

using RefTestWithSwiftTypesOptInMessageForwarder = Ref<WebKit::TestWithSwiftTypesOptInMessageForwarder>;

#endif // ENABLE(SWIFT_TEST_CONDITION)
namespace Messages {
namespace TestWithSwiftTypesOptIn {

static inline IPC::ReceiverName messageReceiverName()
{
    return IPC::ReceiverName::TestWithSwiftTypesOptIn;
}

class CxxTypedMessage {
public:
    using Arguments = std::tuple<uint32_t>;

    static constexpr IPC::MessageName name() { return IPC::MessageName::TestWithSwiftTypesOptIn_CxxTypedMessage; }
    static constexpr bool isSync = false;
    static constexpr bool canDispatchOutOfOrder = false;
    static constexpr bool replyCanDispatchOutOfOrder = false;
    static constexpr bool deferSendingIfSuspended = false;

    static IPC::MessageName asyncMessageReplyName() { return IPC::MessageName::TestWithSwiftTypesOptIn_CxxTypedMessageReply; }
    static constexpr auto callbackThread = WTF::CompletionHandlerCallThread::ConstructionThread;
    using ReplyArguments = std::tuple<uint8_t>;
    using Reply = CompletionHandler<void(uint8_t)>;
    using Promise = WTF::NativePromise<uint8_t, IPC::Error>;
    explicit CxxTypedMessage(uint32_t param)
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

class SwiftTypedMessage {
public:
    using Arguments = std::tuple<String, Vector<uint32_t>>;

    static constexpr IPC::MessageName name() { return IPC::MessageName::TestWithSwiftTypesOptIn_SwiftTypedMessage; }
    static constexpr bool isSync = false;
    static constexpr bool canDispatchOutOfOrder = false;
    static constexpr bool replyCanDispatchOutOfOrder = false;
    static constexpr bool deferSendingIfSuspended = false;

    static IPC::MessageName asyncMessageReplyName() { return IPC::MessageName::TestWithSwiftTypesOptIn_SwiftTypedMessageReply; }
    static constexpr auto callbackThread = WTF::CompletionHandlerCallThread::ConstructionThread;
    using ReplyArguments = std::tuple<uint8_t>;
    using Reply = CompletionHandler<void(uint8_t)>;
    using Promise = WTF::NativePromise<uint8_t, IPC::Error>;
    SwiftTypedMessage(const String& url, const Vector<uint32_t>& values)
        : m_url(url)
        , m_values(values)
    {
    }

    template<typename Encoder>
    void encode(Encoder& encoder)
    {
        encoder << m_url;
        SUPPRESS_FORWARD_DECL_ARG encoder << m_values;
    }

private:
    const String& m_url;
    SUPPRESS_FORWARD_DECL_MEMBER const Vector<uint32_t>& m_values;
};

class SwiftTypedMessageWithoutReply {
public:
    using Arguments = std::tuple<std::optional<uint64_t>>;

    static constexpr IPC::MessageName name() { return IPC::MessageName::TestWithSwiftTypesOptIn_SwiftTypedMessageWithoutReply; }
    static constexpr bool isSync = false;
    static constexpr bool canDispatchOutOfOrder = false;
    static constexpr bool replyCanDispatchOutOfOrder = false;
    static constexpr bool deferSendingIfSuspended = false;

    explicit SwiftTypedMessageWithoutReply(const std::optional<uint64_t>& value)
        : m_value(value)
    {
    }

    template<typename Encoder>
    void encode(Encoder& encoder)
    {
        SUPPRESS_FORWARD_DECL_ARG encoder << m_value;
    }

private:
    SUPPRESS_FORWARD_DECL_MEMBER const std::optional<uint64_t>& m_value;
};

class SwiftTypedSynchronousMessage {
public:
    using Arguments = std::tuple<WebKit::TestSwiftStruct>;

    static constexpr IPC::MessageName name() { return IPC::MessageName::TestWithSwiftTypesOptIn_SwiftTypedSynchronousMessage; }
    static constexpr bool isSync = true;
    static constexpr bool canDispatchOutOfOrder = false;
    static constexpr bool replyCanDispatchOutOfOrder = false;
    static constexpr bool deferSendingIfSuspended = false;

    static constexpr auto callbackThread = WTF::CompletionHandlerCallThread::ConstructionThread;
    using ReplyArguments = std::tuple<String>;
    using Reply = CompletionHandler<void(String&&)>;
    explicit SwiftTypedSynchronousMessage(const WebKit::TestSwiftStruct& payload)
        : m_payload(payload)
    {
    }

    template<typename Encoder>
    void encode(Encoder& encoder)
    {
        SUPPRESS_FORWARD_DECL_ARG encoder << m_payload;
    }

private:
    SUPPRESS_FORWARD_DECL_MEMBER const WebKit::TestSwiftStruct& m_payload;
};

class CxxTypedSynchronousMessage {
public:
    using Arguments = std::tuple<uint32_t>;

    static constexpr IPC::MessageName name() { return IPC::MessageName::TestWithSwiftTypesOptIn_CxxTypedSynchronousMessage; }
    static constexpr bool isSync = true;
    static constexpr bool canDispatchOutOfOrder = false;
    static constexpr bool replyCanDispatchOutOfOrder = false;
    static constexpr bool deferSendingIfSuspended = false;

    static constexpr auto callbackThread = WTF::CompletionHandlerCallThread::ConstructionThread;
    using ReplyArguments = std::tuple<uint8_t>;
    using Reply = CompletionHandler<void(uint8_t)>;
    explicit CxxTypedSynchronousMessage(uint32_t param)
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

class CxxTypedMessageReply {
public:
    using Arguments = std::tuple<uint8_t>;

    static constexpr IPC::MessageName name() { return IPC::MessageName::TestWithSwiftTypesOptIn_CxxTypedMessageReply; }
    static constexpr bool isSync = false;
    static constexpr bool canDispatchOutOfOrder = false;
    static constexpr bool replyCanDispatchOutOfOrder = false;
    static constexpr bool deferSendingIfSuspended = false;

    explicit CxxTypedMessageReply(uint8_t reply)
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

class SwiftTypedMessageReply {
public:
    using Arguments = std::tuple<uint8_t>;

    static constexpr IPC::MessageName name() { return IPC::MessageName::TestWithSwiftTypesOptIn_SwiftTypedMessageReply; }
    static constexpr bool isSync = false;
    static constexpr bool canDispatchOutOfOrder = false;
    static constexpr bool replyCanDispatchOutOfOrder = false;
    static constexpr bool deferSendingIfSuspended = false;

    explicit SwiftTypedMessageReply(uint8_t reply)
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

} // namespace TestWithSwiftTypesOptIn
} // namespace Messages
#if ENABLE(SWIFT_TEST_CONDITION)

namespace CompletionHandlers {
namespace TestWithSwiftTypesOptIn {
using CxxTypedMessageCompletionHandler = WTF::RefCountable<Messages::TestWithSwiftTypesOptIn::CxxTypedMessage::Reply>;
using CxxTypedSynchronousMessageCompletionHandler = WTF::RefCountable<Messages::TestWithSwiftTypesOptIn::CxxTypedSynchronousMessage::Reply>;

void completeWithDefaultReply(CxxTypedMessageCompletionHandler&);

void complete(CxxTypedMessageCompletionHandler&, uint8_t);
} // namespace TestWithSwiftTypesOptIn
} // namespace CompletionHandlers

#endif // ENABLE(SWIFT_TEST_CONDITION)
