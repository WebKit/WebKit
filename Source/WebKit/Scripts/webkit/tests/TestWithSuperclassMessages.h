/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
#include "TestWithSuperclassMessagesReplies.h"
#include <optional>
#include <wtf/Forward.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/text/WTFString.h>

namespace WebKit {
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
    using Arguments = std::tuple<const String&>;

    static IPC::MessageName name() { return IPC::MessageName::TestWithSuperclass_LoadURL; }
    static constexpr bool isSync = false;

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
    using Arguments = std::tuple<WebKit::TestTwoStateEnum>;

    static IPC::MessageName name() { return IPC::MessageName::TestWithSuperclass_TestAsyncMessage; }
    static constexpr bool isSync = false;

    static IPC::MessageName asyncMessageReplyName() { return IPC::MessageName::TestWithSuperclass_TestAsyncMessageReply; }
    using AsyncReply = TestAsyncMessageAsyncReply;
    static constexpr auto callbackThread = WTF::CompletionHandlerCallThread::MainThread;
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
    using Arguments = std::tuple<>;

    static IPC::MessageName name() { return IPC::MessageName::TestWithSuperclass_TestAsyncMessageWithNoArguments; }
    static constexpr bool isSync = false;

    static IPC::MessageName asyncMessageReplyName() { return IPC::MessageName::TestWithSuperclass_TestAsyncMessageWithNoArgumentsReply; }
    using AsyncReply = TestAsyncMessageWithNoArgumentsAsyncReply;
    static constexpr auto callbackThread = WTF::CompletionHandlerCallThread::ConstructionThread;
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
    using Arguments = std::tuple<>;

    static IPC::MessageName name() { return IPC::MessageName::TestWithSuperclass_TestAsyncMessageWithMultipleArguments; }
    static constexpr bool isSync = false;

    static IPC::MessageName asyncMessageReplyName() { return IPC::MessageName::TestWithSuperclass_TestAsyncMessageWithMultipleArgumentsReply; }
    using AsyncReply = TestAsyncMessageWithMultipleArgumentsAsyncReply;
    static constexpr auto callbackThread = WTF::CompletionHandlerCallThread::ConstructionThread;
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
    using Arguments = std::tuple<const int&>;

    static IPC::MessageName name() { return IPC::MessageName::TestWithSuperclass_TestAsyncMessageWithConnection; }
    static constexpr bool isSync = false;

    static IPC::MessageName asyncMessageReplyName() { return IPC::MessageName::TestWithSuperclass_TestAsyncMessageWithConnectionReply; }
    using AsyncReply = TestAsyncMessageWithConnectionAsyncReply;
    static constexpr auto callbackThread = WTF::CompletionHandlerCallThread::ConstructionThread;
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
    using Arguments = std::tuple<uint32_t>;

    static IPC::MessageName name() { return IPC::MessageName::TestWithSuperclass_TestSyncMessage; }
    static constexpr bool isSync = true;

    using DelayedReply = TestSyncMessageDelayedReply;
    static constexpr auto callbackThread = WTF::CompletionHandlerCallThread::ConstructionThread;
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
    using Arguments = std::tuple<bool>;

    static IPC::MessageName name() { return IPC::MessageName::TestWithSuperclass_TestSynchronousMessage; }
    static constexpr bool isSync = true;

    using DelayedReply = TestSynchronousMessageDelayedReply;
    static constexpr auto callbackThread = WTF::CompletionHandlerCallThread::ConstructionThread;
    using ReplyArguments = std::tuple<std::optional<WebKit::TestClassName>>;
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

} // namespace TestWithSuperclass
} // namespace Messages
