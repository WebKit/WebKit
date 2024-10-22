/*
 * Copyright (C) 2014-2015 Apple Inc. All rights reserved.
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

#if ENABLE(ASYNC_SCROLLING)
#include "AsyncScrollingCoordinator.h"

#include "DebugPageOverlays.h"
#include "DeprecatedGlobalSettings.h"
#include "Document.h"
#include "EditorClient.h"
#include "GraphicsLayer.h"
#include "LocalFrame.h"
#include "LocalFrameView.h"
#include "Logging.h"
#include "Page.h"
#include "PerformanceLoggingClient.h"
#include "RemoteFrame.h"
#include "RenderLayerCompositor.h"
#include "RenderView.h"
#include "ScrollAnimator.h"
#include "ScrollbarsController.h"
#include "ScrollingConstraints.h"
#include "ScrollingStateFixedNode.h"
#include "ScrollingStateFrameHostingNode.h"
#include "ScrollingStateFrameScrollingNode.h"
#include "ScrollingStateOverflowScrollProxyNode.h"
#include "ScrollingStateOverflowScrollingNode.h"
#include "ScrollingStatePositionedNode.h"
#include "ScrollingStateStickyNode.h"
#include "ScrollingStateTree.h"
#include "Settings.h"
#include "WheelEventTestMonitor.h"
#include "pal/HysteresisActivity.h"
#include <wtf/ProcessID.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/TextStream.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(AsyncScrollingCoordinator);

AsyncScrollingCoordinator::AsyncScrollingCoordinator(Page* page)
    : ScrollingCoordinator(page)
    , m_hysterisisActivity([this](auto state) { hysterisisTimerFired(state); }, 200_ms)
{
}

void AsyncScrollingCoordinator::hysterisisTimerFired(PAL::HysteresisState state)
{
    if (RefPtr page = this->page(); page && state == PAL::HysteresisState::Stopped)
        page->didFinishScrolling();
}

AsyncScrollingCoordinator::~AsyncScrollingCoordinator() = default;

void AsyncScrollingCoordinator::scrollingStateTreePropertiesChanged()
{
    scheduleTreeStateCommit();
}

void AsyncScrollingCoordinator::scrollingThreadAddedPendingUpdate()
{
    scheduleRenderingUpdate();
}

#if PLATFORM(COCOA)
void AsyncScrollingCoordinator::handleWheelEventPhase(ScrollingNodeID nodeID, PlatformWheelEventPhase phase)
{
    ASSERT(isMainThread());

    if (!page())
        return;

    RefPtr frameView = frameViewForScrollingNode(nodeID);
    if (!frameView)
        return;

    if (nodeID == frameView->scrollingNodeID()) {
        frameView->scrollAnimator().handleWheelEventPhase(phase);
        return;
    }

    if (CheckedPtr scrollableArea = frameView->scrollableAreaForScrollingNodeID(nodeID))
        scrollableArea->scrollAnimator().handleWheelEventPhase(phase);
}
#endif

RefPtr<ScrollingStateNode> AsyncScrollingCoordinator::stateNodeForNodeID(std::optional<ScrollingNodeID> nodeID) const
{
    return WTF::switchOn(m_scrollingStateTrees.rawStorage(), [] (const std::monostate&) -> RefPtr<ScrollingStateNode> {
        return nullptr;
    }, [&] (const KeyValuePair<FrameIdentifier, UniqueRef<ScrollingStateTree>>& pair) {
        return pair.value->stateNodeForID(nodeID);
    }, [&] (const UncheckedKeyHashMap<FrameIdentifier, UniqueRef<ScrollingStateTree>>& map) -> RefPtr<ScrollingStateNode> {
        for (auto& tree : map.values()) {
            if (RefPtr scrollingNode = tree->stateNodeForID(nodeID))
                return scrollingNode;
        }
        return nullptr;
    });
}

RefPtr<ScrollingStateNode> AsyncScrollingCoordinator::stateNodeForScrollableArea(const ScrollableArea& scrollableArea) const
{
    auto scrollingNodeID = scrollableArea.scrollingNodeID();
    if (!scrollingNodeID)
        return nullptr;
    if (auto* scrollingStateTree = existingScrollingStateTreeForRootFrameID(scrollableArea.rootFrameID()))
        return scrollingStateTree->stateNodeForID(scrollingNodeID);
    return nullptr;
}

ScrollingStateTree& AsyncScrollingCoordinator::ensureScrollingStateTreeForRootFrameID(FrameIdentifier rootFrameID)
{
    ASSERT(Frame::isRootFrameIdentifier(rootFrameID));
    return m_scrollingStateTrees.ensure(rootFrameID, [&] {
        return makeUniqueRef<ScrollingStateTree>(this);
    });
}

const ScrollingStateTree* AsyncScrollingCoordinator::existingScrollingStateTreeForRootFrameID(std::optional<FrameIdentifier> rootFrameID) const
{
    auto* result = rootFrameID ? m_scrollingStateTrees.get(*rootFrameID) : nullptr;
    if (!result)
        return nullptr;
    return &result->get();
}

void AsyncScrollingCoordinator::rootFrameWasRemoved(FrameIdentifier rootFrameID)
{
    m_scrollingStateTrees.remove(rootFrameID);
}

ScrollingStateTree* AsyncScrollingCoordinator::stateTreeForNodeID(std::optional<ScrollingNodeID> nodeID) const
{
    return WTF::switchOn(m_scrollingStateTrees.rawStorage(), [] (const std::monostate&) -> ScrollingStateTree* {
        return nullptr;
    }, [&] (const KeyValuePair<FrameIdentifier, UniqueRef<ScrollingStateTree>>& pair) -> ScrollingStateTree* {
        if (RefPtr scrollingNode = pair.value->stateNodeForID(nodeID))
            return pair.value.ptr();
        return nullptr;
    }, [&] (const UncheckedKeyHashMap<FrameIdentifier, UniqueRef<ScrollingStateTree>>& map) -> ScrollingStateTree* {
        for (auto& tree : map.values()) {
            if (RefPtr scrollingNode = tree->stateNodeForID(nodeID))
                return tree.ptr();
        }
        return nullptr;
    });
}

static inline void setStateScrollingNodeSnapOffsetsAsFloat(ScrollingStateScrollingNode& node, const LayoutScrollSnapOffsetsInfo* offsetInfo, float deviceScaleFactor)
{
    if (!offsetInfo) {
        node.setSnapOffsetsInfo(FloatScrollSnapOffsetsInfo());
        return;
    }

    // FIXME: Incorporate current page scale factor in snapping to device pixel. Perhaps we should just convert to float here and let UI process do the pixel snapping?
    node.setSnapOffsetsInfo(offsetInfo->convertUnits<FloatScrollSnapOffsetsInfo>(deviceScaleFactor));
}

void AsyncScrollingCoordinator::willCommitTree(FrameIdentifier rootFrameID)
{
    updateEventTrackingRegions(rootFrameID);
}

void AsyncScrollingCoordinator::updateEventTrackingRegions(FrameIdentifier rootFrameID)
{
    if (!m_eventTrackingRegionsDirty)
        return;

    auto* scrollingStateTree = existingScrollingStateTreeForRootFrameID(rootFrameID);
    if (!scrollingStateTree || !scrollingStateTree->rootStateNode())
        return;

    scrollingStateTree->rootStateNode()->setEventTrackingRegions(absoluteEventTrackingRegions());
    m_eventTrackingRegionsDirty = false;
}

void AsyncScrollingCoordinator::frameViewLayoutUpdated(LocalFrameView& frameView)
{
    ASSERT(isMainThread());
    ASSERT(page());

    m_eventTrackingRegionsDirty = true;

    auto scrollingStateNode = stateNodeForNodeID(frameView.scrollingNodeID());

    // If there isn't a root node yet, don't do anything. We'll be called again after creating one.
    if (!scrollingStateNode)
        return;

    // We have to schedule a commit, but the computed non-fast region may not have actually changed.
    // FIXME: This needs to disambiguate between event regions in the scrolling tree, and those in GraphicsLayers.
    scheduleTreeStateCommit();

#if PLATFORM(COCOA)
    if (!coordinatesScrollingForFrameView(frameView))
        return;

    RefPtr page = frameView.frame().page();
    if (page && page->isMonitoringWheelEvents()) {
        RefPtr frameScrollingNode = dynamicDowncast<ScrollingStateFrameScrollingNode>(stateNodeForScrollableArea(frameView));
        if (!frameScrollingNode)
            return;

        frameScrollingNode->setIsMonitoringWheelEvents(page->isMonitoringWheelEvents());
    }
#else
    UNUSED_PARAM(frameView);
#endif
}

void AsyncScrollingCoordinator::frameViewVisualViewportChanged(LocalFrameView& frameView)
{
    ASSERT(isMainThread());
    ASSERT(page());

    if (!coordinatesScrollingForFrameView(frameView))
        return;
    
    // If the root layer does not have a ScrollingStateNode, then we should create one.
    RefPtr frameScrollingNode = dynamicDowncast<ScrollingStateFrameScrollingNode>(stateNodeForScrollableArea(frameView));
    if (!frameScrollingNode)
        return;

    auto visualViewportIsSmallerThanLayoutViewport = [](const LocalFrameView& frameView) {
        auto layoutViewport = frameView.layoutViewportRect();
        auto visualViewport = frameView.visualViewportRect();
        return visualViewport.width() < layoutViewport.width() || visualViewport.height() < layoutViewport.height();
    };
    frameScrollingNode->setVisualViewportIsSmallerThanLayoutViewport(visualViewportIsSmallerThanLayoutViewport(frameView));
}

void AsyncScrollingCoordinator::frameViewWillBeDetached(LocalFrameView& frameView)
{
    ASSERT(isMainThread());
    ASSERT(page());

    if (!coordinatesScrollingForFrameView(frameView))
        return;

    scrollableAreaWillBeDetached(frameView);
}

void AsyncScrollingCoordinator::scrollableAreaWillBeDetached(ScrollableArea& area)
{
    ASSERT(isMainThread());
    ASSERT(page());

    RefPtr node = dynamicDowncast<ScrollingStateScrollingNode>(stateNodeForScrollableArea(area));
    if (!node)
        return;

    node->setScrollPosition(area.scrollPosition());
}

void AsyncScrollingCoordinator::updateIsMonitoringWheelEventsForFrameView(const LocalFrameView& frameView)
{
    RefPtr page = frameView.frame().page();
    if (!page)
        return;

    RefPtr node = dynamicDowncast<ScrollingStateFrameScrollingNode>(stateNodeForScrollableArea(frameView));
    if (!node)
        return;

    node->setIsMonitoringWheelEvents(page->isMonitoringWheelEvents());
}

void AsyncScrollingCoordinator::frameViewEventTrackingRegionsChanged(LocalFrameView& frameView)
{
    m_eventTrackingRegionsDirty = true;
    if (!ensureScrollingStateTreeForRootFrameID(frameView.frame().rootFrame().frameID()).rootStateNode())
        return;

    // We have to schedule a commit, but the computed non-fast region may not have actually changed.
    // FIXME: This needs to disambiguate between event regions in the scrolling tree, and those in GraphicsLayers.
    scheduleTreeStateCommit();

    DebugPageOverlays::didChangeEventHandlers(frameView.protectedFrame());
}

void AsyncScrollingCoordinator::frameViewRootLayerDidChange(LocalFrameView& frameView)
{
    ASSERT(isMainThread());
    ASSERT(page());

    if (!coordinatesScrollingForFrameView(frameView))
        return;
    
    // FIXME: In some navigation scenarios, the FrameView has no RenderView or that RenderView has not been composited.
    // This needs cleaning up: https://bugs.webkit.org/show_bug.cgi?id=132724
    if (!frameView.scrollingNodeID())
        return;
    
    // If the root layer does not have a ScrollingStateNode, then we should create one.
    ensureRootStateNodeForFrameView(frameView);
    ASSERT(stateNodeForScrollableArea(frameView));

    ScrollingCoordinator::frameViewRootLayerDidChange(frameView);

    RefPtr node = dynamicDowncast<ScrollingStateFrameScrollingNode>(stateNodeForScrollableArea(frameView));
    if (!node)
        return;
    node->setScrollContainerLayer(scrollContainerLayerForFrameView(frameView));
    node->setScrolledContentsLayer(scrolledContentsLayerForFrameView(frameView));
    node->setRootContentsLayer(rootContentsLayerForFrameView(frameView));
    node->setCounterScrollingLayer(counterScrollingLayerForFrameView(frameView));
    node->setInsetClipLayer(insetClipLayerForFrameView(frameView));
    node->setContentShadowLayer(contentShadowLayerForFrameView(frameView));
    node->setHeaderLayer(headerLayerForFrameView(frameView));
    node->setFooterLayer(footerLayerForFrameView(frameView));
    node->setScrollBehaviorForFixedElements(frameView.scrollBehaviorForFixedElements());
    node->setVerticalScrollbarLayer(frameView.layerForVerticalScrollbar());
    node->setHorizontalScrollbarLayer(frameView.layerForHorizontalScrollbar());
    node->setScrollbarLayoutDirection(frameView.shouldPlaceVerticalScrollbarOnLeft() ? UserInterfaceLayoutDirection::RTL : UserInterfaceLayoutDirection::LTR);
}

bool AsyncScrollingCoordinator::requestStartKeyboardScrollAnimation(ScrollableArea& scrollableArea, const KeyboardScroll& scrollData)
{
    ASSERT(isMainThread());
    ASSERT(page());

    auto stateNode = dynamicDowncast<ScrollingStateScrollingNode>(stateNodeForScrollableArea(scrollableArea));
    if (!stateNode)
        return false;

    stateNode->setKeyboardScrollData({ KeyboardScrollAction::StartAnimation, scrollData });
    // FIXME: This should schedule a rendering update
    commitTreeStateIfNeeded();
    return true;
}

bool AsyncScrollingCoordinator::requestStopKeyboardScrollAnimation(ScrollableArea& scrollableArea, bool immediate)
{
    ASSERT(isMainThread());
    ASSERT(page());

    auto stateNode = dynamicDowncast<ScrollingStateScrollingNode>(stateNodeForScrollableArea(scrollableArea));
    if (!stateNode)
        return false;

    stateNode->setKeyboardScrollData({ immediate ? KeyboardScrollAction::StopImmediately : KeyboardScrollAction::StopWithAnimation, std::nullopt });
    // FIXME: This should schedule a rendering update
    commitTreeStateIfNeeded();
    return true;
}

bool AsyncScrollingCoordinator::requestScrollToPosition(ScrollableArea& scrollableArea, const ScrollPosition& scrollPosition, const ScrollPositionChangeOptions& options)
{
    ASSERT(isMainThread());
    ASSERT(page());
    auto scrollingNodeID = scrollableArea.scrollingNodeID();
    if (!scrollingNodeID)
        return false;

    RefPtr frameView = frameViewForScrollingNode(*scrollingNodeID);
    if (!frameView)
        return false;

    if (!coordinatesScrollingForFrameView(*frameView))
        return false;

    setScrollingNodeScrollableAreaGeometry(*scrollingNodeID, scrollableArea);

    bool inBackForwardCache = frameView->frame().document()->backForwardCacheState() != Document::NotInBackForwardCache;
    bool isSnapshotting = page()->isTakingSnapshotsForApplicationSuspension();
    bool inProgrammaticScroll = scrollableArea.currentScrollType() == ScrollType::Programmatic;

    if ((inProgrammaticScroll && options.animated == ScrollIsAnimated::No) || inBackForwardCache) {
        auto scrollUpdate = ScrollUpdate { *scrollingNodeID, scrollPosition, { }, ScrollUpdateType::PositionUpdate, ScrollingLayerPositionAction::Set };
        applyScrollUpdate(WTFMove(scrollUpdate), ScrollType::Programmatic);
    }

    ASSERT(inProgrammaticScroll == (options.type == ScrollType::Programmatic));

    // If this frame view's document is being put into the back/forward cache, we don't want to update our
    // main frame scroll position. Just let the FrameView think that we did.
    if (isSnapshotting)
        return true;

    auto stateNode = dynamicDowncast<ScrollingStateScrollingNode>(stateNodeForScrollableArea(scrollableArea));
    if (!stateNode)
        return false;

    if (options.originalScrollDelta)
        stateNode->setRequestedScrollData({ ScrollRequestType::DeltaUpdate, *options.originalScrollDelta, options.type, options.clamping, options.animated });
    else
        stateNode->setRequestedScrollData({ ScrollRequestType::PositionUpdate, scrollPosition, options.type, options.clamping, options.animated });

    LOG_WITH_STREAM(Scrolling, stream << "AsyncScrollingCoordinator::requestScrollToPosition " << scrollPosition << " for nodeID " << scrollingNodeID << " requestedScrollData " << stateNode->requestedScrollData());

    // FIXME: This should schedule a rendering update
    commitTreeStateIfNeeded();
    return true;
}

void AsyncScrollingCoordinator::stopAnimatedScroll(ScrollableArea& scrollableArea)
{
    ASSERT(isMainThread());
    ASSERT(page());
    auto scrollingNodeID = scrollableArea.scrollingNodeID();
    if (!scrollingNodeID)
        return;

    RefPtr frameView = frameViewForScrollingNode(*scrollingNodeID);
    if (!frameView || !coordinatesScrollingForFrameView(*frameView))
        return;

    auto stateNode = dynamicDowncast<ScrollingStateScrollingNode>(stateNodeForScrollableArea(scrollableArea));
    if (!stateNode)
        return;

    // Animated scrolls are always programmatic.
    stateNode->setRequestedScrollData({ ScrollRequestType::CancelAnimatedScroll, { } });
    // FIXME: This should schedule a rendering update
    commitTreeStateIfNeeded();
}

void AsyncScrollingCoordinator::setScrollbarLayoutDirection(ScrollableArea& scrollableArea, UserInterfaceLayoutDirection scrollbarLayoutDirection)
{
    ASSERT(isMainThread());
    ASSERT(page());
    RefPtr stateNode = dynamicDowncast<ScrollingStateScrollingNode>(stateNodeForScrollableArea(scrollableArea));
    if (!stateNode)
        return;

    stateNode->setScrollbarLayoutDirection(scrollbarLayoutDirection);
}

void AsyncScrollingCoordinator::setMouseIsOverScrollbar(Scrollbar* scrollbar, bool isOverScrollbar)
{
    ASSERT(isMainThread());
    ASSERT(page());
    auto stateNode = dynamicDowncast<ScrollingStateScrollingNode>(stateNodeForScrollableArea(scrollbar->scrollableArea()));
    if (!stateNode)
        return;
    stateNode->setScrollbarHoverState({ scrollbar->orientation() == ScrollbarOrientation::Vertical ? false : isOverScrollbar, scrollbar->orientation() == ScrollbarOrientation::Vertical ? isOverScrollbar : false });
}

void AsyncScrollingCoordinator::setMouseIsOverContentArea(ScrollableArea& scrollableArea, bool isOverContentArea)
{
    ASSERT(isMainThread());
    ASSERT(page());
    auto stateNode = dynamicDowncast<ScrollingStateScrollingNode>(stateNodeForScrollableArea(scrollableArea));
    if (!stateNode)
        return;
    stateNode->setMouseIsOverContentArea(isOverContentArea);
}

void AsyncScrollingCoordinator::setMouseMovedInContentArea(ScrollableArea& scrollableArea)
{
    ASSERT(isMainThread());
    ASSERT(page());
    auto stateNode = dynamicDowncast<ScrollingStateScrollingNode>(stateNodeForScrollableArea(scrollableArea));
    if (!stateNode)
        return;

    auto mousePosition = scrollableArea.lastKnownMousePositionInView();
    auto horizontalScrollbar = scrollableArea.horizontalScrollbar();
    auto verticalScrollbar = scrollableArea.verticalScrollbar();

    MouseLocationState state = { horizontalScrollbar ? horizontalScrollbar->convertFromContainingView(mousePosition) : IntPoint(), verticalScrollbar ? verticalScrollbar->convertFromContainingView(mousePosition) : IntPoint() };
    stateNode->setMouseMovedInContentArea(state);
}

void AsyncScrollingCoordinator::setLayerHostingContextIdentifierForFrameHostingNode(ScrollingNodeID scrollingNodeID, std::optional<LayerHostingContextIdentifier> identifier)
{
    auto stateNode = dynamicDowncast<ScrollingStateFrameHostingNode>(stateNodeForNodeID(scrollingNodeID));
    ASSERT(stateNode);
    if (!stateNode)
        return;
    stateNode->setLayerHostingContextIdentifier(identifier);
}

void AsyncScrollingCoordinator::setScrollbarEnabled(Scrollbar& scrollbar)
{
    ASSERT(isMainThread());
    ASSERT(page());

    auto stateNode = dynamicDowncast<ScrollingStateScrollingNode>(stateNodeForScrollableArea(scrollbar.scrollableArea()));
    if (!stateNode)
        return;
    stateNode->setScrollbarEnabledState(scrollbar.orientation(), scrollbar.enabled());
}

void AsyncScrollingCoordinator::setScrollbarWidth(ScrollableArea& scrollableArea, ScrollbarWidth scrollbarWidth)
{
    ASSERT(isMainThread());
    ASSERT(page());

    auto stateNode = dynamicDowncast<ScrollingStateScrollingNode>(stateNodeForScrollableArea(scrollableArea));
    if (!stateNode)
        return;
    stateNode->setScrollbarWidth(scrollbarWidth);
}

void AsyncScrollingCoordinator::applyScrollingTreeLayerPositions()
{
    m_scrollingTree->applyLayerPositions();
}

void AsyncScrollingCoordinator::synchronizeStateFromScrollingTree()
{
    ASSERT(isMainThread());
    applyPendingScrollUpdates();

    m_scrollingTree->traverseScrollingTree([&](ScrollingNodeID nodeID, ScrollingNodeType, std::optional<FloatPoint> scrollPosition, std::optional<FloatPoint> layoutViewportOrigin, bool scrolledSinceLastCommit) {
        if (scrollPosition && scrolledSinceLastCommit) {
            LOG_WITH_STREAM(Scrolling, stream << "AsyncScrollingCoordinator::synchronizeStateFromScrollingTree - node " << nodeID << " scroll position " << scrollPosition);
            updateScrollPositionAfterAsyncScroll(nodeID, scrollPosition.value(), layoutViewportOrigin, ScrollingLayerPositionAction::Set, ScrollType::User);
        }
    });
}

void AsyncScrollingCoordinator::applyPendingScrollUpdates()
{
    if (!m_scrollingTree)
        return;

    auto scrollUpdates = m_scrollingTree->takePendingScrollUpdates();
    for (auto& update : scrollUpdates) {
        LOG_WITH_STREAM(Scrolling, stream << "AsyncScrollingCoordinator::applyPendingScrollUpdates - node " << update.nodeID << " scroll position " << update.scrollPosition);
        applyScrollPositionUpdate(WTFMove(update), ScrollType::User);
    }
}

void AsyncScrollingCoordinator::scheduleRenderingUpdate()
{
    if (RefPtr page = this->page())
        page->scheduleRenderingUpdate(RenderingUpdateStep::ScrollingTreeUpdate);
}

LocalFrameView* AsyncScrollingCoordinator::frameViewForScrollingNode(LocalFrame& rootFrame, std::optional<ScrollingNodeID> scrollingNodeID) const
{
    ASSERT(rootFrame.isRootFrame());
    auto* scrollingStateTree = existingScrollingStateTreeForRootFrameID(rootFrame.frameID());
    if (!scrollingStateTree || !scrollingStateTree->rootStateNode())
        return nullptr;
    if (scrollingNodeID == scrollingStateTree->rootStateNode()->scrollingNodeID()) {
        if (rootFrame.view() && rootFrame.view()->scrollingNodeID() == scrollingNodeID)
            return rootFrame.view();
    }

    RefPtr stateNode = stateNodeForNodeID(scrollingNodeID);
    if (!stateNode)
        return nullptr;

    // Find the enclosing frame scrolling node.
    RefPtr parentNode = stateNode;
    while (parentNode && !parentNode->isFrameScrollingNode())
        parentNode = parentNode->parent();
    
    if (!parentNode)
        return nullptr;

    // Walk the frame tree to find the matching LocalFrameView. This is not ideal, but avoids back pointers to LocalFrameViews
    // from ScrollingTreeStateNodes.
    for (RefPtr<Frame> frame = &rootFrame; frame; frame = frame->tree().traverseNext()) {
        auto* localFrame = dynamicDowncast<LocalFrame>(frame.get());
        if (!localFrame)
            continue;
        if (auto* view = localFrame->view()) {
            if (view->scrollingNodeID() == parentNode->scrollingNodeID())
                return view;
        }
    }

    return nullptr;
}

LocalFrameView* AsyncScrollingCoordinator::frameViewForScrollingNode(std::optional<ScrollingNodeID> scrollingNodeID) const
{
    if (!page())
        return nullptr;
    for (const auto& rootFrame : page()->rootFrames()) {
        if (RefPtr frameView = frameViewForScrollingNode(rootFrame.get(), scrollingNodeID))
            return frameView.get();
    }
    return nullptr;
}

void AsyncScrollingCoordinator::applyScrollUpdate(ScrollUpdate&& update, ScrollType scrollType)
{
    applyPendingScrollUpdates();
    applyScrollPositionUpdate(WTFMove(update), scrollType);
}

void AsyncScrollingCoordinator::applyScrollPositionUpdate(ScrollUpdate&& update, ScrollType scrollType)
{
    switch (update.updateType) {
    case ScrollUpdateType::AnimatedScrollWillStart:
        animatedScrollWillStartForNode(update.nodeID);
        return;

    case ScrollUpdateType::AnimatedScrollDidEnd:
        animatedScrollDidEndForNode(update.nodeID);
        return;

    case ScrollUpdateType::WheelEventScrollWillStart:
        wheelEventScrollWillStartForNode(update.nodeID);
        return;

    case ScrollUpdateType::WheelEventScrollDidEnd:
        wheelEventScrollDidEndForNode(update.nodeID);
        return;

    case ScrollUpdateType::PositionUpdate:
        updateScrollPositionAfterAsyncScroll(update.nodeID, update.scrollPosition, update.layoutViewportOrigin, update.updateLayerPositionAction, scrollType);
        return;
    }
}

void AsyncScrollingCoordinator::animatedScrollWillStartForNode(ScrollingNodeID scrollingNodeID)
{
    ASSERT(isMainThread());

    RefPtr page = this->page();
    if (!page)
        return;

    auto* frameView = frameViewForScrollingNode(scrollingNodeID);
    if (!frameView)
        return;

    m_hysterisisActivity.start();
    page->willBeginScrolling();
}

void AsyncScrollingCoordinator::animatedScrollDidEndForNode(ScrollingNodeID scrollingNodeID)
{
    ASSERT(isMainThread());

    if (!page())
        return;

    RefPtr frameView = frameViewForScrollingNode(scrollingNodeID);
    if (!frameView)
        return;

    LOG_WITH_STREAM(Scrolling, stream << "AsyncScrollingCoordinator::animatedScrollDidEndForNode node " << scrollingNodeID);

    m_hysterisisActivity.stop();

    if (scrollingNodeID == frameView->scrollingNodeID()) {
        frameView->setScrollAnimationStatus(ScrollAnimationStatus::NotAnimating);
        return;
    }

    if (auto* scrollableArea = frameView->scrollableAreaForScrollingNodeID(scrollingNodeID)) {
        scrollableArea->setScrollAnimationStatus(ScrollAnimationStatus::NotAnimating);
        scrollableArea->animatedScrollDidEnd();
    }
}

void AsyncScrollingCoordinator::wheelEventScrollWillStartForNode(ScrollingNodeID scrollingNodeID)
{
    ASSERT(isMainThread());

    RefPtr page = this->page();
    if (!page)
        return;

    auto* frameView = frameViewForScrollingNode(scrollingNodeID);
    if (!frameView)
        return;

    m_hysterisisActivity.start();
    page->willBeginScrolling();
}

void AsyncScrollingCoordinator::wheelEventScrollDidEndForNode(ScrollingNodeID scrollingNodeID)
{
    ASSERT(isMainThread());

    if (!page())
        return;

    if (!frameViewForScrollingNode(scrollingNodeID))
        return;

    m_hysterisisActivity.stop();
}

void AsyncScrollingCoordinator::updateScrollPositionAfterAsyncScroll(ScrollingNodeID scrollingNodeID, const FloatPoint& scrollPosition, std::optional<FloatPoint> layoutViewportOrigin, ScrollingLayerPositionAction scrollingLayerPositionAction, ScrollType scrollType)
{
    ASSERT(isMainThread());

    RefPtr page = this->page();
    if (!page)
        return;

    RefPtr frameView = frameViewForScrollingNode(scrollingNodeID);
    if (!frameView)
        return;

    LOG_WITH_STREAM(Scrolling, stream << "AsyncScrollingCoordinator::updateScrollPositionAfterAsyncScroll node " << scrollingNodeID << " " << scrollType << " scrollPosition " << scrollPosition << " action " << scrollingLayerPositionAction);

    if (!frameView->frame().isMainFrame()) {
        if (scrollingLayerPositionAction == ScrollingLayerPositionAction::Set)
            page->editorClient().subFrameScrollPositionChanged();
    }

    if (scrollingNodeID == frameView->scrollingNodeID()) {
        reconcileScrollingState(*frameView, scrollPosition, layoutViewportOrigin, scrollType, ViewportRectStability::Stable, scrollingLayerPositionAction);
        return;
    }

    // Overflow-scroll area.
    if (CheckedPtr scrollableArea = frameView->scrollableAreaForScrollingNodeID(scrollingNodeID)) {
        auto previousScrollType = scrollableArea->currentScrollType();
        scrollableArea->setCurrentScrollType(scrollType);
        scrollableArea->notifyScrollPositionChanged(roundedIntPoint(scrollPosition));
        scrollableArea->setCurrentScrollType(previousScrollType);

        if (scrollingLayerPositionAction == ScrollingLayerPositionAction::Set)
            page->editorClient().overflowScrollPositionChanged();
    }
}

void AsyncScrollingCoordinator::reconcileScrollingState(LocalFrameView& frameView, const FloatPoint& scrollPosition, const LayoutViewportOriginOrOverrideRect& layoutViewportOriginOrOverrideRect, ScrollType scrollType, ViewportRectStability viewportRectStability, ScrollingLayerPositionAction scrollingLayerPositionAction)
{
    auto previousScrollType = frameView.currentScrollType();
    frameView.setCurrentScrollType(scrollType);

    LOG_WITH_STREAM(Scrolling, stream << getCurrentProcessID() << " AsyncScrollingCoordinator " << this << " reconcileScrollingState scrollPosition " << scrollPosition << " type " << scrollType << " stability " << viewportRectStability << " " << scrollingLayerPositionAction);

    std::optional<FloatRect> layoutViewportRect;

    WTF::switchOn(layoutViewportOriginOrOverrideRect,
        [&frameView](std::optional<FloatPoint> origin) {
            if (origin)
                frameView.setBaseLayoutViewportOrigin(LayoutPoint(origin.value()), LocalFrameView::TriggerLayoutOrNot::No);
        }, [&layoutViewportRect](std::optional<FloatRect> overrideRect) {
            if (!overrideRect)
                return;

            layoutViewportRect = overrideRect;
        }
    );

    frameView.setScrollClamping(ScrollClamping::Unclamped);
    frameView.notifyScrollPositionChanged(roundedIntPoint(scrollPosition));
    frameView.setScrollClamping(ScrollClamping::Clamped);

    frameView.setCurrentScrollType(previousScrollType);

    if (scrollType == ScrollType::User && scrollingLayerPositionAction != ScrollingLayerPositionAction::Set) {
        auto scrollingNodeID = frameView.scrollingNodeID();
        if (viewportRectStability == ViewportRectStability::Stable)
            reconcileViewportConstrainedLayerPositions(scrollingNodeID, frameView.rectForFixedPositionLayout(), scrollingLayerPositionAction);
        else if (layoutViewportRect)
            reconcileViewportConstrainedLayerPositions(scrollingNodeID, LayoutRect(layoutViewportRect.value()), scrollingLayerPositionAction);
    }

    if (!scrolledContentsLayerForFrameView(frameView))
        return;

    RefPtr counterScrollingLayer = counterScrollingLayerForFrameView(frameView);
    RefPtr insetClipLayer = insetClipLayerForFrameView(frameView);
    RefPtr contentShadowLayer = contentShadowLayerForFrameView(frameView);
    RefPtr rootContentsLayer = rootContentsLayerForFrameView(frameView);
    RefPtr headerLayer = headerLayerForFrameView(frameView);
    RefPtr footerLayer = footerLayerForFrameView(frameView);

    ASSERT(frameView.scrollPosition() == roundedIntPoint(scrollPosition));
    LayoutPoint scrollPositionForFixed = frameView.scrollPositionForFixedPosition();
    float topContentInset = frameView.topContentInset();

    FloatPoint positionForInsetClipLayer;
    if (insetClipLayer)
        positionForInsetClipLayer = FloatPoint(insetClipLayer->position().x(), LocalFrameView::yPositionForInsetClipLayer(scrollPosition, topContentInset));
    FloatPoint positionForContentsLayer = frameView.positionForRootContentLayer();
    
    FloatPoint positionForHeaderLayer = FloatPoint(scrollPositionForFixed.x(), LocalFrameView::yPositionForHeaderLayer(scrollPosition, topContentInset));
    FloatPoint positionForFooterLayer = FloatPoint(scrollPositionForFixed.x(),
        LocalFrameView::yPositionForFooterLayer(scrollPosition, topContentInset, frameView.totalContentsSize().height(), frameView.footerHeight()));

    if (scrollType == ScrollType::Programmatic || scrollingLayerPositionAction == ScrollingLayerPositionAction::Set) {
        reconcileScrollPosition(frameView, ScrollingLayerPositionAction::Set);

        if (counterScrollingLayer)
            counterScrollingLayer->setPosition(scrollPositionForFixed);
        if (insetClipLayer)
            insetClipLayer->setPosition(positionForInsetClipLayer);
        if (contentShadowLayer)
            contentShadowLayer->setPosition(positionForContentsLayer);
        if (rootContentsLayer)
            rootContentsLayer->setPosition(positionForContentsLayer);
        if (headerLayer)
            headerLayer->setPosition(positionForHeaderLayer);
        if (footerLayer)
            footerLayer->setPosition(positionForFooterLayer);
    } else {
        reconcileScrollPosition(frameView, ScrollingLayerPositionAction::Sync);

        if (counterScrollingLayer)
            counterScrollingLayer->syncPosition(scrollPositionForFixed);
        if (insetClipLayer)
            insetClipLayer->syncPosition(positionForInsetClipLayer);
        if (contentShadowLayer)
            contentShadowLayer->syncPosition(positionForContentsLayer);
        if (rootContentsLayer)
            rootContentsLayer->syncPosition(positionForContentsLayer);
        if (headerLayer)
            headerLayer->syncPosition(positionForHeaderLayer);
        if (footerLayer)
            footerLayer->syncPosition(positionForFooterLayer);
    }
}

void AsyncScrollingCoordinator::reconcileScrollPosition(LocalFrameView& frameView, ScrollingLayerPositionAction scrollingLayerPositionAction)
{
#if PLATFORM(IOS_FAMILY)
    // Doing all scrolling like this (UIScrollView style) would simplify code.
    auto* scrollContainerLayer = scrollContainerLayerForFrameView(frameView);
    if (!scrollContainerLayer)
        return;
    if (scrollingLayerPositionAction == ScrollingLayerPositionAction::Set)
        scrollContainerLayer->setBoundsOrigin(frameView.scrollPosition());
    else
        scrollContainerLayer->syncBoundsOrigin(frameView.scrollPosition());
#else
    // This uses scrollPosition because the root content layer accounts for scrollOrigin (see LocalFrameView::positionForRootContentLayer()).
    auto* scrolledContentsLayer = scrolledContentsLayerForFrameView(frameView);
    if (!scrolledContentsLayer)
        return;
    if (scrollingLayerPositionAction == ScrollingLayerPositionAction::Set)
        scrolledContentsLayer->setPosition(-frameView.scrollPosition());
    else
        scrolledContentsLayer->syncPosition(-frameView.scrollPosition());
#endif
}

void AsyncScrollingCoordinator::scrollBySimulatingWheelEventForTesting(ScrollingNodeID nodeID, FloatSize delta)
{
    if (m_scrollingTree)
        m_scrollingTree->scrollBySimulatingWheelEventForTesting(nodeID, delta);
}

void AsyncScrollingCoordinator::scrollableAreaScrollbarLayerDidChange(ScrollableArea& scrollableArea, ScrollbarOrientation orientation)
{
    ASSERT(isMainThread());
    ASSERT(page());

    if (RefPtr scrollingNode = dynamicDowncast<ScrollingStateScrollingNode>(stateNodeForScrollableArea(scrollableArea))) {
        if (orientation == ScrollbarOrientation::Vertical)
            scrollingNode->setVerticalScrollbarLayer(scrollableArea.layerForVerticalScrollbar());
        else
            scrollingNode->setHorizontalScrollbarLayer(scrollableArea.layerForHorizontalScrollbar());
        scrollingNode->setScrollbarLayoutDirection(scrollableArea.shouldPlaceVerticalScrollbarOnLeft() ? UserInterfaceLayoutDirection::RTL : UserInterfaceLayoutDirection::LTR);
    }

    if (orientation == ScrollbarOrientation::Vertical)
        scrollableArea.verticalScrollbarLayerDidChange();
    else
        scrollableArea.horizontalScrollbarLayerDidChange();
}

std::optional<ScrollingNodeID> AsyncScrollingCoordinator::createNode(FrameIdentifier rootFrameID, ScrollingNodeType nodeType, ScrollingNodeID newNodeID)
{
    LOG_WITH_STREAM(ScrollingTree, stream << "AsyncScrollingCoordinator::createNode " << nodeType << " node " << newNodeID);
    auto& scrollingStateTree = ensureScrollingStateTreeForRootFrameID(rootFrameID);
    // TODO: rdar://123052250 Need a better way to fix scrolling tree in iframe process
    if ((!scrollingStateTree.rootStateNode() && nodeType == ScrollingNodeType::Subframe) || (scrollingStateTree.rootStateNode() && scrollingStateTree.rootStateNode()->scrollingNodeID() == newNodeID))
        return scrollingStateTree.insertNode(nodeType, newNodeID, std::nullopt, 0);
    return scrollingStateTree.createUnparentedNode(nodeType, newNodeID);
}

std::optional<ScrollingNodeID> AsyncScrollingCoordinator::insertNode(FrameIdentifier rootFrameID, ScrollingNodeType nodeType, ScrollingNodeID newNodeID, std::optional<ScrollingNodeID> parentID, size_t childIndex)
{
    LOG_WITH_STREAM(ScrollingTree, stream << "AsyncScrollingCoordinator::insertNode " << nodeType << " node " << newNodeID << " parent " << parentID << " index " << childIndex);
    return ensureScrollingStateTreeForRootFrameID(rootFrameID).insertNode(nodeType, newNodeID, parentID, childIndex);
}

void AsyncScrollingCoordinator::unparentNode(ScrollingNodeID nodeID)
{
    if (auto* stateTree = stateTreeForNodeID(nodeID))
        stateTree->unparentNode(nodeID);
}

void AsyncScrollingCoordinator::unparentChildrenAndDestroyNode(std::optional<ScrollingNodeID> nodeID)
{
    if (auto* stateTree = stateTreeForNodeID(nodeID))
        stateTree->unparentChildrenAndDestroyNode(nodeID);
}

void AsyncScrollingCoordinator::detachAndDestroySubtree(ScrollingNodeID nodeID)
{
    if (auto* stateTree = stateTreeForNodeID(nodeID))
        stateTree->detachAndDestroySubtree(nodeID);
}

void AsyncScrollingCoordinator::clearAllNodes(FrameIdentifier rootFrameID)
{
    ensureScrollingStateTreeForRootFrameID(rootFrameID).clear();
}

std::optional<ScrollingNodeID> AsyncScrollingCoordinator::parentOfNode(ScrollingNodeID nodeID) const
{
    auto scrollingNode = stateNodeForNodeID(nodeID);
    if (!scrollingNode)
        return std::nullopt;

    return scrollingNode->parentNodeID();
}

Vector<ScrollingNodeID> AsyncScrollingCoordinator::childrenOfNode(ScrollingNodeID nodeID) const
{
    auto scrollingNode = stateNodeForNodeID(nodeID);
    if (!scrollingNode)
        return { };

    return scrollingNode->children().map([](auto& child) {
        return child->scrollingNodeID();
    });
}

void AsyncScrollingCoordinator::reconcileViewportConstrainedLayerPositions(std::optional<ScrollingNodeID> scrollingNodeID, const LayoutRect& viewportRect, ScrollingLayerPositionAction action)
{
    LOG_WITH_STREAM(Scrolling, stream << getCurrentProcessID() << " AsyncScrollingCoordinator::reconcileViewportConstrainedLayerPositions for viewport rect " << viewportRect << " and node " << scrollingNodeID);
    if (auto* stateTree = stateTreeForNodeID(scrollingNodeID))
        stateTree->reconcileViewportConstrainedLayerPositions(scrollingNodeID, viewportRect, action);
}

void AsyncScrollingCoordinator::ensureRootStateNodeForFrameView(LocalFrameView& frameView)
{
    ASSERT(frameView.scrollingNodeID());
    if (stateNodeForScrollableArea(frameView))
        return;

    // For non-main frames, it is only possible to arrive in this function from
    // RenderLayerCompositor::updateBacking where the node has already been created.
    ASSERT(frameView.frame().isMainFrame());
    insertNode(frameView.frame().rootFrame().frameID(), ScrollingNodeType::MainFrame, *frameView.scrollingNodeID(), { }, 0);
}

void AsyncScrollingCoordinator::setNodeLayers(ScrollingNodeID nodeID, const NodeLayers& nodeLayers)
{
    RefPtr node = stateNodeForNodeID(nodeID);
    ASSERT(node);
    if (!node)
        return;

    node->setLayer(nodeLayers.layer);

    if (auto* scrollingNode = dynamicDowncast<ScrollingStateScrollingNode>(*node)) {
        scrollingNode->setScrollContainerLayer(nodeLayers.scrollContainerLayer);
        scrollingNode->setScrolledContentsLayer(nodeLayers.scrolledContentsLayer);
        scrollingNode->setHorizontalScrollbarLayer(nodeLayers.horizontalScrollbarLayer);
        scrollingNode->setVerticalScrollbarLayer(nodeLayers.verticalScrollbarLayer);
        if (RefPtr frameView = frameViewForScrollingNode(nodeID)) {
            if (CheckedPtr scrollableArea = frameView->scrollableAreaForScrollingNodeID(nodeID))
                scrollingNode->setScrollbarLayoutDirection(scrollableArea->shouldPlaceVerticalScrollbarOnLeft() ? UserInterfaceLayoutDirection::RTL : UserInterfaceLayoutDirection::LTR);
        }

        if (auto* frameScrollingNode = dynamicDowncast<ScrollingStateFrameScrollingNode>(*scrollingNode)) {
            frameScrollingNode->setInsetClipLayer(nodeLayers.insetClipLayer);
            frameScrollingNode->setCounterScrollingLayer(nodeLayers.counterScrollingLayer);
            frameScrollingNode->setRootContentsLayer(nodeLayers.rootContentsLayer);
        }
    }
}

void AsyncScrollingCoordinator::setFrameScrollingNodeState(ScrollingNodeID nodeID, const LocalFrameView& frameView)
{
    RefPtr stateNode = stateNodeForNodeID(nodeID);
    ASSERT(stateNode);
    RefPtr frameScrollingNode = dynamicDowncast<ScrollingStateFrameScrollingNode>(stateNode);
    if (!frameScrollingNode)
        return;

    auto& settings = page()->mainFrame().settings();

    frameScrollingNode->setFrameScaleFactor(frameView.frame().frameScaleFactor());
    frameScrollingNode->setHeaderHeight(frameView.headerHeight());
    frameScrollingNode->setFooterHeight(frameView.footerHeight());
    frameScrollingNode->setTopContentInset(frameView.topContentInset());
    frameScrollingNode->setLayoutViewport(frameView.layoutViewportRect());
    frameScrollingNode->setAsyncFrameOrOverflowScrollingEnabled(settings.asyncFrameScrollingEnabled() || settings.asyncOverflowScrollingEnabled());
    frameScrollingNode->setScrollingPerformanceTestingEnabled(settings.scrollingPerformanceTestingEnabled());
    frameScrollingNode->setOverlayScrollbarsEnabled(ScrollbarTheme::theme().usesOverlayScrollbars());
    frameScrollingNode->setScrollbarWidth(frameView.scrollbarWidthStyle());
    frameScrollingNode->setWheelEventGesturesBecomeNonBlocking(settings.wheelEventGesturesBecomeNonBlocking());

    frameScrollingNode->setMinLayoutViewportOrigin(frameView.minStableLayoutViewportOrigin());
    frameScrollingNode->setMaxLayoutViewportOrigin(frameView.maxStableLayoutViewportOrigin());

    if (auto visualOverrideRect = frameView.visualViewportOverrideRect())
        frameScrollingNode->setOverrideVisualViewportSize(FloatSize(visualOverrideRect.value().size()));
    else
        frameScrollingNode->setOverrideVisualViewportSize(std::nullopt);

    frameScrollingNode->setFixedElementsLayoutRelativeToFrame(frameView.fixedElementsLayoutRelativeToFrame());

    auto visualViewportIsSmallerThanLayoutViewport = [](const LocalFrameView& frameView) {
        auto layoutViewport = frameView.layoutViewportRect();
        auto visualViewport = frameView.visualViewportRect();
        return visualViewport.width() < layoutViewport.width() || visualViewport.height() < layoutViewport.height();
    };
    frameScrollingNode->setVisualViewportIsSmallerThanLayoutViewport(visualViewportIsSmallerThanLayoutViewport(frameView));
    
    frameScrollingNode->setScrollBehaviorForFixedElements(frameView.scrollBehaviorForFixedElements());
}

void AsyncScrollingCoordinator::setScrollingNodeScrollableAreaGeometry(std::optional<ScrollingNodeID> nodeID, ScrollableArea& scrollableArea)
{
    auto scrollingNode = dynamicDowncast<ScrollingStateScrollingNode>(stateNodeForNodeID(nodeID));
    if (!scrollingNode)
        return;

    auto* verticalScrollbar = scrollableArea.verticalScrollbar();
    auto* horizontalScrollbar = scrollableArea.horizontalScrollbar();
    scrollingNode->setScrollerImpsFromScrollbars(verticalScrollbar, horizontalScrollbar);
    if (horizontalScrollbar)
        scrollingNode->setScrollbarEnabledState(ScrollbarOrientation::Horizontal, horizontalScrollbar->enabled());
    if (verticalScrollbar)
        scrollingNode->setScrollbarEnabledState(ScrollbarOrientation::Vertical, verticalScrollbar->enabled());
    scrollingNode->setScrollbarWidth(scrollableArea.scrollbarWidthStyle());

    scrollingNode->setScrollOrigin(scrollableArea.scrollOrigin());
    scrollingNode->setScrollPosition(scrollableArea.scrollPosition());
    scrollingNode->setTotalContentsSize(scrollableArea.totalContentsSize());
    scrollingNode->setReachableContentsSize(scrollableArea.reachableTotalContentsSize());
    scrollingNode->setScrollableAreaSize(scrollableArea.visibleSize());

    ScrollableAreaParameters scrollParameters;
    scrollParameters.horizontalScrollElasticity = scrollableArea.horizontalOverscrollBehavior() == OverscrollBehavior::None ? ScrollElasticity::None : scrollableArea.horizontalScrollElasticity();
    scrollParameters.verticalScrollElasticity = scrollableArea.verticalOverscrollBehavior() == OverscrollBehavior::None ? ScrollElasticity::None : scrollableArea.verticalScrollElasticity();
    scrollParameters.allowsHorizontalScrolling = scrollableArea.allowsHorizontalScrolling();
    scrollParameters.allowsVerticalScrolling = scrollableArea.allowsVerticalScrolling();
    scrollParameters.horizontalOverscrollBehavior = scrollableArea.horizontalOverscrollBehavior();
    scrollParameters.verticalOverscrollBehavior = scrollableArea.verticalOverscrollBehavior();
    scrollParameters.horizontalScrollbarMode = scrollableArea.horizontalScrollbarMode();
    scrollParameters.verticalScrollbarMode = scrollableArea.verticalScrollbarMode();
    scrollParameters.horizontalNativeScrollbarVisibility = scrollableArea.horizontalNativeScrollbarVisibility();
    scrollParameters.verticalNativeScrollbarVisibility = scrollableArea.verticalNativeScrollbarVisibility();
    scrollParameters.useDarkAppearanceForScrollbars = scrollableArea.useDarkAppearanceForScrollbars();
    scrollParameters.scrollbarWidthStyle = scrollableArea.scrollbarWidthStyle();

    scrollingNode->setScrollableAreaParameters(scrollParameters);

    scrollableArea.updateSnapOffsets();
    setStateScrollingNodeSnapOffsetsAsFloat(*scrollingNode, scrollableArea.snapOffsetsInfo(), page()->deviceScaleFactor());
    scrollingNode->setCurrentHorizontalSnapPointIndex(scrollableArea.currentHorizontalSnapPointIndex());
    scrollingNode->setCurrentVerticalSnapPointIndex(scrollableArea.currentVerticalSnapPointIndex());
}

void AsyncScrollingCoordinator::setViewportConstraintedNodeConstraints(ScrollingNodeID nodeID, const ViewportConstraints& constraints)
{
    RefPtr node = stateNodeForNodeID(nodeID);
    if (!node)
        return;

    switch (constraints.constraintType()) {
    case ViewportConstraints::FixedPositionConstraint: {
        if (RefPtr fixedNode = dynamicDowncast<ScrollingStateFixedNode>(node))
            fixedNode->updateConstraints((const FixedPositionViewportConstraints&)constraints);
        break;
    }
    case ViewportConstraints::StickyPositionConstraint: {
        if (RefPtr stickyNode = dynamicDowncast<ScrollingStateStickyNode>(node))
            stickyNode->updateConstraints((const StickyPositionViewportConstraints&)constraints);
        break;
    }
    }
}

void AsyncScrollingCoordinator::setPositionedNodeConstraints(ScrollingNodeID nodeID, const AbsolutePositionConstraints& constraints)
{
    RefPtr node = stateNodeForNodeID(nodeID);
    if (!node)
        return;

    ASSERT(is<ScrollingStatePositionedNode>(*node));
    if (auto* positionedNode = dynamicDowncast<ScrollingStatePositionedNode>(*node))
        positionedNode->updateConstraints(constraints);
}

void AsyncScrollingCoordinator::setRelatedOverflowScrollingNodes(ScrollingNodeID nodeID, Vector<ScrollingNodeID>&& relatedNodes)
{
    RefPtr node = stateNodeForNodeID(nodeID);
    if (!node)
        return;

    if (auto* positionedNode = dynamicDowncast<ScrollingStatePositionedNode>(*node))
        positionedNode->setRelatedOverflowScrollingNodes(WTFMove(relatedNodes));
    else if (auto* overflowScrollProxyNode = dynamicDowncast<ScrollingStateOverflowScrollProxyNode>(*node)) {
        if (!relatedNodes.isEmpty())
            overflowScrollProxyNode->setOverflowScrollingNode(relatedNodes[0]);
        else
            overflowScrollProxyNode->setOverflowScrollingNode(std::nullopt);
    } else
        ASSERT_NOT_REACHED();
}

void AsyncScrollingCoordinator::setSynchronousScrollingReasons(std::optional<ScrollingNodeID> nodeID, OptionSet<SynchronousScrollingReason> reasons)
{
    RefPtr scrollingStateNode = dynamicDowncast<ScrollingStateScrollingNode>(stateNodeForNodeID(nodeID));
    if (!scrollingStateNode)
        return;

    if (reasons && is<ScrollingStateFrameScrollingNode>(*scrollingStateNode)) {
        // The LocalFrameView's GraphicsLayer is likely to be out-of-synch with the PlatformLayer
        // at this point. So we'll update it before we switch back to main thread scrolling
        // in order to avoid layer positioning bugs.
        if (RefPtr frameView = frameViewForScrollingNode(nodeID))
            reconcileScrollPosition(*frameView, ScrollingLayerPositionAction::Set);
    }

    // FIXME: Ideally all the "synchronousScrollingReasons" functions should be #ifdeffed.
#if ENABLE(SCROLLING_THREAD)
    scrollingStateNode->setSynchronousScrollingReasons(reasons);
#endif
}

OptionSet<SynchronousScrollingReason> AsyncScrollingCoordinator::synchronousScrollingReasons(std::optional<ScrollingNodeID> nodeID) const
{
    RefPtr node = dynamicDowncast<ScrollingStateScrollingNode>(stateNodeForNodeID(nodeID));
    if (!node)
        return { };

#if ENABLE(SCROLLING_THREAD)
    return node->synchronousScrollingReasons();
#else
    return { };
#endif
}

void AsyncScrollingCoordinator::windowScreenDidChange(PlatformDisplayID displayID, std::optional<FramesPerSecond> nominalFramesPerSecond)
{
    if (m_scrollingTree)
        m_scrollingTree->windowScreenDidChange(displayID, nominalFramesPerSecond);
}

bool AsyncScrollingCoordinator::hasSubscrollers(FrameIdentifier rootFrameID) const
{
    auto* scrollingStateTree = existingScrollingStateTreeForRootFrameID(rootFrameID);
    return scrollingStateTree && scrollingStateTree->scrollingNodeCount() > 1;
}

bool AsyncScrollingCoordinator::isUserScrollInProgress(std::optional<ScrollingNodeID> nodeID) const
{
    if (m_scrollingTree)
        return m_scrollingTree->isUserScrollInProgressForNode(nodeID);

    return false;
}

bool AsyncScrollingCoordinator::isRubberBandInProgress(std::optional<ScrollingNodeID> nodeID) const
{
    if (m_scrollingTree)
        return m_scrollingTree->isRubberBandInProgressForNode(nodeID);

    return false;
}

void AsyncScrollingCoordinator::setScrollPinningBehavior(ScrollPinningBehavior pinning)
{
    scrollingTree()->setScrollPinningBehavior(pinning);
}

std::optional<ScrollingNodeID> AsyncScrollingCoordinator::scrollableContainerNodeID(const RenderObject& renderer) const
{
    if (auto overflowScrollingNodeID = renderer.view().compositor().asyncScrollableContainerNodeID(renderer))
        return overflowScrollingNodeID;

    // If we're in a scrollable frame, return that.
    RefPtr frameView = renderer.frame().view();
    if (!frameView)
        return std::nullopt;

    if (auto scrollingNodeID = frameView->scrollingNodeID())
        return scrollingNodeID;

    // Otherwise, look for a scrollable element in the containing frame.
    if (RefPtr ownerElement = renderer.document().ownerElement()) {
        if (CheckedPtr frameRenderer = ownerElement->renderer())
            return scrollableContainerNodeID(*frameRenderer);
    }

    return std::nullopt;
}

String AsyncScrollingCoordinator::scrollingStateTreeAsText(OptionSet<ScrollingStateTreeAsTextBehavior> behavior) const
{
    StringBuilder stateTree;
    m_scrollingStateTrees.forEach([&] (auto& key, auto& tree) {
        if (tree->rootStateNode()) {
            if (m_eventTrackingRegionsDirty)
                tree->rootStateNode()->setEventTrackingRegions(absoluteEventTrackingRegions());
            if (m_scrollingStateTrees.size() > 1)
                stateTree.append(makeString("Tree-for-root-frameID: "_s, key.toString()));
            stateTree.append(tree->scrollingStateTreeAsText(behavior));
        }
    });

    return stateTree.toString();
}

String AsyncScrollingCoordinator::scrollingTreeAsText(OptionSet<ScrollingStateTreeAsTextBehavior> behavior) const
{
    if (!m_scrollingTree)
        return emptyString();

    return m_scrollingTree->scrollingTreeAsText(behavior);
}

bool AsyncScrollingCoordinator::haveScrollingTree() const
{
    return !!m_scrollingTree;
}

void AsyncScrollingCoordinator::setActiveScrollSnapIndices(ScrollingNodeID scrollingNodeID, std::optional<unsigned> horizontalIndex, std::optional<unsigned> verticalIndex)
{
    ASSERT(isMainThread());
    
    if (!page())
        return;
    
    RefPtr frameView = frameViewForScrollingNode(scrollingNodeID);
    if (!frameView)
        return;
    
    if (CheckedPtr scrollableArea = frameView->scrollableAreaForScrollingNodeID(scrollingNodeID)) {
        scrollableArea->setCurrentHorizontalSnapPointIndex(horizontalIndex);
        scrollableArea->setCurrentVerticalSnapPointIndex(verticalIndex);
    }
}

bool AsyncScrollingCoordinator::isScrollSnapInProgress(std::optional<ScrollingNodeID> nodeID) const
{
    if (m_scrollingTree)
        return m_scrollingTree->isScrollSnapInProgressForNode(nodeID);

    return false;
}

void AsyncScrollingCoordinator::updateScrollSnapPropertiesWithFrameView(const LocalFrameView& frameView)
{
    if (RefPtr node = dynamicDowncast<ScrollingStateFrameScrollingNode>(stateNodeForScrollableArea(frameView))) {
        setStateScrollingNodeSnapOffsetsAsFloat(*node, frameView.snapOffsetsInfo(), page()->deviceScaleFactor());
        node->setCurrentHorizontalSnapPointIndex(frameView.currentHorizontalSnapPointIndex());
        node->setCurrentVerticalSnapPointIndex(frameView.currentVerticalSnapPointIndex());
    }
}

void AsyncScrollingCoordinator::reportExposedUnfilledArea(MonotonicTime timestamp, unsigned unfilledArea)
{
    if (RefPtr page = this->page(); page && page->performanceLoggingClient())
        page->performanceLoggingClient()->logScrollingEvent(PerformanceLoggingClient::ScrollingEvent::ExposedTilelessArea, timestamp, unfilledArea);
}

void AsyncScrollingCoordinator::reportSynchronousScrollingReasonsChanged(MonotonicTime timestamp, OptionSet<SynchronousScrollingReason> reasons)
{
    if (RefPtr page = this->page(); page && page->performanceLoggingClient())
        page->performanceLoggingClient()->logScrollingEvent(PerformanceLoggingClient::ScrollingEvent::SwitchedScrollingMode, timestamp, reasons.toRaw());
}

bool AsyncScrollingCoordinator::scrollAnimatorEnabled() const
{
    ASSERT(isMainThread());
    auto* localMainFrame = dynamicDowncast<LocalFrame>(page()->mainFrame());
    if (!localMainFrame)
        return false;
    auto& settings = localMainFrame->settings();
    return settings.scrollAnimatorEnabled();
}

std::unique_ptr<ScrollingStateTree> AsyncScrollingCoordinator::commitTreeStateForRootFrameID(FrameIdentifier rootFrameID, LayerRepresentation::Type type)
{
    auto& scrollingStateTree = ensureScrollingStateTreeForRootFrameID(rootFrameID);
    return scrollingStateTree.commit(type);
}

} // namespace WebCore

#endif // ENABLE(ASYNC_SCROLLING)
