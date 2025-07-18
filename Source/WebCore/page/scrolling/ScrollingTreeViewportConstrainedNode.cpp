/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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
#include "ScrollingTreeViewportConstrainedNode.h"

#if ENABLE(ASYNC_SCROLLING)

#include "ScrollingConstraints.h"
#include "ScrollingStateStickyNode.h"
#include "ScrollingTree.h"
#include "ScrollingTreeFixedNode.h"
#include "ScrollingTreeFrameScrollingNode.h"
#include "ScrollingTreeOverflowScrollProxyNode.h"
#include "ScrollingTreeOverflowScrollingNode.h"
#include "ScrollingTreePositionedNode.h"
#include "ScrollingTreeStickyNode.h"

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(ScrollingTreeViewportConstrainedNode);

ScrollingTreeViewportConstrainedNode::ScrollingTreeViewportConstrainedNode(ScrollingTree& scrollingTree, ScrollingNodeType nodeType, ScrollingNodeID nodeID)
    : ScrollingTreeNode(scrollingTree, nodeType, nodeID)
{
    scrollingTree.fixedOrStickyNodeAdded(*this);
}

ScrollingTreeViewportConstrainedNode::~ScrollingTreeViewportConstrainedNode() = default;

FloatPoint ScrollingTreeViewportConstrainedNode::computeLayerPosition() const
{
    FloatSize overflowScrollDelta;
    RefPtr<ScrollingTreeStickyNode> lastStickyNode;
    for (RefPtr ancestor = parent(); ancestor; ancestor = ancestor->parent()) {
        if (RefPtr scrollingNode = dynamicDowncast<ScrollingTreeFrameScrollingNode>(*ancestor)) {
            // Fixed nodes are positioned relative to the containing frame scrolling node.
            // We bail out after finding one.
            auto layoutViewport = scrollingNode->layoutViewport();
            return constraints().viewportRelativeLayerPosition(layoutViewport) - overflowScrollDelta;
        }

        if (RefPtr overflowNode = dynamicDowncast<ScrollingTreeOverflowScrollingNode>(*ancestor)) {
            // To keep the layer still during async scrolling we adjust by how much the position has changed since layout.
            overflowScrollDelta -= overflowNode->scrollDeltaSinceLastCommit();
            continue;
        }

        if (RefPtr overflowNode = dynamicDowncast<ScrollingTreeOverflowScrollProxyNode>(*ancestor)) {
            // To keep the layer still during async scrolling we adjust by how much the position has changed since layout.
            overflowScrollDelta -= overflowNode->scrollDeltaSinceLastCommit();
            continue;
        }

        if (RefPtr positioningAncestor = dynamicDowncast<ScrollingTreePositionedNode>(*ancestor)) {
            // See if sticky node already handled this positioning node.
            // FIXME: Include positioning node information to sticky/fixed node to avoid these tests.
            if (lastStickyNode && lastStickyNode->layer() == positioningAncestor->layer())
                continue;
            if (positioningAncestor->layer() != layer())
                overflowScrollDelta -= positioningAncestor->scrollDeltaSinceLastCommit();
            continue;
        }

        if (RefPtr stickyNode = dynamicDowncast<ScrollingTreeStickyNode>(*ancestor)) {
            overflowScrollDelta += stickyNode->scrollDeltaSinceLastCommit();
            lastStickyNode = stickyNode;
            continue;
        }

        if (is<ScrollingTreeFixedNode>(*ancestor)) {
            // The ancestor fixed node has already applied the needed corrections to say put.
            return constraints().layerPositionAtLastLayout() - overflowScrollDelta;
        }
    }
    ASSERT_NOT_REACHED();
    return { };
}

} // namespace WebCore

#endif // ENABLE(ASYNC_SCROLLING)
