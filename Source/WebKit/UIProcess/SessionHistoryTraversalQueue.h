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

#include <WebCore/BackForwardFrameItemIdentifier.h>
#include <WebCore/NavigationIdentifier.h>
#include <wtf/CheckedPtr.h>
#include <wtf/CompletionHandler.h>
#include <wtf/Deque.h>
#include <wtf/Markable.h>
#include <wtf/RunLoop.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/WeakPtr.h>
#include <wtf/WeakRef.h>

namespace API {
class Navigation;
}

namespace WebKit {

class WebPageProxy;

enum class FrameTraversalResult : uint8_t;

class SessionHistoryTraversalQueue final : public CanMakeCheckedPtr<SessionHistoryTraversalQueue>, public CanMakeWeakPtr<SessionHistoryTraversalQueue> {
    WTF_MAKE_TZONE_ALLOCATED(SessionHistoryTraversalQueue);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(SessionHistoryTraversalQueue);
public:
    explicit SessionHistoryTraversalQueue(WebPageProxy&);
    ~SessionHistoryTraversalQueue();

    void enqueueDelta(int32_t delta);
    void enqueueItem(WebCore::BackForwardFrameItemIdentifier, CompletionHandler<void(bool didTakeTraversal)>&&);
    void cancel();

    void frameDidHandleTraversal(WebCore::NavigationIdentifier, FrameTraversalResult, bool isMainFrame);
    void mainFrameLoadDidFinish();
    void navigationWasDestroyed(WebCore::NavigationIdentifier);

    // -1 for a back traversal, +1 for forward, and 0 when no traversal is in flight.
    int32_t inFlightDirection() const { return m_inFlightTraversal ? m_inFlightTraversal->direction : 0; }

private:
    void scheduleFlush();
    void flush();
    CompletionHandler<void()> startTraversal(int8_t direction, CompletionHandler<void(bool didTakeTraversal)>&& = { });
    void traversalWasDispatched(API::Navigation*);
    void finishTraversalIfDone();

    struct ItemTraversal {
        WebCore::BackForwardFrameItemIdentifier frameItemID;
        CompletionHandler<void(bool)> completionHandler;
    };

    struct InFlightTraversal {
        uint64_t identifier { 0 };
        int8_t direction { 0 };
        Markable<WebCore::NavigationIdentifier> navigationID;
        bool isWaitingForFrames { true };
        bool isWaitingForMainFrameLoad { false };
        bool didAnyFrameTakeTraversal { false };
        CompletionHandler<void(bool)> completionHandler;
    };

    WeakRef<WebPageProxy> m_pageProxy;
    RunLoop::Timer m_flushTimer;
    Deque<Variant<int32_t, ItemTraversal>> m_pendingTraversals;
    std::optional<InFlightTraversal> m_inFlightTraversal;
    uint64_t m_lastTraversalIdentifier { 0 };
};

} // namespace WebKit
