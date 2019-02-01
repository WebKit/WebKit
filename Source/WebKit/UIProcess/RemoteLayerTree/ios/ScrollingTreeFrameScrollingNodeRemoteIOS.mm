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
    : ScrollingTreeFrameScrollingNodeIOS(scrollingTree, nodeType, nodeID)
{
}

ScrollingTreeFrameScrollingNodeRemoteIOS::~ScrollingTreeFrameScrollingNodeRemoteIOS()
{
}

void ScrollingTreeFrameScrollingNodeRemoteIOS::commitStateBeforeChildren(const ScrollingStateNode& stateNode)
{
    ScrollingTreeFrameScrollingNodeIOS::commitStateBeforeChildren(stateNode);

    // FIXME: Should be ScrollContainerLayer.
    if (stateNode.hasChangedProperty(ScrollingStateScrollingNode::ScrolledContentsLayer)) {
        if (scrolledContentsLayer() && [[scrolledContentsLayer() delegate] isKindOfClass:[UIScrollView self]])
            m_scrollingNodeDelegate = std::make_unique<ScrollingTreeScrollingNodeDelegateIOS>(*this);
        else
            m_scrollingNodeDelegate = nullptr;
    }

    if (m_scrollingNodeDelegate)
        m_scrollingNodeDelegate->commitStateBeforeChildren(downcast<ScrollingStateScrollingNode>(stateNode));
}

void ScrollingTreeFrameScrollingNodeRemoteIOS::commitStateAfterChildren(const ScrollingStateNode& stateNode)
{
    ScrollingTreeFrameScrollingNodeIOS::commitStateAfterChildren(stateNode);

    if (m_scrollingNodeDelegate)
        m_scrollingNodeDelegate->commitStateAfterChildren(downcast<ScrollingStateScrollingNode>(stateNode));
}

void ScrollingTreeFrameScrollingNodeRemoteIOS::updateLayersAfterAncestorChange(const ScrollingTreeNode& changedNode, const FloatRect& fixedPositionRect, const FloatSize& cumulativeDelta)
{
    if (m_scrollingNodeDelegate) {
        m_scrollingNodeDelegate->updateLayersAfterAncestorChange(changedNode, fixedPositionRect, cumulativeDelta);
        return;
    }
    ScrollingTreeFrameScrollingNodeIOS::updateLayersAfterAncestorChange(changedNode, fixedPositionRect, cumulativeDelta);
}

FloatPoint ScrollingTreeFrameScrollingNodeRemoteIOS::scrollPosition() const
{
    if (m_scrollingNodeDelegate)
        return m_scrollingNodeDelegate->scrollPosition();

    return ScrollingTreeFrameScrollingNodeIOS::scrollPosition();
}

void ScrollingTreeFrameScrollingNodeRemoteIOS::setScrollLayerPosition(const FloatPoint& scrollPosition, const FloatRect& layoutViewport)
{
    if (m_scrollingNodeDelegate) {
        m_scrollingNodeDelegate->setScrollLayerPosition(scrollPosition);
        return;
    }
    ScrollingTreeFrameScrollingNodeIOS::setScrollLayerPosition(scrollPosition, layoutViewport);
}

void ScrollingTreeFrameScrollingNodeRemoteIOS::updateLayersAfterDelegatedScroll(const FloatPoint& scrollPosition)
{
    if (m_scrollingNodeDelegate) {
        m_scrollingNodeDelegate->updateChildNodesAfterScroll(scrollPosition);
        return;
    }
    ScrollingTreeFrameScrollingNodeIOS::updateLayersAfterDelegatedScroll(scrollPosition);
}

}

#endif
