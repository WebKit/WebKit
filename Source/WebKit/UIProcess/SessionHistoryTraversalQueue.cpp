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
#include "SessionHistoryTraversalQueue.h"

#include "APINavigation.h"
#include "GoToBackForwardItemParameters.h"
#include "WebPageProxy.h"
#include <utility>
#include <wtf/CheckedArithmetic.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebKit {

WTF_MAKE_TZONE_ALLOCATED_IMPL(SessionHistoryTraversalQueue);

SessionHistoryTraversalQueue::SessionHistoryTraversalQueue(WebPageProxy& page)
    : m_pageProxy(page)
    , m_flushTimer(RunLoop::mainSingleton(), "SessionHistoryTraversalQueue::FlushTimer"_s, this, &SessionHistoryTraversalQueue::flush)
{
}

SessionHistoryTraversalQueue::~SessionHistoryTraversalQueue()
{
    cancel();
}

void SessionHistoryTraversalQueue::enqueueDelta(int32_t delta)
{
    if (!m_pendingTraversals.isEmpty()) {
        if (auto* pendingDelta = std::get_if<int32_t>(&m_pendingTraversals.last())) {
            auto sum = WTF::CheckedInt32 { *pendingDelta } + delta;
            if (sum.hasOverflowed()) {
                m_pendingTraversals.removeLast();
                return;
            }
            *pendingDelta = sum.value();
            scheduleFlush();
            return;
        }
    }

    m_pendingTraversals.append(delta);
    scheduleFlush();
}

void SessionHistoryTraversalQueue::enqueueItem(WebCore::BackForwardFrameItemIdentifier frameItemID, CompletionHandler<void(bool)>&& completionHandler)
{
    m_pendingTraversals.append(ItemTraversal { frameItemID, WTF::move(completionHandler) });
    scheduleFlush();
}

void SessionHistoryTraversalQueue::cancel()
{
    m_flushTimer.stop();
    if (m_inFlightTraversal) {
        if (auto completionHandler = std::exchange(m_inFlightTraversal->completionHandler, { }))
            completionHandler(false);
    }
    m_inFlightTraversal = std::nullopt;
    for (auto& traversal : std::exchange(m_pendingTraversals, { })) {
        if (auto* itemTraversal = std::get_if<ItemTraversal>(&traversal))
            itemTraversal->completionHandler(false);
    }
}

void SessionHistoryTraversalQueue::frameDidHandleTraversal(WebCore::NavigationIdentifier navigationID, FrameTraversalResult result, bool isMainFrame)
{
    if (!m_inFlightTraversal || m_inFlightTraversal->navigationID != navigationID)
        return;

    if (result != FrameTraversalResult::NotTaken)
        m_inFlightTraversal->didAnyFrameTakeTraversal = true;
    if (isMainFrame && result == FrameTraversalResult::StartedLoad)
        m_inFlightTraversal->isWaitingForMainFrameLoad = true;
}

void SessionHistoryTraversalQueue::mainFrameLoadDidFinish()
{
    if (!m_inFlightTraversal || !m_inFlightTraversal->isWaitingForMainFrameLoad)
        return;

    m_inFlightTraversal->isWaitingForMainFrameLoad = false;
    finishTraversalIfDone();
}

void SessionHistoryTraversalQueue::navigationWasDestroyed(WebCore::NavigationIdentifier navigationID)
{
    if (m_inFlightTraversal && m_inFlightTraversal->navigationID == navigationID)
        mainFrameLoadDidFinish();
}

void SessionHistoryTraversalQueue::scheduleFlush()
{
    // While a traversal is in flight the back/forward list's current index has not advanced, so a delta resolved now
    // would recompute against the stale index, and a later traversal must not overtake it; hold both until it finishes.
    if (!m_inFlightTraversal && !m_pendingTraversals.isEmpty())
        m_flushTimer.startOneShot(0_s);
}

void SessionHistoryTraversalQueue::flush()
{
    Ref pageProxy = m_pageProxy;
    while (!m_inFlightTraversal && !m_pendingTraversals.isEmpty()) {
        auto traversal = m_pendingTraversals.takeFirst();
        WTF::switchOn(traversal, [&](int32_t delta) {
            if (!delta)
                return;
            auto framesDidHandleTraversal = startTraversal(delta < 0 ? -1 : 1);
            traversalWasDispatched(pageProxy->goToBackForwardItemAtIndexForTraversal(delta, WTF::move(framesDidHandleTraversal)).get());
        }, [&](ItemTraversal& itemTraversal) {
            auto framesDidHandleTraversal = startTraversal(0, WTF::move(itemTraversal.completionHandler));
            traversalWasDispatched(pageProxy->goToBackForwardItemForTraversal(itemTraversal.frameItemID, WTF::move(framesDidHandleTraversal)).get());
        });
    }
}

CompletionHandler<void()> SessionHistoryTraversalQueue::startTraversal(int8_t direction, CompletionHandler<void(bool)>&& completionHandler)
{
    auto identifier = ++m_lastTraversalIdentifier;
    m_inFlightTraversal = InFlightTraversal { identifier, direction, { }, true, false, false, WTF::move(completionHandler) };
    return [weakThis = WeakPtr { *this }, identifier] {
        CheckedPtr checkedThis = weakThis;
        if (!checkedThis || !checkedThis->m_inFlightTraversal || checkedThis->m_inFlightTraversal->identifier != identifier)
            return;
        auto& traversal = *checkedThis->m_inFlightTraversal;
        traversal.isWaitingForFrames = false;
        // A traversal that no frame took, for example because the main frame's navigate event canceled it before the other
        // frames got it, never reaches the frame that asked for it, so its promises are rejected through this reply.
        if (auto completionHandler = std::exchange(traversal.completionHandler, { }))
            completionHandler(traversal.didAnyFrameTakeTraversal);
        checkedThis->finishTraversalIfDone();
    };
}

void SessionHistoryTraversalQueue::traversalWasDispatched(API::Navigation* navigation)
{
    // The frames may already have handled a traversal that reached none of them, which ends it before this point.
    if (navigation && m_inFlightTraversal)
        m_inFlightTraversal->navigationID = navigation->navigationID();
}

void SessionHistoryTraversalQueue::finishTraversalIfDone()
{
    if (m_inFlightTraversal->isWaitingForFrames || m_inFlightTraversal->isWaitingForMainFrameLoad)
        return;

    m_inFlightTraversal = std::nullopt;
    scheduleFlush();
}

} // namespace WebKit
