/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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
#include "ScrollingTreeStickyNode.h"

#if ENABLE(ASYNC_SCROLLING)

#include "ScrollingStateStickyNode.h"
#include "ScrollingTree.h"
#include "ScrollingTreeOverflowScrollingNode.h"
#include <QuartzCore/CALayer.h>

namespace WebCore {

PassRefPtr<ScrollingTreeStickyNode> ScrollingTreeStickyNode::create(ScrollingTree& scrollingTree, ScrollingNodeID nodeID)
{
    return adoptRef(new ScrollingTreeStickyNode(scrollingTree, nodeID));
}

ScrollingTreeStickyNode::ScrollingTreeStickyNode(ScrollingTree& scrollingTree, ScrollingNodeID nodeID)
    : ScrollingTreeNode(scrollingTree, StickyNode, nodeID)
{
    scrollingTree.fixedOrStickyNodeAdded();
}

ScrollingTreeStickyNode::~ScrollingTreeStickyNode()
{
    scrollingTree().fixedOrStickyNodeRemoved();
}

void ScrollingTreeStickyNode::updateBeforeChildren(const ScrollingStateNode& stateNode)
{
    const ScrollingStateStickyNode& stickyStateNode = toScrollingStateStickyNode(stateNode);

    if (stickyStateNode.hasChangedProperty(ScrollingStateNode::ScrollLayer))
        m_layer = stickyStateNode.layer();

    if (stateNode.hasChangedProperty(ScrollingStateStickyNode::ViewportConstraints))
        m_constraints = stickyStateNode.viewportConstraints();
}

static inline CGPoint operator*(CGPoint& a, const CGSize& b)
{
    return CGPointMake(a.x * b.width, a.y * b.height);
}

void ScrollingTreeStickyNode::updateLayersAfterAncestorChange(const ScrollingTreeNode& changedNode, const FloatRect& fixedPositionRect, const FloatSize& cumulativeDelta)
{
    bool adjustStickyLayer = false;
    FloatRect constrainingRect;

    if (parent()->isOverflowScrollingNode()) {
        constrainingRect = FloatRect(toScrollingTreeOverflowScrollingNode(parent())->scrollPosition(), m_constraints.constrainingRectAtLastLayout().size());
        adjustStickyLayer = true;
    } else if (parent()->isFrameScrollingNode()) {
        constrainingRect = fixedPositionRect;
        adjustStickyLayer = true;
    }

    FloatSize deltaForDescendants = cumulativeDelta;

    if (adjustStickyLayer) {
        FloatPoint layerPosition = m_constraints.layerPositionForConstrainingRect(constrainingRect);

        CGRect layerBounds = [m_layer bounds];
        CGPoint anchorPoint = [m_layer anchorPoint];
        CGPoint newPosition = layerPosition - m_constraints.alignmentOffset() + anchorPoint * layerBounds.size;
        [m_layer setPosition:newPosition];

        deltaForDescendants = layerPosition - m_constraints.layerPositionAtLastLayout() + cumulativeDelta;
    }

    if (!m_children)
        return;

    for (auto& child : *m_children)
        child->updateLayersAfterAncestorChange(changedNode, fixedPositionRect, deltaForDescendants);
}

} // namespace WebCore

#endif // ENABLE(ASYNC_SCROLLING)
