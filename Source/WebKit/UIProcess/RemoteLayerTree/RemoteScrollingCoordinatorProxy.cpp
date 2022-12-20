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

#include "RemoteLayerTreeDrawingAreaProxy.h"
#include "RemoteScrollingCoordinator.h"
#include "RemoteScrollingCoordinatorMessages.h"
#include "RemoteScrollingCoordinatorTransaction.h"
#include "WebPageProxy.h"
#include "WebProcessProxy.h"
#include <WebCore/PerformanceLoggingClient.h>
#include <WebCore/RuntimeApplicationChecks.h>
#include <WebCore/ScrollingStateTree.h>
#include <WebCore/ScrollingTreeFrameScrollingNode.h>
#include <wtf/cocoa/RuntimeApplicationChecksCocoa.h>

namespace WebKit {
using namespace WebCore;

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
    m_scrollingTree->commitTreeState(WTFMove(stateTree));

    establishLayerTreeScrollingRelations(*layerTreeHost);

    return std::exchange(m_requestedScroll, { });
}

WheelEventHandlingResult RemoteScrollingCoordinatorProxy::handleWheelEvent(const PlatformWheelEvent& wheelEvent)
{
    if (!m_scrollingTree)
        return WheelEventHandlingResult::unhandled();

    auto processingSteps = m_scrollingTree->determineWheelEventProcessing(wheelEvent);
    if (processingSteps.containsAny({ WheelEventProcessingSteps::MainThreadForScrolling, WheelEventProcessingSteps::MainThreadForNonBlockingDOMEventDispatch, WheelEventProcessingSteps::MainThreadForBlockingDOMEventDispatch }))
        return WheelEventHandlingResult::unhandled(processingSteps);

    if (m_scrollingTree->willWheelEventStartSwipeGesture(wheelEvent))
        return WheelEventHandlingResult::unhandled();

    m_scrollingTree->willProcessWheelEvent();

    auto filteredEvent = filteredWheelEvent(wheelEvent);
    auto result = m_scrollingTree->handleWheelEvent(filteredEvent);
    didReceiveWheelEvent(result.wasHandled);
    return result;
}

void RemoteScrollingCoordinatorProxy::handleMouseEvent(const WebCore::PlatformMouseEvent& event)
{
    m_scrollingTree->handleMouseEvent(event);
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
        m_webPageProxy.sendWithAsyncReply(Messages::RemoteScrollingCoordinator::ScrollPositionChangedForNode(update.nodeID, update.scrollPosition, update.updateLayerPositionAction == ScrollingLayerPositionAction::Sync), [weakThis = WeakPtr { *this }, isLastUpdate] {
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

String RemoteScrollingCoordinatorProxy::scrollingTreeAsText() const
{
    if (m_scrollingTree)
        return m_scrollingTree->scrollingTreeAsText();
    
    return emptyString();
}

bool RemoteScrollingCoordinatorProxy::hasScrollableMainFrame() const
{
    auto* rootNode = m_scrollingTree->rootNode();
    return rootNode && rootNode->canHaveScrollbars();
}

ScrollingTreeScrollingNode* RemoteScrollingCoordinatorProxy::rootNode() const
{
    return m_scrollingTree->rootNode();
}

void RemoteScrollingCoordinatorProxy::displayDidRefresh(PlatformDisplayID displayID)
{
    m_scrollingTree->displayDidRefresh(displayID);
}

bool RemoteScrollingCoordinatorProxy::hasScrollableOrZoomedMainFrame() const
{
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

void RemoteScrollingCoordinatorProxy::reportExposedUnfilledArea(MonotonicTime timestamp, unsigned unfilledArea)
{
    m_webPageProxy.logScrollingEvent(static_cast<uint32_t>(PerformanceLoggingClient::ScrollingEvent::ExposedTilelessArea), timestamp, unfilledArea);
}

void RemoteScrollingCoordinatorProxy::reportSynchronousScrollingReasonsChanged(MonotonicTime timestamp, OptionSet<SynchronousScrollingReason> reasons)
{
    m_webPageProxy.logScrollingEvent(static_cast<uint32_t>(PerformanceLoggingClient::ScrollingEvent::SwitchedScrollingMode), timestamp, reasons.toRaw());
}

void RemoteScrollingCoordinatorProxy::startMonitoringWheelEventsForTesting()
{
    m_scrollingTree->setIsMonitoringWheelEvents(true);
}

} // namespace WebKit

#endif // ENABLE(UI_SIDE_COMPOSITING)
