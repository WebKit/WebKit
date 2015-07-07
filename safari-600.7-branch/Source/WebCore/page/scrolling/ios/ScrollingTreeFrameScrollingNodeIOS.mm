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

#if ENABLE(ASYNC_SCROLLING)

#import "FrameView.h"
#import "ScrollingCoordinator.h"
#import "ScrollingTree.h"
#import "ScrollingStateTree.h"
#import "Settings.h"
#import "TileController.h"
#import "WebLayer.h"

#import <QuartzCore/QuartzCore.h>

namespace WebCore {

PassRefPtr<ScrollingTreeFrameScrollingNodeIOS> ScrollingTreeFrameScrollingNodeIOS::create(ScrollingTree& scrollingTree, ScrollingNodeID nodeID)
{
    return adoptRef(new ScrollingTreeFrameScrollingNodeIOS(scrollingTree, nodeID));
}

ScrollingTreeFrameScrollingNodeIOS::ScrollingTreeFrameScrollingNodeIOS(ScrollingTree& scrollingTree, ScrollingNodeID nodeID)
    : ScrollingTreeFrameScrollingNode(scrollingTree, nodeID)
{
}

ScrollingTreeFrameScrollingNodeIOS::~ScrollingTreeFrameScrollingNodeIOS()
{
}

void ScrollingTreeFrameScrollingNodeIOS::updateBeforeChildren(const ScrollingStateNode& stateNode)
{
    ScrollingTreeFrameScrollingNode::updateBeforeChildren(stateNode);
    
    const auto& scrollingStateNode = toScrollingStateFrameScrollingNode(stateNode);

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

void ScrollingTreeFrameScrollingNodeIOS::updateAfterChildren(const ScrollingStateNode& stateNode)
{
    ScrollingTreeFrameScrollingNode::updateAfterChildren(stateNode);

    const auto& scrollingStateNode = toScrollingStateFrameScrollingNode(stateNode);

    // Update the scroll position after child nodes have been updated, because they need to have updated their constraints before any scrolling happens.
    if (scrollingStateNode.hasChangedProperty(ScrollingStateScrollingNode::RequestedScrollPosition))
        setScrollPosition(scrollingStateNode.requestedScrollPosition());
}

FloatPoint ScrollingTreeFrameScrollingNodeIOS::scrollPosition() const
{
    if (shouldUpdateScrollLayerPositionSynchronously())
        return m_probableMainThreadScrollPosition;

    CGPoint scrollLayerPosition = m_scrollLayer.get().position;
    return IntPoint(-scrollLayerPosition.x + scrollOrigin().x(), -scrollLayerPosition.y + scrollOrigin().y());
}

void ScrollingTreeFrameScrollingNodeIOS::setScrollPositionWithoutContentEdgeConstraints(const FloatPoint& scrollPosition)
{
    if (shouldUpdateScrollLayerPositionSynchronously()) {
        m_probableMainThreadScrollPosition = scrollPosition;
        scrollingTree().scrollingTreeNodeDidScroll(scrollingNodeID(), scrollPosition, SetScrollingLayerPosition);
        return;
    }

    setScrollLayerPosition(scrollPosition);
    scrollingTree().scrollingTreeNodeDidScroll(scrollingNodeID(), scrollPosition);
}

void ScrollingTreeFrameScrollingNodeIOS::setScrollLayerPosition(const FloatPoint& scrollPosition)
{
    ASSERT(!shouldUpdateScrollLayerPositionSynchronously());
    [m_scrollLayer setPosition:CGPointMake(-scrollPosition.x() + scrollOrigin().x(), -scrollPosition.y() + scrollOrigin().y())];

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

void ScrollingTreeFrameScrollingNodeIOS::updateLayersAfterDelegatedScroll(const FloatPoint& scrollPosition)
{
    updateChildNodesAfterScroll(scrollPosition);
}

void ScrollingTreeFrameScrollingNodeIOS::updateChildNodesAfterScroll(const FloatPoint& scrollPosition)
{
    ScrollBehaviorForFixedElements behaviorForFixed = scrollBehaviorForFixedElements();
    FloatPoint scrollOffset = scrollPosition - toIntSize(scrollOrigin());
    FloatRect viewportRect(FloatPoint(), scrollableAreaSize());
    FloatSize scrollOffsetForFixedChildren = FrameView::scrollOffsetForFixedPosition(enclosingLayoutRect(viewportRect), roundedLayoutSize(totalContentsSize()), roundedLayoutPoint(scrollOffset), scrollOrigin(), frameScaleFactor(), false, behaviorForFixed, headerHeight(), footerHeight());

    [m_counterScrollingLayer setPosition:FloatPoint(scrollOffsetForFixedChildren)];

    if (m_headerLayer || m_footerLayer) {
        // Generally the banners should have the same horizontal-position computation as a fixed element. However,
        // the banners are not affected by the frameScaleFactor(), so if there is currently a non-1 frameScaleFactor()
        // then we should recompute scrollOffsetForFixedChildren for the banner with a scale factor of 1.
        float horizontalScrollOffsetForBanner = scrollOffsetForFixedChildren.width();
        if (frameScaleFactor() != 1)
            horizontalScrollOffsetForBanner = FrameView::scrollOffsetForFixedPosition(enclosingLayoutRect(viewportRect), roundedLayoutSize(totalContentsSize()), roundedLayoutPoint(scrollOffset), scrollOrigin(), 1, false, behaviorForFixed, headerHeight(), footerHeight()).width();

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
        fixedPositionRect = FloatRect(scrollOffset, scrollableAreaSize());

    for (auto& child : *m_children)
        child->updateLayersAfterAncestorChange(*this, fixedPositionRect, FloatSize());
}

FloatPoint ScrollingTreeFrameScrollingNodeIOS::minimumScrollPosition() const
{
    FloatPoint position;
    
    if (scrollingTree().rootNode() == this && scrollingTree().scrollPinningBehavior() == PinToBottom)
        position.setY(maximumScrollPosition().y());

    return position;
}

FloatPoint ScrollingTreeFrameScrollingNodeIOS::maximumScrollPosition() const
{
    FloatPoint position(totalContentsSizeForRubberBand().width() - scrollableAreaSize().width(),
        totalContentsSizeForRubberBand().height() - scrollableAreaSize().height());

    position = position.expandedTo(FloatPoint());

    if (scrollingTree().rootNode() == this && scrollingTree().scrollPinningBehavior() == PinToTop)
        position.setY(minimumScrollPosition().y());

    return position;
}

} // namespace WebCore

#endif // ENABLE(ASYNC_SCROLLING)
