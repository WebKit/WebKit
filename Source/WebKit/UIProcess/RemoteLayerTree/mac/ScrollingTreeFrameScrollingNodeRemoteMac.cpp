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

ScrollingTreeFrameScrollingNodeRemoteMac::ScrollingTreeFrameScrollingNodeRemoteMac(WebCore::ScrollingTree& tree, WebCore::ScrollingNodeType nodeType, WebCore::ScrollingNodeID nodeID)
    : WebCore::ScrollingTreeFrameScrollingNodeMac(tree, nodeType, nodeID)
    , m_scrollerPair(std::make_unique<ScrollerPairMac>(*this))
{
}

ScrollingTreeFrameScrollingNodeRemoteMac::~ScrollingTreeFrameScrollingNodeRemoteMac()
{
}

Ref<ScrollingTreeFrameScrollingNodeRemoteMac> ScrollingTreeFrameScrollingNodeRemoteMac::create(WebCore::ScrollingTree& tree, WebCore::ScrollingNodeType nodeType, WebCore::ScrollingNodeID nodeID)
{
    return adoptRef(*new ScrollingTreeFrameScrollingNodeRemoteMac(tree, nodeType, nodeID));
}

void ScrollingTreeFrameScrollingNodeRemoteMac::commitStateBeforeChildren(const WebCore::ScrollingStateNode& stateNode)
{
    WebCore::ScrollingTreeFrameScrollingNodeMac::commitStateBeforeChildren(stateNode);
    const auto& scrollingStateNode = downcast<WebCore::ScrollingStateFrameScrollingNode>(stateNode);

    if (scrollingStateNode.hasChangedProperty(WebCore::ScrollingStateFrameScrollingNode::VerticalScrollbarLayer))
        m_scrollerPair->verticalScroller().setHostLayer(scrollingStateNode.verticalScrollbarLayer());

    if (scrollingStateNode.hasChangedProperty(WebCore::ScrollingStateFrameScrollingNode::HorizontalScrollbarLayer))
        m_scrollerPair->horizontalScroller().setHostLayer(scrollingStateNode.horizontalScrollbarLayer());

    m_scrollerPair->updateValues();
}

void ScrollingTreeFrameScrollingNodeRemoteMac::setScrollLayerPosition(const WebCore::FloatPoint& position, const WebCore::FloatRect& layoutViewport)
{
    ScrollingTreeFrameScrollingNodeMac::setScrollLayerPosition(position, layoutViewport);

    m_scrollerPair->updateValues();
}

void ScrollingTreeFrameScrollingNodeRemoteMac::handleWheelEvent(const WebCore::PlatformWheelEvent& wheelEvent)
{
    ScrollingTreeFrameScrollingNodeMac::handleWheelEvent(wheelEvent);

    m_scrollerPair->handleWheelEvent(wheelEvent);
}

void ScrollingTreeFrameScrollingNodeRemoteMac::handleMouseEvent(const WebCore::PlatformMouseEvent& mouseEvent)
{
    m_scrollerPair->handleMouseEvent(mouseEvent);
}

}

#endif
