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
#include "TestWithCVPixelBufferMessagesReplies.h"
#if PLATFORM(COCOA)
#include <WebCore/CVUtilities.h>
#endif
#include <wtf/Forward.h>
#include <wtf/RetainPtr.h>
#include <wtf/ThreadSafeRefCounted.h>


namespace Messages {
namespace TestWithCVPixelBuffer {

static inline IPC::ReceiverName messageReceiverName()
{
    return IPC::ReceiverName::TestWithCVPixelBuffer;
}

#if USE(AVFOUNDATION)
class SendCVPixelBuffer {
public:
    using Arguments = std::tuple<const RetainPtr<CVPixelBufferRef>&>;

    static IPC::MessageName name() { return IPC::MessageName::TestWithCVPixelBuffer_SendCVPixelBuffer; }
    static constexpr bool isSync = false;

    explicit SendCVPixelBuffer(const RetainPtr<CVPixelBufferRef>& s0)
        : m_arguments(s0)
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

#if USE(AVFOUNDATION)
class ReceiveCVPixelBuffer {
public:
    using Arguments = std::tuple<>;

    static IPC::MessageName name() { return IPC::MessageName::TestWithCVPixelBuffer_ReceiveCVPixelBuffer; }
    static constexpr bool isSync = false;

    static void callReply(IPC::Decoder&, CompletionHandler<void(RetainPtr<CVPixelBufferRef>&&)>&&);
    static void cancelReply(CompletionHandler<void(RetainPtr<CVPixelBufferRef>&&)>&&);
    static IPC::MessageName asyncMessageReplyName() { return IPC::MessageName::TestWithCVPixelBuffer_ReceiveCVPixelBufferReply; }
    using AsyncReply = ReceiveCVPixelBufferAsyncReply;
    static constexpr auto callbackThread = WTF::CompletionHandlerCallThread::ConstructionThread;
    using ReplyArguments = std::tuple<RetainPtr<CVPixelBufferRef>>;
    const Arguments& arguments() const
    {
        return m_arguments;
    }

private:
    Arguments m_arguments;
};
#endif

} // namespace TestWithCVPixelBuffer
} // namespace Messages
