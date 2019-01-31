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

#include "config.h"
#include "ScrollingTreeFrameScrollingNodeRemoteMac.h"

#if ENABLE(ASYNC_SCROLLING) && PLATFORM(MAC)

#include "RemoteScrollingTree.h"
#include "ScrollerPairMac.h"

namespace WebKit {
using namespace WebCore;

ScrollingTreeFrameScrollingNodeRemoteMac::ScrollingTreeFrameScrollingNodeRemoteMac(ScrollingTree& tree, ScrollingNodeType nodeType, ScrollingNodeID nodeID)
    : ScrollingTreeFrameScrollingNodeMac(tree, nodeType, nodeID)
    , m_scrollerPair(std::make_unique<ScrollerPairMac>(*this))
{
}

ScrollingTreeFrameScrollingNodeRemoteMac::~ScrollingTreeFrameScrollingNodeRemoteMac()
{
}

Ref<ScrollingTreeFrameScrollingNodeRemoteMac> ScrollingTreeFrameScrollingNodeRemoteMac::create(ScrollingTree& tree, ScrollingNodeType nodeType, ScrollingNodeID nodeID)
{
    return adoptRef(*new ScrollingTreeFrameScrollingNodeRemoteMac(tree, nodeType, nodeID));
}

void ScrollingTreeFrameScrollingNodeRemoteMac::commitStateBeforeChildren(const ScrollingStateNode& stateNode)
{
    ScrollingTreeFrameScrollingNodeMac::commitStateBeforeChildren(stateNode);
    const auto& scrollingStateNode = downcast<ScrollingStateFrameScrollingNode>(stateNode);

    if (scrollingStateNode.hasChangedProperty(ScrollingStateFrameScrollingNode::VerticalScrollbarLayer))
        m_scrollerPair->verticalScroller().setHostLayer(scrollingStateNode.verticalScrollbarLayer());

    if (scrollingStateNode.hasChangedProperty(ScrollingStateFrameScrollingNode::HorizontalScrollbarLayer))
        m_scrollerPair->horizontalScroller().setHostLayer(scrollingStateNode.horizontalScrollbarLayer());

    m_scrollerPair->updateValues();
}

void ScrollingTreeFrameScrollingNodeRemoteMac::setScrollLayerPosition(const FloatPoint& position, const FloatRect& layoutViewport)
{
    ScrollingTreeFrameScrollingNodeMac::setScrollLayerPosition(position, layoutViewport);

    m_scrollerPair->updateValues();
}

ScrollingEventResult ScrollingTreeFrameScrollingNodeRemoteMac::handleWheelEvent(const PlatformWheelEvent& wheelEvent)
{
    ScrollingTreeFrameScrollingNodeMac::handleWheelEvent(wheelEvent);

    return m_scrollerPair->handleWheelEvent(wheelEvent) ? ScrollingEventResult::DidHandleEvent : ScrollingEventResult::DidNotHandleEvent;
}

bool ScrollingTreeFrameScrollingNodeRemoteMac::handleMouseEvent(const PlatformMouseEvent& mouseEvent)
{
    return m_scrollerPair->handleMouseEvent(mouseEvent);
}

}

#endif
