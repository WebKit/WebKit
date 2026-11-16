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
#include "WebBackForwardList.h"
#include "WebBackForwardListFrameItem.h"
#include "WebBackForwardListItem.h"
#include "WebPageProxy.h"
#include <utility>
#include <wtf/CheckedArithmetic.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebKit {

WTF_MAKE_TZONE_ALLOCATED_IMPL(SessionHistoryTraversalQueue);

// A web process can name a frame's entry only by its frame item identifier: the UI process files the first entry of a
// cross-process iframe under its parent's back/forward item, whose identifier the iframe's process never learns.
static RefPtr<WebBackForwardListFrameItem> frameItemInBackForwardList(WebBackForwardListWrapper& backForwardList, WebCore::BackForwardFrameItemIdentifier frameItemID)
{
    for (int32_t direction : { -1, 1 }) {
        for (int32_t delta = direction < 0 ? 0 : 1; RefPtr item = backForwardList.itemAtDeltaFromCurrentIndex(delta, AllowSkippingBackForwardItems::No); delta += direction) {
            if (RefPtr frameItem = protect(item->mainFrameItem())->childItemForFrameItemID(frameItemID))
                return frameItem;
        }
    }
    return nullptr;
}

// A frame's session history entry is part of every back/forward item made while it was current. Like the
// disambiguate-* WPTs expect, a traversal to it goes to the nearest of those items, and to none when the item at
// baseDelta holds it.
static std::optional<int32_t> deltaToNearestItemContaining(WebBackForwardListWrapper& backForwardList, WebCore::BackForwardFrameItemIdentifier frameItemID, int32_t baseDelta)
{
    RefPtr frameItem = frameItemInBackForwardList(backForwardList, frameItemID);
    if (!frameItem)
        return std::nullopt;

    auto itemSequenceNumber = frameItem->frameState().itemSequenceNumber;
    for (int32_t direction : { -1, 1 }) {
        for (int32_t delta = direction < 0 ? baseDelta : baseDelta + 1; RefPtr item = backForwardList.itemAtDeltaFromCurrentIndex(delta, AllowSkippingBackForwardItems::No); delta += direction) {
            if (protect(item->mainFrameItem())->containsItemSequenceNumber(itemSequenceNumber))
                return delta;
        }
    }
    return std::nullopt;
}

static std::optional<int32_t> deltaToItem(WebBackForwardListWrapper& backForwardList, WebBackForwardListItem& targetItem)
{
    for (int32_t direction : { -1, 1 }) {
        for (int32_t delta = direction < 0 ? 0 : 1; RefPtr item = backForwardList.itemAtDeltaFromCurrentIndex(delta, AllowSkippingBackForwardItems::No); delta += direction) {
            if (item == &targetItem)
                return delta;
        }
    }
    return std::nullopt;
}

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

void SessionHistoryTraversalQueue::mainFrameLoadDidFinish(std::optional<WebCore::NavigationIdentifier> navigationID)
{
    if (!m_inFlightTraversal || !m_inFlightTraversal->isWaitingForMainFrameLoad)
        return;

    if (navigationID && m_inFlightTraversal->navigationID && *navigationID != *m_inFlightTraversal->navigationID)
        return;

    m_inFlightTraversal->isWaitingForMainFrameLoad = false;
    finishTraversalIfDone();
}

void SessionHistoryTraversalQueue::navigationWasDestroyed(WebCore::NavigationIdentifier navigationID)
{
    if (m_inFlightTraversal && m_inFlightTraversal->navigationID == navigationID)
        mainFrameLoadDidFinish(navigationID);
}

bool SessionHistoryTraversalQueue::canBeSuperseded() const
{
    return m_inFlightTraversal && !m_inFlightTraversal->isWaitingForFrames && m_inFlightTraversal->isWaitingForMainFrameLoad && m_inFlightTraversal->targetItem && m_inFlightTraversal->committedItem;
}

void SessionHistoryTraversalQueue::scheduleFlush()
{
    if ((!m_inFlightTraversal || canBeSuperseded()) && !m_pendingTraversals.isEmpty())
        m_flushTimer.startOneShot(0_s);
}

void SessionHistoryTraversalQueue::flush()
{
    Ref pageProxy = m_pageProxy;
    Ref backForwardList = pageProxy->backForwardListWrapper();
    while (!m_pendingTraversals.isEmpty()) {
        int32_t baseDelta = 0;
        RefPtr<WebBackForwardListItem> committedItem;
        if (m_inFlightTraversal) {
            if (!canBeSuperseded())
                return;
            auto deltaToTarget = deltaToItem(backForwardList, Ref { *m_inFlightTraversal->targetItem });
            if (!deltaToTarget)
                return;
            baseDelta = *deltaToTarget;
            committedItem = m_inFlightTraversal->committedItem;
        }

        std::optional<int32_t> delta;
        WTF::switchOn(m_pendingTraversals.first(), [&](int32_t pendingDelta) {
            auto sum = WTF::CheckedInt32 { baseDelta } + pendingDelta;
            if (pendingDelta && !sum.hasOverflowed())
                delta = sum.value();
        }, [&](ItemTraversal& itemTraversal) {
            // Not a message check: the entry may have left the back/forward list since the web process looked it up.
            delta = deltaToNearestItemContaining(backForwardList, itemTraversal.frameItemID, baseDelta);
            if (delta && *delta == baseDelta)
                delta = std::nullopt;
        });

        if (delta && committedItem && backForwardList->itemAtDeltaFromCurrentIndex(*delta, AllowSkippingBackForwardItems::No) == committedItem)
            return;

        auto traversal = m_pendingTraversals.takeFirst();
        auto* itemTraversal = std::get_if<ItemTraversal>(&traversal);
        if (!delta || !*delta) {
            if (itemTraversal)
                itemTraversal->completionHandler(false);
            continue;
        }

        auto framesDidHandleTraversal = startTraversal(*delta < 0 ? -1 : 1, itemTraversal ? WTF::move(itemTraversal->completionHandler) : CompletionHandler<void(bool)> { });
        traversalWasDispatched(pageProxy->goToBackForwardItemAtIndexForTraversal(*delta, WTF::move(framesDidHandleTraversal), committedItem.get()).get());
    }
}

CompletionHandler<void()> SessionHistoryTraversalQueue::startTraversal(int8_t direction, CompletionHandler<void(bool)>&& completionHandler)
{
    auto identifier = ++m_lastTraversalIdentifier;
    m_inFlightTraversal = InFlightTraversal {
        .identifier = identifier,
        .direction = direction,
        .navigationID = { },
        .targetItem = nullptr,
        .committedItem = nullptr,
        .isWaitingForFrames = true,
        .isWaitingForMainFrameLoad = false,
        .didAnyFrameTakeTraversal = false,
        .completionHandler = WTF::move(completionHandler),
    };
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
    if (navigation && m_inFlightTraversal) {
        m_inFlightTraversal->navigationID = navigation->navigationID();
        m_inFlightTraversal->targetItem = navigation->targetItem();
        m_inFlightTraversal->committedItem = navigation->fromItem();
    }
}

void SessionHistoryTraversalQueue::finishTraversalIfDone()
{
    if (m_inFlightTraversal->isWaitingForFrames || m_inFlightTraversal->isWaitingForMainFrameLoad) {
        scheduleFlush();
        return;
    }

    m_inFlightTraversal = std::nullopt;
    scheduleFlush();
}

} // namespace WebKit
