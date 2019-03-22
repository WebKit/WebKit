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
#import "ScrollingTreePositionedNode.h"

#if ENABLE(ASYNC_SCROLLING)

#import "Logging.h"
#import "ScrollingStatePositionedNode.h"
#import "ScrollingTree.h"
#import "ScrollingTreeOverflowScrollingNode.h"
#import "ScrollingTreeScrollingNode.h"
#import <QuartzCore/CALayer.h>
#import <wtf/text/TextStream.h>

namespace WebCore {

Ref<ScrollingTreePositionedNode> ScrollingTreePositionedNode::create(ScrollingTree& scrollingTree, ScrollingNodeID nodeID)
{
    return adoptRef(*new ScrollingTreePositionedNode(scrollingTree, nodeID));
}

ScrollingTreePositionedNode::ScrollingTreePositionedNode(ScrollingTree& scrollingTree, ScrollingNodeID nodeID)
    : ScrollingTreeNode(scrollingTree, ScrollingNodeType::Positioned, nodeID)
{
}

ScrollingTreePositionedNode::~ScrollingTreePositionedNode() = default;

void ScrollingTreePositionedNode::commitStateBeforeChildren(const ScrollingStateNode& stateNode)
{
    const ScrollingStatePositionedNode& positionedStateNode = downcast<ScrollingStatePositionedNode>(stateNode);

    if (positionedStateNode.hasChangedProperty(ScrollingStateNode::Layer))
        m_layer = positionedStateNode.layer();

    if (positionedStateNode.hasChangedProperty(ScrollingStatePositionedNode::RelatedOverflowScrollingNodes))
        m_relatedOverflowScrollingNodes = positionedStateNode.relatedOverflowScrollingNodes();

    if (positionedStateNode.hasChangedProperty(ScrollingStatePositionedNode::LayoutConstraintData))
        m_constraints = positionedStateNode.layoutConstraints();

    // Tell the ScrollingTree about non-ancestor overflow nodes which affect this node.
    if (m_constraints.scrollPositioningBehavior() == ScrollPositioningBehavior::Moves) {
        auto& relatedNodes = scrollingTree().overflowRelatedNodes();
        for (auto overflowNodeID : m_relatedOverflowScrollingNodes) {
            relatedNodes.ensure(overflowNodeID, [] {
                return Vector<ScrollingNodeID>();
            }).iterator->value.append(scrollingNodeID());
        }
    }
    if (!m_relatedOverflowScrollingNodes.isEmpty() && m_constraints.scrollPositioningBehavior() != ScrollPositioningBehavior::None)
        scrollingTree().positionedNodesWithRelatedOverflow().add(scrollingNodeID());
}

void ScrollingTreePositionedNode::applyLayerPositions(const FloatRect&, FloatSize& cumulativeDelta)
{
    // Note that we ignore cumulativeDelta because it will contain the delta for ancestor scrollers,
    // but not non-ancestor ones, so it's simpler to just recompute from the scrollers we know about here.
    FloatSize scrollOffsetSinceLastCommit;
    for (auto nodeID : m_relatedOverflowScrollingNodes) {
        if (auto* node = scrollingTree().nodeForID(nodeID)) {
            if (is<ScrollingTreeOverflowScrollingNode>(node)) {
                auto& overflowNode = downcast<ScrollingTreeOverflowScrollingNode>(*node);
                scrollOffsetSinceLastCommit += overflowNode.lastCommittedScrollPosition() - overflowNode.currentScrollPosition();
            }
        }
    }
    auto layerOffset = -scrollOffsetSinceLastCommit;
    if (m_constraints.scrollPositioningBehavior() == ScrollPositioningBehavior::Stationary) {
        // Stationary nodes move in the opposite direction.
        layerOffset = -layerOffset;
    }

    FloatPoint layerPosition = m_constraints.layerPositionAtLastLayout() - layerOffset;

    LOG_WITH_STREAM(Scrolling, stream << "ScrollingTreePositionedNode " << scrollingNodeID() << " applyLayerPositions: overflow delta " << scrollOffsetSinceLastCommit << " moving layer to " << layerPosition);

    [m_layer _web_setLayerTopLeftPosition:layerPosition - m_constraints.alignmentOffset()];

    // FIXME: Should our scroller deltas propagate to descendants?
    cumulativeDelta = layerPosition - m_constraints.layerPositionAtLastLayout();
}

void ScrollingTreePositionedNode::relatedNodeScrollPositionDidChange(const ScrollingTreeScrollingNode& changedNode, const FloatRect& layoutViewport, FloatSize& cumulativeDelta)
{
    if (!m_relatedOverflowScrollingNodes.contains(changedNode.scrollingNodeID()))
        return;

    applyLayerPositions(layoutViewport, cumulativeDelta);
}

void ScrollingTreePositionedNode::dumpProperties(TextStream& ts, ScrollingStateTreeAsTextBehavior behavior) const
{
    ts << "positioned node";
    ScrollingTreeNode::dumpProperties(ts, behavior);

    ts.dumpProperty("layout constraints", m_constraints);
    ts.dumpProperty("related overflow nodes", m_relatedOverflowScrollingNodes.size());

    if (behavior & ScrollingStateTreeAsTextBehaviorIncludeNodeIDs) {
        if (!m_relatedOverflowScrollingNodes.isEmpty()) {
            TextStream::GroupScope scope(ts);
            ts << "overflow nodes";
            for (auto nodeID : m_relatedOverflowScrollingNodes)
                ts << "\n" << indent << "nodeID " << nodeID;
        }
    }
}

} // namespace WebCore

#endif // ENABLE(ASYNC_SCROLLING)
