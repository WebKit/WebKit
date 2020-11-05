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
#include "ScrollingStateStickyNode.h"

#if ENABLE(ASYNC_SCROLLING)

#include "GraphicsLayer.h"
#include "Logging.h"
#include "ScrollingStateFixedNode.h"
#include "ScrollingStateFrameScrollingNode.h"
#include "ScrollingStateOverflowScrollProxyNode.h"
#include "ScrollingStateOverflowScrollingNode.h"
#include "ScrollingStateTree.h"
#include "ScrollingTree.h"
#include <wtf/text/TextStream.h>

namespace WebCore {

Ref<ScrollingStateStickyNode> ScrollingStateStickyNode::create(ScrollingStateTree& stateTree, ScrollingNodeID nodeID)
{
    return adoptRef(*new ScrollingStateStickyNode(stateTree, nodeID));
}

ScrollingStateStickyNode::ScrollingStateStickyNode(ScrollingStateTree& tree, ScrollingNodeID nodeID)
    : ScrollingStateNode(ScrollingNodeType::Sticky, tree, nodeID)
{
}

ScrollingStateStickyNode::ScrollingStateStickyNode(const ScrollingStateStickyNode& node, ScrollingStateTree& adoptiveTree)
    : ScrollingStateNode(node, adoptiveTree)
    , m_constraints(StickyPositionViewportConstraints(node.viewportConstraints()))
{
}

ScrollingStateStickyNode::~ScrollingStateStickyNode() = default;

Ref<ScrollingStateNode> ScrollingStateStickyNode::clone(ScrollingStateTree& adoptiveTree)
{
    return adoptRef(*new ScrollingStateStickyNode(*this, adoptiveTree));
}

OptionSet<ScrollingStateNode::Property> ScrollingStateStickyNode::applicableProperties() const
{
    constexpr OptionSet<Property> nodeProperties = { Property::ViewportConstraints };

    auto properties = ScrollingStateNode::applicableProperties();
    properties.add(nodeProperties);
    return properties;
}

void ScrollingStateStickyNode::updateConstraints(const StickyPositionViewportConstraints& constraints)
{
    if (m_constraints == constraints)
        return;

    LOG_WITH_STREAM(Scrolling, stream << "ScrollingStateStickyNode " << scrollingNodeID() << " updateConstraints with constraining rect " << constraints.constrainingRectAtLastLayout() << " sticky offset " << constraints.stickyOffsetAtLastLayout() << " layer pos at last layout " << constraints.layerPositionAtLastLayout());

    m_constraints = constraints;
    setPropertyChanged(Property::ViewportConstraints);
}

FloatPoint ScrollingStateStickyNode::computeLayerPosition(const LayoutRect& viewportRect) const
{
    // This logic follows ScrollingTreeStickyNode::computeLayerPosition().
    auto computeLayerPositionForScrollingNode = [&](ScrollingStateNode& scrollingStateNode) {
        FloatRect constrainingRect;
        if (is<ScrollingStateFrameScrollingNode>(scrollingStateNode))
            constrainingRect = viewportRect;
        else {
            auto& overflowScrollingNode = downcast<ScrollingStateOverflowScrollingNode>(scrollingStateNode);
            constrainingRect = FloatRect(overflowScrollingNode.scrollPosition(), m_constraints.constrainingRectAtLastLayout().size());
        }
        return m_constraints.layerPositionForConstrainingRect(constrainingRect);
    };

    for (auto* ancestor = parent(); ancestor; ancestor = ancestor->parent()) {
        if (is<ScrollingStateOverflowScrollProxyNode>(*ancestor)) {
            auto& overflowProxyNode = downcast<ScrollingStateOverflowScrollProxyNode>(*ancestor);
            auto overflowNode = scrollingStateTree().stateNodeForID(overflowProxyNode.overflowScrollingNode());
            if (!overflowNode)
                break;

            return computeLayerPositionForScrollingNode(*overflowNode);
        }

        if (is<ScrollingStateScrollingNode>(*ancestor))
            return computeLayerPositionForScrollingNode(*ancestor);

        if (is<ScrollingStateFixedNode>(*ancestor) || is<ScrollingStateStickyNode>(*ancestor)) {
            // FIXME: Do we need scrolling tree nodes at all for nested cases?
            return m_constraints.layerPositionAtLastLayout();
        }
    }
    ASSERT_NOT_REACHED();
    return m_constraints.layerPositionAtLastLayout();
}

void ScrollingStateStickyNode::reconcileLayerPositionForViewportRect(const LayoutRect& viewportRect, ScrollingLayerPositionAction action)
{
    FloatPoint position = computeLayerPosition(viewportRect);
    if (layer().representsGraphicsLayer()) {
        auto* graphicsLayer = static_cast<GraphicsLayer*>(layer());

        LOG_WITH_STREAM(Compositing, stream << "ScrollingStateStickyNode " << scrollingNodeID() << " reconcileLayerPositionForViewportRect " << action << " position of layer " << graphicsLayer->primaryLayerID() << " to " << position << " sticky offset " << m_constraints.stickyOffsetAtLastLayout());
        
        switch (action) {
        case ScrollingLayerPositionAction::Set:
            graphicsLayer->setPosition(position);
            break;

        case ScrollingLayerPositionAction::SetApproximate:
            graphicsLayer->setApproximatePosition(position);
            break;
        
        case ScrollingLayerPositionAction::Sync:
            graphicsLayer->syncPosition(position);
            break;
        }
    }
}

void ScrollingStateStickyNode::dumpProperties(TextStream& ts, ScrollingStateTreeAsTextBehavior behavior) const
{
    ts << "Sticky node";
    ScrollingStateNode::dumpProperties(ts, behavior);

    if (m_constraints.anchorEdges()) {
        TextStream::GroupScope scope(ts);
        ts << "anchor edges: ";
        if (m_constraints.hasAnchorEdge(ViewportConstraints::AnchorEdgeLeft))
            ts << "AnchorEdgeLeft ";
        if (m_constraints.hasAnchorEdge(ViewportConstraints::AnchorEdgeRight))
            ts << "AnchorEdgeRight ";
        if (m_constraints.hasAnchorEdge(ViewportConstraints::AnchorEdgeTop))
            ts << "AnchorEdgeTop ";
        if (m_constraints.hasAnchorEdge(ViewportConstraints::AnchorEdgeBottom))
            ts << "AnchorEdgeBottom";
    }

    if (m_constraints.hasAnchorEdge(ViewportConstraints::AnchorEdgeLeft))
        ts.dumpProperty("left offset", m_constraints.leftOffset());
    if (m_constraints.hasAnchorEdge(ViewportConstraints::AnchorEdgeRight))
        ts.dumpProperty("right offset", m_constraints.rightOffset());
    if (m_constraints.hasAnchorEdge(ViewportConstraints::AnchorEdgeTop))
        ts.dumpProperty("top offset", m_constraints.topOffset());
    if (m_constraints.hasAnchorEdge(ViewportConstraints::AnchorEdgeBottom))
        ts.dumpProperty("bottom offset", m_constraints.bottomOffset());

    ts.dumpProperty("containing block rect", m_constraints.containingBlockRect());

    ts.dumpProperty("sticky box rect", m_constraints.stickyBoxRect());

    ts.dumpProperty("constraining rect", m_constraints.constrainingRectAtLastLayout());

    ts.dumpProperty("sticky offset at last layout", m_constraints.stickyOffsetAtLastLayout());

    ts.dumpProperty("layer position at last layout", m_constraints.layerPositionAtLastLayout());
}

} // namespace WebCore

#endif // ENABLE(ASYNC_SCROLLING)
