/*
 * Copyright (C) 2010-2020 Apple Inc. All rights reserved.
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
#include "TestClassName.h"
#include "WebPageMessagesReplies.h"
#include <wtf/Forward.h>
#include <wtf/Optional.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/text/WTFString.h>

namespace WebKit {
enum class TestTwoStateEnum : bool;
}

namespace Messages {
namespace WebPage {

static inline IPC::ReceiverName messageReceiverName()
{
    return IPC::ReceiverName::WebPage;
}

class LoadURL {
public:
    typedef std::tuple<const String&> Arguments;

    static IPC::MessageName name() { return IPC::MessageName::WebPage_LoadURL; }
    static const bool isSync = false;

    explicit LoadURL(const String& url)
        : m_arguments(url)
    {
    }

    const Arguments& arguments() const
    {
        return m_arguments;
    }

private:
    Arguments m_arguments;
};

#if ENABLE(TEST_FEATURE)
class TestAsyncMessage {
public:
    typedef std::tuple<WebKit::TestTwoStateEnum> Arguments;

    static IPC::MessageName name() { return IPC::MessageName::WebPage_TestAsyncMessage; }
    static const bool isSync = false;

    static void callReply(IPC::Decoder&, CompletionHandler<void(uint64_t&&)>&&);
    static void cancelReply(CompletionHandler<void(uint64_t&&)>&&);
    static IPC::MessageName asyncMessageReplyName() { return IPC::MessageName::WebPage_TestAsyncMessageReply; }
    using AsyncReply = TestAsyncMessageAsyncReply;
    static void send(std::unique_ptr<IPC::Encoder>&&, IPC::Connection&, uint64_t result);
    using Reply = std::tuple<uint64_t&>;
    using ReplyArguments = std::tuple<uint64_t>;
    explicit TestAsyncMessage(WebKit::TestTwoStateEnum twoStateEnum)
        : m_arguments(twoStateEnum)
    {
    }

    const Arguments& arguments() const
    {
        return m_arguments;
    }

private:
    Arguments m_arguments;
};
#endif

#if ENABLE(TEST_FEATURE)
class TestAsyncMessageWithNoArguments {
public:
    typedef std::tuple<> Arguments;

    static IPC::MessageName name() { return IPC::MessageName::WebPage_TestAsyncMessageWithNoArguments; }
    static const bool isSync = false;

    static void callReply(IPC::Decoder&, CompletionHandler<void()>&&);
    static void cancelReply(CompletionHandler<void()>&&);
    static IPC::MessageName asyncMessageReplyName() { return IPC::MessageName::WebPage_TestAsyncMessageWithNoArgumentsReply; }
    using AsyncReply = TestAsyncMessageWithNoArgumentsAsyncReply;
    static void send(std::unique_ptr<IPC::Encoder>&&, IPC::Connection&);
    using Reply = std::tuple<>;
    using ReplyArguments = std::tuple<>;
    const Arguments& arguments() const
    {
        return m_arguments;
    }

private:
    Arguments m_arguments;
};
#endif

#if ENABLE(TEST_FEATURE)
class TestAsyncMessageWithMultipleArguments {
public:
    typedef std::tuple<> Arguments;

    static IPC::MessageName name() { return IPC::MessageName::WebPage_TestAsyncMessageWithMultipleArguments; }
    static const bool isSync = false;

    static void callReply(IPC::Decoder&, CompletionHandler<void(bool&&, uint64_t&&)>&&);
    static void cancelReply(CompletionHandler<void(bool&&, uint64_t&&)>&&);
    static IPC::MessageName asyncMessageReplyName() { return IPC::MessageName::WebPage_TestAsyncMessageWithMultipleArgumentsReply; }
    using AsyncReply = TestAsyncMessageWithMultipleArgumentsAsyncReply;
    static void send(std::unique_ptr<IPC::Encoder>&&, IPC::Connection&, bool flag, uint64_t value);
    using Reply = std::tuple<bool&, uint64_t&>;
    using ReplyArguments = std::tuple<bool, uint64_t>;
    const Arguments& arguments() const
    {
        return m_arguments;
    }

private:
    Arguments m_arguments;
};
#endif

#if ENABLE(TEST_FEATURE)
class TestAsyncMessageWithConnection {
public:
    typedef std::tuple<const int&> Arguments;

    static IPC::MessageName name() { return IPC::MessageName::WebPage_TestAsyncMessageWithConnection; }
    static const bool isSync = false;

    static void callReply(IPC::Decoder&, CompletionHandler<void(bool&&)>&&);
    static void cancelReply(CompletionHandler<void(bool&&)>&&);
    static IPC::MessageName asyncMessageReplyName() { return IPC::MessageName::WebPage_TestAsyncMessageWithConnectionReply; }
    using AsyncReply = TestAsyncMessageWithConnectionAsyncReply;
    static void send(std::unique_ptr<IPC::Encoder>&&, IPC::Connection&, bool flag);
    using Reply = std::tuple<bool&>;
    using ReplyArguments = std::tuple<bool>;
    explicit TestAsyncMessageWithConnection(const int& value)
        : m_arguments(value)
    {
    }

    const Arguments& arguments() const
    {
        return m_arguments;
    }

private:
    Arguments m_arguments;
};
#endif

class TestSyncMessage {
public:
    typedef std::tuple<uint32_t> Arguments;

    static IPC::MessageName name() { return IPC::MessageName::WebPage_TestSyncMessage; }
    static const bool isSync = true;

    using DelayedReply = TestSyncMessageDelayedReply;
    static void send(std::unique_ptr<IPC::Encoder>&&, IPC::Connection&, uint8_t reply);
    using Reply = std::tuple<uint8_t&>;
    using ReplyArguments = std::tuple<uint8_t>;
    explicit TestSyncMessage(uint32_t param)
        : m_arguments(param)
    {
    }

    const Arguments& arguments() const
    {
        return m_arguments;
    }

private:
    Arguments m_arguments;
};

class TestSynchronousMessage {
public:
    typedef std::tuple<bool> Arguments;

    static IPC::MessageName name() { return IPC::MessageName::WebPage_TestSynchronousMessage; }
    static const bool isSync = true;

    using DelayedReply = TestSynchronousMessageDelayedReply;
    static void send(std::unique_ptr<IPC::Encoder>&&, IPC::Connection&, const Optional<WebKit::TestClassName>& optionalReply);
    using Reply = std::tuple<Optional<WebKit::TestClassName>&>;
    using ReplyArguments = std::tuple<Optional<WebKit::TestClassName>>;
    explicit TestSynchronousMessage(bool value)
        : m_arguments(value)
    {
    }

    const Arguments& arguments() const
    {
        return m_arguments;
    }

private:
    Arguments m_arguments;
};

} // namespace WebPage
} // namespace Messages
