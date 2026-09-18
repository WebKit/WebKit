/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "SwiftDeferredReplySupport.h"

#include <wtf/HashMap.h>
#include <wtf/MainThread.h>
#include <wtf/NeverDestroyed.h>

namespace TestWebKitAPI {

void CxxDeferredReplyReceiver::handleMessageDeferringItsReply(DeferredReply& reply)
{
    m_handlerStartedOnMainThread = isMainThread();
    m_pendingReply = &reply;
}

void CxxDeferredReplyReceiver::handleMessageReleasingTheDeferredReply()
{
    RefPtr reply = std::exchange(m_pendingReply, nullptr);
    if (!reply)
        return;
    m_handlerResumedOnMainThread = isMainThread();
    m_handlerDidResume = true;
    reply->answer(deferredReplyValue);
}

HashMap<uint64_t, Ref<DeferredReply>>& deferredRepliesForSwift()
{
    static NeverDestroyed<HashMap<uint64_t, Ref<DeferredReply>>> replies;
    return replies.get();
}

uint64_t registerDeferredReplyForSwift(DeferredReply& reply)
{
    static uint64_t nextToken = 1;
    auto token = nextToken++;
    deferredRepliesForSwift().add(token, Ref { reply });
    return token;
}

void forgetDeferredRepliesForSwift()
{
    deferredRepliesForSwift().clear();
}

CxxDeferredReplyReceiver& cxxDeferredReplyReceiver()
{
    static NeverDestroyed<CxxDeferredReplyReceiver> receiver;
    return receiver.get();
}

void resetCxxDeferredReplyReceiver()
{
    cxxDeferredReplyReceiver() = CxxDeferredReplyReceiver { };
}

} // namespace TestWebKitAPI

void answerDeferredReplyForSwift(uint64_t token)
{
    if (RefPtr reply = TestWebKitAPI::deferredRepliesForSwift().take(token))
        reply->answer(TestWebKitAPI::deferredReplyValue);
}
