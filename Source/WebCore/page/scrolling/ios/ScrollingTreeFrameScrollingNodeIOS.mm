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

#import "config.h"
#import "ScrollingTreeFrameScrollingNodeIOS.h"

#if ENABLE(ASYNC_SCROLLING) && PLATFORM(IOS_FAMILY)

#import "FrameView.h"
#import "ScrollingCoordinator.h"
#import "ScrollingStateFrameScrollingNode.h"
#import "ScrollingStateTree.h"
#import "ScrollingTree.h"
#import "TileController.h"
#import "WebLayer.h"
#import <QuartzCore/QuartzCore.h>

namespace WebCore {

Ref<ScrollingTreeFrameScrollingNodeIOS> ScrollingTreeFrameScrollingNodeIOS::create(ScrollingTree& scrollingTree, ScrollingNodeType nodeType, ScrollingNodeID nodeID)
{
    return adoptRef(*new ScrollingTreeFrameScrollingNodeIOS(scrollingTree, nodeType, nodeID));
}

ScrollingTreeFrameScrollingNodeIOS::ScrollingTreeFrameScrollingNodeIOS(ScrollingTree& scrollingTree, ScrollingNodeType nodeType, ScrollingNodeID nodeID)
    : ScrollingTreeFrameScrollingNode(scrollingTree, nodeType, nodeID)
{
}

ScrollingTreeFrameScrollingNodeIOS::~ScrollingTreeFrameScrollingNodeIOS()
{
}

void ScrollingTreeFrameScrollingNodeIOS::commitStateBeforeChildren(const ScrollingStateNode& stateNode)
{
    ScrollingTreeFrameScrollingNode::commitStateBeforeChildren(stateNode);
    
    const auto& scrollingStateNode = downcast<ScrollingStateFrameScrollingNode>(stateNode);

    if (scrollingStateNode.hasChangedProperty(ScrollingStateNode::ScrollLayer))
        m_scrollLayer = scrollingStateNode.layer();

    if (scrollingStateNode.hasChangedProperty(ScrollingStateFrameScrollingNode::CounterScrollingLayer))
        m_counterScrollingLayer = scrollingStateNode.counterScrollingLayer();

    if (scrollingStateNode.hasChangedProperty(ScrollingStateFrameScrollingNode::HeaderLayer))
        m_headerLayer = scrollingStateNode.headerLayer();

    if (scrollingStateNode.hasChangedProperty(ScrollingStateFrameScrollingNode::FooterLayer))
        m_footerLayer = scrollingStateNode.footerLayer();

    if (scrollingStateNode.hasChangedProperty(ScrollingStateFrameScrollingNode::ReasonsForSynchronousScrolling)) {
        if (shouldUpdateScrollLayerPositionSynchronously()) {
            // We're transitioning to the slow "update scroll layer position on the main thread" mode.
            // Initialize the probable main thread scroll position with the current scroll layer position.
            if (scrollingStateNode.hasChangedProperty(ScrollingStateScrollingNode::RequestedScrollPosition))
                m_probableMainThreadScrollPosition = scrollingStateNode.requestedScrollPosition();
            else {
                CGPoint scrollLayerPosition = m_scrollLayer.get().position;
                m_probableMainThreadScrollPosition = IntPoint(-scrollLayerPosition.x, -scrollLayerPosition.y);
            }
        }
    }
}

void ScrollingTreeFrameScrollingNodeIOS::commitStateAfterChildren(const ScrollingStateNode& stateNode)
{
    ScrollingTreeFrameScrollingNode::commitStateAfterChildren(stateNode);

    const auto& scrollingStateNode = downcast<ScrollingStateFrameScrollingNode>(stateNode);

    // Update the scroll position after child nodes have been updated, because they need to have updated their constraints before any scrolling happens.
    if (scrollingStateNode.hasChangedProperty(ScrollingStateScrollingNode::RequestedScrollPosition))
        setScrollPosition(scrollingStateNode.requestedScrollPosition());
}

FloatPoint ScrollingTreeFrameScrollingNodeIOS::scrollPosition() const
{
    if (shouldUpdateScrollLayerPositionSynchronously())
        return m_probableMainThreadScrollPosition;

    return -m_scrollLayer.get().position;
}

void ScrollingTreeFrameScrollingNodeIOS::setScrollPositionWithoutContentEdgeConstraints(const FloatPoint& scrollPosition)
{
    if (shouldUpdateScrollLayerPositionSynchronously()) {
        m_probableMainThreadScrollPosition = scrollPosition;
        scrollingTree().scrollingTreeNodeDidScroll(scrollingNodeID(), scrollPosition, WTF::nullopt, ScrollingLayerPositionAction::Set);
        return;
    }

    FloatRect layoutViewport; // FIXME: implement for iOS WK1.
    setScrollLayerPosition(scrollPosition, layoutViewport);
    scrollingTree().scrollingTreeNodeDidScroll(scrollingNodeID(), scrollPosition, WTF::nullopt);
}

void ScrollingTreeFrameScrollingNodeIOS::setScrollLayerPosition(const FloatPoint& scrollPosition, const FloatRect&)
{
    ASSERT(!shouldUpdateScrollLayerPositionSynchronously());
    [m_scrollLayer setPosition:-scrollPosition];

    updateChildNodesAfterScroll(scrollPosition);
}

void ScrollingTreeFrameScrollingNodeIOS::updateLayersAfterViewportChange(const FloatRect& fixedPositionRect, double /*scale*/)
{
    // Note: we never currently have a m_counterScrollingLayer (which is used for background-attachment:fixed) on iOS.
    [m_counterScrollingLayer setPosition:fixedPositionRect.location()];

    if (!m_children)
        return;

    for (auto& child : *m_children)
        child->updateLayersAfterAncestorChange(*this, fixedPositionRect, FloatSize());
}

void ScrollingTreeFrameScrollingNodeIOS::updateLayersAfterAncestorChange(const ScrollingTreeNode& changedNode, const FloatRect&, const FloatSize&)
{
    if (!m_children)
        return;

    FloatRect fixedPositionRect(scrollPosition(), scrollableAreaSize());
    for (auto& child : *m_children)
        child->updateLayersAfterAncestorChange(changedNode, fixedPositionRect, FloatSize());
}

void ScrollingTreeFrameScrollingNodeIOS::updateLayersAfterDelegatedScroll(const FloatPoint& scrollPosition)
{
    updateChildNodesAfterScroll(scrollPosition);
}

void ScrollingTreeFrameScrollingNodeIOS::updateChildNodesAfterScroll(const FloatPoint& scrollPosition)
{
    ScrollBehaviorForFixedElements behaviorForFixed = scrollBehaviorForFixedElements();
    FloatRect viewportRect(scrollPosition, scrollableAreaSize());
    FloatPoint scrollPositionForFixedChildren = FrameView::scrollPositionForFixedPosition(enclosingLayoutRect(viewportRect), LayoutSize(totalContentsSize()), LayoutPoint(scrollPosition), scrollOrigin(), frameScaleFactor(), fixedElementsLayoutRelativeToFrame(), behaviorForFixed, headerHeight(), footerHeight());

    [m_counterScrollingLayer setPosition:scrollPositionForFixedChildren];

    if (m_headerLayer || m_footerLayer) {
        // Generally the banners should have the same horizontal-position computation as a fixed element. However,
        // the banners are not affected by the frameScaleFactor(), so if there is currently a non-1 frameScaleFactor()
        // then we should recompute scrollPositionForFixedChildren for the banner with a scale factor of 1.
        float horizontalScrollOffsetForBanner = scrollPositionForFixedChildren.x();
        if (frameScaleFactor() != 1)
            horizontalScrollOffsetForBanner = FrameView::scrollPositionForFixedPosition(enclosingLayoutRect(viewportRect), LayoutSize(totalContentsSize()), LayoutPoint(scrollPosition), scrollOrigin(), 1, fixedElementsLayoutRelativeToFrame(), behaviorForFixed, headerHeight(), footerHeight()).x();

        if (m_headerLayer)
            [m_headerLayer setPosition:FloatPoint(horizontalScrollOffsetForBanner, 0)];

        if (m_footerLayer)
            [m_footerLayer setPosition:FloatPoint(horizontalScrollOffsetForBanner, totalContentsSize().height() - footerHeight())];
    }
    
    if (!m_children)
        return;


    FloatRect fixedPositionRect;
    if (!parent())
        fixedPositionRect = scrollingTree().fixedPositionRect();
    else
        fixedPositionRect = FloatRect(scrollPosition, scrollableAreaSize());

    for (auto& child : *m_children)
        child->updateLayersAfterAncestorChange(*this, fixedPositionRect, FloatSize());
}

FloatPoint ScrollingTreeFrameScrollingNodeIOS::minimumScrollPosition() const
{
    FloatPoint position = ScrollableArea::scrollPositionFromOffset(FloatPoint(), toFloatSize(scrollOrigin()));
    
    if (scrollingTree().rootNode() == this && scrollingTree().scrollPinningBehavior() == PinToBottom)
        position.setY(maximumScrollPosition().y());

    return position;
}

FloatPoint ScrollingTreeFrameScrollingNodeIOS::maximumScrollPosition() const
{
    FloatPoint position = ScrollableArea::scrollPositionFromOffset(FloatPoint(totalContentsSizeForRubberBand() - scrollableAreaSize()), toFloatSize(scrollOrigin()));
    position = position.expandedTo(FloatPoint());

    if (scrollingTree().rootNode() == this && scrollingTree().scrollPinningBehavior() == PinToTop)
        position.setY(minimumScrollPosition().y());

    return position;
}

} // namespace WebCore

#endif // ENABLE(ASYNC_SCROLLING) && PLATFORM(IOS_FAMILY)
