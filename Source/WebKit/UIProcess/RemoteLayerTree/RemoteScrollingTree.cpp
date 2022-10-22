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

#if ENABLE(UI_SIDE_COMPOSITING)

#include "RemoteLayerTreeHost.h"
#include "RemoteScrollingCoordinatorProxy.h"
#include <WebCore/ScrollingTreeFixedNodeCocoa.h>
#include <WebCore/ScrollingTreeFrameHostingNode.h>
#include <WebCore/ScrollingTreeOverflowScrollProxyNodeCocoa.h>
#include <WebCore/ScrollingTreePositionedNodeCocoa.h>
#include <WebCore/ScrollingTreeStickyNodeCocoa.h>

namespace WebKit {
using namespace WebCore;

RemoteScrollingTree::RemoteScrollingTree(RemoteScrollingCoordinatorProxy& scrollingCoordinator)
    : m_scrollingCoordinatorProxy(scrollingCoordinator)
{
}

RemoteScrollingTree::~RemoteScrollingTree() = default;

void RemoteScrollingTree::invalidate()
{
    Locker locker { m_treeLock };
    removeAllNodes();
}

void RemoteScrollingTree::scrollingTreeNodeDidScroll(ScrollingTreeScrollingNode& node, ScrollingLayerPositionAction scrollingLayerPositionAction)
{
    std::optional<FloatPoint> layoutViewportOrigin;
    if (is<ScrollingTreeFrameScrollingNode>(node))
        layoutViewportOrigin = downcast<ScrollingTreeFrameScrollingNode>(node).layoutViewport().location();

    m_scrollingCoordinatorProxy.scrollingTreeNodeDidScroll(node.scrollingNodeID(), node.currentScrollPosition(), layoutViewportOrigin, scrollingLayerPositionAction);
}

void RemoteScrollingTree::scrollingTreeNodeDidStopAnimatedScroll(ScrollingTreeScrollingNode& node)
{
    m_scrollingCoordinatorProxy.scrollingTreeNodeDidStopAnimatedScroll(node.scrollingNodeID());
}

bool RemoteScrollingTree::scrollingTreeNodeRequestsScroll(ScrollingNodeID nodeID, const RequestedScrollData& request)
{
    return m_scrollingCoordinatorProxy.scrollingTreeNodeRequestsScroll(nodeID, request);
}

Ref<ScrollingTreeNode> RemoteScrollingTree::createScrollingTreeNode(ScrollingNodeType nodeType, ScrollingNodeID nodeID)
{
    switch (nodeType) {
    case ScrollingNodeType::MainFrame:
    case ScrollingNodeType::Subframe:
    case ScrollingNodeType::Overflow:
        ASSERT_NOT_REACHED(); // Subclass should have handled this.
        break;

    case ScrollingNodeType::FrameHosting:
        return ScrollingTreeFrameHostingNode::create(*this, nodeID);
    case ScrollingNodeType::OverflowProxy:
        return ScrollingTreeOverflowScrollProxyNodeCocoa::create(*this, nodeID);
    case ScrollingNodeType::Fixed:
        return ScrollingTreeFixedNodeCocoa::create(*this, nodeID);
    case ScrollingNodeType::Sticky:
        return ScrollingTreeStickyNodeCocoa::create(*this, nodeID);
    case ScrollingNodeType::Positioned:
        return ScrollingTreePositionedNodeCocoa::create(*this, nodeID);
    }
    ASSERT_NOT_REACHED();
    return ScrollingTreeFixedNodeCocoa::create(*this, nodeID);
}

void RemoteScrollingTree::currentSnapPointIndicesDidChange(ScrollingNodeID nodeID, std::optional<unsigned> horizontal, std::optional<unsigned> vertical)
{
    m_scrollingCoordinatorProxy.currentSnapPointIndicesDidChange(nodeID, horizontal, vertical);
}

} // namespace WebKit

#endif // ENABLE(UI_SIDE_COMPOSITING)
