/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
 * Copyright (C) 2019 Igalia S.L.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials provided
 *    with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "ScrollingTreeStickyNode.h"

#if ENABLE(ASYNC_SCROLLING)

#include "Logging.h"
#include "ScrollingStateStickyNode.h"
#include "ScrollingTree.h"
#include "ScrollingTreeFixedNode.h"
#include "ScrollingTreeFrameScrollingNode.h"
#include "ScrollingTreeOverflowScrollProxyNode.h"
#include "ScrollingTreeOverflowScrollingNode.h"
#include <wtf/text/TextStream.h>

namespace WebCore {

ScrollingTreeStickyNode::ScrollingTreeStickyNode(ScrollingTree& scrollingTree, ScrollingNodeID nodeID)
    : ScrollingTreeNode(scrollingTree, ScrollingNodeType::Sticky, nodeID)
{
    scrollingTree.fixedOrStickyNodeAdded();
}

ScrollingTreeStickyNode::~ScrollingTreeStickyNode()
{
    scrollingTree().fixedOrStickyNodeRemoved();
}

bool ScrollingTreeStickyNode::commitStateBeforeChildren(const ScrollingStateNode& stateNode)
{
    if (!is<ScrollingStateStickyNode>(stateNode))
        return false;

    const auto& stickyStateNode = downcast<ScrollingStateStickyNode>(stateNode);

    if (stickyStateNode.hasChangedProperty(ScrollingStateNode::Property::ViewportConstraints))
        m_constraints = stickyStateNode.viewportConstraints();

    return true;
}

void ScrollingTreeStickyNode::dumpProperties(TextStream& ts, OptionSet<ScrollingStateTreeAsTextBehavior> behavior) const
{
    ts << "sticky node";
    ScrollingTreeNode::dumpProperties(ts, behavior);
    ts.dumpProperty("sticky constraints", m_constraints);
    if (behavior & ScrollingStateTreeAsTextBehavior::IncludeLayerPositions)
        ts.dumpProperty("layer top left", layerTopLeft());
}

FloatPoint ScrollingTreeStickyNode::computeLayerPosition() const
{
    FloatSize offsetFromStickyAncestors;
    auto computeLayerPositionForScrollingNode = [&](ScrollingTreeNode& scrollingNode) {
        FloatRect constrainingRect;
        if (is<ScrollingTreeFrameScrollingNode>(scrollingNode)) {
            auto& frameScrollingNode = downcast<ScrollingTreeFrameScrollingNode>(scrollingNode);
            constrainingRect = frameScrollingNode.layoutViewport();
        } else if (RefPtr overflowScrollingNode = dynamicDowncast<ScrollingTreeOverflowScrollingNode>(scrollingNode)) {
            constrainingRect = m_constraints.constrainingRectAtLastLayout();
            constrainingRect.move(overflowScrollingNode->scrollDeltaSinceLastCommit());
        }
        constrainingRect.move(-offsetFromStickyAncestors);
        return m_constraints.layerPositionForConstrainingRect(constrainingRect);
    };

    for (RefPtr ancestor = parent(); ancestor; ancestor = ancestor->parent()) {
        if (is<ScrollingTreeOverflowScrollProxyNode>(*ancestor)) {
            auto& overflowProxyNode = downcast<ScrollingTreeOverflowScrollProxyNode>(*ancestor);
            auto overflowNode = scrollingTree().nodeForID(overflowProxyNode.overflowScrollingNodeID());
            if (!overflowNode)
                break;

            return computeLayerPositionForScrollingNode(*overflowNode);
        }

        if (is<ScrollingTreeScrollingNode>(*ancestor))
            return computeLayerPositionForScrollingNode(*ancestor);

        if (is<ScrollingTreeStickyNode>(*ancestor))
            offsetFromStickyAncestors += downcast<ScrollingTreeStickyNode>(*ancestor).scrollDeltaSinceLastCommit();

        if (is<ScrollingTreeFixedNode>(*ancestor)) {
            // FIXME: Do we need scrolling tree nodes at all for nested cases?
            return m_constraints.layerPositionAtLastLayout();
        }
    }
    ASSERT_NOT_REACHED();
    return m_constraints.layerPositionAtLastLayout();
}

FloatSize ScrollingTreeStickyNode::scrollDeltaSinceLastCommit() const
{
    auto layerPosition = computeLayerPosition();
    return layerPosition - m_constraints.layerPositionAtLastLayout();
}

} // namespace WebCore

#endif // ENABLE(ASYNC_SCROLLING) && USE(NICOSIA)
