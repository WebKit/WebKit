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


namespace Messages {
namespace TestWithValidator {

static inline IPC::ReceiverName messageReceiverName()
{
    return IPC::ReceiverName::TestWithValidator;
}

class AlwaysEnabled {
public:
    using Arguments = std::tuple<String>;

    static IPC::MessageName name() { return IPC::MessageName::TestWithValidator_AlwaysEnabled; }
    static constexpr bool isSync = false;
    static constexpr bool canDispatchOutOfOrder = false;
    static constexpr bool replyCanDispatchOutOfOrder = false;
    static constexpr bool deferSendingIfSuspended = false;

    explicit AlwaysEnabled(const String& url)
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

class EnabledIfPassValidation {
public:
    using Arguments = std::tuple<String>;

    static IPC::MessageName name() { return IPC::MessageName::TestWithValidator_EnabledIfPassValidation; }
    static constexpr bool isSync = false;
    static constexpr bool canDispatchOutOfOrder = false;
    static constexpr bool replyCanDispatchOutOfOrder = false;
    static constexpr bool deferSendingIfSuspended = false;

    explicit EnabledIfPassValidation(const String& url)
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

class EnabledIfSomeFeatureEnabledAndPassValidation {
public:
    using Arguments = std::tuple<String>;

    static IPC::MessageName name() { return IPC::MessageName::TestWithValidator_EnabledIfSomeFeatureEnabledAndPassValidation; }
    static constexpr bool isSync = false;
    static constexpr bool canDispatchOutOfOrder = false;
    static constexpr bool replyCanDispatchOutOfOrder = false;
    static constexpr bool deferSendingIfSuspended = false;

    explicit EnabledIfSomeFeatureEnabledAndPassValidation(const String& url)
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

class MessageWithReply {
public:
    using Arguments = std::tuple<String>;

    static IPC::MessageName name() { return IPC::MessageName::TestWithValidator_MessageWithReply; }
    static constexpr bool isSync = false;
    static constexpr bool canDispatchOutOfOrder = false;
    static constexpr bool replyCanDispatchOutOfOrder = false;
    static constexpr bool deferSendingIfSuspended = false;

    static IPC::MessageName asyncMessageReplyName() { return IPC::MessageName::TestWithValidator_MessageWithReplyReply; }
    static constexpr auto callbackThread = WTF::CompletionHandlerCallThread::ConstructionThread;
    using ReplyArguments = std::tuple<String, double>;
    using Reply = CompletionHandler<void(String&&, double)>;
    using Promise = WTF::NativePromise<std::tuple<String, double>, IPC::Error>;
    explicit MessageWithReply(const String& url)
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

class MessageWithReplyReply {
public:
    using Arguments = std::tuple<String, double>;

    static IPC::MessageName name() { return IPC::MessageName::TestWithValidator_MessageWithReplyReply; }
    static constexpr bool isSync = false;
    static constexpr bool canDispatchOutOfOrder = false;
    static constexpr bool replyCanDispatchOutOfOrder = false;
    static constexpr bool deferSendingIfSuspended = false;

    MessageWithReplyReply(const String& reply, double value)
        : m_reply(reply)
        , m_value(value)
    {
    }

    template<typename Encoder>
    void encode(Encoder& encoder)
    {
        encoder << m_reply;
        encoder << m_value;
    }

private:
    const String& m_reply;
    double m_value;
};

} // namespace TestWithValidator
} // namespace Messages
