/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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
#import "ScrollingTreeFrameScrollingNodeRemoteIOS.h"

#if PLATFORM(IOS_FAMILY) && ENABLE(ASYNC_SCROLLING)

#import "ScrollingTreeScrollingNodeDelegateIOS.h"
#import <WebCore/ScrollingStateScrollingNode.h>

namespace WebKit {
using namespace WebCore;

Ref<ScrollingTreeFrameScrollingNodeRemoteIOS> ScrollingTreeFrameScrollingNodeRemoteIOS::create(ScrollingTree& scrollingTree, ScrollingNodeType nodeType, ScrollingNodeID nodeID)
{
    return adoptRef(*new ScrollingTreeFrameScrollingNodeRemoteIOS(scrollingTree, nodeType, nodeID));
}

ScrollingTreeFrameScrollingNodeRemoteIOS::ScrollingTreeFrameScrollingNodeRemoteIOS(ScrollingTree& scrollingTree, ScrollingNodeType nodeType, ScrollingNodeID nodeID)
    : ScrollingTreeFrameScrollingNode(scrollingTree, nodeType, nodeID)
{
}

ScrollingTreeFrameScrollingNodeRemoteIOS::~ScrollingTreeFrameScrollingNodeRemoteIOS()
{
}

void ScrollingTreeFrameScrollingNodeRemoteIOS::commitStateBeforeChildren(const ScrollingStateNode& stateNode)
{
    ScrollingTreeFrameScrollingNode::commitStateBeforeChildren(stateNode);
    
    const auto& scrollingStateNode = downcast<ScrollingStateFrameScrollingNode>(stateNode);

    if (scrollingStateNode.hasChangedProperty(ScrollingStateFrameScrollingNode::CounterScrollingLayer))
        m_counterScrollingLayer = scrollingStateNode.counterScrollingLayer();

    if (scrollingStateNode.hasChangedProperty(ScrollingStateFrameScrollingNode::HeaderLayer))
        m_headerLayer = scrollingStateNode.headerLayer();

    if (scrollingStateNode.hasChangedProperty(ScrollingStateFrameScrollingNode::FooterLayer))
        m_footerLayer = scrollingStateNode.footerLayer();

    if (stateNode.hasChangedProperty(ScrollingStateScrollingNode::ScrollContainerLayer)) {
        if (scrollContainerLayer())
            m_scrollingNodeDelegate = std::make_unique<ScrollingTreeScrollingNodeDelegateIOS>(*this);
        else
            m_scrollingNodeDelegate = nullptr;
    }

    if (m_scrollingNodeDelegate)
        m_scrollingNodeDelegate->commitStateBeforeChildren(downcast<ScrollingStateScrollingNode>(stateNode));
}

void ScrollingTreeFrameScrollingNodeRemoteIOS::commitStateAfterChildren(const ScrollingStateNode& stateNode)
{
    ScrollingTreeFrameScrollingNode::commitStateAfterChildren(stateNode);

    const auto& scrollingStateNode = downcast<ScrollingStateFrameScrollingNode>(stateNode);

    // Update the scroll position after child nodes have been updated, because they need to have updated their constraints before any scrolling happens.
    if (scrollingStateNode.hasChangedProperty(ScrollingStateScrollingNode::RequestedScrollPosition))
        setScrollPosition(scrollingStateNode.requestedScrollPosition());

    if (m_scrollingNodeDelegate)
        m_scrollingNodeDelegate->commitStateAfterChildren(downcast<ScrollingStateScrollingNode>(stateNode));
}

FloatPoint ScrollingTreeFrameScrollingNodeRemoteIOS::minimumScrollPosition() const
{
    FloatPoint position = ScrollableArea::scrollPositionFromOffset(FloatPoint(), toFloatSize(scrollOrigin()));
    
    if (isRootNode() && scrollingTree().scrollPinningBehavior() == PinToBottom)
        position.setY(maximumScrollPosition().y());

    return position;
}

FloatPoint ScrollingTreeFrameScrollingNodeRemoteIOS::maximumScrollPosition() const
{
    FloatPoint position = ScrollableArea::scrollPositionFromOffset(FloatPoint(totalContentsSizeForRubberBand() - scrollableAreaSize()), toFloatSize(scrollOrigin()));
    position = position.expandedTo(FloatPoint());

    if (isRootNode() && scrollingTree().scrollPinningBehavior() == PinToTop)
        position.setY(minimumScrollPosition().y());

    return position;
}

FloatPoint ScrollingTreeFrameScrollingNodeRemoteIOS::scrollPosition() const
{
    if (m_scrollingNodeDelegate)
        return m_scrollingNodeDelegate->scrollPosition();

    return -scrolledContentsLayer().position;
}

void ScrollingTreeFrameScrollingNodeRemoteIOS::setScrollPosition(const FloatPoint& position, ScrollPositionClamp clamp)
{
    auto scrollPosition = position;
    if (clamp == ScrollPositionClamp::ToContentEdges)
        scrollPosition = clampScrollPosition(scrollPosition);

    FloatRect newLayoutViewport = layoutViewportForScrollPosition(scrollPosition, frameScaleFactor());
    setLayoutViewport(newLayoutViewport);
    auto layoutViewportOrigin = newLayoutViewport.location();

    setScrollLayerPosition(scrollPosition, layoutViewport());
    scrollingTree().scrollingTreeNodeDidScroll(scrollingNodeID(), scrollPosition, layoutViewportOrigin);
}

void ScrollingTreeFrameScrollingNodeRemoteIOS::setScrollLayerPosition(const FloatPoint& scrollPosition, const FloatRect& layoutViewport)
{
    if (m_scrollingNodeDelegate) {
        m_scrollingNodeDelegate->setScrollLayerPosition(scrollPosition);
        return;
    }

    [scrolledContentsLayer() setPosition:-scrollPosition];
    updateChildNodesAfterScroll(scrollPosition);
}

void ScrollingTreeFrameScrollingNodeRemoteIOS::updateChildNodesAfterScroll(const FloatPoint& scrollPosition)
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


    FloatRect layoutViewport;
    if (isRootNode())
        layoutViewport = this->layoutViewport();
    else
        layoutViewport = FloatRect(scrollPosition, scrollableAreaSize()); // FIXME: We'll just use layoutViewport() once we correctly update it after a scroll.

    for (auto& child : *m_children)
        child->updateLayersAfterAncestorChange(*this, layoutViewport, FloatSize());
}

void ScrollingTreeFrameScrollingNodeRemoteIOS::updateLayersAfterDelegatedScroll(const FloatPoint& scrollPosition)
{
    if (m_scrollingNodeDelegate) {
        m_scrollingNodeDelegate->updateChildNodesAfterScroll(scrollPosition);
        return;
    }

    updateChildNodesAfterScroll(scrollPosition);
}

void ScrollingTreeFrameScrollingNodeRemoteIOS::updateLayersAfterViewportChange(const FloatRect& layoutViewport, double /*scale*/)
{
    // Note: we never currently have a m_counterScrollingLayer (which is used for background-attachment:fixed) on iOS.
    [m_counterScrollingLayer setPosition:layoutViewport.location()];

    if (!m_children)
        return;

    for (auto& child : *m_children)
        child->updateLayersAfterAncestorChange(*this, layoutViewport, FloatSize());
}

void ScrollingTreeFrameScrollingNodeRemoteIOS::updateLayersAfterAncestorChange(const ScrollingTreeNode& changedNode, const FloatRect& layoutViewport, const FloatSize& cumulativeDelta)
{
    if (m_scrollingNodeDelegate) {
        m_scrollingNodeDelegate->updateLayersAfterAncestorChange(changedNode, layoutViewport, cumulativeDelta);
        return;
    }

    if (!m_children)
        return;

    FloatRect currFrameLayoutViewport(scrollPosition(), scrollableAreaSize()); // FIXME: use layoutViewport() once it's correctly updated.
    for (auto& child : *m_children)
        child->updateLayersAfterAncestorChange(changedNode, currFrameLayoutViewport, { });
}

}
#endif
