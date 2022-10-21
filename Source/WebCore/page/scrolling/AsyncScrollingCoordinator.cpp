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
#include "Document.h"
#include "EditorClient.h"
#include "Frame.h"
#include "FrameView.h"
#include "GraphicsLayer.h"
#include "Logging.h"
#include "Page.h"
#include "PerformanceLoggingClient.h"
#include "RenderLayerCompositor.h"
#include "RenderView.h"
#include "ScrollAnimator.h"
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
#include <wtf/ProcessID.h>
#include <wtf/text/TextStream.h>

namespace WebCore {

AsyncScrollingCoordinator::AsyncScrollingCoordinator(Page* page)
    : ScrollingCoordinator(page)
    , m_scrollingStateTree(makeUnique<ScrollingStateTree>(this))
{
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

    if (!m_page)
        return;

    auto* frameView = frameViewForScrollingNode(nodeID);
    if (!frameView)
        return;

    if (nodeID == frameView->scrollingNodeID()) {
        frameView->scrollAnimator().handleWheelEventPhase(phase);
        return;
    }

    if (auto* scrollableArea = frameView->scrollableAreaForScrollingNodeID(nodeID))
        scrollableArea->scrollAnimator().handleWheelEventPhase(phase);
}
#endif

static inline void setStateScrollingNodeSnapOffsetsAsFloat(ScrollingStateScrollingNode& node, const LayoutScrollSnapOffsetsInfo* offsetInfo, float deviceScaleFactor)
{
    if (!offsetInfo) {
        node.setSnapOffsetsInfo(FloatScrollSnapOffsetsInfo());
        return;
    }

    // FIXME: Incorporate current page scale factor in snapping to device pixel. Perhaps we should just convert to float here and let UI process do the pixel snapping?
    node.setSnapOffsetsInfo(offsetInfo->convertUnits<FloatScrollSnapOffsetsInfo>(deviceScaleFactor));
}

void AsyncScrollingCoordinator::setEventTrackingRegionsDirty()
{
    m_eventTrackingRegionsDirty = true;
    // We have to schedule a commit, but the computed non-fast region may not have actually changed.
    // FIXME: This needs to disambiguate between event regions in the scrolling tree, and those in GraphicsLayers.
    scheduleTreeStateCommit();
}

void AsyncScrollingCoordinator::willCommitTree()
{
    updateEventTrackingRegions();
}

void AsyncScrollingCoordinator::updateEventTrackingRegions()
{
    if (!m_eventTrackingRegionsDirty)
        return;

    if (!m_scrollingStateTree->rootStateNode())
        return;

    m_scrollingStateTree->rootStateNode()->setEventTrackingRegions(absoluteEventTrackingRegions());
    m_eventTrackingRegionsDirty = false;
}

void AsyncScrollingCoordinator::frameViewLayoutUpdated(FrameView& frameView)
{
    ASSERT(isMainThread());
    ASSERT(m_page);

    // If there isn't a root node yet, don't do anything. We'll be called again after creating one.
    if (!m_scrollingStateTree->rootStateNode())
        return;

    setEventTrackingRegionsDirty();

#if PLATFORM(COCOA)
    if (!coordinatesScrollingForFrameView(frameView))
        return;

    auto* page = frameView.frame().page();
    if (page && page->isMonitoringWheelEvents()) {
        auto* node = m_scrollingStateTree->stateNodeForID(frameView.scrollingNodeID());
        if (!is<ScrollingStateFrameScrollingNode>(node))
            return;

        auto& frameScrollingNode = downcast<ScrollingStateFrameScrollingNode>(*node);
        frameScrollingNode.setIsMonitoringWheelEvents(page->isMonitoringWheelEvents());
    }
#else
    UNUSED_PARAM(frameView);
#endif
}

void AsyncScrollingCoordinator::frameViewVisualViewportChanged(FrameView& frameView)
{
    ASSERT(isMainThread());
    ASSERT(m_page);

    if (!coordinatesScrollingForFrameView(frameView))
        return;
    
    // If the root layer does not have a ScrollingStateNode, then we should create one.
    auto* node = m_scrollingStateTree->stateNodeForID(frameView.scrollingNodeID());
    if (!node)
        return;

    auto& frameScrollingNode = downcast<ScrollingStateFrameScrollingNode>(*node);

    auto visualViewportIsSmallerThanLayoutViewport = [](const FrameView& frameView) {
        auto layoutViewport = frameView.layoutViewportRect();
        auto visualViewport = frameView.visualViewportRect();
        return visualViewport.width() < layoutViewport.width() || visualViewport.height() < layoutViewport.height();
    };
    frameScrollingNode.setVisualViewportIsSmallerThanLayoutViewport(visualViewportIsSmallerThanLayoutViewport(frameView));
}

void AsyncScrollingCoordinator::frameViewWillBeDetached(FrameView& frameView)
{
    ASSERT(isMainThread());
    ASSERT(m_page);

    if (!coordinatesScrollingForFrameView(frameView))
        return;

    auto* scrollingNode = downcast<ScrollingStateScrollingNode>(m_scrollingStateTree->stateNodeForID(frameView.scrollingNodeID()));
    if (!scrollingNode)
        return;

    scrollingNode->setScrollPosition(frameView.scrollPosition());
}

void AsyncScrollingCoordinator::updateIsMonitoringWheelEventsForFrameView(const FrameView& frameView)
{
    auto* page = frameView.frame().page();
    if (!page)
        return;

    auto* node = downcast<ScrollingStateFrameScrollingNode>(m_scrollingStateTree->stateNodeForID(frameView.scrollingNodeID()));
    if (!node)
        return;

    node->setIsMonitoringWheelEvents(page->isMonitoringWheelEvents());
}

void AsyncScrollingCoordinator::frameViewEventTrackingRegionsChanged(FrameView& frameView)
{
    if (!m_scrollingStateTree->rootStateNode())
        return;

    setEventTrackingRegionsDirty();
    DebugPageOverlays::didChangeEventHandlers(frameView.frame());
}

void AsyncScrollingCoordinator::frameViewRootLayerDidChange(FrameView& frameView)
{
    ASSERT(isMainThread());
    ASSERT(m_page);

    if (!coordinatesScrollingForFrameView(frameView))
        return;
    
    // FIXME: In some navigation scenarios, the FrameView has no RenderView or that RenderView has not been composited.
    // This needs cleaning up: https://bugs.webkit.org/show_bug.cgi?id=132724
    if (!frameView.scrollingNodeID())
        return;
    
    // If the root layer does not have a ScrollingStateNode, then we should create one.
    ensureRootStateNodeForFrameView(frameView);
    ASSERT(m_scrollingStateTree->stateNodeForID(frameView.scrollingNodeID()));

    ScrollingCoordinator::frameViewRootLayerDidChange(frameView);

    auto* node = downcast<ScrollingStateFrameScrollingNode>(m_scrollingStateTree->stateNodeForID(frameView.scrollingNodeID()));
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
}

bool AsyncScrollingCoordinator::requestScrollPositionUpdate(ScrollableArea& scrollableArea, const ScrollPosition& scrollPosition, ScrollType scrollType, ScrollClamping clamping)
{
    ASSERT(isMainThread());
    ASSERT(m_page);
    auto scrollingNodeID = scrollableArea.scrollingNodeID();
    if (!scrollingNodeID)
        return false;

    auto* frameView = frameViewForScrollingNode(scrollingNodeID);
    if (!frameView)
        return false;

    if (!coordinatesScrollingForFrameView(*frameView))
        return false;

    setScrollingNodeScrollableAreaGeometry(scrollingNodeID, scrollableArea);

    bool inBackForwardCache = frameView->frame().document()->backForwardCacheState() != Document::NotInBackForwardCache;
    bool isSnapshotting = m_page->isTakingSnapshotsForApplicationSuspension();
    bool inProgrammaticScroll = scrollableArea.currentScrollType() == ScrollType::Programmatic;
    if (inProgrammaticScroll || inBackForwardCache) {
        auto scrollUpdate = ScrollUpdate { scrollingNodeID, scrollPosition, { }, ScrollUpdateType::PositionUpdate, ScrollingLayerPositionAction::Set };
        applyScrollUpdate(WTFMove(scrollUpdate), ScrollType::Programmatic);
    }

    ASSERT(inProgrammaticScroll == (scrollType == ScrollType::Programmatic));

    // If this frame view's document is being put into the back/forward cache, we don't want to update our
    // main frame scroll position. Just let the FrameView think that we did.
    if (inBackForwardCache || isSnapshotting)
        return true;

    auto* stateNode = downcast<ScrollingStateScrollingNode>(m_scrollingStateTree->stateNodeForID(scrollingNodeID));
    if (!stateNode)
        return false;

    stateNode->setRequestedScrollData({ ScrollRequestType::PositionUpdate, scrollPosition, scrollType, clamping, ScrollIsAnimated::No });
    // FIXME: This should schedule a rendering update
    commitTreeStateIfNeeded();
    return true;
}

bool AsyncScrollingCoordinator::requestAnimatedScrollToPosition(ScrollableArea& scrollableArea, const ScrollPosition& scrollPosition, ScrollClamping clamping)
{
    ASSERT(isMainThread());
    ASSERT(m_page);
    auto scrollingNodeID = scrollableArea.scrollingNodeID();
    if (!scrollingNodeID)
        return false;

    auto* frameView = frameViewForScrollingNode(scrollingNodeID);
    if (!frameView || !coordinatesScrollingForFrameView(*frameView))
        return false;

    auto* stateNode = downcast<ScrollingStateScrollingNode>(m_scrollingStateTree->stateNodeForID(scrollingNodeID));
    if (!stateNode)
        return false;

    // Animated scrolls are always programmatic.
    stateNode->setRequestedScrollData({ ScrollRequestType::PositionUpdate, scrollPosition, ScrollType::Programmatic, clamping, ScrollIsAnimated::Yes });
    // FIXME: This should schedule a rendering update
    commitTreeStateIfNeeded();
    return true;
}

void AsyncScrollingCoordinator::stopAnimatedScroll(ScrollableArea& scrollableArea)
{
    ASSERT(isMainThread());
    ASSERT(m_page);
    auto scrollingNodeID = scrollableArea.scrollingNodeID();
    if (!scrollingNodeID)
        return;

    auto* frameView = frameViewForScrollingNode(scrollingNodeID);
    if (!frameView || !coordinatesScrollingForFrameView(*frameView))
        return;

    auto* stateNode = downcast<ScrollingStateScrollingNode>(m_scrollingStateTree->stateNodeForID(scrollingNodeID));
    if (!stateNode)
        return;

    // Animated scrolls are always programmatic.
    stateNode->setRequestedScrollData({ ScrollRequestType::CancelAnimatedScroll, { } });
    // FIXME: This should schedule a rendering update
    commitTreeStateIfNeeded();
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
    if (m_page)
        m_page->scheduleRenderingUpdate(RenderingUpdateStep::ScrollingTreeUpdate);
}

FrameView* AsyncScrollingCoordinator::frameViewForScrollingNode(ScrollingNodeID scrollingNodeID) const
{
    if (!m_scrollingStateTree->rootStateNode())
        return nullptr;
    
    if (scrollingNodeID == m_scrollingStateTree->rootStateNode()->scrollingNodeID())
        return m_page->mainFrame().view();

    auto* stateNode = m_scrollingStateTree->stateNodeForID(scrollingNodeID);
    if (!stateNode)
        return nullptr;

    // Find the enclosing frame scrolling node.
    auto* parentNode = stateNode;
    while (parentNode && !parentNode->isFrameScrollingNode())
        parentNode = parentNode->parent();
    
    if (!parentNode)
        return nullptr;
    
    // Walk the frame tree to find the matching FrameView. This is not ideal, but avoids back pointers to FrameViews
    // from ScrollingTreeStateNodes.
    for (AbstractFrame* frame = &m_page->mainFrame(); frame; frame = frame->tree().traverseNext()) {
        auto* localFrame = dynamicDowncast<LocalFrame>(frame);
        if (!localFrame)
            continue;
        if (auto* view = localFrame->view()) {
            if (view->scrollingNodeID() == parentNode->scrollingNodeID())
                return view;
        }
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
    if (update.updateType == ScrollUpdateType::AnimatedScrollDidEnd) {
        animatedScrollDidEndForNode(update.nodeID);
        return;
    }

    updateScrollPositionAfterAsyncScroll(update.nodeID, update.scrollPosition, update.layoutViewportOrigin, update.updateLayerPositionAction, scrollType);
}

void AsyncScrollingCoordinator::animatedScrollDidEndForNode(ScrollingNodeID scrollingNodeID)
{
    ASSERT(isMainThread());

    if (!m_page)
        return;

    auto* frameView = frameViewForScrollingNode(scrollingNodeID);
    if (!frameView)
        return;

    LOG_WITH_STREAM(Scrolling, stream << "AsyncScrollingCoordinator::animatedScrollDidEndForNode node " << scrollingNodeID);

    if (scrollingNodeID == frameView->scrollingNodeID()) {
        frameView->setScrollAnimationStatus(ScrollAnimationStatus::NotAnimating);
        return;
    }

    if (auto* scrollableArea = frameView->scrollableAreaForScrollingNodeID(scrollingNodeID))
        scrollableArea->setScrollAnimationStatus(ScrollAnimationStatus::NotAnimating);
}

void AsyncScrollingCoordinator::updateScrollPositionAfterAsyncScroll(ScrollingNodeID scrollingNodeID, const FloatPoint& scrollPosition, std::optional<FloatPoint> layoutViewportOrigin, ScrollingLayerPositionAction scrollingLayerPositionAction, ScrollType scrollType)
{
    ASSERT(isMainThread());

    if (!m_page)
        return;

    auto* frameViewPtr = frameViewForScrollingNode(scrollingNodeID);
    if (!frameViewPtr)
        return;

    LOG_WITH_STREAM(Scrolling, stream << "AsyncScrollingCoordinator::updateScrollPositionAfterAsyncScroll node " << scrollingNodeID << " " << scrollType << " scrollPosition " << scrollPosition << " action " << scrollingLayerPositionAction);

    auto& frameView = *frameViewPtr;
    
    if (!frameViewPtr->frame().isMainFrame()) {
        if (scrollingLayerPositionAction == ScrollingLayerPositionAction::Set)
            m_page->editorClient().subFrameScrollPositionChanged();
    }

    if (scrollingNodeID == frameView.scrollingNodeID()) {
        reconcileScrollingState(frameView, scrollPosition, layoutViewportOrigin, scrollType, ViewportRectStability::Stable, scrollingLayerPositionAction);
        return;
    }

    // Overflow-scroll area.
    if (auto* scrollableArea = frameView.scrollableAreaForScrollingNodeID(scrollingNodeID)) {
        auto previousScrollType = scrollableArea->currentScrollType();
        scrollableArea->setCurrentScrollType(scrollType);
        scrollableArea->notifyScrollPositionChanged(roundedIntPoint(scrollPosition));
        scrollableArea->setCurrentScrollType(previousScrollType);

        if (scrollingLayerPositionAction == ScrollingLayerPositionAction::Set)
            m_page->editorClient().overflowScrollPositionChanged();
    }
}

void AsyncScrollingCoordinator::reconcileScrollingState(FrameView& frameView, const FloatPoint& scrollPosition, const LayoutViewportOriginOrOverrideRect& layoutViewportOriginOrOverrideRect, ScrollType scrollType, ViewportRectStability viewportRectStability, ScrollingLayerPositionAction scrollingLayerPositionAction)
{
    auto previousScrollType = frameView.currentScrollType();
    frameView.setCurrentScrollType(scrollType);

    LOG_WITH_STREAM(Scrolling, stream << getCurrentProcessID() << " AsyncScrollingCoordinator " << this << " reconcileScrollingState scrollPosition " << scrollPosition << " type " << scrollType << " stability " << viewportRectStability << " " << scrollingLayerPositionAction);

    std::optional<FloatRect> layoutViewportRect;

    WTF::switchOn(layoutViewportOriginOrOverrideRect,
        [&frameView](std::optional<FloatPoint> origin) {
            if (origin)
                frameView.setBaseLayoutViewportOrigin(LayoutPoint(origin.value()), FrameView::TriggerLayoutOrNot::No);
        }, [&frameView, &layoutViewportRect, viewportRectStability](std::optional<FloatRect> overrideRect) {
            if (!overrideRect)
                return;

            layoutViewportRect = overrideRect;
            if (viewportRectStability != ViewportRectStability::ChangingObscuredInsetsInteractively)
                frameView.setLayoutViewportOverrideRect(LayoutRect(overrideRect.value()), viewportRectStability == ViewportRectStability::Stable ? FrameView::TriggerLayoutOrNot::Yes : FrameView::TriggerLayoutOrNot::No);
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

    auto* counterScrollingLayer = counterScrollingLayerForFrameView(frameView);
    auto* insetClipLayer = insetClipLayerForFrameView(frameView);
    auto* contentShadowLayer = contentShadowLayerForFrameView(frameView);
    auto* rootContentsLayer = rootContentsLayerForFrameView(frameView);
    auto* headerLayer = headerLayerForFrameView(frameView);
    auto* footerLayer = footerLayerForFrameView(frameView);

    ASSERT(frameView.scrollPosition() == roundedIntPoint(scrollPosition));
    LayoutPoint scrollPositionForFixed = frameView.scrollPositionForFixedPosition();
    float topContentInset = frameView.topContentInset();

    FloatPoint positionForInsetClipLayer;
    if (insetClipLayer)
        positionForInsetClipLayer = FloatPoint(insetClipLayer->position().x(), FrameView::yPositionForInsetClipLayer(scrollPosition, topContentInset));
    FloatPoint positionForContentsLayer = frameView.positionForRootContentLayer();
    
    FloatPoint positionForHeaderLayer = FloatPoint(scrollPositionForFixed.x(), FrameView::yPositionForHeaderLayer(scrollPosition, topContentInset));
    FloatPoint positionForFooterLayer = FloatPoint(scrollPositionForFixed.x(),
        FrameView::yPositionForFooterLayer(scrollPosition, topContentInset, frameView.totalContentsSize().height(), frameView.footerHeight()));

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

void AsyncScrollingCoordinator::reconcileScrollPosition(FrameView& frameView, ScrollingLayerPositionAction scrollingLayerPositionAction)
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
    // This uses scrollPosition because the root content layer accounts for scrollOrigin (see FrameView::positionForRootContentLayer()).
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
    ASSERT(m_page);

    auto* node = m_scrollingStateTree->stateNodeForID(scrollableArea.scrollingNodeID());
    if (is<ScrollingStateScrollingNode>(node)) {
        auto& scrollingNode = downcast<ScrollingStateScrollingNode>(*node);
        if (orientation == ScrollbarOrientation::Vertical)
            scrollingNode.setVerticalScrollbarLayer(scrollableArea.layerForVerticalScrollbar());
        else
            scrollingNode.setHorizontalScrollbarLayer(scrollableArea.layerForHorizontalScrollbar());
    }

    if (orientation == ScrollbarOrientation::Vertical)
        scrollableArea.verticalScrollbarLayerDidChange();
    else
        scrollableArea.horizontalScrollbarLayerDidChange();
}

ScrollingNodeID AsyncScrollingCoordinator::createNode(ScrollingNodeType nodeType, ScrollingNodeID newNodeID)
{
    LOG_WITH_STREAM(ScrollingTree, stream << "AsyncScrollingCoordinator::createNode " << nodeType << " node " << newNodeID);
    return m_scrollingStateTree->createUnparentedNode(nodeType, newNodeID);
}

ScrollingNodeID AsyncScrollingCoordinator::insertNode(ScrollingNodeType nodeType, ScrollingNodeID newNodeID, ScrollingNodeID parentID, size_t childIndex)
{
    LOG_WITH_STREAM(ScrollingTree, stream << "AsyncScrollingCoordinator::insertNode " << nodeType << " node " << newNodeID << " parent " << parentID << " index " << childIndex);
    return m_scrollingStateTree->insertNode(nodeType, newNodeID, parentID, childIndex);
}

void AsyncScrollingCoordinator::unparentNode(ScrollingNodeID nodeID)
{
    m_scrollingStateTree->unparentNode(nodeID);
}

void AsyncScrollingCoordinator::unparentChildrenAndDestroyNode(ScrollingNodeID nodeID)
{
    m_scrollingStateTree->unparentChildrenAndDestroyNode(nodeID);
}

void AsyncScrollingCoordinator::detachAndDestroySubtree(ScrollingNodeID nodeID)
{
    m_scrollingStateTree->detachAndDestroySubtree(nodeID);
}

void AsyncScrollingCoordinator::clearAllNodes()
{
    m_scrollingStateTree->clear();
}

ScrollingNodeID AsyncScrollingCoordinator::parentOfNode(ScrollingNodeID nodeID) const
{
    auto* scrollingNode = m_scrollingStateTree->stateNodeForID(nodeID);
    if (!scrollingNode)
        return 0;

    return scrollingNode->parentNodeID();
}

Vector<ScrollingNodeID> AsyncScrollingCoordinator::childrenOfNode(ScrollingNodeID nodeID) const
{
    auto* scrollingNode = m_scrollingStateTree->stateNodeForID(nodeID);
    if (!scrollingNode)
        return { };

    auto* children = scrollingNode->children();
    if (!children || children->isEmpty())
        return { };
    
    return children->map([](auto& child) {
        return child->scrollingNodeID();
    });
}

void AsyncScrollingCoordinator::reconcileViewportConstrainedLayerPositions(ScrollingNodeID scrollingNodeID, const LayoutRect& viewportRect, ScrollingLayerPositionAction action)
{
    LOG_WITH_STREAM(Scrolling, stream << getCurrentProcessID() << " AsyncScrollingCoordinator::reconcileViewportConstrainedLayerPositions for viewport rect " << viewportRect << " and node " << scrollingNodeID);

    m_scrollingStateTree->reconcileViewportConstrainedLayerPositions(scrollingNodeID, viewportRect, action);
}

void AsyncScrollingCoordinator::ensureRootStateNodeForFrameView(FrameView& frameView)
{
    ASSERT(frameView.scrollingNodeID());
    if (m_scrollingStateTree->stateNodeForID(frameView.scrollingNodeID()))
        return;

    // For non-main frames, it is only possible to arrive in this function from
    // RenderLayerCompositor::updateBacking where the node has already been created.
    ASSERT(frameView.frame().isMainFrame());
    insertNode(ScrollingNodeType::MainFrame, frameView.scrollingNodeID(), 0, 0);
}

void AsyncScrollingCoordinator::setNodeLayers(ScrollingNodeID nodeID, const NodeLayers& nodeLayers)
{
    auto* node = m_scrollingStateTree->stateNodeForID(nodeID);
    ASSERT(node);
    if (!node)
        return;

    node->setLayer(nodeLayers.layer);

    if (is<ScrollingStateScrollingNode>(node)) {
        auto& scrollingNode = downcast<ScrollingStateScrollingNode>(*node);
        scrollingNode.setScrollContainerLayer(nodeLayers.scrollContainerLayer);
        scrollingNode.setScrolledContentsLayer(nodeLayers.scrolledContentsLayer);
        scrollingNode.setHorizontalScrollbarLayer(nodeLayers.horizontalScrollbarLayer);
        scrollingNode.setVerticalScrollbarLayer(nodeLayers.verticalScrollbarLayer);

        if (is<ScrollingStateFrameScrollingNode>(node)) {
            auto& frameScrollingNode = downcast<ScrollingStateFrameScrollingNode>(*node);
            frameScrollingNode.setInsetClipLayer(nodeLayers.insetClipLayer);
            frameScrollingNode.setCounterScrollingLayer(nodeLayers.counterScrollingLayer);
            frameScrollingNode.setRootContentsLayer(nodeLayers.rootContentsLayer);
        }
    }
}

void AsyncScrollingCoordinator::setFrameScrollingNodeState(ScrollingNodeID nodeID, const FrameView& frameView)
{
    auto* stateNode = m_scrollingStateTree->stateNodeForID(nodeID);
    ASSERT(stateNode);
    if (!is<ScrollingStateFrameScrollingNode>(stateNode))
        return;

    auto& settings = m_page->mainFrame().settings();
    auto& frameScrollingNode = downcast<ScrollingStateFrameScrollingNode>(*stateNode);

    frameScrollingNode.setFrameScaleFactor(frameView.frame().frameScaleFactor());
    frameScrollingNode.setHeaderHeight(frameView.headerHeight());
    frameScrollingNode.setFooterHeight(frameView.footerHeight());
    frameScrollingNode.setTopContentInset(frameView.topContentInset());
    frameScrollingNode.setLayoutViewport(frameView.layoutViewportRect());
    frameScrollingNode.setAsyncFrameOrOverflowScrollingEnabled(settings.asyncFrameScrollingEnabled() || settings.asyncOverflowScrollingEnabled());
    frameScrollingNode.setScrollingPerformanceTestingEnabled(settings.scrollingPerformanceTestingEnabled());
    frameScrollingNode.setWheelEventGesturesBecomeNonBlocking(settings.wheelEventGesturesBecomeNonBlocking());

    frameScrollingNode.setMinLayoutViewportOrigin(frameView.minStableLayoutViewportOrigin());
    frameScrollingNode.setMaxLayoutViewportOrigin(frameView.maxStableLayoutViewportOrigin());

    if (auto visualOverrideRect = frameView.visualViewportOverrideRect())
        frameScrollingNode.setOverrideVisualViewportSize(FloatSize(visualOverrideRect.value().size()));
    else
        frameScrollingNode.setOverrideVisualViewportSize(std::nullopt);

    frameScrollingNode.setFixedElementsLayoutRelativeToFrame(frameView.fixedElementsLayoutRelativeToFrame());

    auto visualViewportIsSmallerThanLayoutViewport = [](const FrameView& frameView) {
        auto layoutViewport = frameView.layoutViewportRect();
        auto visualViewport = frameView.visualViewportRect();
        return visualViewport.width() < layoutViewport.width() || visualViewport.height() < layoutViewport.height();
    };
    frameScrollingNode.setVisualViewportIsSmallerThanLayoutViewport(visualViewportIsSmallerThanLayoutViewport(frameView));
    
    frameScrollingNode.setScrollBehaviorForFixedElements(frameView.scrollBehaviorForFixedElements());
}

void AsyncScrollingCoordinator::setScrollingNodeScrollableAreaGeometry(ScrollingNodeID nodeID, ScrollableArea& scrollableArea)
{
    auto* stateNode = m_scrollingStateTree->stateNodeForID(nodeID);
    ASSERT(stateNode);
    if (!stateNode)
        return;

    auto& scrollingNode = downcast<ScrollingStateScrollingNode>(*stateNode);

    auto* verticalScrollbar = scrollableArea.verticalScrollbar();
    auto* horizontalScrollbar = scrollableArea.horizontalScrollbar();
    scrollingNode.setScrollerImpsFromScrollbars(verticalScrollbar, horizontalScrollbar);

    scrollingNode.setScrollOrigin(scrollableArea.scrollOrigin());
    scrollingNode.setScrollPosition(scrollableArea.scrollPosition());
    scrollingNode.setTotalContentsSize(scrollableArea.totalContentsSize());
    scrollingNode.setReachableContentsSize(scrollableArea.reachableTotalContentsSize());
    scrollingNode.setScrollableAreaSize(scrollableArea.visibleSize());

    ScrollableAreaParameters scrollParameters;
    scrollParameters.horizontalScrollElasticity = scrollableArea.horizontalOverscrollBehavior() == OverscrollBehavior::None ? ScrollElasticity::None : scrollableArea.horizontalScrollElasticity();
    scrollParameters.verticalScrollElasticity = scrollableArea.verticalOverscrollBehavior() == OverscrollBehavior::None ? ScrollElasticity::None : scrollableArea.verticalScrollElasticity();
    scrollParameters.allowsHorizontalScrolling = scrollableArea.allowsHorizontalScrolling();
    scrollParameters.allowsVerticalScrolling = scrollableArea.allowsVerticalScrolling();
    scrollParameters.horizontalOverscrollBehavior = scrollableArea.horizontalOverscrollBehavior();
    scrollParameters.verticalOverscrollBehavior = scrollableArea.verticalOverscrollBehavior();
    scrollParameters.horizontalScrollbarMode = scrollableArea.horizontalScrollbarMode();
    scrollParameters.verticalScrollbarMode = scrollableArea.verticalScrollbarMode();
    scrollParameters.horizontalScrollbarHiddenByStyle = scrollableArea.horizontalScrollbarHiddenByStyle();
    scrollParameters.verticalScrollbarHiddenByStyle = scrollableArea.verticalScrollbarHiddenByStyle();
    scrollParameters.useDarkAppearanceForScrollbars = scrollableArea.useDarkAppearanceForScrollbars();

    scrollingNode.setScrollableAreaParameters(scrollParameters);

    scrollableArea.updateSnapOffsets();
    setStateScrollingNodeSnapOffsetsAsFloat(scrollingNode, scrollableArea.snapOffsetsInfo(), m_page->deviceScaleFactor());
    scrollingNode.setCurrentHorizontalSnapPointIndex(scrollableArea.currentHorizontalSnapPointIndex());
    scrollingNode.setCurrentVerticalSnapPointIndex(scrollableArea.currentVerticalSnapPointIndex());
}

void AsyncScrollingCoordinator::setViewportConstraintedNodeConstraints(ScrollingNodeID nodeID, const ViewportConstraints& constraints)
{
    auto* node = m_scrollingStateTree->stateNodeForID(nodeID);
    if (!node)
        return;

    switch (constraints.constraintType()) {
    case ViewportConstraints::FixedPositionConstraint: {
        auto& fixedNode = downcast<ScrollingStateFixedNode>(*node);
        fixedNode.updateConstraints((const FixedPositionViewportConstraints&)constraints);
        break;
    }
    case ViewportConstraints::StickyPositionConstraint: {
        auto& stickyNode = downcast<ScrollingStateStickyNode>(*node);
        stickyNode.updateConstraints((const StickyPositionViewportConstraints&)constraints);
        break;
    }
    }
}

void AsyncScrollingCoordinator::setPositionedNodeConstraints(ScrollingNodeID nodeID, const AbsolutePositionConstraints& constraints)
{
    auto* node = m_scrollingStateTree->stateNodeForID(nodeID);
    if (!node)
        return;

    ASSERT(is<ScrollingStatePositionedNode>(*node));
    if (auto* positionedNode = downcast<ScrollingStatePositionedNode>(node))
        positionedNode->updateConstraints(constraints);
}

void AsyncScrollingCoordinator::setRelatedOverflowScrollingNodes(ScrollingNodeID nodeID, Vector<ScrollingNodeID>&& relatedNodes)
{
    auto* node = m_scrollingStateTree->stateNodeForID(nodeID);
    if (!node)
        return;

    if (is<ScrollingStatePositionedNode>(node))
        downcast<ScrollingStatePositionedNode>(node)->setRelatedOverflowScrollingNodes(WTFMove(relatedNodes));
    else if (is<ScrollingStateOverflowScrollProxyNode>(node)) {
        auto* overflowScrollProxyNode = downcast<ScrollingStateOverflowScrollProxyNode>(node);
        if (!relatedNodes.isEmpty())
            overflowScrollProxyNode->setOverflowScrollingNode(relatedNodes[0]);
        else
            overflowScrollProxyNode->setOverflowScrollingNode(0);
    } else
        ASSERT_NOT_REACHED();
}

void AsyncScrollingCoordinator::setSynchronousScrollingReasons(ScrollingNodeID nodeID, OptionSet<SynchronousScrollingReason> reasons)
{
    auto* node = m_scrollingStateTree->stateNodeForID(nodeID);
    if (!is<ScrollingStateScrollingNode>(node))
        return;

    auto& scrollingStateNode = downcast<ScrollingStateScrollingNode>(*node);
    if (reasons && is<ScrollingStateFrameScrollingNode>(scrollingStateNode)) {
        // The FrameView's GraphicsLayer is likely to be out-of-synch with the PlatformLayer
        // at this point. So we'll update it before we switch back to main thread scrolling
        // in order to avoid layer positioning bugs.
        if (auto* frameView = frameViewForScrollingNode(nodeID))
            reconcileScrollPosition(*frameView, ScrollingLayerPositionAction::Set);
    }

    // FIXME: Ideally all the "synchronousScrollingReasons" functions should be #ifdeffed.
#if ENABLE(SCROLLING_THREAD)
    scrollingStateNode.setSynchronousScrollingReasons(reasons);
#endif
}

OptionSet<SynchronousScrollingReason> AsyncScrollingCoordinator::synchronousScrollingReasons(ScrollingNodeID nodeID) const
{
    auto* node = m_scrollingStateTree->stateNodeForID(nodeID);
    if (!is<ScrollingStateScrollingNode>(node))
        return { };

#if ENABLE(SCROLLING_THREAD)
    return downcast<ScrollingStateScrollingNode>(*node).synchronousScrollingReasons();
#else
    return { };
#endif
}

void AsyncScrollingCoordinator::windowScreenDidChange(PlatformDisplayID displayID, std::optional<FramesPerSecond> nominalFramesPerSecond)
{
    if (m_scrollingTree)
        m_scrollingTree->windowScreenDidChange(displayID, nominalFramesPerSecond);
}

bool AsyncScrollingCoordinator::hasSubscrollers() const
{
    return m_scrollingStateTree && m_scrollingStateTree->scrollingNodeCount() > 1;
}

bool AsyncScrollingCoordinator::isUserScrollInProgress(ScrollingNodeID nodeID) const
{
    if (m_scrollingTree)
        return m_scrollingTree->isUserScrollInProgressForNode(nodeID);

    return false;
}

bool AsyncScrollingCoordinator::isRubberBandInProgress(ScrollingNodeID nodeID) const
{
    if (m_scrollingTree)
        return m_scrollingTree->isRubberBandInProgressForNode(nodeID);

    return false;
}

void AsyncScrollingCoordinator::setScrollPinningBehavior(ScrollPinningBehavior pinning)
{
    scrollingTree()->setScrollPinningBehavior(pinning);
}

ScrollingNodeID AsyncScrollingCoordinator::scrollableContainerNodeID(const RenderObject& renderer) const
{
    if (auto overflowScrollingNodeID = renderer.view().compositor().asyncScrollableContainerNodeID(renderer))
        return overflowScrollingNodeID;

    // If we're in a scrollable frame, return that.
    auto* frameView = renderer.frame().view();
    if (!frameView)
        return 0;

    if (auto scrollingNodeID = frameView->scrollingNodeID())
        return scrollingNodeID;

    // Otherwise, look for a scrollable element in the containing frame.
    if (auto* ownerElement = renderer.document().ownerElement()) {
        if (auto* frameRenderer = ownerElement->renderer())
            return scrollableContainerNodeID(*frameRenderer);
    }

    return 0;
}

String AsyncScrollingCoordinator::scrollingStateTreeAsText(OptionSet<ScrollingStateTreeAsTextBehavior> behavior) const
{
    if (m_scrollingStateTree->rootStateNode()) {
        if (m_eventTrackingRegionsDirty)
            m_scrollingStateTree->rootStateNode()->setEventTrackingRegions(absoluteEventTrackingRegions());
        return m_scrollingStateTree->scrollingStateTreeAsText(behavior);
    }

    return emptyString();
}

String AsyncScrollingCoordinator::scrollingTreeAsText(OptionSet<ScrollingStateTreeAsTextBehavior> behavior) const
{
    if (!m_scrollingTree)
        return emptyString();

    return m_scrollingTree->scrollingTreeAsText(behavior);
}

void AsyncScrollingCoordinator::setActiveScrollSnapIndices(ScrollingNodeID scrollingNodeID, std::optional<unsigned> horizontalIndex, std::optional<unsigned> verticalIndex)
{
    ASSERT(isMainThread());
    
    if (!m_page)
        return;
    
    auto* frameView = frameViewForScrollingNode(scrollingNodeID);
    if (!frameView)
        return;
    
    if (auto* scrollableArea = frameView->scrollableAreaForScrollingNodeID(scrollingNodeID)) {
        scrollableArea->setCurrentHorizontalSnapPointIndex(horizontalIndex);
        scrollableArea->setCurrentVerticalSnapPointIndex(verticalIndex);
    }
}

bool AsyncScrollingCoordinator::isScrollSnapInProgress(ScrollingNodeID nodeID) const
{
    if (m_scrollingTree)
        return m_scrollingTree->isScrollSnapInProgressForNode(nodeID);

    return false;
}

void AsyncScrollingCoordinator::updateScrollSnapPropertiesWithFrameView(const FrameView& frameView)
{
    if (auto node = downcast<ScrollingStateFrameScrollingNode>(m_scrollingStateTree->stateNodeForID(frameView.scrollingNodeID()))) {
        setStateScrollingNodeSnapOffsetsAsFloat(*node, frameView.snapOffsetsInfo(), m_page->deviceScaleFactor());
        node->setCurrentHorizontalSnapPointIndex(frameView.currentHorizontalSnapPointIndex());
        node->setCurrentVerticalSnapPointIndex(frameView.currentVerticalSnapPointIndex());
    }
}

void AsyncScrollingCoordinator::reportExposedUnfilledArea(MonotonicTime timestamp, unsigned unfilledArea)
{
    if (m_page && m_page->performanceLoggingClient())
        m_page->performanceLoggingClient()->logScrollingEvent(PerformanceLoggingClient::ScrollingEvent::ExposedTilelessArea, timestamp, unfilledArea);
}

void AsyncScrollingCoordinator::reportSynchronousScrollingReasonsChanged(MonotonicTime timestamp, OptionSet<SynchronousScrollingReason> reasons)
{
    if (m_page && m_page->performanceLoggingClient())
        m_page->performanceLoggingClient()->logScrollingEvent(PerformanceLoggingClient::ScrollingEvent::SwitchedScrollingMode, timestamp, reasons.toRaw());
}

bool AsyncScrollingCoordinator::scrollAnimatorEnabled() const
{
    ASSERT(isMainThread());
    auto& settings = m_page->mainFrame().settings();
    return settings.scrollAnimatorEnabled();
}

} // namespace WebCore

#endif // ENABLE(ASYNC_SCROLLING)
