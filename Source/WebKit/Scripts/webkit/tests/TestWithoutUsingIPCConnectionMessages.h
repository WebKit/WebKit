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
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/text/WTFString.h>


namespace Messages {
namespace TestWithoutUsingIPCConnection {

static inline IPC::ReceiverName messageReceiverName()
{
    return IPC::ReceiverName::TestWithoutUsingIPCConnection;
}

class MessageWithoutArgument {
public:
    using Arguments = std::tuple<>;

    static IPC::MessageName name() { return IPC::MessageName::TestWithoutUsingIPCConnection_MessageWithoutArgument; }
    static constexpr bool isSync = false;
    static constexpr bool canDispatchOutOfOrder = false;
    static constexpr bool replyCanDispatchOutOfOrder = false;

    auto&& arguments()
    {
        return WTFMove(m_arguments);
    }

private:
    std::tuple<> m_arguments;
};

class MessageWithoutArgumentAndEmptyReply {
public:
    using Arguments = std::tuple<>;

    static IPC::MessageName name() { return IPC::MessageName::TestWithoutUsingIPCConnection_MessageWithoutArgumentAndEmptyReply; }
    static constexpr bool isSync = false;
    static constexpr bool canDispatchOutOfOrder = false;
    static constexpr bool replyCanDispatchOutOfOrder = false;

    static IPC::MessageName asyncMessageReplyName() { return IPC::MessageName::TestWithoutUsingIPCConnection_MessageWithoutArgumentAndEmptyReplyReply; }
    static constexpr auto callbackThread = WTF::CompletionHandlerCallThread::ConstructionThread;
    using ReplyArguments = std::tuple<>;
    using Reply = CompletionHandler<void()>;
    using Promise = WTF::NativePromise<void, IPC::Error>;
    auto&& arguments()
    {
        return WTFMove(m_arguments);
    }

private:
    std::tuple<> m_arguments;
};

class MessageWithoutArgumentAndReplyWithArgument {
public:
    using Arguments = std::tuple<>;

    static IPC::MessageName name() { return IPC::MessageName::TestWithoutUsingIPCConnection_MessageWithoutArgumentAndReplyWithArgument; }
    static constexpr bool isSync = false;
    static constexpr bool canDispatchOutOfOrder = false;
    static constexpr bool replyCanDispatchOutOfOrder = false;

    static IPC::MessageName asyncMessageReplyName() { return IPC::MessageName::TestWithoutUsingIPCConnection_MessageWithoutArgumentAndReplyWithArgumentReply; }
    static constexpr auto callbackThread = WTF::CompletionHandlerCallThread::ConstructionThread;
    using ReplyArguments = std::tuple<String>;
    using Reply = CompletionHandler<void(String&&)>;
    using Promise = WTF::NativePromise<String, IPC::Error>;
    auto&& arguments()
    {
        return WTFMove(m_arguments);
    }

private:
    std::tuple<> m_arguments;
};

class MessageWithArgument {
public:
    using Arguments = std::tuple<String>;

    static IPC::MessageName name() { return IPC::MessageName::TestWithoutUsingIPCConnection_MessageWithArgument; }
    static constexpr bool isSync = false;
    static constexpr bool canDispatchOutOfOrder = false;
    static constexpr bool replyCanDispatchOutOfOrder = false;

    explicit MessageWithArgument(const String& argument)
        : m_arguments(argument)
    {
    }

    auto&& arguments()
    {
        return WTFMove(m_arguments);
    }

private:
    std::tuple<const String&> m_arguments;
};

class MessageWithArgumentAndEmptyReply {
public:
    using Arguments = std::tuple<String>;

    static IPC::MessageName name() { return IPC::MessageName::TestWithoutUsingIPCConnection_MessageWithArgumentAndEmptyReply; }
    static constexpr bool isSync = false;
    static constexpr bool canDispatchOutOfOrder = false;
    static constexpr bool replyCanDispatchOutOfOrder = false;

    static IPC::MessageName asyncMessageReplyName() { return IPC::MessageName::TestWithoutUsingIPCConnection_MessageWithArgumentAndEmptyReplyReply; }
    static constexpr auto callbackThread = WTF::CompletionHandlerCallThread::ConstructionThread;
    using ReplyArguments = std::tuple<>;
    using Reply = CompletionHandler<void()>;
    using Promise = WTF::NativePromise<void, IPC::Error>;
    explicit MessageWithArgumentAndEmptyReply(const String& argument)
        : m_arguments(argument)
    {
    }

    auto&& arguments()
    {
        return WTFMove(m_arguments);
    }

private:
    std::tuple<const String&> m_arguments;
};

class MessageWithArgumentAndReplyWithArgument {
public:
    using Arguments = std::tuple<String>;

    static IPC::MessageName name() { return IPC::MessageName::TestWithoutUsingIPCConnection_MessageWithArgumentAndReplyWithArgument; }
    static constexpr bool isSync = false;
    static constexpr bool canDispatchOutOfOrder = false;
    static constexpr bool replyCanDispatchOutOfOrder = false;

    static IPC::MessageName asyncMessageReplyName() { return IPC::MessageName::TestWithoutUsingIPCConnection_MessageWithArgumentAndReplyWithArgumentReply; }
    static constexpr auto callbackThread = WTF::CompletionHandlerCallThread::ConstructionThread;
    using ReplyArguments = std::tuple<String>;
    using Reply = CompletionHandler<void(String&&)>;
    using Promise = WTF::NativePromise<String, IPC::Error>;
    explicit MessageWithArgumentAndReplyWithArgument(const String& argument)
        : m_arguments(argument)
    {
    }

    auto&& arguments()
    {
        return WTFMove(m_arguments);
    }

private:
    std::tuple<const String&> m_arguments;
};

} // namespace TestWithoutUsingIPCConnection
} // namespace Messages
