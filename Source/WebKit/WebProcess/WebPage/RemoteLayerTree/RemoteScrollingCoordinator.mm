/*
 * Copyright (C) 2014-2025 Apple Inc. All rights reserved.
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

#import "config.h"
#import "RemoteScrollingCoordinator.h"

#if ENABLE(ASYNC_SCROLLING)

#import "ArgumentCoders.h"
#import "GraphicsLayerCARemote.h"
#import "Logging.h"
#import "RemoteLayerTreeDrawingArea.h"
#import "RemoteScrollingCoordinatorMessages.h"
#import "RemoteScrollingCoordinatorTransaction.h"
#import "RemoteScrollingUIState.h"
#import "WebPage.h"
#import "WebProcess.h"
#import <WebCore/AXObjectCache.h>
#import <WebCore/GraphicsLayer.h>
#import <WebCore/LocalFrame.h>
#import <WebCore/LocalFrameView.h>
#import <WebCore/Page.h>
#import <WebCore/RenderLayerCompositor.h>
#import <WebCore/RenderView.h>
#import <WebCore/ScrollbarsController.h>
#import <WebCore/ScrollingStateFrameScrollingNode.h>
#import <WebCore/ScrollingStateTree.h>
#import <WebCore/ScrollingTreeFixedNodeCocoa.h>
#import <WebCore/ScrollingTreeStickyNodeCocoa.h>
#import <WebCore/WheelEventTestMonitor.h>
#import <wtf/TZoneMallocInlines.h>

namespace WebKit {
using namespace WebCore;

WTF_MAKE_TZONE_ALLOCATED_IMPL(RemoteScrollingCoordinator);

RemoteScrollingCoordinator::RemoteScrollingCoordinator(WebPage* page)
    : AsyncScrollingCoordinator(page->corePage())
    , m_webPage(page)
    , m_pageIdentifier(page->identifier())
{
    WebProcess::singleton().addMessageReceiver(Messages::RemoteScrollingCoordinator::messageReceiverName(), m_pageIdentifier, *this);
}

RemoteScrollingCoordinator::~RemoteScrollingCoordinator()
{
    WebProcess::singleton().removeMessageReceiver(Messages::RemoteScrollingCoordinator::messageReceiverName(), m_pageIdentifier);
}

void RemoteScrollingCoordinator::scheduleTreeStateCommit()
{
    if (RefPtr webPage = m_webPage.get())
        protect(webPage->drawingArea())->triggerRenderingUpdate();
}

bool RemoteScrollingCoordinator::coordinatesScrollingForFrameView(const LocalFrameView& frameView) const
{
    CheckedPtr renderView = frameView.renderView();
    return renderView && renderView->usesCompositing();
}

bool RemoteScrollingCoordinator::isRubberBandInProgress(std::optional<ScrollingNodeID> nodeID) const
{
    if (!nodeID)
        return false;
    return m_nodesWithActiveRubberBanding.contains(*nodeID);
}

bool RemoteScrollingCoordinator::isUserScrollInProgress(std::optional<ScrollingNodeID> nodeID) const
{
    if (!nodeID)
        return false;
    return m_nodesWithActiveUserScrolls.contains(*nodeID);
}

bool RemoteScrollingCoordinator::isScrollSnapInProgress(std::optional<ScrollingNodeID> nodeID) const
{
    if (!nodeID)
        return false;
    return m_nodesWithActiveScrollSnap.contains(*nodeID);
}

void RemoteScrollingCoordinator::setScrollPinningBehavior(ScrollPinningBehavior)
{
    // FIXME: send to the UI process.
}

RemoteScrollingCoordinatorTransaction RemoteScrollingCoordinator::buildTransaction(FrameIdentifier rootFrameID)
{
    willCommitTree(rootFrameID);

    return {
        ensureCheckedScrollingStateTreeForRootFrameID(rootFrameID)->commit(LayerRepresentation::PlatformLayerIDRepresentation),
        std::exchange(m_clearScrollLatchingInNextTransaction, false),
        { },
        RemoteScrollingCoordinatorTransaction::FromDeserialization::No
    };
}

// Notification from the UI process that we scrolled.
void RemoteScrollingCoordinator::scrollUpdateForNode(ScrollUpdate update, CompletionHandler<void()>&& completionHandler)
{
    LOG_WITH_STREAM(Scrolling, stream << "RemoteScrollingCoordinator::scrollUpdateForNode: " << update);

    applyScrollUpdate(WTF::move(update));
    completionHandler();
}

void RemoteScrollingCoordinator::currentSnapPointIndicesChangedForNode(ScrollingNodeID nodeID, std::optional<unsigned> horizontal, std::optional<unsigned> vertical)
{
    setActiveScrollSnapIndices(nodeID, horizontal, vertical);
}

void RemoteScrollingCoordinator::scrollingStateInUIProcessChanged(const RemoteScrollingUIState& uiState)
{
    // FIXME: Also track m_nodesWithActiveRubberBanding.
    if (uiState.changes().contains(RemoteScrollingUIStateChanges::ScrollSnapNodes))
        m_nodesWithActiveScrollSnap = uiState.nodesWithActiveScrollSnap();

    if (uiState.changes().contains(RemoteScrollingUIStateChanges::UserScrollNodes))
        m_nodesWithActiveUserScrolls = uiState.nodesWithActiveUserScrolls();

    if (uiState.changes().contains(RemoteScrollingUIStateChanges::RubberbandingNodes))
        m_nodesWithActiveRubberBanding = uiState.nodesWithActiveRubberband();
}

void RemoteScrollingCoordinator::addNodeWithActiveRubberBanding(ScrollingNodeID nodeID)
{
    m_nodesWithActiveRubberBanding.add(nodeID);
}

void RemoteScrollingCoordinator::removeNodeWithActiveRubberBanding(ScrollingNodeID nodeID)
{
    m_nodesWithActiveRubberBanding.remove(nodeID);
}

void RemoteScrollingCoordinator::startMonitoringWheelEvents(bool clearLatchingState)
{
    if (clearLatchingState)
        m_clearScrollLatchingInNextTransaction = true;
}

void RemoteScrollingCoordinator::receivedWheelEventWithPhases(WebCore::PlatformWheelEventPhase phase, WebCore::PlatformWheelEventPhase momentumPhase)
{
    if (auto monitor = protectedPage()->wheelEventTestMonitor())
        monitor->receivedWheelEventWithPhases(phase, momentumPhase);
}

void RemoteScrollingCoordinator::startDeferringScrollingTestCompletionForNode(WebCore::ScrollingNodeID nodeID, OptionSet<WebCore::WheelEventTestMonitor::DeferReason> reason)
{
    if (auto monitor = protectedPage()->wheelEventTestMonitor())
        monitor->deferForReason(nodeID, reason);
}

void RemoteScrollingCoordinator::stopDeferringScrollingTestCompletionForNode(WebCore::ScrollingNodeID nodeID, OptionSet<WebCore::WheelEventTestMonitor::DeferReason> reason)
{
    if (auto monitor = protectedPage()->wheelEventTestMonitor())
        monitor->removeDeferralForReason(nodeID, reason);
}

WheelEventHandlingResult RemoteScrollingCoordinator::handleWheelEventForScrolling(const PlatformWheelEvent& wheelEvent, ScrollingNodeID targetNodeID, std::optional<WheelScrollGestureState> gestureState)
{
    LOG_WITH_STREAM(Scrolling, stream << "RemoteScrollingCoordinator::handleWheelEventForScrolling " << wheelEvent << " - node " << targetNodeID << " gestureState " << gestureState << " will start swipe " << (m_currentWheelEventWillStartSwipe && *m_currentWheelEventWillStartSwipe));

    if (m_currentWheelEventWillStartSwipe && *m_currentWheelEventWillStartSwipe)
        return WheelEventHandlingResult::unhandled();

    m_currentWheelGestureInfo = NodeAndGestureState { targetNodeID, gestureState };
    return WheelEventHandlingResult::handled();
}

void RemoteScrollingCoordinator::scrollingTreeNodeScrollbarVisibilityDidChange(ScrollingNodeID nodeID, ScrollbarOrientation orientation, bool isVisible)
{
    RefPtr frameView = frameViewForScrollingNode(nodeID);
    if (!frameView)
        return;

    if (CheckedPtr scrollableArea = frameView->scrollableAreaForScrollingNodeID(nodeID))
        scrollableArea->scrollbarsController().setScrollbarVisibilityState(orientation, isVisible);
}

void RemoteScrollingCoordinator::scrollingTreeNodeScrollbarMinimumThumbLengthDidChange(ScrollingNodeID nodeID, ScrollbarOrientation orientation, int minimumThumbLength)
{
    RefPtr frameView = frameViewForScrollingNode(nodeID);
    if (!frameView)
        return;

    if (CheckedPtr scrollableArea = frameView->scrollableAreaForScrollingNodeID(nodeID))
        scrollableArea->scrollbarsController().setScrollbarMinimumThumbLength(orientation, minimumThumbLength);
}

} // namespace WebKit

#endif // ENABLE(ASYNC_SCROLLING)
