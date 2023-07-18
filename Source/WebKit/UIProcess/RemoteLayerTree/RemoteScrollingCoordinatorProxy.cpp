/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

#if ENABLE(UI_SIDE_COMPOSITING)
#include "RemoteScrollingCoordinatorProxy.h"

#include "MessageSenderInlines.h"
#include "NativeWebWheelEvent.h"
#include "RemoteLayerTreeDrawingAreaProxy.h"
#include "RemoteLayerTreeScrollingPerformanceData.h"
#include "RemoteScrollingCoordinator.h"
#include "RemoteScrollingCoordinatorMessages.h"
#include "RemoteScrollingCoordinatorTransaction.h"
#include "WebEventConversion.h"
#include "WebPageProxy.h"
#include "WebProcessProxy.h"
#include <WebCore/PerformanceLoggingClient.h>
#include <WebCore/RuntimeApplicationChecks.h>
#include <WebCore/ScrollingStateTree.h>
#include <WebCore/ScrollingTreeFrameScrollingNode.h>
#include <wtf/cocoa/RuntimeApplicationChecksCocoa.h>

namespace WebKit {
using namespace WebCore;

#define MESSAGE_CHECK_WITH_RETURN_VALUE(assertion, returnValue) MESSAGE_CHECK_WITH_RETURN_VALUE_BASE(assertion, (webPageProxy().process().connection()), returnValue)

RemoteScrollingCoordinatorProxy::RemoteScrollingCoordinatorProxy(WebPageProxy& webPageProxy)
    : m_webPageProxy(webPageProxy)
    , m_scrollingTree(RemoteScrollingTree::create(*this))
{
}

RemoteScrollingCoordinatorProxy::~RemoteScrollingCoordinatorProxy()
{
    m_scrollingTree->invalidate();
}

ScrollingNodeID RemoteScrollingCoordinatorProxy::rootScrollingNodeID() const
{
    // FIXME: Locking
    if (!m_scrollingTree->rootNode())
        return 0;

    return m_scrollingTree->rootNode()->scrollingNodeID();
}

const RemoteLayerTreeHost* RemoteScrollingCoordinatorProxy::layerTreeHost() const
{
    auto* drawingArea = m_webPageProxy.drawingArea();
    if (!is<RemoteLayerTreeDrawingAreaProxy>(drawingArea)) {
        ASSERT_NOT_REACHED();
        return nullptr;
    }

    auto& remoteDrawingArea = downcast<RemoteLayerTreeDrawingAreaProxy>(*drawingArea);
    return &remoteDrawingArea.remoteLayerTreeHost();
}

std::optional<RequestedScrollData> RemoteScrollingCoordinatorProxy::commitScrollingTreeState(const RemoteScrollingCoordinatorTransaction& transaction)
{
    m_requestedScroll = { };

    auto stateTree = WTFMove(const_cast<RemoteScrollingCoordinatorTransaction&>(transaction).scrollingStateTree());

    auto* layerTreeHost = this->layerTreeHost();
    if (!layerTreeHost) {
        ASSERT_NOT_REACHED();
        return { };
    }

    connectStateNodeLayers(*stateTree, *layerTreeHost);
    bool succeeded = m_scrollingTree->commitTreeState(WTFMove(stateTree));
    MESSAGE_CHECK_WITH_RETURN_VALUE(succeeded, std::nullopt);

    establishLayerTreeScrollingRelations(*layerTreeHost);
    
    if (transaction.clearScrollLatching())
        m_scrollingTree->clearLatchedNode();

    return std::exchange(m_requestedScroll, { });
}

void RemoteScrollingCoordinatorProxy::handleWheelEvent(const WebWheelEvent& wheelEvent, RectEdges<bool> rubberBandableEdges)
{
#if !(PLATFORM(MAC) && ENABLE(UI_SIDE_COMPOSITING))
    if (!m_scrollingTree)
        return;

    auto platformWheelEvent = platform(wheelEvent);

    // Replicate the hack in EventDispatcher::internalWheelEvent(). We could pass rubberBandableEdges all the way through the
    // WebProcess and back via the ScrollingTree, but we only ever need to consult it here.
    if (platformWheelEvent.phase() == PlatformWheelEventPhase::Began)
        m_scrollingTree->setMainFrameCanRubberBand(rubberBandableEdges);

    auto processingSteps = m_scrollingTree->determineWheelEventProcessing(platformWheelEvent);
    if (!processingSteps.contains(WheelEventProcessingSteps::AsyncScrolling)) {
        continueWheelEventHandling(wheelEvent, { processingSteps, false });
        return;
    }

    m_scrollingTree->willProcessWheelEvent();

    auto filteredEvent = filteredWheelEvent(platformWheelEvent);
    auto result = m_scrollingTree->handleWheelEvent(filteredEvent, processingSteps);
    didReceiveWheelEvent(result.wasHandled);

    continueWheelEventHandling(wheelEvent, result);
#else
    UNUSED_PARAM(wheelEvent);
    UNUSED_PARAM(rubberBandableEdges);
#endif
}

void RemoteScrollingCoordinatorProxy::continueWheelEventHandling(const WebWheelEvent& wheelEvent, WheelEventHandlingResult result)
{
    bool willStartSwipe = m_scrollingTree->willWheelEventStartSwipeGesture(platform(wheelEvent));
    webPageProxy().continueWheelEventHandling(wheelEvent, result, willStartSwipe);
}

TrackingType RemoteScrollingCoordinatorProxy::eventTrackingTypeForPoint(WebCore::EventTrackingRegions::EventType eventType, IntPoint p) const
{
    return m_scrollingTree->eventTrackingTypeForPoint(eventType, p);
}

void RemoteScrollingCoordinatorProxy::viewportChangedViaDelegatedScrolling(const FloatPoint& scrollPosition, const FloatRect& layoutViewport, double scale)
{
    m_scrollingTree->mainFrameViewportChangedViaDelegatedScrolling(scrollPosition, layoutViewport, scale);
}

void RemoteScrollingCoordinatorProxy::applyScrollingTreeLayerPositionsAfterCommit()
{
    // FIXME: (rdar://106293351) Set the `m_needsApplyLayerPositionsAfterCommit` flag in a more
    // reasonable place once UI-side compositing scrolling synchronization is implemented
    m_scrollingTree->setNeedsApplyLayerPositionsAfterCommit();
    m_scrollingTree->applyLayerPositionsAfterCommit();
}

void RemoteScrollingCoordinatorProxy::currentSnapPointIndicesDidChange(WebCore::ScrollingNodeID nodeID, std::optional<unsigned> horizontal, std::optional<unsigned> vertical)
{
    m_webPageProxy.send(Messages::RemoteScrollingCoordinator::CurrentSnapPointIndicesChangedForNode(nodeID, horizontal, vertical));
}

// This comes from the scrolling tree.
void RemoteScrollingCoordinatorProxy::scrollingTreeNodeDidScroll(ScrollingNodeID scrolledNodeID, const FloatPoint& newScrollPosition, const std::optional<FloatPoint>& layoutViewportOrigin, ScrollingLayerPositionAction scrollingLayerPositionAction)
{
    // Scroll updates for the main frame are sent via WebPageProxy::updateVisibleContentRects()
    // so don't send them here.
    if (!propagatesMainFrameScrolls() && scrolledNodeID == rootScrollingNodeID())
        return;

    if (m_webPageProxy.scrollingUpdatesDisabledForTesting())
        return;

#if PLATFORM(IOS_FAMILY)
    m_webPageProxy.scrollingNodeScrollViewDidScroll(scrolledNodeID);
#endif

    if (m_scrollingTree->isHandlingProgrammaticScroll())
        return;

    auto scrollUpdate = ScrollUpdate { scrolledNodeID, newScrollPosition, layoutViewportOrigin, ScrollUpdateType::PositionUpdate, scrollingLayerPositionAction };
    m_scrollingTree->addPendingScrollUpdate(WTFMove(scrollUpdate));

    LOG_WITH_STREAM(Scrolling, stream << "RemoteScrollingCoordinatorProxy::scrollingTreeNodeDidScroll " << scrolledNodeID << " to " << newScrollPosition << " waitingForDidScrollReply " << m_waitingForDidScrollReply);

    if (WebKit::RemoteLayerTreeScrollingPerformanceData* scrollPerfData = m_webPageProxy.scrollingPerformanceData())
        scrollPerfData->didScroll(m_scrollingTree->layoutViewport());

    if (!m_waitingForDidScrollReply) {
        sendScrollingTreeNodeDidScroll();
        return;
    }
}

void RemoteScrollingCoordinatorProxy::sendScrollingTreeNodeDidScroll()
{
    if (!m_scrollingTree) {
        m_waitingForDidScrollReply = false;
        return;
    }

    auto scrollUpdates = m_scrollingTree->takePendingScrollUpdates();
    for (unsigned i = 0; i < scrollUpdates.size(); ++i) {
        const auto& update = scrollUpdates[i];
        bool isLastUpdate = i == scrollUpdates.size() - 1;

        LOG_WITH_STREAM(Scrolling, stream << "RemoteScrollingCoordinatorProxy::sendScrollingTreeNodeDidScroll - node " << update.nodeID << " scroll position " << update.scrollPosition << " isLastUpdate " << isLastUpdate);
        m_webPageProxy.sendWithAsyncReply(Messages::RemoteScrollingCoordinator::ScrollPositionChangedForNode(update.nodeID, update.scrollPosition, update.layoutViewportOrigin, update.updateLayerPositionAction == ScrollingLayerPositionAction::Sync), [weakThis = WeakPtr { *this }, isLastUpdate] {
            if (!weakThis)
                return;

            if (isLastUpdate)
                weakThis->receivedLastScrollingTreeNodeDidScrollReply();
        });
        m_waitingForDidScrollReply = true;
    }
}

void RemoteScrollingCoordinatorProxy::receivedLastScrollingTreeNodeDidScrollReply()
{
    LOG_WITH_STREAM(Scrolling, stream << "RemoteScrollingCoordinatorProxy::receivedLastScrollingTreeNodeDidScrollReply - has pending updates " << (m_scrollingTree && m_scrollingTree->hasPendingScrollUpdates()));
    m_waitingForDidScrollReply = false;

    if (!m_scrollingTree || !m_scrollingTree->hasPendingScrollUpdates())
        return;

    RunLoop::main().dispatch([weakThis = WeakPtr { *this }]() {
        if (!weakThis)
            return;
        weakThis->sendScrollingTreeNodeDidScroll();
    });
}

void RemoteScrollingCoordinatorProxy::scrollingTreeNodeDidStopAnimatedScroll(ScrollingNodeID scrolledNodeID)
{
    m_webPageProxy.send(Messages::RemoteScrollingCoordinator::AnimatedScrollDidEndForNode(scrolledNodeID));
}

bool RemoteScrollingCoordinatorProxy::scrollingTreeNodeRequestsScroll(ScrollingNodeID scrolledNodeID, const RequestedScrollData& request)
{
    if (scrolledNodeID == rootScrollingNodeID()) {
        m_requestedScroll = request;
        return true;
    }

    return false;
}

bool RemoteScrollingCoordinatorProxy::scrollingTreeNodeRequestsKeyboardScroll(ScrollingNodeID scrolledNodeID, const RequestedKeyboardScrollData&)
{
    return false;
}

String RemoteScrollingCoordinatorProxy::scrollingTreeAsText() const
{
    if (m_scrollingTree)
        return m_scrollingTree->scrollingTreeAsText();
    
    return emptyString();
}

bool RemoteScrollingCoordinatorProxy::hasScrollableMainFrame() const
{
    // FIXME: Locking
    auto* rootNode = m_scrollingTree->rootNode();
    return rootNode && rootNode->canHaveScrollbars();
}

OverscrollBehavior RemoteScrollingCoordinatorProxy::mainFrameHorizontalOverscrollBehavior() const
{
    return m_scrollingTree->mainFrameHorizontalOverscrollBehavior();
}

OverscrollBehavior RemoteScrollingCoordinatorProxy::mainFrameVerticalOverscrollBehavior() const
{
    return m_scrollingTree->mainFrameVerticalOverscrollBehavior();
}

WebCore::FloatRect RemoteScrollingCoordinatorProxy::computeVisibleContentRect()
{
    auto scrollPosition = currentMainFrameScrollPosition();
    auto visibleContentRect = scrollingTree()->layoutViewport();
    visibleContentRect.setX(scrollPosition.x());
    visibleContentRect.setY(scrollPosition.y());
    return visibleContentRect;
}

float RemoteScrollingCoordinatorProxy::topContentInset() const
{
    return m_scrollingTree->mainFrameTopContentInset();
}

WebCore::FloatPoint RemoteScrollingCoordinatorProxy::currentMainFrameScrollPosition() const
{
    return m_scrollingTree->mainFrameScrollPosition();
}

IntPoint RemoteScrollingCoordinatorProxy::scrollOrigin() const
{
    return m_scrollingTree->mainFrameScrollOrigin();
}

int RemoteScrollingCoordinatorProxy::headerHeight() const
{
    return m_scrollingTree->mainFrameHeaderHeight();
}

int RemoteScrollingCoordinatorProxy::footerHeight() const
{
    return m_scrollingTree->mainFrameFooterHeight();
}

float RemoteScrollingCoordinatorProxy::mainFrameScaleFactor() const
{
    return m_scrollingTree->mainFrameScaleFactor();
}

FloatSize RemoteScrollingCoordinatorProxy::totalContentsSize() const
{
    return m_scrollingTree->totalContentsSize();
}

void RemoteScrollingCoordinatorProxy::displayDidRefresh(PlatformDisplayID displayID)
{
    m_scrollingTree->displayDidRefresh(displayID);
}

bool RemoteScrollingCoordinatorProxy::hasScrollableOrZoomedMainFrame() const
{
    // FIXME: Locking
    auto* rootNode = m_scrollingTree->rootNode();
    if (!rootNode)
        return false;

#if PLATFORM(IOS_FAMILY)
    if (WebCore::IOSApplication::isEventbrite() && !linkedOnOrAfterSDKWithBehavior(SDKAlignedBehavior::SupportsOverflowHiddenOnMainFrame))
        return true;
#endif

    return rootNode->canHaveScrollbars() || rootNode->visualViewportIsSmallerThanLayoutViewport();
}

void RemoteScrollingCoordinatorProxy::sendUIStateChangedIfNecessary()
{
    if (!m_uiState.changes())
        return;

    m_webPageProxy.send(Messages::RemoteScrollingCoordinator::ScrollingStateInUIProcessChanged(m_uiState));
    m_uiState.clearChanges();
}

void RemoteScrollingCoordinatorProxy::resetStateAfterProcessExited()
{
    m_currentHorizontalSnapPointIndex = 0;
    m_currentVerticalSnapPointIndex = 0;
    m_uiState.reset();
}

void RemoteScrollingCoordinatorProxy::reportFilledVisibleFreshTile(MonotonicTime timestamp, unsigned unfilledArea)
{
    m_webPageProxy.logScrollingEvent(static_cast<uint32_t>(PerformanceLoggingClient::ScrollingEvent::FilledTile), timestamp, unfilledArea);
}

void RemoteScrollingCoordinatorProxy::reportExposedUnfilledArea(MonotonicTime, unsigned)
{
}

void RemoteScrollingCoordinatorProxy::reportSynchronousScrollingReasonsChanged(MonotonicTime timestamp, OptionSet<SynchronousScrollingReason> reasons)
{
    if (WebKit::RemoteLayerTreeScrollingPerformanceData* scrollPerfData = m_webPageProxy.scrollingPerformanceData())
        scrollPerfData->didChangeSynchronousScrollingReasons(timestamp, reasons.toRaw());
}

void RemoteScrollingCoordinatorProxy::receivedWheelEventWithPhases(PlatformWheelEventPhase phase, PlatformWheelEventPhase momentumPhase)
{
    m_webPageProxy.send(Messages::RemoteScrollingCoordinator::ReceivedWheelEventWithPhases(phase, momentumPhase));
}

void RemoteScrollingCoordinatorProxy::deferWheelEventTestCompletionForReason(ScrollingNodeID nodeID, WheelEventTestMonitor::DeferReason reason)
{
    m_webPageProxy.send(Messages::RemoteScrollingCoordinator::StartDeferringScrollingTestCompletionForNode(nodeID, reason));
}

void RemoteScrollingCoordinatorProxy::removeWheelEventTestCompletionDeferralForReason(ScrollingNodeID nodeID, WheelEventTestMonitor::DeferReason reason)
{
    m_webPageProxy.send(Messages::RemoteScrollingCoordinator::StopDeferringScrollingTestCompletionForNode(nodeID, reason));
}

void RemoteScrollingCoordinatorProxy::viewWillStartLiveResize()
{
    if (m_scrollingTree)
        m_scrollingTree->viewWillStartLiveResize();
}

void RemoteScrollingCoordinatorProxy::viewWillEndLiveResize()
{
    if (m_scrollingTree)
        m_scrollingTree->viewWillEndLiveResize();
}

void RemoteScrollingCoordinatorProxy::viewSizeDidChange()
{
    if (m_scrollingTree)
        m_scrollingTree->viewSizeDidChange();
}

bool RemoteScrollingCoordinatorProxy::overlayScrollbarsEnabled()
{
    return m_scrollingTree->overlayScrollbarsEnabled();
}

String RemoteScrollingCoordinatorProxy::scrollbarStateForScrollingNodeID(WebCore::ScrollingNodeID scrollingNodeID, bool isVertical)
{
    if (auto node = m_scrollingTree->nodeForID(scrollingNodeID)) {
        if (auto* scrollingNode = dynamicDowncast<ScrollingTreeScrollingNode>(*node))
            return scrollingNode->scrollbarStateForOrientation(isVertical ? ScrollbarOrientation::Vertical : ScrollbarOrientation::Horizontal);
    }
    return ""_s;
}

bool RemoteScrollingCoordinatorProxy::scrollingPerformanceTestingEnabled() const
{
    return m_scrollingTree->scrollingPerformanceTestingEnabled();
}

void RemoteScrollingCoordinatorProxy::scrollingTreeNodeScrollbarVisibilityDidChange(WebCore::ScrollingNodeID nodeID, ScrollbarOrientation orientation, bool isVisible)
{
    m_webPageProxy.send(Messages::RemoteScrollingCoordinator::ScrollingTreeNodeScrollbarVisibilityDidChange(nodeID, orientation, isVisible));
}

void RemoteScrollingCoordinatorProxy::scrollingTreeNodeScrollbarMinimumThumbLengthDidChange(WebCore::ScrollingNodeID nodeID, ScrollbarOrientation orientation, int minimumThumbLength)
{
    m_webPageProxy.send(Messages::RemoteScrollingCoordinator::ScrollingTreeNodeScrollbarMinimumThumbLengthDidChange(nodeID, orientation, minimumThumbLength));
}

} // namespace WebKit

#endif // ENABLE(UI_SIDE_COMPOSITING)
