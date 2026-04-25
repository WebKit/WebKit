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
#include <wtf/text/WTFString.h>

namespace WebKit {
class TestClassName;
enum class TestTwoStateEnum : bool;
}

namespace Messages {
namespace TestWithSuperclass {

static inline IPC::ReceiverName messageReceiverName()
{
    return IPC::ReceiverName::TestWithSuperclass;
}

class LoadURL {
public:
    using Arguments = std::tuple<String>;

    static IPC::MessageName name() { return IPC::MessageName::TestWithSuperclass_LoadURL; }
    static constexpr bool isSync = false;
    static constexpr bool canDispatchOutOfOrder = false;
    static constexpr bool replyCanDispatchOutOfOrder = false;
    static constexpr bool deferSendingIfSuspended = false;

    explicit LoadURL(const String& url)
        : m_url(url)
    {
    }

    template<typename Encoder>
    void encode(Encoder& encoder)
    {
        encoder << m_url;
    }

private:
    const String& m_url;
};

#if ENABLE(TEST_FEATURE)
class TestAsyncMessage {
public:
    using Arguments = std::tuple<WebKit::TestTwoStateEnum>;

    static IPC::MessageName name() { return IPC::MessageName::TestWithSuperclass_TestAsyncMessage; }
    static constexpr bool isSync = false;
    static constexpr bool canDispatchOutOfOrder = false;
    static constexpr bool replyCanDispatchOutOfOrder = false;
    static constexpr bool deferSendingIfSuspended = false;

    static IPC::MessageName asyncMessageReplyName() { return IPC::MessageName::TestWithSuperclass_TestAsyncMessageReply; }
    static constexpr auto callbackThread = WTF::CompletionHandlerCallThread::MainThread;
    using ReplyArguments = std::tuple<uint64_t>;
    using Reply = CompletionHandler<void(uint64_t)>;
    using Promise = WTF::NativePromise<uint64_t, IPC::Error>;
    explicit TestAsyncMessage(WebKit::TestTwoStateEnum twoStateEnum)
        : m_twoStateEnum(twoStateEnum)
    {
    }

    template<typename Encoder>
    void encode(Encoder& encoder)
    {
        encoder << m_twoStateEnum;
    }

private:
    WebKit::TestTwoStateEnum m_twoStateEnum;
};
#endif

#if ENABLE(TEST_FEATURE)
class TestAsyncMessageWithNoArguments {
public:
    using Arguments = std::tuple<>;

    static IPC::MessageName name() { return IPC::MessageName::TestWithSuperclass_TestAsyncMessageWithNoArguments; }
    static constexpr bool isSync = false;
    static constexpr bool canDispatchOutOfOrder = false;
    static constexpr bool replyCanDispatchOutOfOrder = false;
    static constexpr bool deferSendingIfSuspended = false;

    static IPC::MessageName asyncMessageReplyName() { return IPC::MessageName::TestWithSuperclass_TestAsyncMessageWithNoArgumentsReply; }
    static constexpr auto callbackThread = WTF::CompletionHandlerCallThread::ConstructionThread;
    using ReplyArguments = std::tuple<>;
    using Reply = CompletionHandler<void()>;
    using Promise = WTF::NativePromise<void, IPC::Error>;
    TestAsyncMessageWithNoArguments()
    {
    }

    template<typename Encoder>
    void encode(Encoder& encoder)
    {
    }

private:
};
#endif

#if ENABLE(TEST_FEATURE)
class TestAsyncMessageWithMultipleArguments {
public:
    using Arguments = std::tuple<>;

    static IPC::MessageName name() { return IPC::MessageName::TestWithSuperclass_TestAsyncMessageWithMultipleArguments; }
    static constexpr bool isSync = false;
    static constexpr bool canDispatchOutOfOrder = false;
    static constexpr bool replyCanDispatchOutOfOrder = false;
    static constexpr bool deferSendingIfSuspended = false;

    static IPC::MessageName asyncMessageReplyName() { return IPC::MessageName::TestWithSuperclass_TestAsyncMessageWithMultipleArgumentsReply; }
    static constexpr auto callbackThread = WTF::CompletionHandlerCallThread::ConstructionThread;
    using ReplyArguments = std::tuple<bool, uint64_t>;
    using Reply = CompletionHandler<void(bool, uint64_t)>;
    using Promise = WTF::NativePromise<std::tuple<bool, uint64_t>, IPC::Error>;
    TestAsyncMessageWithMultipleArguments()
    {
    }

    template<typename Encoder>
    void encode(Encoder& encoder)
    {
    }

private:
};
#endif

#if ENABLE(TEST_FEATURE)
class TestAsyncMessageWithConnection {
public:
    using Arguments = std::tuple<int>;

    static IPC::MessageName name() { return IPC::MessageName::TestWithSuperclass_TestAsyncMessageWithConnection; }
    static constexpr bool isSync = false;
    static constexpr bool canDispatchOutOfOrder = false;
    static constexpr bool replyCanDispatchOutOfOrder = false;
    static constexpr bool deferSendingIfSuspended = false;

    static IPC::MessageName asyncMessageReplyName() { return IPC::MessageName::TestWithSuperclass_TestAsyncMessageWithConnectionReply; }
    static constexpr auto callbackThread = WTF::CompletionHandlerCallThread::ConstructionThread;
    using ReplyArguments = std::tuple<bool>;
    using Reply = CompletionHandler<void(bool)>;
    using Promise = WTF::NativePromise<bool, IPC::Error>;
    explicit TestAsyncMessageWithConnection(const int& value)
        : m_value(value)
    {
    }

    template<typename Encoder>
    void encode(Encoder& encoder)
    {
        SUPPRESS_FORWARD_DECL_ARG encoder << m_value;
    }

private:
    SUPPRESS_FORWARD_DECL_MEMBER const int& m_value;
};
#endif

class TestSyncMessage {
public:
    using Arguments = std::tuple<uint32_t>;

    static IPC::MessageName name() { return IPC::MessageName::TestWithSuperclass_TestSyncMessage; }
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

class TestSynchronousMessage {
public:
    using Arguments = std::tuple<bool>;

    static IPC::MessageName name() { return IPC::MessageName::TestWithSuperclass_TestSynchronousMessage; }
    static constexpr bool isSync = true;
    static constexpr bool canDispatchOutOfOrder = false;
    static constexpr bool replyCanDispatchOutOfOrder = false;
    static constexpr bool deferSendingIfSuspended = false;

    static constexpr auto callbackThread = WTF::CompletionHandlerCallThread::ConstructionThread;
    using ReplyArguments = std::tuple<std::optional<WebKit::TestClassName>>;
    using Reply = CompletionHandler<void(std::optional<WebKit::TestClassName>&&)>;
    explicit TestSynchronousMessage(bool value)
        : m_value(value)
    {
    }

    template<typename Encoder>
    void encode(Encoder& encoder)
    {
        encoder << m_value;
    }

private:
    bool m_value;
};

#if ENABLE(TEST_FEATURE)
class TestAsyncMessageReply {
public:
    using Arguments = std::tuple<uint64_t>;

    static IPC::MessageName name() { return IPC::MessageName::TestWithSuperclass_TestAsyncMessageReply; }
    static constexpr bool isSync = false;
    static constexpr bool canDispatchOutOfOrder = false;
    static constexpr bool replyCanDispatchOutOfOrder = false;
    static constexpr bool deferSendingIfSuspended = false;

    explicit TestAsyncMessageReply(uint64_t result)
        : m_result(result)
    {
    }

    template<typename Encoder>
    void encode(Encoder& encoder)
    {
        encoder << m_result;
    }

private:
    uint64_t m_result;
};
#endif

#if ENABLE(TEST_FEATURE)
class TestAsyncMessageWithNoArgumentsReply {
public:
    using Arguments = std::tuple<>;

    static IPC::MessageName name() { return IPC::MessageName::TestWithSuperclass_TestAsyncMessageWithNoArgumentsReply; }
    static constexpr bool isSync = false;
    static constexpr bool canDispatchOutOfOrder = false;
    static constexpr bool replyCanDispatchOutOfOrder = false;
    static constexpr bool deferSendingIfSuspended = false;

    TestAsyncMessageWithNoArgumentsReply()
    {
    }

    template<typename Encoder>
    void encode(Encoder& encoder)
    {
    }

private:
};
#endif

#if ENABLE(TEST_FEATURE)
class TestAsyncMessageWithMultipleArgumentsReply {
public:
    using Arguments = std::tuple<bool, uint64_t>;

    static IPC::MessageName name() { return IPC::MessageName::TestWithSuperclass_TestAsyncMessageWithMultipleArgumentsReply; }
    static constexpr bool isSync = false;
    static constexpr bool canDispatchOutOfOrder = false;
    static constexpr bool replyCanDispatchOutOfOrder = false;
    static constexpr bool deferSendingIfSuspended = false;

    TestAsyncMessageWithMultipleArgumentsReply(bool flag, uint64_t value)
        : m_flag(flag)
        , m_value(value)
    {
    }

    template<typename Encoder>
    void encode(Encoder& encoder)
    {
        encoder << m_flag;
        encoder << m_value;
    }

private:
    bool m_flag;
    uint64_t m_value;
};
#endif

#if ENABLE(TEST_FEATURE)
class TestAsyncMessageWithConnectionReply {
public:
    using Arguments = std::tuple<bool>;

    static IPC::MessageName name() { return IPC::MessageName::TestWithSuperclass_TestAsyncMessageWithConnectionReply; }
    static constexpr bool isSync = false;
    static constexpr bool canDispatchOutOfOrder = false;
    static constexpr bool replyCanDispatchOutOfOrder = false;
    static constexpr bool deferSendingIfSuspended = false;

    explicit TestAsyncMessageWithConnectionReply(bool flag)
        : m_flag(flag)
    {
    }

    template<typename Encoder>
    void encode(Encoder& encoder)
    {
        encoder << m_flag;
    }

private:
    bool m_flag;
};
#endif

} // namespace TestWithSuperclass
} // namespace Messages
