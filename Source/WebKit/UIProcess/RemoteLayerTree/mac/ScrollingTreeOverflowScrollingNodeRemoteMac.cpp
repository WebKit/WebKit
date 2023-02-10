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
#include "ScrollingTreeOverflowScrollingNodeRemoteMac.h"

#if ENABLE(ASYNC_SCROLLING) && PLATFORM(MAC)

#include "RemoteScrollingTree.h"
#include "ScrollerPairMac.h"
#include <WebCore/ScrollingStateOverflowScrollingNode.h>

namespace WebKit {
using namespace WebCore;

Ref<ScrollingTreeOverflowScrollingNodeRemoteMac> ScrollingTreeOverflowScrollingNodeRemoteMac::create(ScrollingTree& tree, ScrollingNodeID nodeID)
{
    return adoptRef(*new ScrollingTreeOverflowScrollingNodeRemoteMac(tree, nodeID));
}

ScrollingTreeOverflowScrollingNodeRemoteMac::ScrollingTreeOverflowScrollingNodeRemoteMac(ScrollingTree& tree, ScrollingNodeID nodeID)
    : ScrollingTreeOverflowScrollingNodeMac(tree, nodeID)
    , m_scrollerPair(makeUnique<ScrollerPairMac>(*this))
{
}

ScrollingTreeOverflowScrollingNodeRemoteMac::~ScrollingTreeOverflowScrollingNodeRemoteMac()
{
}

bool ScrollingTreeOverflowScrollingNodeRemoteMac::commitStateBeforeChildren(const ScrollingStateNode& stateNode)
{
    if (!ScrollingTreeOverflowScrollingNodeMac::commitStateBeforeChildren(stateNode))
        return false;

    if (!is<ScrollingStateOverflowScrollingNode>(stateNode))
        return false;

    const auto& scrollingStateNode = downcast<ScrollingStateOverflowScrollingNode>(stateNode);

    if (scrollingStateNode.hasChangedProperty(ScrollingStateNode::Property::HorizontalScrollbarLayer) && horizontalNativeScrollbarVisibility() == NativeScrollbarVisibility::Visible)
        m_scrollerPair->horizontalScroller().setHostLayer(static_cast<CALayer*>(scrollingStateNode.horizontalScrollbarLayer()));
    
    if (scrollingStateNode.hasChangedProperty(ScrollingStateNode::Property::VerticalScrollbarLayer) && verticalNativeScrollbarVisibility() == NativeScrollbarVisibility::Visible)
        m_scrollerPair->verticalScroller().setHostLayer(static_cast<CALayer*>(scrollingStateNode.verticalScrollbarLayer()));

    if (scrollingStateNode.hasChangedProperty(ScrollingStateNode::Property::ScrollableAreaParams)) {
        if (scrollingStateNode.scrollableAreaParameters().horizontalNativeScrollbarVisibility != NativeScrollbarVisibility::Visible)
            m_scrollerPair->horizontalScroller().setHostLayer(nullptr);
        if (scrollingStateNode.scrollableAreaParameters().verticalNativeScrollbarVisibility != NativeScrollbarVisibility::Visible)
            m_scrollerPair->verticalScroller().setHostLayer(nullptr);
    }

    m_scrollerPair->updateValues();
    return true;
}

void ScrollingTreeOverflowScrollingNodeRemoteMac::repositionRelatedLayers()
{
    ScrollingTreeOverflowScrollingNodeMac::repositionRelatedLayers();

    m_scrollerPair->updateValues();
}

WheelEventHandlingResult ScrollingTreeOverflowScrollingNodeRemoteMac::handleWheelEvent(const PlatformWheelEvent& wheelEvent, EventTargeting eventTargeting)
{
    auto result = ScrollingTreeOverflowScrollingNodeMac::handleWheelEvent(wheelEvent, eventTargeting);
    m_scrollerPair->handleWheelEvent(wheelEvent);
    return result;
}

bool ScrollingTreeOverflowScrollingNodeRemoteMac::handleMouseEvent(const PlatformMouseEvent& mouseEvent)
{
    return m_scrollerPair->handleMouseEvent(mouseEvent);
}

}

#endif
