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
#include "ScrollingStateFixedNode.h"

#include "GraphicsLayer.h"
#include "Logging.h"
#include "ScrollingStateTree.h"
#include <wtf/text/TextStream.h>

#if ENABLE(ASYNC_SCROLLING) || USE(COORDINATED_GRAPHICS)

namespace WebCore {

Ref<ScrollingStateFixedNode> ScrollingStateFixedNode::create(ScrollingStateTree& stateTree, ScrollingNodeID nodeID)
{
    return adoptRef(*new ScrollingStateFixedNode(stateTree, nodeID));
}

ScrollingStateFixedNode::ScrollingStateFixedNode(ScrollingStateTree& tree, ScrollingNodeID nodeID)
    : ScrollingStateNode(ScrollingNodeType::Fixed, tree, nodeID)
{
}

ScrollingStateFixedNode::ScrollingStateFixedNode(const ScrollingStateFixedNode& node, ScrollingStateTree& adoptiveTree)
    : ScrollingStateNode(node, adoptiveTree)
    , m_constraints(FixedPositionViewportConstraints(node.viewportConstraints()))
{
}

ScrollingStateFixedNode::~ScrollingStateFixedNode() = default;

Ref<ScrollingStateNode> ScrollingStateFixedNode::clone(ScrollingStateTree& adoptiveTree)
{
    return adoptRef(*new ScrollingStateFixedNode(*this, adoptiveTree));
}

void ScrollingStateFixedNode::updateConstraints(const FixedPositionViewportConstraints& constraints)
{
    if (m_constraints == constraints)
        return;

    LOG_WITH_STREAM(Scrolling, stream << "ScrollingStateFixedNode " << scrollingNodeID() << " updateConstraints with viewport rect " << constraints.viewportRectAtLastLayout() << " layer pos at last layout " << constraints.layerPositionAtLastLayout() << " offset from top " << (constraints.layerPositionAtLastLayout().y() - constraints.viewportRectAtLastLayout().y()));

    m_constraints = constraints;
    setPropertyChanged(ViewportConstraints);
}

void ScrollingStateFixedNode::reconcileLayerPositionForViewportRect(const LayoutRect& viewportRect, ScrollingLayerPositionAction action)
{
    ScrollingStateNode::reconcileLayerPositionForViewportRect(viewportRect, action);

    FloatPoint position = m_constraints.layerPositionForViewportRect(viewportRect);
    if (layer().representsGraphicsLayer()) {
        auto* graphicsLayer = static_cast<GraphicsLayer*>(layer());

        LOG_WITH_STREAM(Scrolling, stream << "ScrollingStateFixedNode " << scrollingNodeID() <<" reconcileLayerPositionForViewportRect " << action << " position of layer " << graphicsLayer->primaryLayerID() << " to " << position);
        
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

void ScrollingStateFixedNode::dumpProperties(TextStream& ts, ScrollingStateTreeAsTextBehavior behavior) const
{
    ts << "Fixed node";
    ScrollingStateNode::dumpProperties(ts, behavior);

    if (m_constraints.anchorEdges()) {
        TextStream::GroupScope scope(ts);
        ts << "anchor edges: ";
        if (m_constraints.hasAnchorEdge(ViewportConstraints::AnchorEdgeLeft))
            ts << "AnchorEdgeLeft ";
        if (m_constraints.hasAnchorEdge(ViewportConstraints::AnchorEdgeRight))
            ts << "AnchorEdgeRight ";
        if (m_constraints.hasAnchorEdge(ViewportConstraints::AnchorEdgeTop))
            ts << "AnchorEdgeTop";
        if (m_constraints.hasAnchorEdge(ViewportConstraints::AnchorEdgeBottom))
            ts << "AnchorEdgeBottom";
    }

    if (!m_constraints.alignmentOffset().isEmpty())
        ts.dumpProperty("alignment offset", m_constraints.alignmentOffset());

    if (!m_constraints.viewportRectAtLastLayout().isEmpty())
        ts.dumpProperty("viewport rect at last layout", m_constraints.viewportRectAtLastLayout());

    if (m_constraints.layerPositionAtLastLayout() != FloatPoint())
        ts.dumpProperty("layer position at last layout", m_constraints.layerPositionAtLastLayout());
}

} // namespace WebCore

#endif // ENABLE(ASYNC_SCROLLING) || USE(COORDINATED_GRAPHICS)
