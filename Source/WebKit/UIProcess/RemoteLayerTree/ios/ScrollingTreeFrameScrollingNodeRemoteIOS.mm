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
#import <WebCore/ScrollingStateFrameScrollingNode.h>
#import <WebCore/ScrollingStateScrollingNode.h>
#import <WebCore/ScrollingTree.h>
#import <wtf/BlockObjCExceptions.h>

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

UIScrollView *ScrollingTreeFrameScrollingNodeRemoteIOS::scrollView() const
{
    return m_scrollingNodeDelegate ? m_scrollingNodeDelegate->scrollView() : nil;
}

void ScrollingTreeFrameScrollingNodeRemoteIOS::commitStateBeforeChildren(const ScrollingStateNode& stateNode)
{
    ScrollingTreeFrameScrollingNode::commitStateBeforeChildren(stateNode);
    
    const auto& scrollingStateNode = downcast<ScrollingStateFrameScrollingNode>(stateNode);

    if (scrollingStateNode.hasChangedProperty(ScrollingStateNode::Property::CounterScrollingLayer))
        m_counterScrollingLayer = static_cast<CALayer*>(scrollingStateNode.counterScrollingLayer());

    if (scrollingStateNode.hasChangedProperty(ScrollingStateNode::Property::HeaderLayer))
        m_headerLayer = static_cast<CALayer*>(scrollingStateNode.headerLayer());

    if (scrollingStateNode.hasChangedProperty(ScrollingStateNode::Property::FooterLayer))
        m_footerLayer = static_cast<CALayer*>(scrollingStateNode.footerLayer());

    if (stateNode.hasChangedProperty(ScrollingStateNode::Property::ScrollContainerLayer)) {
        if (scrollContainerLayer())
            m_scrollingNodeDelegate = makeUnique<ScrollingTreeScrollingNodeDelegateIOS>(*this);
        else
            m_scrollingNodeDelegate = nullptr;
    }

    if (m_scrollingNodeDelegate)
        m_scrollingNodeDelegate->commitStateBeforeChildren(downcast<ScrollingStateScrollingNode>(stateNode));
}

void ScrollingTreeFrameScrollingNodeRemoteIOS::commitStateAfterChildren(const ScrollingStateNode& stateNode)
{
    const auto& scrollingStateNode = downcast<ScrollingStateFrameScrollingNode>(stateNode);
    if (m_scrollingNodeDelegate)
        m_scrollingNodeDelegate->commitStateAfterChildren(scrollingStateNode);

    ScrollingTreeFrameScrollingNode::commitStateAfterChildren(stateNode);
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

void ScrollingTreeFrameScrollingNodeRemoteIOS::repositionScrollingLayers()
{
    if (m_scrollingNodeDelegate) {
        m_scrollingNodeDelegate->repositionScrollingLayers();
        return;
    }

    // Main frame scrolling is handled by the main UIScrollView.
}

void ScrollingTreeFrameScrollingNodeRemoteIOS::repositionRelatedLayers()
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS
    auto layoutViewport = this->layoutViewport();

    [m_counterScrollingLayer setPosition:layoutViewport.location()];

    // FIXME: I don' think we never have headers and footers on iOS.
    if (m_headerLayer || m_footerLayer) {
        // Generally the banners should have the same horizontal-position computation as a fixed element. However,
        // the banners are not affected by the frameScaleFactor(), so if there is currently a non-1 frameScaleFactor()
        // then we should recompute scrollPositionForFixedChildren for the banner with a scale factor of 1.
        if (m_headerLayer)
            [m_headerLayer setPosition:FloatPoint(layoutViewport.x(), 0)];

        if (m_footerLayer)
            [m_footerLayer setPosition:FloatPoint(layoutViewport.x(), totalContentsSize().height() - footerHeight())];
    }
    END_BLOCK_OBJC_EXCEPTIONS
}

bool ScrollingTreeFrameScrollingNodeRemoteIOS::startAnimatedScrollToPosition(FloatPoint destinationPosition)
{
    // Main frame animated scrolls are handled via PageClientImpl::requestScroll().
    if (!m_scrollingNodeDelegate)
        return false;

    bool started = m_scrollingNodeDelegate->startAnimatedScrollToPosition(destinationPosition);
    if (started)
        willStartAnimatedScroll();
    return started;
}

void ScrollingTreeFrameScrollingNodeRemoteIOS::stopAnimatedScroll()
{
    // Main frame animated scrolls are handled via PageClientImpl::requestScroll().
    if (!m_scrollingNodeDelegate)
        return;

    m_scrollingNodeDelegate->stopAnimatedScroll();
}

}
#endif
