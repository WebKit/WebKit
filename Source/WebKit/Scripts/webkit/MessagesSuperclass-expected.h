/*
 * Copyright (C) 2010-2018 Apple Inc. All rights reserved.
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
#include <wtf/Forward.h>
#include <wtf/text/WTFString.h>

namespace WebKit {
    enum class TestTwoStateEnum : bool;
}

namespace Messages {
namespace WebPage {

static inline IPC::StringReference messageReceiverName()
{
    return IPC::StringReference("WebPage");
}

class LoadURL {
public:
    typedef std::tuple<const String&> Arguments;

    static IPC::StringReference receiverName() { return messageReceiverName(); }
    static IPC::StringReference name() { return IPC::StringReference("LoadURL"); }
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

    static IPC::StringReference receiverName() { return messageReceiverName(); }
    static IPC::StringReference name() { return IPC::StringReference("TestAsyncMessage"); }
    static const bool isSync = false;

    static void callReply(IPC::Decoder&, CompletionHandler<void(uint64_t&&)>&&);
    static void cancelReply(CompletionHandler<void(uint64_t&&)>&&);
    static IPC::StringReference asyncMessageReplyName() { return { "TestAsyncMessageReply" }; }
    using AsyncReply = CompletionHandler<void(uint64_t result)>;
    static void send(std::unique_ptr<IPC::Encoder>&&, IPC::Connection&, uint64_t result);
    typedef std::tuple<uint64_t&> Reply;
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

} // namespace WebPage
} // namespace Messages
