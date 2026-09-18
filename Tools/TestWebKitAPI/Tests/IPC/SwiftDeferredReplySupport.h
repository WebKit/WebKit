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

#pragma once

#include <cstdint>
#include <wtf/CompletionHandler.h>
#include <wtf/HashMap.h>
#include <wtf/Ref.h>
#include <wtf/RefPtr.h>
#include <wtf/SwiftBridging.h>
#include <wtf/ThreadSafeRefCounted.h>

namespace TestWebKitAPI {

// What both receivers reply with, so a test can tell a real reply from a default-constructed one.
constexpr uint64_t deferredReplyValue = 4242;

// The reply to a message a receiver cannot answer yet. It holds the WTF::CompletionHandler that a
// generated receiver would be handed, and is refcounted so it can outlive the dispatch which created
// it.
//
// A generated receiver takes WTF::RefCountable<Reply> directly. That cannot be used here: Swift
// imports the alias for it under cmake but not under Xcode, where the template instantiation does
// not come through. This wrapper is a plain class, so both agree on it.
class DeferredReply final : public ThreadSafeRefCounted<DeferredReply> {
public:
    static Ref<DeferredReply> create(CompletionHandler<void(uint64_t)>&& completionHandler)
    {
        return adoptRef(*new DeferredReply(WTF::move(completionHandler)));
    }

    // Swift looks for these on the class itself, not on the base. Same workaround as
    // WTF::RefCountable (rdar://165684636).
    void ref() const { ThreadSafeRefCounted<DeferredReply>::ref(); }
    void deref() const { ThreadSafeRefCounted<DeferredReply>::deref(); }

    void answer(uint64_t value)
    {
        if (m_completionHandler)
            m_completionHandler(value);
    }

private:
    explicit DeferredReply(CompletionHandler<void(uint64_t)>&& completionHandler)
        : m_completionHandler(WTF::move(completionHandler))
    {
    }

    CompletionHandler<void(uint64_t)> m_completionHandler;
} SWIFT_SHARED_REFERENCE(.ref, .deref);

// A C++ receiver which cannot answer until a later message says so. The Swift receiver in
// SwiftDeferredReplyReceiver.swift does the same with an async handler, so the tests can compare
// them.
class CxxDeferredReplyReceiver {
public:
    void handleMessageDeferringItsReply(DeferredReply&);
    void handleMessageReleasingTheDeferredReply();

    bool handlerStartedOnMainThread() const { return m_handlerStartedOnMainThread; }
    bool handlerResumedOnMainThread() const { return m_handlerResumedOnMainThread; }
    bool handlerDidResume() const { return m_handlerDidResume; }

private:
    RefPtr<DeferredReply> m_pendingReply;
    bool m_handlerStartedOnMainThread { false };
    bool m_handlerResumedOnMainThread { false };
    bool m_handlerDidResume { false };
};

CxxDeferredReplyReceiver& cxxDeferredReplyReceiver();
void resetCxxDeferredReplyReceiver();

// A generated receiver hands Swift the reply itself, as WTF::RefCountable<Reply>. Swift cannot see
// that here: under Xcode the bridging header pulls the WTF headers in textually rather than as a
// module, and a class whose base and members come from them does not import, though the namespace
// around it does. So Swift gets a token for a reply this side is holding, and answers through these.
uint64_t registerDeferredReplyForSwift(DeferredReply&);
HashMap<uint64_t, Ref<DeferredReply>>& deferredRepliesForSwift();
void forgetDeferredRepliesForSwift();

} // namespace TestWebKitAPI

// Global rather than in TestWebKitAPI because Swift resolves a namespace member from a bridging
// header inconsistently between the two build systems.
void answerDeferredReplyForSwift(uint64_t token);
