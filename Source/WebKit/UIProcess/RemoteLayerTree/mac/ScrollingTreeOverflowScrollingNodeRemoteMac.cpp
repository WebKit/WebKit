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
#include <WebCore/ScrollingStateOverflowScrollingNode.h>
#include <WebCore/ScrollingTreeScrollingNodeDelegate.h>

namespace WebKit {
using namespace WebCore;

Ref<ScrollingTreeOverflowScrollingNodeRemoteMac> ScrollingTreeOverflowScrollingNodeRemoteMac::create(ScrollingTree& tree, ScrollingNodeID nodeID)
{
    return adoptRef(*new ScrollingTreeOverflowScrollingNodeRemoteMac(tree, nodeID));
}

ScrollingTreeOverflowScrollingNodeRemoteMac::ScrollingTreeOverflowScrollingNodeRemoteMac(ScrollingTree& tree, ScrollingNodeID nodeID)
    : ScrollingTreeOverflowScrollingNodeMac(tree, nodeID)
{
    m_delegate->initScrollbars();
}

ScrollingTreeOverflowScrollingNodeRemoteMac::~ScrollingTreeOverflowScrollingNodeRemoteMac()
{
}

void ScrollingTreeOverflowScrollingNodeRemoteMac::commitStateBeforeChildren(const ScrollingStateNode& stateNode)
{
    ScrollingTreeOverflowScrollingNodeMac::commitStateBeforeChildren(stateNode);
    const auto& scrollingStateNode = downcast<ScrollingStateOverflowScrollingNode>(stateNode);
    m_delegate->updateFromStateNode(scrollingStateNode);
}

void ScrollingTreeOverflowScrollingNodeRemoteMac::repositionRelatedLayers()
{
    ScrollingTreeOverflowScrollingNodeMac::repositionRelatedLayers();
    m_delegate->updateScrollbarLayers();
}

WheelEventHandlingResult ScrollingTreeOverflowScrollingNodeRemoteMac::handleWheelEvent(const PlatformWheelEvent& wheelEvent, EventTargeting eventTargeting)
{
    auto result = ScrollingTreeOverflowScrollingNodeMac::handleWheelEvent(wheelEvent, eventTargeting);
    m_delegate->handleWheelEventForScrollbars(wheelEvent);
    return result;
}

bool ScrollingTreeOverflowScrollingNodeRemoteMac::handleMouseEvent(const PlatformMouseEvent& mouseEvent)
{
    return m_delegate->handleMouseEventForScrollbars(mouseEvent);
}

}

#endif
