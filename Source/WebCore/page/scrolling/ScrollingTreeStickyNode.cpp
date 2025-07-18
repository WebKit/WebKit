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
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/TextStream.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(ScrollingTreeStickyNode);

ScrollingTreeStickyNode::ScrollingTreeStickyNode(ScrollingTree& scrollingTree, ScrollingNodeID nodeID)
    : ScrollingTreeViewportConstrainedNode(scrollingTree, ScrollingNodeType::Sticky, nodeID)
{
}

ScrollingTreeStickyNode::~ScrollingTreeStickyNode() = default;

bool ScrollingTreeStickyNode::commitStateBeforeChildren(const ScrollingStateNode& stateNode)
{
    auto* stickyStateNode = dynamicDowncast<ScrollingStateStickyNode>(stateNode);
    if (!stickyStateNode)
        return false;

    if (stickyStateNode->hasChangedProperty(ScrollingStateNode::Property::ViewportConstraints))
        m_constraints = stickyStateNode->viewportConstraints();

    return true;
}

void ScrollingTreeStickyNode::dumpProperties(TextStream& ts, OptionSet<ScrollingStateTreeAsTextBehavior> behavior) const
{
    ts << "sticky node"_s;
    ScrollingTreeNode::dumpProperties(ts, behavior);
    ts.dumpProperty("sticky constraints"_s, m_constraints);
    if (behavior & ScrollingStateTreeAsTextBehavior::IncludeLayerPositions)
        ts.dumpProperty("layer top left"_s, layerTopLeft());
}

FloatPoint ScrollingTreeStickyNode::computeClippingLayerPosition() const
{
    if (!hasViewportClippingLayer()) {
        ASSERT_NOT_REACHED();
        return { };
    }

    return computeLayerPosition();
}

std::optional<FloatRect> ScrollingTreeStickyNode::findConstrainingRect() const
{
    FloatSize offsetFromStickyAncestors;
    auto computeConstrainingRectForScrollingNode = [&](ScrollingTreeNode& scrollingNode) {
        FloatRect constrainingRect;
        if (auto* frameScrollingNode = dynamicDowncast<ScrollingTreeFrameScrollingNode>(scrollingNode))
            constrainingRect = frameScrollingNode->layoutViewport();
        else if (auto* overflowScrollingNode = dynamicDowncast<ScrollingTreeOverflowScrollingNode>(scrollingNode)) {
            constrainingRect = m_constraints.constrainingRectAtLastLayout();
            constrainingRect.move(overflowScrollingNode->scrollDeltaSinceLastCommit());
        }
        constrainingRect.move(-offsetFromStickyAncestors);
        return constrainingRect;
    };

    for (RefPtr ancestor = parent(); ancestor; ancestor = ancestor->parent()) {
        if (auto* overflowProxyNode = dynamicDowncast<ScrollingTreeOverflowScrollProxyNode>(*ancestor)) {
            auto overflowNode = scrollingTree()->nodeForID(overflowProxyNode->overflowScrollingNodeID());
            if (!overflowNode)
                break;

            return computeConstrainingRectForScrollingNode(*overflowNode);
        }

        if (is<ScrollingTreeScrollingNode>(*ancestor))
            return computeConstrainingRectForScrollingNode(*ancestor);

        if (auto* stickyNode = dynamicDowncast<ScrollingTreeStickyNode>(*ancestor))
            offsetFromStickyAncestors += stickyNode->scrollDeltaSinceLastCommit();

        if (is<ScrollingTreeFixedNode>(*ancestor)) {
            // FIXME: Do we need scrolling tree nodes at all for nested cases?
            return std::nullopt;
        }
    }
    ASSERT_NOT_REACHED();
    return std::nullopt;
}

std::pair<std::optional<FloatRect>, FloatPoint> ScrollingTreeStickyNode::computeConstrainingRectAndAnchorLayerPosition() const
{
    if (auto constrainingRect = findConstrainingRect())
        return { constrainingRect, m_constraints.anchorLayerPositionForConstrainingRect(*constrainingRect) };

    return { std::nullopt, m_constraints.layerPositionAtLastLayout() };
}

FloatSize ScrollingTreeStickyNode::scrollDeltaSinceLastCommit() const
{
    auto anchorLayerPosition = computeConstrainingRectAndAnchorLayerPosition().second;
    return anchorLayerPosition - m_constraints.anchorLayerPositionAtLastLayout();
}

bool ScrollingTreeStickyNode::isCurrentlySticking() const
{
    auto constrainingRect = findConstrainingRect();
    return constrainingRect && isCurrentlySticking(*constrainingRect);
}

bool ScrollingTreeStickyNode::isCurrentlySticking(const FloatRect& constrainingRect) const
{
    auto stickyOffset = m_constraints.computeStickyOffset(constrainingRect);
    auto stickyRect = m_constraints.stickyBoxRect();
    auto containingRect = m_constraints.containingBlockRect();

    // FIXME: This should also account for horizontal scrolling.
    return stickyOffset.height() > 0 && stickyOffset.height() < containingRect.height() - stickyRect.height();
}

} // namespace WebCore

#endif // ENABLE(ASYNC_SCROLLING)
