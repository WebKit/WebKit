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

#if ENABLE(ASYNC_SCROLLING)
#include "AsyncScrollingCoordinator.h"

#include "EditorClient.h"
#include "Frame.h"
#include "FrameView.h"
#include "GraphicsLayer.h"
#include "MainFrame.h"
#include "Page.h"
#include "ScrollingConstraints.h"
#include "ScrollingStateFixedNode.h"
#include "ScrollingStateFrameScrollingNode.h"
#include "ScrollingStateOverflowScrollingNode.h"
#include "ScrollingStateStickyNode.h"
#include "ScrollingStateTree.h"

namespace WebCore {

AsyncScrollingCoordinator::AsyncScrollingCoordinator(Page* page)
    : ScrollingCoordinator(page)
    , m_updateNodeScrollPositionTimer(this, &AsyncScrollingCoordinator::updateScrollPositionAfterAsyncScrollTimerFired)
    , m_scrollingStateTree(ScrollingStateTree::create(this))
{
}

AsyncScrollingCoordinator::~AsyncScrollingCoordinator()
{
}

void AsyncScrollingCoordinator::scrollingStateTreePropertiesChanged()
{
    scheduleTreeStateCommit();
}

void AsyncScrollingCoordinator::frameViewLayoutUpdated(FrameView* frameView)
{
    ASSERT(isMainThread());
    ASSERT(m_page);

    // If there isn't a root node yet, don't do anything. We'll be called again after creating one.
    if (!m_scrollingStateTree->rootStateNode())
        return;

    // Compute the region of the page that we can't do fast scrolling for. This currently includes
    // all scrollable areas, such as subframes, overflow divs and list boxes. We need to do this even if the
    // frame view whose layout was updated is not the main frame.
    // In the future, we may want to have the ability to set non-fast scrolling regions for more than
    // just the root node. But right now, this concept only applies to the root.
    m_scrollingStateTree->rootStateNode()->setNonFastScrollableRegion(computeNonFastScrollableRegion(&m_page->mainFrame(), IntPoint()));

    if (!coordinatesScrollingForFrameView(frameView))
        return;

    ScrollingStateFrameScrollingNode* node = toScrollingStateFrameScrollingNode(m_scrollingStateTree->stateNodeForID(frameView->scrollLayerID()));
    if (!node)
        return;

    Scrollbar* verticalScrollbar = frameView->verticalScrollbar();
    Scrollbar* horizontalScrollbar = frameView->horizontalScrollbar();
    node->setScrollbarPaintersFromScrollbars(verticalScrollbar, horizontalScrollbar);
    
    node->setFrameScaleFactor(frameView->frame().frameScaleFactor());
    node->setHeaderHeight(frameView->headerHeight());
    node->setFooterHeight(frameView->footerHeight());
    node->setTopContentInset(frameView->topContentInset());

    node->setScrollOrigin(frameView->scrollOrigin());
    node->setScrollableAreaSize(frameView->visibleContentRect().size());
    node->setTotalContentsSize(frameView->totalContentsSize());
    node->setReachableContentsSize(frameView->totalContentsSize());

    ScrollableAreaParameters scrollParameters;
    scrollParameters.horizontalScrollElasticity = frameView->horizontalScrollElasticity();
    scrollParameters.verticalScrollElasticity = frameView->verticalScrollElasticity();
    scrollParameters.hasEnabledHorizontalScrollbar = horizontalScrollbar && horizontalScrollbar->enabled();
    scrollParameters.hasEnabledVerticalScrollbar = verticalScrollbar && verticalScrollbar->enabled();
    scrollParameters.horizontalScrollbarMode = frameView->horizontalScrollbarMode();
    scrollParameters.verticalScrollbarMode = frameView->verticalScrollbarMode();

    node->setScrollableAreaParameters(scrollParameters);
}

void AsyncScrollingCoordinator::frameViewNonFastScrollableRegionChanged(FrameView*)
{
    if (!m_scrollingStateTree->rootStateNode())
        return;

    m_scrollingStateTree->rootStateNode()->setNonFastScrollableRegion(computeNonFastScrollableRegion(&m_page->mainFrame(), IntPoint()));
}

void AsyncScrollingCoordinator::frameViewRootLayerDidChange(FrameView* frameView)
{
    ASSERT(isMainThread());
    ASSERT(m_page);

    if (!coordinatesScrollingForFrameView(frameView))
        return;
    
    // FIXME: In some navigation scenarios, the FrameView has no RenderView or that RenderView has not been composited.
    // This needs cleaning up: https://bugs.webkit.org/show_bug.cgi?id=132724
    if (!frameView->scrollLayerID())
        return;
    
    // If the root layer does not have a ScrollingStateNode, then we should create one.
    ensureRootStateNodeForFrameView(frameView);
    ASSERT(m_scrollingStateTree->rootStateNode());

    ScrollingCoordinator::frameViewRootLayerDidChange(frameView);

    ScrollingStateFrameScrollingNode* node = toScrollingStateFrameScrollingNode(m_scrollingStateTree->stateNodeForID(frameView->scrollLayerID()));
    node->setLayer(scrollLayerForFrameView(frameView));
    node->setScrolledContentsLayer(rootContentLayerForFrameView(frameView));
    node->setCounterScrollingLayer(counterScrollingLayerForFrameView(frameView));
    node->setInsetClipLayer(insetClipLayerForFrameView(frameView));
    node->setContentShadowLayer(contentShadowLayerForFrameView(frameView));
    node->setHeaderLayer(headerLayerForFrameView(frameView));
    node->setFooterLayer(footerLayerForFrameView(frameView));
    node->setScrollBehaviorForFixedElements(frameView->scrollBehaviorForFixedElements());
}

bool AsyncScrollingCoordinator::requestScrollPositionUpdate(FrameView* frameView, const IntPoint& scrollPosition)
{
    ASSERT(isMainThread());
    ASSERT(m_page);

    if (!coordinatesScrollingForFrameView(frameView))
        return false;

    if (frameView->inProgrammaticScroll() || frameView->frame().document()->inPageCache())
        updateScrollPositionAfterAsyncScroll(frameView->scrollLayerID(), scrollPosition, frameView->inProgrammaticScroll(), SetScrollingLayerPosition);

    // If this frame view's document is being put into the page cache, we don't want to update our
    // main frame scroll position. Just let the FrameView think that we did.
    if (frameView->frame().document()->inPageCache())
        return true;

    ScrollingStateScrollingNode* stateNode = toScrollingStateScrollingNode(m_scrollingStateTree->stateNodeForID(frameView->scrollLayerID()));
    if (!stateNode)
        return false;

    stateNode->setRequestedScrollPosition(scrollPosition, frameView->inProgrammaticScroll());
    return true;
}

void AsyncScrollingCoordinator::scheduleUpdateScrollPositionAfterAsyncScroll(ScrollingNodeID nodeID, const FloatPoint& scrollPosition, bool programmaticScroll, SetOrSyncScrollingLayerPosition scrollingLayerPositionAction)
{
    ScheduledScrollUpdate scrollUpdate(nodeID, scrollPosition, programmaticScroll, scrollingLayerPositionAction);
    
    if (m_updateNodeScrollPositionTimer.isActive()) {
        if (m_scheduledScrollUpdate.matchesUpdateType(scrollUpdate)) {
            m_scheduledScrollUpdate.scrollPosition = scrollPosition;
            return;
        }
    
        // If the parameters don't match what was previously scheduled, dispatch immediately.
        m_updateNodeScrollPositionTimer.stop();
        updateScrollPositionAfterAsyncScroll(m_scheduledScrollUpdate.nodeID, m_scheduledScrollUpdate.scrollPosition, m_scheduledScrollUpdate.isProgrammaticScroll, m_scheduledScrollUpdate.updateLayerPositionAction);
        updateScrollPositionAfterAsyncScroll(nodeID, scrollPosition, programmaticScroll, scrollingLayerPositionAction);
        return;
    }

    m_scheduledScrollUpdate = scrollUpdate;
    m_updateNodeScrollPositionTimer.startOneShot(0);
}

void AsyncScrollingCoordinator::updateScrollPositionAfterAsyncScrollTimerFired(Timer<AsyncScrollingCoordinator>*)
{
    updateScrollPositionAfterAsyncScroll(m_scheduledScrollUpdate.nodeID, m_scheduledScrollUpdate.scrollPosition, m_scheduledScrollUpdate.isProgrammaticScroll, m_scheduledScrollUpdate.updateLayerPositionAction);
}

FrameView* AsyncScrollingCoordinator::frameViewForScrollingNode(ScrollingNodeID scrollingNodeID) const
{
    if (!m_scrollingStateTree->rootStateNode())
        return nullptr;
    
    if (scrollingNodeID == m_scrollingStateTree->rootStateNode()->scrollingNodeID())
        return m_page->mainFrame().view();

    ScrollingStateNode* stateNode = m_scrollingStateTree->stateNodeForID(scrollingNodeID);
    if (!stateNode)
        return nullptr;

    // Find the enclosing frame scrolling node.
    ScrollingStateNode* parentNode = stateNode;
    while (parentNode && parentNode->nodeType() != FrameScrollingNode)
        parentNode = parentNode->parent();
    
    if (!parentNode)
        return nullptr;
    
    // Walk the frame tree to find the matching FrameView. This is not ideal, but avoids back pointers to FrameViews
    // from ScrollingTreeStateNodes.
    for (Frame* frame = &m_page->mainFrame(); frame; frame = frame->tree().traverseNext()) {
        if (FrameView* view = frame->view()) {
            if (view->scrollLayerID() == parentNode->scrollingNodeID())
                return view;
        }
    }

    return nullptr;
}

void AsyncScrollingCoordinator::updateScrollPositionAfterAsyncScroll(ScrollingNodeID scrollingNodeID, const FloatPoint& scrollPosition, bool programmaticScroll, SetOrSyncScrollingLayerPosition scrollingLayerPositionAction)
{
    ASSERT(isMainThread());

    if (!m_page)
        return;

    FrameView* frameView = frameViewForScrollingNode(scrollingNodeID);
    if (!frameView)
        return;

    if (scrollingNodeID == frameView->scrollLayerID()) {
        bool oldProgrammaticScroll = frameView->inProgrammaticScroll();
        frameView->setInProgrammaticScroll(programmaticScroll);

        frameView->setConstrainsScrollingToContentEdge(false);
        frameView->notifyScrollPositionChanged(roundedIntPoint(scrollPosition));
        frameView->setConstrainsScrollingToContentEdge(true);

        frameView->setInProgrammaticScroll(oldProgrammaticScroll);

        if (GraphicsLayer* scrollLayer = scrollLayerForFrameView(frameView)) {
            GraphicsLayer* counterScrollingLayer = counterScrollingLayerForFrameView(frameView);
            GraphicsLayer* insetClipLayer = insetClipLayerForFrameView(frameView);
            GraphicsLayer* contentShadowLayer = contentShadowLayerForFrameView(frameView);
            GraphicsLayer* scrolledContentsLayer = rootContentLayerForFrameView(frameView);
            GraphicsLayer* headerLayer = headerLayerForFrameView(frameView);
            GraphicsLayer* footerLayer = footerLayerForFrameView(frameView);
            LayoutSize scrollOffsetForFixed = frameView->scrollOffsetForFixedPosition();

            float topContentInset = frameView->topContentInset();
            FloatPoint positionForInsetClipLayer = FloatPoint(0, FrameView::yPositionForInsetClipLayer(scrollPosition, topContentInset));
            FloatPoint positionForContentsLayer = FloatPoint(scrolledContentsLayer->position().x(),
                FrameView::yPositionForRootContentLayer(scrollPosition, topContentInset, frameView->headerHeight()));
            FloatPoint positionForHeaderLayer = FloatPoint(scrollOffsetForFixed.width(), FrameView::yPositionForHeaderLayer(scrollPosition, topContentInset));
            FloatPoint positionForFooterLayer = FloatPoint(scrollOffsetForFixed.width(),
                FrameView::yPositionForFooterLayer(scrollPosition, topContentInset, frameView->totalContentsSize().height(), frameView->footerHeight()));

            if (programmaticScroll || scrollingLayerPositionAction == SetScrollingLayerPosition) {
                scrollLayer->setPosition(-frameView->scrollPosition());
                if (counterScrollingLayer)
                    counterScrollingLayer->setPosition(toLayoutPoint(scrollOffsetForFixed));
                if (insetClipLayer)
                    insetClipLayer->setPosition(positionForInsetClipLayer);
                if (contentShadowLayer)
                    contentShadowLayer->setPosition(positionForContentsLayer);
                if (scrolledContentsLayer)
                    scrolledContentsLayer->setPosition(positionForContentsLayer);
                if (headerLayer)
                    headerLayer->setPosition(positionForHeaderLayer);
                if (footerLayer)
                    footerLayer->setPosition(positionForFooterLayer);
            } else {
                scrollLayer->syncPosition(-frameView->scrollPosition());
                if (counterScrollingLayer)
                    counterScrollingLayer->syncPosition(toLayoutPoint(scrollOffsetForFixed));
                if (insetClipLayer)
                    insetClipLayer->syncPosition(positionForInsetClipLayer);
                if (contentShadowLayer)
                    contentShadowLayer->syncPosition(positionForContentsLayer);
                if (scrolledContentsLayer)
                    scrolledContentsLayer->syncPosition(positionForContentsLayer);
                if (headerLayer)
                    headerLayer->syncPosition(positionForHeaderLayer);
                if (footerLayer)
                    footerLayer->syncPosition(positionForFooterLayer);

                LayoutRect viewportRect = frameView->viewportConstrainedVisibleContentRect();
                syncChildPositions(viewportRect);
            }
        }

        return;
    }

    // Overflow-scroll area.
    if (ScrollableArea* scrollableArea = frameView->scrollableAreaForScrollLayerID(scrollingNodeID)) {
        scrollableArea->setIsUserScroll(scrollingLayerPositionAction == SyncScrollingLayerPosition);
        scrollableArea->scrollToOffsetWithoutAnimation(scrollPosition);
        scrollableArea->setIsUserScroll(false);
        if (scrollingLayerPositionAction == SetScrollingLayerPosition)
            m_page->editorClient()->overflowScrollPositionChanged();
    }
}

void AsyncScrollingCoordinator::scrollableAreaScrollbarLayerDidChange(ScrollableArea* scrollableArea, ScrollbarOrientation orientation)
{
    ASSERT(isMainThread());
    ASSERT(m_page);

    if (scrollableArea != static_cast<ScrollableArea*>(m_page->mainFrame().view()))
        return;

    if (orientation == VerticalScrollbar)
        scrollableArea->verticalScrollbarLayerDidChange();
    else
        scrollableArea->horizontalScrollbarLayerDidChange();
}

ScrollingNodeID AsyncScrollingCoordinator::attachToStateTree(ScrollingNodeType nodeType, ScrollingNodeID newNodeID, ScrollingNodeID parentID)
{
    return m_scrollingStateTree->attachNode(nodeType, newNodeID, parentID);
}

void AsyncScrollingCoordinator::detachFromStateTree(ScrollingNodeID nodeID)
{
    m_scrollingStateTree->detachNode(nodeID);
}

void AsyncScrollingCoordinator::clearStateTree()
{
    m_scrollingStateTree->clear();
}

void AsyncScrollingCoordinator::syncChildPositions(const LayoutRect& viewportRect)
{
    if (!m_scrollingStateTree->rootStateNode())
        return;

    auto children = m_scrollingStateTree->rootStateNode()->children();
    if (!children)
        return;

    // FIXME: We'll have to traverse deeper into the tree at some point.
    for (auto& child : *children)
        child->syncLayerPositionForViewportRect(viewportRect);
}

void AsyncScrollingCoordinator::ensureRootStateNodeForFrameView(FrameView* frameView)
{
    ASSERT(frameView->scrollLayerID());
    attachToStateTree(FrameScrollingNode, frameView->scrollLayerID(), 0);
}

void AsyncScrollingCoordinator::updateFrameScrollingNode(ScrollingNodeID nodeID, GraphicsLayer* layer, GraphicsLayer* scrolledContentsLayer, GraphicsLayer* counterScrollingLayer, GraphicsLayer* insetClipLayer, const ScrollingGeometry* scrollingGeometry)
{
    ScrollingStateFrameScrollingNode* node = toScrollingStateFrameScrollingNode(m_scrollingStateTree->stateNodeForID(nodeID));
    ASSERT(node);
    if (!node)
        return;

    node->setLayer(layer);
    node->setInsetClipLayer(insetClipLayer);
    node->setScrolledContentsLayer(scrolledContentsLayer);
    node->setCounterScrollingLayer(counterScrollingLayer);

    if (scrollingGeometry) {
        node->setScrollOrigin(scrollingGeometry->scrollOrigin);
        node->setScrollPosition(scrollingGeometry->scrollPosition);
        node->setTotalContentsSize(scrollingGeometry->contentSize);
        node->setReachableContentsSize(scrollingGeometry->reachableContentSize);
        node->setScrollableAreaSize(scrollingGeometry->scrollableAreaSize);
    }
}
    
void AsyncScrollingCoordinator::updateOverflowScrollingNode(ScrollingNodeID nodeID, GraphicsLayer* layer, GraphicsLayer* scrolledContentsLayer, const ScrollingGeometry* scrollingGeometry)
{
    ScrollingStateOverflowScrollingNode* node = toScrollingStateOverflowScrollingNode(m_scrollingStateTree->stateNodeForID(nodeID));
    ASSERT(node);
    if (!node)
        return;

    node->setLayer(layer);
    node->setScrolledContentsLayer(scrolledContentsLayer);
    
    if (scrollingGeometry) {
        node->setScrollOrigin(scrollingGeometry->scrollOrigin);
        node->setScrollPosition(scrollingGeometry->scrollPosition);
        node->setTotalContentsSize(scrollingGeometry->contentSize);
        node->setReachableContentsSize(scrollingGeometry->reachableContentSize);
        node->setScrollableAreaSize(scrollingGeometry->scrollableAreaSize);
    }
}

void AsyncScrollingCoordinator::updateViewportConstrainedNode(ScrollingNodeID nodeID, const ViewportConstraints& constraints, GraphicsLayer* graphicsLayer)
{
    ASSERT(supportsFixedPositionLayers());

    ScrollingStateNode* node = m_scrollingStateTree->stateNodeForID(nodeID);
    if (!node)
        return;

    switch (constraints.constraintType()) {
    case ViewportConstraints::FixedPositionConstraint: {
        ScrollingStateFixedNode* fixedNode = toScrollingStateFixedNode(node);
        fixedNode->setLayer(graphicsLayer);
        fixedNode->updateConstraints((const FixedPositionViewportConstraints&)constraints);
        break;
    }
    case ViewportConstraints::StickyPositionConstraint: {
        ScrollingStateStickyNode* stickyNode = toScrollingStateStickyNode(node);
        stickyNode->setLayer(graphicsLayer);
        stickyNode->updateConstraints((const StickyPositionViewportConstraints&)constraints);
        break;
    }
    }
}

void AsyncScrollingCoordinator::setSynchronousScrollingReasons(SynchronousScrollingReasons reasons)
{
    if (!m_scrollingStateTree->rootStateNode())
        return;

    // The FrameView's GraphicsLayer is likely to be out-of-synch with the PlatformLayer
    // at this point. So we'll update it before we switch back to main thread scrolling
    // in order to avoid layer positioning bugs.
    if (reasons)
        updateMainFrameScrollLayerPosition();
    m_scrollingStateTree->rootStateNode()->setSynchronousScrollingReasons(reasons);
}

void AsyncScrollingCoordinator::updateMainFrameScrollLayerPosition()
{
    ASSERT(isMainThread());

    if (!m_page)
        return;

    FrameView* frameView = m_page->mainFrame().view();
    if (!frameView)
        return;

    if (GraphicsLayer* scrollLayer = scrollLayerForFrameView(frameView))
        scrollLayer->setPosition(-frameView->scrollPosition());
}

void AsyncScrollingCoordinator::recomputeWheelEventHandlerCountForFrameView(FrameView* frameView)
{
    ScrollingStateFrameScrollingNode* node = toScrollingStateFrameScrollingNode(m_scrollingStateTree->stateNodeForID(frameView->scrollLayerID()));
    if (!node)
        return;
    node->setWheelEventHandlerCount(computeCurrentWheelEventHandlerCount());
}

bool AsyncScrollingCoordinator::isRubberBandInProgress() const
{
    return scrollingTree()->isRubberBandInProgress();
}

void AsyncScrollingCoordinator::setScrollPinningBehavior(ScrollPinningBehavior pinning)
{
    scrollingTree()->setScrollPinningBehavior(pinning);
}

String AsyncScrollingCoordinator::scrollingStateTreeAsText() const
{
    if (m_scrollingStateTree->rootStateNode())
        return m_scrollingStateTree->rootStateNode()->scrollingStateTreeAsText();

    return String();
}

} // namespace WebCore

#endif // ENABLE(ASYNC_SCROLLING)
