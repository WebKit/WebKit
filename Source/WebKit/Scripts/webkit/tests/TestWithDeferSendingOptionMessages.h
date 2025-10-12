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
namespace TestWithDeferSendingOption {

static inline IPC::ReceiverName messageReceiverName()
{
    return IPC::ReceiverName::TestWithDeferSendingOption;
}

class NoOptions {
public:
    using Arguments = std::tuple<String>;

    static IPC::MessageName name() { return IPC::MessageName::TestWithDeferSendingOption_NoOptions; }
    static constexpr bool isSync = false;
    static constexpr bool canDispatchOutOfOrder = false;
    static constexpr bool replyCanDispatchOutOfOrder = false;
    static constexpr bool deferSendingIfSuspended = false;

    explicit NoOptions(const String& url)
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

class NoIndices {
public:
    using Arguments = std::tuple<String>;

    static IPC::MessageName name() { return IPC::MessageName::TestWithDeferSendingOption_NoIndices; }
    static constexpr bool isSync = false;
    static constexpr bool canDispatchOutOfOrder = false;
    static constexpr bool replyCanDispatchOutOfOrder = false;
    static constexpr bool deferSendingIfSuspended = true;

    explicit NoIndices(const String& url)
        : m_url(url)
    {
    }

    void encodeCoalescingKey(IPC::Encoder&) const
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

class OneIndex {
public:
    using Arguments = std::tuple<String>;

    static IPC::MessageName name() { return IPC::MessageName::TestWithDeferSendingOption_OneIndex; }
    static constexpr bool isSync = false;
    static constexpr bool canDispatchOutOfOrder = false;
    static constexpr bool replyCanDispatchOutOfOrder = false;
    static constexpr bool deferSendingIfSuspended = true;

    explicit OneIndex(const String& url)
        : m_url(url)
    {
    }

    void encodeCoalescingKey(IPC::Encoder& encoder) const
    {
        encoder << m_url;
    }

    template<typename Encoder>
    void encode(Encoder& encoder)
    {
        encoder << m_url;
    }

private:
    const String& m_url;
};

class MultipleIndices {
public:
    using Arguments = std::tuple<String, int, int, int>;

    static IPC::MessageName name() { return IPC::MessageName::TestWithDeferSendingOption_MultipleIndices; }
    static constexpr bool isSync = false;
    static constexpr bool canDispatchOutOfOrder = false;
    static constexpr bool replyCanDispatchOutOfOrder = false;
    static constexpr bool deferSendingIfSuspended = true;

    MultipleIndices(const String& url, const int& foo, const int& bar, const int& baz)
        : m_url(url)
        , m_foo(foo)
        , m_bar(bar)
        , m_baz(baz)
    {
    }

    void encodeCoalescingKey(IPC::Encoder& encoder) const
    {
        SUPPRESS_FORWARD_DECL_ARG encoder << m_bar;
        encoder << m_url;
        SUPPRESS_FORWARD_DECL_ARG encoder << m_foo;
    }

    template<typename Encoder>
    void encode(Encoder& encoder)
    {
        encoder << m_url;
        SUPPRESS_FORWARD_DECL_ARG encoder << m_foo;
        SUPPRESS_FORWARD_DECL_ARG encoder << m_bar;
        SUPPRESS_FORWARD_DECL_ARG encoder << m_baz;
    }

private:
    const String& m_url;
    SUPPRESS_FORWARD_DECL_MEMBER const int& m_foo;
    SUPPRESS_FORWARD_DECL_MEMBER const int& m_bar;
    SUPPRESS_FORWARD_DECL_MEMBER const int& m_baz;
};

} // namespace TestWithDeferSendingOption
} // namespace Messages
