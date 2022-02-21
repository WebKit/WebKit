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
#import "ScrollingTreeOverflowScrollingNodeIOS.h"

#if PLATFORM(IOS_FAMILY)
#if ENABLE(ASYNC_SCROLLING)

#import "ScrollingTreeScrollingNodeDelegateIOS.h"

#import <WebCore/ScrollingStateOverflowScrollingNode.h>

namespace WebKit {
using namespace WebCore;

Ref<ScrollingTreeOverflowScrollingNodeIOS> ScrollingTreeOverflowScrollingNodeIOS::create(WebCore::ScrollingTree& scrollingTree, WebCore::ScrollingNodeID nodeID)
{
    return adoptRef(*new ScrollingTreeOverflowScrollingNodeIOS(scrollingTree, nodeID));
}

ScrollingTreeOverflowScrollingNodeIOS::ScrollingTreeOverflowScrollingNodeIOS(WebCore::ScrollingTree& scrollingTree, WebCore::ScrollingNodeID nodeID)
    : ScrollingTreeOverflowScrollingNode(scrollingTree, nodeID)
    , m_scrollingNodeDelegate(makeUnique<ScrollingTreeScrollingNodeDelegateIOS>(*this))
{
}

ScrollingTreeOverflowScrollingNodeIOS::~ScrollingTreeOverflowScrollingNodeIOS()
{
}

UIScrollView* ScrollingTreeOverflowScrollingNodeIOS::scrollView() const
{
    return m_scrollingNodeDelegate->scrollView();
}

void ScrollingTreeOverflowScrollingNodeIOS::commitStateBeforeChildren(const WebCore::ScrollingStateNode& stateNode)
{
    if (stateNode.hasChangedProperty(ScrollingStateNode::Property::ScrollContainerLayer))
        m_scrollingNodeDelegate->resetScrollViewDelegate();

    ScrollingTreeOverflowScrollingNode::commitStateBeforeChildren(stateNode);
    m_scrollingNodeDelegate->commitStateBeforeChildren(downcast<ScrollingStateScrollingNode>(stateNode));
}

void ScrollingTreeOverflowScrollingNodeIOS::commitStateAfterChildren(const ScrollingStateNode& stateNode)
{
    const auto& scrollingStateNode = downcast<ScrollingStateScrollingNode>(stateNode);
    m_scrollingNodeDelegate->commitStateAfterChildren(scrollingStateNode);

    ScrollingTreeOverflowScrollingNode::commitStateAfterChildren(stateNode);
}

void ScrollingTreeOverflowScrollingNodeIOS::repositionScrollingLayers()
{
    m_scrollingNodeDelegate->repositionScrollingLayers();
}

bool ScrollingTreeOverflowScrollingNodeIOS::startAnimatedScrollToPosition(FloatPoint destinationPosition)
{
    bool started = m_scrollingNodeDelegate->startAnimatedScrollToPosition(destinationPosition);
    if (started)
        willStartAnimatedScroll();
    return started;
}

void ScrollingTreeOverflowScrollingNodeIOS::stopAnimatedScroll()
{
    m_scrollingNodeDelegate->stopAnimatedScroll();
}

} // namespace WebKit

#endif // ENABLE(ASYNC_SCROLLING)
#endif // PLATFORM(IOS_FAMILY)
