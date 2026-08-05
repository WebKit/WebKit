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
#include "StreamMessageReceiver.h"
#include <wtf/Forward.h>
#include <wtf/RuntimeApplicationChecks.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/text/WTFString.h>

namespace WebKit {
class TestWithStreamSwiftEnabledBy;
class TestWithStreamSwiftEnabledByMessageForwarder;
class TestWithStreamSwiftEnabledByWeakRef;
}

namespace WebKit {

class TestWithStreamSwiftEnabledByMessageForwarder final : public IPC::StreamMessageReceiver {
public:
    static Ref<TestWithStreamSwiftEnabledByMessageForwarder> createFromWeak(WebKit::TestWithStreamSwiftEnabledByWeakRef* _Nonnull handler)
    {
        return adoptRef(*new TestWithStreamSwiftEnabledByMessageForwarder(handler));
    }
    ~TestWithStreamSwiftEnabledByMessageForwarder();
    void didReceiveStreamMessage(IPC::StreamServerConnection&, IPC::Decoder&) final;
    void ref() const { StreamMessageReceiver::ref(); }
    void deref() const { StreamMessageReceiver::deref(); }
private:
    TestWithStreamSwiftEnabledByMessageForwarder(WebKit::TestWithStreamSwiftEnabledByWeakRef* _Nonnull);
    std::unique_ptr<WebKit::TestWithStreamSwiftEnabledBy> getMessageTarget();
    std::unique_ptr<WebKit::TestWithStreamSwiftEnabledByWeakRef> m_handler;
} SWIFT_SHARED_REFERENCE(.ref, .deref);

}

using RefTestWithStreamSwiftEnabledByMessageForwarder = Ref<WebKit::TestWithStreamSwiftEnabledByMessageForwarder>;

namespace Messages {
namespace TestWithStreamSwiftEnabledBy {

static inline IPC::ReceiverName messageReceiverName()
{
    return IPC::ReceiverName::TestWithStreamSwiftEnabledBy;
}

class SendString {
public:
    using Arguments = std::tuple<String>;

    static IPC::MessageName name() { return IPC::MessageName::TestWithStreamSwiftEnabledBy_SendString; }
    static constexpr bool isSync = false;
    static constexpr bool canDispatchOutOfOrder = false;
    static constexpr bool replyCanDispatchOutOfOrder = false;
    static constexpr bool deferSendingIfSuspended = false;
    static constexpr bool isStreamEncodable = true;
    static constexpr bool isStreamBatched = false;

    explicit SendString(const String& url)
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

} // namespace TestWithStreamSwiftEnabledBy
} // namespace Messages

namespace CompletionHandlers {
namespace TestWithStreamSwiftEnabledBy {

} // namespace TestWithStreamSwiftEnabledBy
} // namespace CompletionHandlers

