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

#include "config.h"
#include "RemoteScrollingTree.h"

#if ENABLE(ASYNC_SCROLLING)

#include "RemoteLayerTreeHost.h"
#include "RemoteScrollingCoordinatorProxy.h"
#include <WebCore/ScrollingTreeFixedNode.h>
#include <WebCore/ScrollingTreeFrameHostingNode.h>
#include <WebCore/ScrollingTreeStickyNode.h>

#if PLATFORM(IOS_FAMILY)
#include "ScrollingTreeFrameScrollingNodeRemoteIOS.h"
#include "ScrollingTreeOverflowScrollingNodeIOS.h"
#else
#include "ScrollingTreeFrameScrollingNodeRemoteMac.h"
#endif

namespace WebKit {
using namespace WebCore;

Ref<RemoteScrollingTree> RemoteScrollingTree::create(RemoteScrollingCoordinatorProxy& scrollingCoordinator)
{
    return adoptRef(*new RemoteScrollingTree(scrollingCoordinator));
}

RemoteScrollingTree::RemoteScrollingTree(RemoteScrollingCoordinatorProxy& scrollingCoordinator)
    : m_scrollingCoordinatorProxy(scrollingCoordinator)
{
}

RemoteScrollingTree::~RemoteScrollingTree()
{
}

ScrollingEventResult RemoteScrollingTree::tryToHandleWheelEvent(const PlatformWheelEvent& wheelEvent)
{
    if (shouldHandleWheelEventSynchronously(wheelEvent))
        return ScrollingEventResult::SendToMainThread;

    if (willWheelEventStartSwipeGesture(wheelEvent))
        return ScrollingEventResult::DidNotHandleEvent;

    handleWheelEvent(wheelEvent);
    return ScrollingEventResult::DidHandleEvent;
}

#if PLATFORM(MAC)
void RemoteScrollingTree::handleWheelEventPhase(PlatformWheelEventPhase phase)
{
    // FIXME: hand off to m_scrollingCoordinatorProxy?
}
#endif

#if PLATFORM(IOS_FAMILY)
void RemoteScrollingTree::scrollingTreeNodeWillStartPanGesture()
{
    m_scrollingCoordinatorProxy.scrollingTreeNodeWillStartPanGesture();
}

void RemoteScrollingTree::scrollingTreeNodeWillStartScroll()
{
    m_scrollingCoordinatorProxy.scrollingTreeNodeWillStartScroll();
}

void RemoteScrollingTree::scrollingTreeNodeDidEndScroll()
{
    m_scrollingCoordinatorProxy.scrollingTreeNodeDidEndScroll();
}

#endif

void RemoteScrollingTree::scrollingTreeNodeDidScroll(ScrollingNodeID nodeID, const FloatPoint& scrollPosition, const Optional<FloatPoint>& layoutViewportOrigin, ScrollingLayerPositionAction scrollingLayerPositionAction)
{
    m_scrollingCoordinatorProxy.scrollingTreeNodeDidScroll(nodeID, scrollPosition, layoutViewportOrigin, scrollingLayerPositionAction);
}

void RemoteScrollingTree::scrollingTreeNodeRequestsScroll(ScrollingNodeID nodeID, const FloatPoint& scrollPosition, bool representsProgrammaticScroll)
{
    m_scrollingCoordinatorProxy.scrollingTreeNodeRequestsScroll(nodeID, scrollPosition, representsProgrammaticScroll);
}

Ref<ScrollingTreeNode> RemoteScrollingTree::createScrollingTreeNode(ScrollingNodeType nodeType, ScrollingNodeID nodeID)
{
    switch (nodeType) {
    case ScrollingNodeType::MainFrame:
    case ScrollingNodeType::Subframe:
#if PLATFORM(IOS_FAMILY)
        return ScrollingTreeFrameScrollingNodeRemoteIOS::create(*this, nodeType, nodeID);
#else
        return ScrollingTreeFrameScrollingNodeRemoteMac::create(*this, nodeType, nodeID);
#endif
    case ScrollingNodeType::FrameHosting:
        return ScrollingTreeFrameHostingNode::create(*this, nodeID);
    case ScrollingNodeType::Overflow:
#if PLATFORM(IOS_FAMILY)
        return ScrollingTreeOverflowScrollingNodeIOS::create(*this, nodeID);
#else
        ASSERT_NOT_REACHED();
        break;
#endif
    case ScrollingNodeType::Fixed:
        return ScrollingTreeFixedNode::create(*this, nodeID);
    case ScrollingNodeType::Sticky:
        return ScrollingTreeStickyNode::create(*this, nodeID);
    }
    ASSERT_NOT_REACHED();
    return ScrollingTreeFixedNode::create(*this, nodeID);
}

void RemoteScrollingTree::currentSnapPointIndicesDidChange(ScrollingNodeID nodeID, unsigned horizontal, unsigned vertical)
{
    m_scrollingCoordinatorProxy.currentSnapPointIndicesDidChange(nodeID, horizontal, vertical);
}

void RemoteScrollingTree::handleMouseEvent(const WebCore::PlatformMouseEvent& event)
{
#if ENABLE(ASYNC_SCROLLING) && PLATFORM(MAC)
    static_cast<ScrollingTreeFrameScrollingNodeRemoteMac&>(*rootNode()).handleMouseEvent(event);
#else
    UNUSED_PARAM(event);
#endif
}

} // namespace WebKit

#endif // ENABLE(ASYNC_SCROLLING)
