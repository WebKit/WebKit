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
#include <wtf/Markable.h>
#include <wtf/OptionSet.h>
#include <wtf/Ref.h>
#include <wtf/RefCounted.h>
#include <wtf/RuntimeApplicationChecks.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace WebCore {
struct FrameIdentifierType;
using FrameIdentifier = ObjectIdentifier<FrameIdentifierType>;
}

namespace WebKit {
class TestSwiftStruct;
enum class TestSwiftEnum : uint8_t;
}
namespace WebKit {
class TestWithSwiftTypes;
class TestWithSwiftTypesMessageForwarder;
class TestWithSwiftTypesWeakRef;
}

namespace WebKit {

class TestWithSwiftTypesMessageForwarder: public RefCounted<TestWithSwiftTypesMessageForwarder>, public IPC::MessageReceiver {
public:
    static Ref<TestWithSwiftTypesMessageForwarder> createFromWeak(WebKit::TestWithSwiftTypesWeakRef* WTF_NONNULL handler)
    {
        return adoptRef(*new TestWithSwiftTypesMessageForwarder(handler));
    }
    ~TestWithSwiftTypesMessageForwarder();
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&);
    void didReceiveSyncMessage(IPC::Connection&, IPC::Decoder&, UniqueRef<IPC::Encoder>&);
    void ref() const final { RefCounted::ref(); }
    void deref() const final { RefCounted::deref(); }
private:
    TestWithSwiftTypesMessageForwarder(WebKit::TestWithSwiftTypesWeakRef* WTF_NONNULL);
    std::unique_ptr<WebKit::TestWithSwiftTypes> getMessageTarget();
    std::unique_ptr<WebKit::TestWithSwiftTypesWeakRef> m_handler;
} DERIVED_CLASS_SWIFT_SHARED_REFERENCE(.ref, .deref);

}

using RefTestWithSwiftTypesMessageForwarder = Ref<WebKit::TestWithSwiftTypesMessageForwarder>;

namespace WebKit {
using RefFrameState = WTF::Ref<WebKit::FrameState>;
}

namespace Messages {
namespace TestWithSwiftTypes {

static inline IPC::ReceiverName messageReceiverName()
{
    return IPC::ReceiverName::TestWithSwiftTypes;
}

class MessageWithoutReply {
public:
    using Arguments = std::tuple<uint32_t, String, Vector<uint64_t>>;

    static constexpr IPC::MessageName name() { return IPC::MessageName::TestWithSwiftTypes_MessageWithoutReply; }
    static constexpr bool isSync = false;
    static constexpr bool canDispatchOutOfOrder = false;
    static constexpr bool replyCanDispatchOutOfOrder = false;
    static constexpr bool deferSendingIfSuspended = false;

    MessageWithoutReply(uint32_t value, const String& name, const Vector<uint64_t>& values)
        : m_value(value)
        , m_name(name)
        , m_values(values)
    {
    }

    template<typename Encoder>
    void encode(Encoder& encoder)
    {
        encoder << m_value;
        encoder << m_name;
        SUPPRESS_FORWARD_DECL_ARG encoder << m_values;
    }

private:
    uint32_t m_value;
    const String& m_name;
    SUPPRESS_FORWARD_DECL_MEMBER const Vector<uint64_t>& m_values;
};

class MessageWithoutParameters {
public:
    using Arguments = std::tuple<>;

    static constexpr IPC::MessageName name() { return IPC::MessageName::TestWithSwiftTypes_MessageWithoutParameters; }
    static constexpr bool isSync = false;
    static constexpr bool canDispatchOutOfOrder = false;
    static constexpr bool replyCanDispatchOutOfOrder = false;
    static constexpr bool deferSendingIfSuspended = false;

    MessageWithoutParameters()
    {
    }

    template<typename Encoder>
    void encode(Encoder& encoder)
    {
    }

private:
};

class MessageWithReply {
public:
    using Arguments = std::tuple<WebKit::TestSwiftStruct, std::optional<int32_t>>;

    static constexpr IPC::MessageName name() { return IPC::MessageName::TestWithSwiftTypes_MessageWithReply; }
    static constexpr bool isSync = false;
    static constexpr bool canDispatchOutOfOrder = false;
    static constexpr bool replyCanDispatchOutOfOrder = false;
    static constexpr bool deferSendingIfSuspended = false;

    static IPC::MessageName asyncMessageReplyName() { return IPC::MessageName::TestWithSwiftTypes_MessageWithReplyReply; }
    static constexpr auto callbackThread = WTF::CompletionHandlerCallThread::ConstructionThread;
    using ReplyArguments = std::tuple<WebKit::TestSwiftStruct>;
    using Reply = CompletionHandler<void(WebKit::TestSwiftStruct&&)>;
    using Promise = WTF::NativePromise<WebKit::TestSwiftStruct, IPC::Error>;
    MessageWithReply(const WebKit::TestSwiftStruct& payload, const std::optional<int32_t>& delta)
        : m_payload(payload)
        , m_delta(delta)
    {
    }

    template<typename Encoder>
    void encode(Encoder& encoder)
    {
        SUPPRESS_FORWARD_DECL_ARG encoder << m_payload;
        SUPPRESS_FORWARD_DECL_ARG encoder << m_delta;
    }

private:
    SUPPRESS_FORWARD_DECL_MEMBER const WebKit::TestSwiftStruct& m_payload;
    SUPPRESS_FORWARD_DECL_MEMBER const std::optional<int32_t>& m_delta;
};

class MessageWithMultipleReplies {
public:
    using Arguments = std::tuple<Vector<String>, Markable<WebCore::FrameIdentifier>>;

    static constexpr IPC::MessageName name() { return IPC::MessageName::TestWithSwiftTypes_MessageWithMultipleReplies; }
    static constexpr bool isSync = false;
    static constexpr bool canDispatchOutOfOrder = false;
    static constexpr bool replyCanDispatchOutOfOrder = false;
    static constexpr bool deferSendingIfSuspended = false;

    static IPC::MessageName asyncMessageReplyName() { return IPC::MessageName::TestWithSwiftTypes_MessageWithMultipleRepliesReply; }
    static constexpr auto callbackThread = WTF::CompletionHandlerCallThread::ConstructionThread;
    using ReplyArguments = std::tuple<bool, Vector<String>, std::optional<double>>;
    using Reply = CompletionHandler<void(bool, Vector<String>&&, std::optional<double>&&)>;
    using Promise = WTF::NativePromise<std::tuple<bool, Vector<String>, std::optional<double>>, IPC::Error>;
    MessageWithMultipleReplies(const Vector<String>& names, const Markable<WebCore::FrameIdentifier>& frameID)
        : m_names(names)
        , m_frameID(frameID)
    {
    }

    template<typename Encoder>
    void encode(Encoder& encoder)
    {
        SUPPRESS_FORWARD_DECL_ARG encoder << m_names;
        SUPPRESS_FORWARD_DECL_ARG encoder << m_frameID;
    }

private:
    SUPPRESS_FORWARD_DECL_MEMBER const Vector<String>& m_names;
    SUPPRESS_FORWARD_DECL_MEMBER const Markable<WebCore::FrameIdentifier>& m_frameID;
};

class MessageWithEmptyReply {
public:
    using Arguments = std::tuple<WebCore::FrameIdentifier>;

    static constexpr IPC::MessageName name() { return IPC::MessageName::TestWithSwiftTypes_MessageWithEmptyReply; }
    static constexpr bool isSync = false;
    static constexpr bool canDispatchOutOfOrder = false;
    static constexpr bool replyCanDispatchOutOfOrder = false;
    static constexpr bool deferSendingIfSuspended = false;

    static IPC::MessageName asyncMessageReplyName() { return IPC::MessageName::TestWithSwiftTypes_MessageWithEmptyReplyReply; }
    static constexpr auto callbackThread = WTF::CompletionHandlerCallThread::ConstructionThread;
    using ReplyArguments = std::tuple<>;
    using Reply = CompletionHandler<void()>;
    using Promise = WTF::NativePromise<void, IPC::Error>;
    explicit MessageWithEmptyReply(const WebCore::FrameIdentifier& frameID)
        : m_frameID(frameID)
    {
    }

    template<typename Encoder>
    void encode(Encoder& encoder)
    {
        SUPPRESS_FORWARD_DECL_ARG encoder << m_frameID;
    }

private:
    SUPPRESS_FORWARD_DECL_MEMBER const WebCore::FrameIdentifier& m_frameID;
};

class MessageWithRefs {
public:
    using Arguments = std::tuple<Ref<WebKit::TestSwiftRefCountedClass>, RefPtr<WebKit::TestSwiftRefCountedClass>, Vector<Ref<WebKit::TestSwiftRefCountedClass>>>;

    static constexpr IPC::MessageName name() { return IPC::MessageName::TestWithSwiftTypes_MessageWithRefs; }
    static constexpr bool isSync = false;
    static constexpr bool canDispatchOutOfOrder = false;
    static constexpr bool replyCanDispatchOutOfOrder = false;
    static constexpr bool deferSendingIfSuspended = false;

    static IPC::MessageName asyncMessageReplyName() { return IPC::MessageName::TestWithSwiftTypes_MessageWithRefsReply; }
    static constexpr auto callbackThread = WTF::CompletionHandlerCallThread::ConstructionThread;
    using ReplyArguments = std::tuple<RefPtr<WebKit::TestSwiftRefCountedClass>>;
    using Reply = CompletionHandler<void(RefPtr<WebKit::TestSwiftRefCountedClass>&&)>;
    using Promise = WTF::NativePromise<RefPtr<WebKit::TestSwiftRefCountedClass>, IPC::Error>;
    MessageWithRefs(const Ref<WebKit::TestSwiftRefCountedClass>& object, const RefPtr<WebKit::TestSwiftRefCountedClass>& optionalObject, const Vector<Ref<WebKit::TestSwiftRefCountedClass>>& objects)
        : m_object(object)
        , m_optionalObject(optionalObject)
        , m_objects(objects)
    {
    }

    template<typename Encoder>
    void encode(Encoder& encoder)
    {
        SUPPRESS_FORWARD_DECL_ARG encoder << m_object;
        SUPPRESS_FORWARD_DECL_ARG encoder << m_optionalObject;
        SUPPRESS_FORWARD_DECL_ARG encoder << m_objects;
    }

private:
    SUPPRESS_FORWARD_DECL_MEMBER const Ref<WebKit::TestSwiftRefCountedClass>& m_object;
    SUPPRESS_FORWARD_DECL_MEMBER const RefPtr<WebKit::TestSwiftRefCountedClass>& m_optionalObject;
    SUPPRESS_FORWARD_DECL_MEMBER const Vector<Ref<WebKit::TestSwiftRefCountedClass>>& m_objects;
};

class SynchronousMessage {
public:
    using Arguments = std::tuple<WebKit::TestSwiftEnum, OptionSet<WebKit::TestSwiftOptionSet>>;

    static constexpr IPC::MessageName name() { return IPC::MessageName::TestWithSwiftTypes_SynchronousMessage; }
    static constexpr bool isSync = true;
    static constexpr bool canDispatchOutOfOrder = false;
    static constexpr bool replyCanDispatchOutOfOrder = false;
    static constexpr bool deferSendingIfSuspended = false;

    static constexpr auto callbackThread = WTF::CompletionHandlerCallThread::ConstructionThread;
    using ReplyArguments = std::tuple<uint8_t>;
    using Reply = CompletionHandler<void(uint8_t)>;
    SynchronousMessage(WebKit::TestSwiftEnum kind, const OptionSet<WebKit::TestSwiftOptionSet>& options)
        : m_kind(kind)
        , m_options(options)
    {
    }

    template<typename Encoder>
    void encode(Encoder& encoder)
    {
        encoder << m_kind;
        SUPPRESS_FORWARD_DECL_ARG encoder << m_options;
    }

private:
    WebKit::TestSwiftEnum m_kind;
    SUPPRESS_FORWARD_DECL_MEMBER const OptionSet<WebKit::TestSwiftOptionSet>& m_options;
};

class SynchronousMessageWithoutParameters {
public:
    using Arguments = std::tuple<>;

    static constexpr IPC::MessageName name() { return IPC::MessageName::TestWithSwiftTypes_SynchronousMessageWithoutParameters; }
    static constexpr bool isSync = true;
    static constexpr bool canDispatchOutOfOrder = false;
    static constexpr bool replyCanDispatchOutOfOrder = false;
    static constexpr bool deferSendingIfSuspended = false;

    static constexpr auto callbackThread = WTF::CompletionHandlerCallThread::ConstructionThread;
    using ReplyArguments = std::tuple<RefPtr<WebKit::TestSwiftRefCountedClass>, String>;
    using Reply = CompletionHandler<void(RefPtr<WebKit::TestSwiftRefCountedClass>&&, String&&)>;
    SynchronousMessageWithoutParameters()
    {
    }

    template<typename Encoder>
    void encode(Encoder& encoder)
    {
    }

private:
};

class MessageWithCxxTypes {
public:
    using Arguments = std::tuple<Ref<WebKit::FrameState>, uint32_t>;

    static constexpr IPC::MessageName name() { return IPC::MessageName::TestWithSwiftTypes_MessageWithCxxTypes; }
    static constexpr bool isSync = false;
    static constexpr bool canDispatchOutOfOrder = false;
    static constexpr bool replyCanDispatchOutOfOrder = false;
    static constexpr bool deferSendingIfSuspended = false;

    static IPC::MessageName asyncMessageReplyName() { return IPC::MessageName::TestWithSwiftTypes_MessageWithCxxTypesReply; }
    static constexpr auto callbackThread = WTF::CompletionHandlerCallThread::ConstructionThread;
    using ReplyArguments = std::tuple<uint8_t>;
    using Reply = CompletionHandler<void(uint8_t)>;
    using Promise = WTF::NativePromise<uint8_t, IPC::Error>;
    MessageWithCxxTypes(const Ref<WebKit::FrameState>& frameState, uint32_t param)
        : m_frameState(frameState)
        , m_param(param)
    {
    }

    template<typename Encoder>
    void encode(Encoder& encoder)
    {
        SUPPRESS_FORWARD_DECL_ARG encoder << m_frameState;
        encoder << m_param;
    }

private:
    SUPPRESS_FORWARD_DECL_MEMBER const Ref<WebKit::FrameState>& m_frameState;
    uint32_t m_param;
};

class MessageWithReplyReply {
public:
    using Arguments = std::tuple<WebKit::TestSwiftStruct>;

    static constexpr IPC::MessageName name() { return IPC::MessageName::TestWithSwiftTypes_MessageWithReplyReply; }
    static constexpr bool isSync = false;
    static constexpr bool canDispatchOutOfOrder = false;
    static constexpr bool replyCanDispatchOutOfOrder = false;
    static constexpr bool deferSendingIfSuspended = false;

    explicit MessageWithReplyReply(const WebKit::TestSwiftStruct& reply)
        : m_reply(reply)
    {
    }

    template<typename Encoder>
    void encode(Encoder& encoder)
    {
        SUPPRESS_FORWARD_DECL_ARG encoder << m_reply;
    }

private:
    SUPPRESS_FORWARD_DECL_MEMBER const WebKit::TestSwiftStruct& m_reply;
};

class MessageWithMultipleRepliesReply {
public:
    using Arguments = std::tuple<bool, Vector<String>, std::optional<double>>;

    static constexpr IPC::MessageName name() { return IPC::MessageName::TestWithSwiftTypes_MessageWithMultipleRepliesReply; }
    static constexpr bool isSync = false;
    static constexpr bool canDispatchOutOfOrder = false;
    static constexpr bool replyCanDispatchOutOfOrder = false;
    static constexpr bool deferSendingIfSuspended = false;

    MessageWithMultipleRepliesReply(bool found, const Vector<String>& matches, const std::optional<double>& score)
        : m_found(found)
        , m_matches(matches)
        , m_score(score)
    {
    }

    template<typename Encoder>
    void encode(Encoder& encoder)
    {
        encoder << m_found;
        SUPPRESS_FORWARD_DECL_ARG encoder << m_matches;
        SUPPRESS_FORWARD_DECL_ARG encoder << m_score;
    }

private:
    bool m_found;
    SUPPRESS_FORWARD_DECL_MEMBER const Vector<String>& m_matches;
    SUPPRESS_FORWARD_DECL_MEMBER const std::optional<double>& m_score;
};

class MessageWithEmptyReplyReply {
public:
    using Arguments = std::tuple<>;

    static constexpr IPC::MessageName name() { return IPC::MessageName::TestWithSwiftTypes_MessageWithEmptyReplyReply; }
    static constexpr bool isSync = false;
    static constexpr bool canDispatchOutOfOrder = false;
    static constexpr bool replyCanDispatchOutOfOrder = false;
    static constexpr bool deferSendingIfSuspended = false;

    MessageWithEmptyReplyReply()
    {
    }

    template<typename Encoder>
    void encode(Encoder& encoder)
    {
    }

private:
};

class MessageWithRefsReply {
public:
    using Arguments = std::tuple<RefPtr<WebKit::TestSwiftRefCountedClass>>;

    static constexpr IPC::MessageName name() { return IPC::MessageName::TestWithSwiftTypes_MessageWithRefsReply; }
    static constexpr bool isSync = false;
    static constexpr bool canDispatchOutOfOrder = false;
    static constexpr bool replyCanDispatchOutOfOrder = false;
    static constexpr bool deferSendingIfSuspended = false;

    explicit MessageWithRefsReply(const RefPtr<WebKit::TestSwiftRefCountedClass>& reply)
        : m_reply(reply)
    {
    }

    template<typename Encoder>
    void encode(Encoder& encoder)
    {
        SUPPRESS_FORWARD_DECL_ARG encoder << m_reply;
    }

private:
    SUPPRESS_FORWARD_DECL_MEMBER const RefPtr<WebKit::TestSwiftRefCountedClass>& m_reply;
};

class MessageWithCxxTypesReply {
public:
    using Arguments = std::tuple<uint8_t>;

    static constexpr IPC::MessageName name() { return IPC::MessageName::TestWithSwiftTypes_MessageWithCxxTypesReply; }
    static constexpr bool isSync = false;
    static constexpr bool canDispatchOutOfOrder = false;
    static constexpr bool replyCanDispatchOutOfOrder = false;
    static constexpr bool deferSendingIfSuspended = false;

    explicit MessageWithCxxTypesReply(uint8_t reply)
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

} // namespace TestWithSwiftTypes
} // namespace Messages

namespace CompletionHandlers {
namespace TestWithSwiftTypes {
using MessageWithCxxTypesCompletionHandler = WTF::RefCountable<Messages::TestWithSwiftTypes::MessageWithCxxTypes::Reply>;

void completeWithDefaultReply(MessageWithCxxTypesCompletionHandler&);

void complete(MessageWithCxxTypesCompletionHandler&, uint8_t);
} // namespace TestWithSwiftTypes
} // namespace CompletionHandlers

