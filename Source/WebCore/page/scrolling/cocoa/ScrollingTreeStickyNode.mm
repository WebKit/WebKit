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

#import "config.h"
#import "ScrollingTreeStickyNode.h"

#if ENABLE(ASYNC_SCROLLING)

#import "Logging.h"
#import "ScrollingStateStickyNode.h"
#import "ScrollingTree.h"
#import "ScrollingTreeFixedNode.h"
#import "ScrollingTreeFrameScrollingNode.h"
#import "ScrollingTreeOverflowScrollProxyNode.h"
#import "ScrollingTreeOverflowScrollingNode.h"
#import "WebCoreCALayerExtras.h"
#import <wtf/text/TextStream.h>

namespace WebCore {

Ref<ScrollingTreeStickyNode> ScrollingTreeStickyNode::create(ScrollingTree& scrollingTree, ScrollingNodeID nodeID)
{
    return adoptRef(*new ScrollingTreeStickyNode(scrollingTree, nodeID));
}

ScrollingTreeStickyNode::ScrollingTreeStickyNode(ScrollingTree& scrollingTree, ScrollingNodeID nodeID)
    : ScrollingTreeNode(scrollingTree, ScrollingNodeType::Sticky, nodeID)
{
    scrollingTree.fixedOrStickyNodeAdded();
}

ScrollingTreeStickyNode::~ScrollingTreeStickyNode()
{
    scrollingTree().fixedOrStickyNodeRemoved();
}

void ScrollingTreeStickyNode::commitStateBeforeChildren(const ScrollingStateNode& stateNode)
{
    const ScrollingStateStickyNode& stickyStateNode = downcast<ScrollingStateStickyNode>(stateNode);

    if (stickyStateNode.hasChangedProperty(ScrollingStateNode::Property::Layer))
        m_layer = static_cast<CALayer*>(stickyStateNode.layer());

    if (stateNode.hasChangedProperty(ScrollingStateNode::Property::ViewportConstraints))
        m_constraints = stickyStateNode.viewportConstraints();
}

FloatPoint ScrollingTreeStickyNode::computeLayerPosition() const
{
    auto computeLayerPositionForScrollingNode = [&](ScrollingTreeNode& scrollingNode) {
        FloatRect constrainingRect;
        if (is<ScrollingTreeFrameScrollingNode>(scrollingNode)) {
            auto& frameScrollingNode = downcast<ScrollingTreeFrameScrollingNode>(scrollingNode);
            constrainingRect = frameScrollingNode.layoutViewport();
        } else {
            auto& overflowScrollingNode = downcast<ScrollingTreeOverflowScrollingNode>(scrollingNode);
            constrainingRect = FloatRect(overflowScrollingNode.currentScrollOffset(), m_constraints.constrainingRectAtLastLayout().size());
        }
        return m_constraints.layerPositionForConstrainingRect(constrainingRect);
    };

    for (auto* ancestor = parent(); ancestor; ancestor = ancestor->parent()) {
        if (is<ScrollingTreeOverflowScrollProxyNode>(*ancestor)) {
            auto& overflowProxyNode = downcast<ScrollingTreeOverflowScrollProxyNode>(*ancestor);
            auto overflowNode = scrollingTree().nodeForID(overflowProxyNode.overflowScrollingNodeID());
            if (!overflowNode)
                break;

            return computeLayerPositionForScrollingNode(*overflowNode);
        }

        if (is<ScrollingTreeScrollingNode>(*ancestor))
            return computeLayerPositionForScrollingNode(*ancestor);

        if (is<ScrollingTreeFixedNode>(*ancestor) || is<ScrollingTreeStickyNode>(*ancestor)) {
            // FIXME: Do we need scrolling tree nodes at all for nested cases?
            return m_constraints.layerPositionAtLastLayout();
        }
    }
    ASSERT_NOT_REACHED();
    return m_constraints.layerPositionAtLastLayout();
}

void ScrollingTreeStickyNode::applyLayerPositions()
{
    auto layerPosition = computeLayerPosition();

    LOG_WITH_STREAM(Scrolling, stream << "ScrollingTreeStickyNode " << scrollingNodeID() << " constrainingRectAtLastLayout " << m_constraints.constrainingRectAtLastLayout() << " last layer pos " << m_constraints.layerPositionAtLastLayout() << " layerPosition " << layerPosition);

    [m_layer _web_setLayerTopLeftPosition:layerPosition - m_constraints.alignmentOffset()];
}

FloatSize ScrollingTreeStickyNode::scrollDeltaSinceLastCommit() const
{
    auto layerPosition = computeLayerPosition();
    return layerPosition - m_constraints.layerPositionAtLastLayout();
}

void ScrollingTreeStickyNode::dumpProperties(TextStream& ts, ScrollingStateTreeAsTextBehavior behavior) const
{
    ts << "sticky node";

    ScrollingTreeNode::dumpProperties(ts, behavior);
    ts.dumpProperty("sticky constraints", m_constraints);

    if (behavior & ScrollingStateTreeAsTextBehaviorIncludeLayerPositions) {
        FloatRect layerBounds = [m_layer bounds];
        FloatPoint anchorPoint = [m_layer anchorPoint];
        FloatPoint position = [m_layer position];
        FloatPoint layerTopLeft = position - toFloatSize(anchorPoint) * layerBounds.size() + m_constraints.alignmentOffset();
        ts.dumpProperty("layer top left", layerTopLeft);
    }
}

} // namespace WebCore

#endif // ENABLE(ASYNC_SCROLLING)
