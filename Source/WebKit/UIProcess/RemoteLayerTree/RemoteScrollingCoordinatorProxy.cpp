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
#include <WebCore/ScrollingStateTree.h>
#include <WebCore/ScrollingTreeFrameScrollingNode.h>
#include <wtf/RuntimeApplicationChecks.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebKit {
using namespace WebCore;

#define MESSAGE_CHECK_WITH_RETURN_VALUE(assertion, returnValue) MESSAGE_CHECK_WITH_RETURN_VALUE_BASE(assertion, connection, returnValue)

WTF_MAKE_TZONE_ALLOCATED_IMPL(RemoteScrollingCoordinatorProxy);

RemoteScrollingCoordinatorProxy::RemoteScrollingCoordinatorProxy(WebPageProxy& webPageProxy)
    : m_webPageProxy(webPageProxy)
    , m_scrollingTree(RemoteScrollingTree::create(*this))
{
}

RemoteScrollingCoordinatorProxy::~RemoteScrollingCoordinatorProxy()
{
    m_scrollingTree->invalidate();
}

WebPageProxy& RemoteScrollingCoordinatorProxy::webPageProxy() const
{
    return m_webPageProxy.get();
}

std::optional<ScrollingNodeID> RemoteScrollingCoordinatorProxy::rootScrollingNodeID() const
{
    // FIXME: Locking
    if (!m_scrollingTree->rootNode())
        return std::nullopt;

    return m_scrollingTree->rootNode()->scrollingNodeID();
}

const RemoteLayerTreeHost* RemoteScrollingCoordinatorProxy::layerTreeHost() const
{
    if (auto* remoteDrawingArea = dynamicDowncast<RemoteLayerTreeDrawingAreaProxy>(m_webPageProxy->drawingArea()))
        return &remoteDrawingArea->remoteLayerTreeHost();
    ASSERT_NOT_REACHED();
    return nullptr;
}

ScrollRequestData RemoteScrollingCoordinatorProxy::commitScrollingTreeState(IPC::Connection& connection, const RemoteScrollingCoordinatorTransaction& transaction, std::optional<LayerHostingContextIdentifier> identifier)
{
    m_scrollRequestData.clear();

    auto stateTree = WTF::move(const_cast<RemoteScrollingCoordinatorTransaction&>(transaction).scrollingStateTree());

    auto* layerTreeHost = this->layerTreeHost();
    if (!layerTreeHost) {
        ASSERT_NOT_REACHED();
        return { };
    }

    stateTree->setRootFrameIdentifier(transaction.rootFrameIdentifier());

    ASSERT(stateTree);
    connectStateNodeLayers(*stateTree, *layerTreeHost);
    bool succeeded = m_scrollingTree->commitTreeState(WTF::move(stateTree), identifier);

    MESSAGE_CHECK_WITH_RETURN_VALUE(succeeded, ScrollRequestData());

    establishLayerTreeScrollingRelations(*layerTreeHost);
    
    if (transaction.clearScrollLatching())
        m_scrollingTree->clearLatchedNode();

    return std::exchange(m_scrollRequestData, { });
}

void RemoteScrollingCoordinatorProxy::adjustMainFrameDelegatedScrollPosition(ScrollRequestData&& requestData)
{
    // Note that this comes from the scrolling tree, and is not updated live when -requestScroll is called below.
    auto scrollPosition = currentMainFrameScrollPosition();

    auto handleOneRequest = [&](const RequestedScrollData& request) {
        switch (request.requestType) {
        case ScrollRequestType::PositionUpdate:
        case ScrollRequestType::AnimatedPositionUpdate:
        case ScrollRequestType::DeltaUpdate:
        case ScrollRequestType::AnimatedDeltaUpdate:
        case ScrollRequestType::ImplicitDeltaUpdate: {
            auto interruptScrollAnimation = request.requestType == ScrollRequestType::ImplicitDeltaUpdate ? InterruptScrollAnimation::No : InterruptScrollAnimation::Yes;
            scrollPosition = request.destinationPosition(scrollPosition);
            LOG_WITH_STREAM(Scrolling, stream << "RemoteScrollingCoordinatorProxy::adjustViewScrollPosition requesting scroll to " << scrollPosition << " animated " << isAnimatedUpdate(request.requestType) << " interrupting " << (interruptScrollAnimation == InterruptScrollAnimation::Yes));
            protect(webPageProxy())->requestScroll(scrollPosition, scrollOrigin(), isAnimatedUpdate(request.requestType) ? ScrollIsAnimated::Yes : ScrollIsAnimated::No, interruptScrollAnimation);
            break;
        }
        case ScrollRequestType::CancelAnimatedScroll:
            protect(webPageProxy())->requestScroll(scrollPosition, scrollOrigin(), ScrollIsAnimated::No, InterruptScrollAnimation::Yes);
            break;
        }

        if (auto rootNodeID = rootScrollingNodeID()) {
            auto shouldFireScrollEnd = ShouldFireScrollEnd::No;

            switch (request.requestType) {
            case ScrollRequestType::PositionUpdate:
            case ScrollRequestType::DeltaUpdate:
            case ScrollRequestType::ImplicitDeltaUpdate:
                shouldFireScrollEnd = ShouldFireScrollEnd::Yes;
                break;
            case ScrollRequestType::AnimatedPositionUpdate:
            case ScrollRequestType::AnimatedDeltaUpdate:
            case ScrollRequestType::CancelAnimatedScroll:
                break;
            }

            m_scrollingTree->didHandleScrollRequestForNode(*rootNodeID, request.requestType, scrollPosition, shouldFireScrollEnd, *request.identifier);
        }
    };

    for (auto& request : requestData)
        handleOneRequest(request);
}

void RemoteScrollingCoordinatorProxy::stickyScrollingTreeNodeBeganSticking(ScrollingNodeID)
{
    protect(webPageProxy())->stickyScrollingTreeNodeBeganSticking();
}

void RemoteScrollingCoordinatorProxy::handleWheelEvent(const WebWheelEvent& wheelEvent, RectEdges<WebCore::RubberBandingBehavior> rubberBandableEdges)
{
#if !(PLATFORM(MAC) && ENABLE(UI_SIDE_COMPOSITING))
    auto platformWheelEvent = platform(wheelEvent);

    // Replicate the hack in EventDispatcher::internalWheelEvent(). We could pass rubberBandableEdges all the way through the
    // WebProcess and back via the ScrollingTree, but we only ever need to consult it here.
    if (platformWheelEvent.phase() == PlatformWheelEventPhase::Began)
        m_scrollingTree->setClientAllowedMainFrameRubberBandableEdges(rubberBandableEdges);

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
    protect(webPageProxy())->continueWheelEventHandling(wheelEvent, result, willStartSwipe);
}

TrackingType RemoteScrollingCoordinatorProxy::eventTrackingTypeForPoint(WebCore::EventTrackingRegions::EventType eventType, IntPoint p) const
{
    return m_scrollingTree->eventTrackingTypeForPoint(eventType, p);
}

WebCore::RectEdges<bool> RemoteScrollingCoordinatorProxy::pinnedStateIncludingAncestorsAtPoint(FloatPoint p)
{
    return m_scrollingTree->pinnedStateIncludingAncestorsAtPoint(p);
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
    protect(protect(webPageProxy())->legacyMainFrameProcess())->send(Messages::RemoteScrollingCoordinator::CurrentSnapPointIndicesChangedForNode(nodeID, horizontal, vertical), m_webPageProxy->webPageIDInMainFrameProcess());
}

void RemoteScrollingCoordinatorProxy::sendScrollingTreeNodeUpdate()
{
    Ref webPageProxy = m_webPageProxy.get();
    if (webPageProxy->scrollingUpdatesDisabledForTesting())
        return;

    auto scrollUpdates = m_scrollingTree->takePendingScrollUpdates();
    for (unsigned i = 0; i < scrollUpdates.size(); ++i) {
        const auto& update = scrollUpdates[i];
        bool isLastUpdate = i == scrollUpdates.size() - 1;

        if (std::holds_alternative<ScrollUpdateData>(update.data)) {
            const auto& updateData = std::get<ScrollUpdateData>(update.data);
            if (updateData.updateType == ScrollUpdateType::PositionUpdate) {
                webPageProxy->scrollingNodeScrollViewDidScroll(update.nodeID);
                auto* scrollPerfData = webPageProxy->scrollingPerformanceData();
                // update.layoutViewportOrigin is set for frame scrolls.
                if (scrollPerfData && updateData.layoutViewportOrigin) {
                    auto layoutViewport = m_scrollingTree->layoutViewport();
                    layoutViewport.setLocation(*updateData.layoutViewportOrigin);
                    scrollPerfData->didScroll(layoutViewport);
                }
            }
        }

        LOG_WITH_STREAM(Scrolling, stream << "RemoteScrollingCoordinatorProxy::sendScrollingTreeNodeUpdate " << update << " isLastUpdate " << isLastUpdate);

        webPageProxy->sendScrollUpdateForNode(m_scrollingTree->frameIDForScrollingNodeID(update.nodeID), update, isLastUpdate);
        m_waitingForDidScrollReply = true;
    }
}

void RemoteScrollingCoordinatorProxy::scrollingThreadAddedPendingUpdate()
{
    LOG_WITH_STREAM(Scrolling, stream << "RemoteScrollingCoordinatorProxy::scrollingThreadAddedPendingUpdate - m_waitingForDidScrollReply " << m_waitingForDidScrollReply);

    if (m_waitingForDidScrollReply)
        return;

    sendScrollingTreeNodeUpdate();
}

void RemoteScrollingCoordinatorProxy::receivedLastScrollingTreeNodeUpdateReply()
{
    LOG_WITH_STREAM(Scrolling, stream << "RemoteScrollingCoordinatorProxy::receivedLastScrollingTreeNodeUpdateReply - has pending updates " << m_scrollingTree->hasPendingScrollUpdates());
    m_waitingForDidScrollReply = false;

    if (!m_scrollingTree->hasPendingScrollUpdates())
        return;

    RunLoop::mainSingleton().dispatch([weakThis = WeakPtr { *this }]() {
        if (!weakThis)
            return;
        weakThis->sendScrollingTreeNodeUpdate();
    });
}

RequestsScrollHandling RemoteScrollingCoordinatorProxy::scrollingTreeNodeRequestsScroll(ScrollingNodeID scrolledNodeID, const RequestedScrollData& request)
{
    if (scrolledNodeID == rootScrollingNodeID()) {
        m_scrollRequestData.append(request);
        return RequestsScrollHandling::Delayed;
    }

    return RequestsScrollHandling::Unhandled;
}

bool RemoteScrollingCoordinatorProxy::scrollingTreeNodeRequestsKeyboardScroll(ScrollingNodeID scrolledNodeID, const RequestedKeyboardScrollData&)
{
    return false;
}

String RemoteScrollingCoordinatorProxy::scrollingTreeAsText() const
{
    return m_scrollingTree->scrollingTreeAsText();
}

bool RemoteScrollingCoordinatorProxy::hasScrollableMainFrame() const
{
    // FIXME: Locking
    auto* rootNode = m_scrollingTree->rootNode();
    return rootNode && rootNode->canHaveScrollbars();
}

WebCore::ScrollbarWidth RemoteScrollingCoordinatorProxy::mainFrameScrollbarWidth() const
{
    return m_scrollingTree->mainFrameScrollbarWidth();
}

std::optional<ScrollbarColor> RemoteScrollingCoordinatorProxy::mainFrameScrollbarColor() const
{
    return m_scrollingTree->mainFrameScrollbarColor();
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
    auto visibleContentRect = m_scrollingTree->layoutViewport();
    visibleContentRect.setX(scrollPosition.x());
    visibleContentRect.setY(scrollPosition.y());
    return visibleContentRect;
}

WebCore::FloatBoxExtent RemoteScrollingCoordinatorProxy::obscuredContentInsets() const
{
    return m_scrollingTree->mainFrameObscuredContentInsets();
}

#if ENABLE(BANNER_VIEW_OVERLAYS)

void RemoteScrollingCoordinatorProxy::setBannerViewHeight(float offset)
{
    auto previousOffset = m_scrollingTree->bannerViewHeight();
    m_scrollingTree->setBannerViewHeight(offset);

    if (offset < previousOffset)
        m_scrollingTree->triggerMainFrameRubberBandSnapBack();
    else if (offset > previousOffset)
        m_scrollingTree->mainFrameRubberBandTargetOffsetDidChange();
}

void RemoteScrollingCoordinatorProxy::setBannerViewMaximumHeight(float offset)
{
    m_scrollingTree->setBannerViewMaximumHeight(offset);
}

void RemoteScrollingCoordinatorProxy::setHasBannerViewOverlay(bool hasBannerView)
{
    m_scrollingTree->setHasBannerViewOverlay(hasBannerView);
}

#endif

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

    return rootNode->canHaveScrollbars() || rootNode->visualViewportIsSmallerThanLayoutViewport();
}

void RemoteScrollingCoordinatorProxy::sendUIStateChangedIfNecessary()
{
    if (!m_uiState.changes())
        return;

    protect(protect(webPageProxy())->legacyMainFrameProcess())->send(Messages::RemoteScrollingCoordinator::ScrollingStateInUIProcessChanged(m_uiState), m_webPageProxy->webPageIDInMainFrameProcess());
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
    protect(webPageProxy())->logScrollingEvent(static_cast<uint32_t>(PerformanceLoggingClient::ScrollingEvent::FilledTile), timestamp, unfilledArea);
}

void RemoteScrollingCoordinatorProxy::reportExposedUnfilledArea(MonotonicTime, unsigned)
{
}

void RemoteScrollingCoordinatorProxy::reportSynchronousScrollingReasonsChanged(MonotonicTime timestamp, OptionSet<SynchronousScrollingReason> reasons)
{
    if (WebKit::RemoteLayerTreeScrollingPerformanceData* scrollPerfData = m_webPageProxy->scrollingPerformanceData())
        scrollPerfData->didChangeSynchronousScrollingReasons(timestamp, reasons.toRaw());
}

void RemoteScrollingCoordinatorProxy::receivedWheelEventWithPhases(PlatformWheelEventPhase phase, PlatformWheelEventPhase momentumPhase)
{
    protect(protect(webPageProxy())->legacyMainFrameProcess())->send(Messages::RemoteScrollingCoordinator::ReceivedWheelEventWithPhases(phase, momentumPhase), m_webPageProxy->webPageIDInMainFrameProcess());
}

void RemoteScrollingCoordinatorProxy::deferWheelEventTestCompletionForReason(std::optional<ScrollingNodeID> nodeID, WheelEventTestMonitor::DeferReason reason)
{
    if (isMonitoringWheelEvents() && nodeID)
        protect(protect(webPageProxy())->legacyMainFrameProcess())->send(Messages::RemoteScrollingCoordinator::StartDeferringScrollingTestCompletionForNode(*nodeID, reason), m_webPageProxy->webPageIDInMainFrameProcess());
}

void RemoteScrollingCoordinatorProxy::removeWheelEventTestCompletionDeferralForReason(std::optional<ScrollingNodeID> nodeID, WheelEventTestMonitor::DeferReason reason)
{
    if (isMonitoringWheelEvents() && nodeID)
        protect(protect(webPageProxy())->legacyMainFrameProcess())->send(Messages::RemoteScrollingCoordinator::StopDeferringScrollingTestCompletionForNode(*nodeID, reason), m_webPageProxy->webPageIDInMainFrameProcess());
}

void RemoteScrollingCoordinatorProxy::viewWillStartLiveResize()
{
    m_scrollingTree->viewWillStartLiveResize();
}

void RemoteScrollingCoordinatorProxy::viewWillEndLiveResize()
{
    m_scrollingTree->viewWillEndLiveResize();
}

void RemoteScrollingCoordinatorProxy::viewSizeDidChange()
{
    m_scrollingTree->viewSizeDidChange();
}

bool RemoteScrollingCoordinatorProxy::overlayScrollbarsEnabled()
{
    return m_scrollingTree->overlayScrollbarsEnabled();
}

String RemoteScrollingCoordinatorProxy::scrollbarStateForScrollingNodeID(std::optional<WebCore::ScrollingNodeID> scrollingNodeID, bool isVertical)
{
    if (RefPtr node = m_scrollingTree->nodeForID(scrollingNodeID)) {
        if (RefPtr scrollingNode = dynamicDowncast<ScrollingTreeScrollingNode>(node.releaseNonNull()))
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
    protect(webPageProxy())->sendToProcessContainingFrame(m_scrollingTree->frameIDForScrollingNodeID(nodeID), Messages::RemoteScrollingCoordinator::ScrollingTreeNodeScrollbarVisibilityDidChange(nodeID, orientation, isVisible));
}

void RemoteScrollingCoordinatorProxy::scrollingTreeNodeScrollbarMinimumThumbLengthDidChange(WebCore::ScrollingNodeID nodeID, ScrollbarOrientation orientation, int minimumThumbLength)
{
    protect(webPageProxy())->sendToProcessContainingFrame(m_scrollingTree->frameIDForScrollingNodeID(nodeID), Messages::RemoteScrollingCoordinator::ScrollingTreeNodeScrollbarMinimumThumbLengthDidChange(nodeID, orientation, minimumThumbLength));
}

bool RemoteScrollingCoordinatorProxy::isMonitoringWheelEvents()
{
    return m_scrollingTree->isMonitoringWheelEvents();
}

bool RemoteScrollingCoordinatorProxy::hasFixedOrSticky() const
{
    return m_scrollingTree->hasFixedOrSticky();
}

#undef MESSAGE_CHECK_WITH_RETURN_VALUE

} // namespace WebKit

#endif // ENABLE(UI_SIDE_COMPOSITING)
