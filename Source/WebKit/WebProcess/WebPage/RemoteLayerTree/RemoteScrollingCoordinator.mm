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
        protect(ensureScrollingStateTreeForRootFrameID(rootFrameID))->commit(LayerRepresentation::PlatformLayerIDRepresentation),
        std::exchange(m_clearScrollLatchingInNextTransaction, false),
        { },
        RemoteScrollingCoordinatorTransaction::FromDeserialization::No
    };
}

void RemoteScrollingCoordinator::willSendScrollPositionRequest(ScrollingNodeID nodeID, RequestedScrollData& request)
{
    request.identifier = ScrollRequestIdentifier::generate();
    // This may clobber an older one, but that's OK.
    m_scrollRequestsPendingResponse.set(nodeID, PendingScrollResponseInfo { *request.identifier, false });
}

/*
    This logic deals with overlapping scrolling IPC between the UI process and the web process.
    It's possible to have user scrolls coming from the UI process, while programmatic scrolls
    go in the other direction; the IPC messages in the two directions can overlap. This code
    attempts to maintain a coherent scroll position in the web process under these conditions.

    When sending a programmatic scroll to the UI process, we've already updated the web process
    position synchronously (see AsyncScrollingCoordinator::requestScrollToPosition()). We need
    to avoid applying a user scroll that overlaps until we're received the programmatic scroll
    response (otherwise we'll apply a stale position), but we do need to apply its scroll position
    because it may have been a delta scroll, where the response's scrollPosition reflects the UI
    process state.

    scrollTo interleaved with user scrolls:

    User scroll        *       *           *
                      100     110         120   200
    UI process  --------------------------------------------------
                        \       \           \   / \
                         \       \           \ /   \
                          \       \           /     \
                           \       \         / \     \
                           _\|     _\|      /  _\|   _\|
    Web process --------------------------------------------------
                            100     110   200  [120]  200
                                          ^   ignore!
                                          |
                                    scrollTo(0, 200)

    scrollBy (including anchor positioning adjustment) interleaved with user scrolls:

    User scroll        *       *           *              *
                      100     110         120   320      330
    UI process  --------------------------------------------------
                        \       \           \   / \        \
                         \       \           \ /   \        \
                          \       \           /     \        \
                           \       \         / \     \        \
                           _\|     _\|      /  _\|   _\|      _\|
    Web process --------------------------------------------------
                            100      110  310  [120]  320      330
                                          ^   ignore!
                                          |
                                    scrollBy(0, 200)


    If we have multiple programmatic scrolls in flight, we avoid applying replies to older ones,
    because they will be stale.

                             200   250
    UI process  -------------------------------------------------
                             / \   / \
                            /   \ /   \
                           /     /     \
                          /     / \     \
                         /     /  _\|   _\|
    Web process --------------------------------------------------
                       200   250  [200]  250
                        ^     ^  ignore!
                        |     |
            scrollTo(0, 200)  |
                       scrollTo(0, 250)

*/

void RemoteScrollingCoordinator::scrollUpdateForNode(ScrollUpdate&& update, CompletionHandler<void()>&& completionHandler)
{
    LOG_WITH_STREAM(Scrolling, stream << "RemoteScrollingCoordinator::scrollUpdateForNode: " << update);

    auto scopeExit = WTF::makeScopeExit([&] {
        completionHandler();
    });

    auto pendingResponseIt = m_scrollRequestsPendingResponse.find(update.nodeID);
    bool havePendingStateForNode = pendingResponseIt != m_scrollRequestsPendingResponse.end();

    if (std::holds_alternative<ScrollRequestResponseData>(update.data)) {
        auto& responseData = std::get<ScrollRequestResponseData>(update.data);

        ASSERT(responseData.responseIdentifier);
        if (!responseData.responseIdentifier)
            return;

        auto requestIdentifier = *responseData.responseIdentifier;

        // We should at least be waiting for the identifier we've just received back.
        ASSERT(havePendingStateForNode);
        if (!havePendingStateForNode)
            return;

        auto latestPendingIdentifier = *pendingResponseIt->value.identifier;
        ASSERT(requestIdentifier <= latestPendingIdentifier);

        pendingResponseIt->value.pendingScrollEnd |= (update.shouldFireScrollEnd == ShouldFireScrollEnd::Yes);

        if (latestPendingIdentifier == requestIdentifier) {
            auto fireScrollEnd = pendingResponseIt->value.pendingScrollEnd ? ShouldFireScrollEnd::Yes : ShouldFireScrollEnd::No;
            m_scrollRequestsPendingResponse.remove(pendingResponseIt);

            LOG_WITH_STREAM(Scrolling, stream << " identifier " << requestIdentifier << " is the last sent; updating scroll position to " << update.scrollPosition << ". firing scrollend " << (fireScrollEnd == ShouldFireScrollEnd::Yes));

            if (responseData.requestType == ScrollRequestType::PositionUpdate) {
                auto scrollUpdate = ScrollUpdate {
                    .nodeID = update.nodeID,
                    .scrollPosition = update.scrollPosition,
                    .shouldFireScrollEnd = fireScrollEnd,
                    .data = ScrollUpdateData {
                        .updateType = ScrollUpdateType::PositionUpdate,
                        .updateLayerPositionAction = ScrollingLayerPositionAction::Set,
                    }
                };
                applyScrollUpdate(WTF::move(scrollUpdate), ScrollType::User, ViewportRectStability::Stable);
            } else if (fireScrollEnd == ShouldFireScrollEnd::Yes) {
                // This just ensures that the scrollend is fired.
                applyScrollUpdate(WTF::move(update), ScrollType::User, ViewportRectStability::Stable);
            }
        } else
            LOG_WITH_STREAM(Scrolling, stream << " identifier " << requestIdentifier << " is not the most recent; ignoring (will fire scroll end eventually " << pendingResponseIt->value.pendingScrollEnd << ")");
        return;
    }

    if (havePendingStateForNode) {
        LOG_WITH_STREAM(Scrolling, stream << " waiting for scroll request id " << *pendingResponseIt->value.identifier << "; ignoring scroll update");
        return;
    }

    applyScrollUpdate(WTF::move(update), ScrollType::User, ViewportRectStability::Stable);
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
    if (auto monitor = protect(page())->wheelEventTestMonitor())
        monitor->receivedWheelEventWithPhases(phase, momentumPhase);
}

void RemoteScrollingCoordinator::startDeferringScrollingTestCompletionForNode(WebCore::ScrollingNodeID nodeID, OptionSet<WebCore::WheelEventTestMonitor::DeferReason> reason)
{
    if (auto monitor = protect(page())->wheelEventTestMonitor())
        monitor->deferForReason(nodeID, reason);
}

void RemoteScrollingCoordinator::stopDeferringScrollingTestCompletionForNode(WebCore::ScrollingNodeID nodeID, OptionSet<WebCore::WheelEventTestMonitor::DeferReason> reason)
{
    if (auto monitor = protect(page())->wheelEventTestMonitor())
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
