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
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/TextStream.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(ScrollingStateStickyNode);

ScrollingStateStickyNode::ScrollingStateStickyNode(ScrollingNodeID nodeID, Vector<Ref<ScrollingStateNode>>&& children, OptionSet<ScrollingStateNodeProperty> changedProperties, std::optional<PlatformLayerIdentifier> layerID, StickyPositionViewportConstraints&& constraints, LayerRepresentation&& viewportAnchorLayer)
    : ScrollingStateNode(ScrollingNodeType::Sticky, nodeID, WTFMove(children), changedProperties, layerID)
    , m_constraints(WTFMove(constraints))
    , m_viewportAnchorLayer(WTFMove(viewportAnchorLayer))
{
}

ScrollingStateStickyNode::ScrollingStateStickyNode(ScrollingStateTree& tree, ScrollingNodeID nodeID)
    : ScrollingStateNode(ScrollingNodeType::Sticky, tree, nodeID)
{
}

ScrollingStateStickyNode::ScrollingStateStickyNode(const ScrollingStateStickyNode& node, ScrollingStateTree& adoptiveTree)
    : ScrollingStateNode(node, adoptiveTree)
    , m_constraints(StickyPositionViewportConstraints(node.viewportConstraints()))
{
    if (hasChangedProperty(Property::ViewportAnchorLayer))
        setViewportAnchorLayer(node.viewportAnchorLayer().toRepresentation(adoptiveTree.preferredLayerRepresentation()));
}

ScrollingStateStickyNode::~ScrollingStateStickyNode() = default;

Ref<ScrollingStateNode> ScrollingStateStickyNode::clone(ScrollingStateTree& adoptiveTree)
{
    return adoptRef(*new ScrollingStateStickyNode(*this, adoptiveTree));
}

OptionSet<ScrollingStateNode::Property> ScrollingStateStickyNode::applicableProperties() const
{
    static constexpr OptionSet nodeProperties = {
        Property::ViewportAnchorLayer,
        Property::ViewportConstraints,
    };

    auto properties = ScrollingStateNode::applicableProperties();
    properties.add(nodeProperties);
    return properties;
}

void ScrollingStateStickyNode::setViewportAnchorLayer(const LayerRepresentation& layer)
{
    if (layer == m_viewportAnchorLayer)
        return;

    m_viewportAnchorLayer = layer;
    setPropertyChanged(Property::ViewportAnchorLayer);
}

void ScrollingStateStickyNode::updateConstraints(const StickyPositionViewportConstraints& constraints)
{
    if (m_constraints == constraints)
        return;

    LOG_WITH_STREAM(Scrolling, stream << "ScrollingStateStickyNode " << scrollingNodeID() << " updateConstraints with constraining rect " << constraints.constrainingRectAtLastLayout() << " sticky offset " << constraints.stickyOffsetAtLastLayout() << " layer pos at last layout " << constraints.layerPositionAtLastLayout());

    m_constraints = constraints;
    setPropertyChanged(Property::ViewportConstraints);
}

FloatPoint ScrollingStateStickyNode::computeAnchorLayerPosition(const LayoutRect& viewportRect) const
{
    // This logic follows ScrollingTreeStickyNode::computeConstrainingRectAndAnchorLayerPosition().
    FloatSize offsetFromStickyAncestors;
    auto computeLayerPositionForScrollingNode = [&](ScrollingStateNode& scrollingStateNode) {
        FloatRect constrainingRect;
        if (is<ScrollingStateFrameScrollingNode>(scrollingStateNode))
            constrainingRect = viewportRect;
        else if (RefPtr overflowScrollingNode = dynamicDowncast<ScrollingStateOverflowScrollingNode>(scrollingStateNode))
            constrainingRect = FloatRect(overflowScrollingNode->scrollPosition(), m_constraints.constrainingRectAtLastLayout().size());

        constrainingRect.move(offsetFromStickyAncestors);
        return m_constraints.anchorLayerPositionForConstrainingRect(constrainingRect);
    };

    for (auto ancestor = parent(); ancestor; ancestor = ancestor->parent()) {
        if (auto* overflowProxyNode = dynamicDowncast<ScrollingStateOverflowScrollProxyNode>(*ancestor)) {
            auto overflowNode = scrollingStateTree().stateNodeForID(overflowProxyNode->overflowScrollingNode());
            if (!overflowNode)
                break;

            return computeLayerPositionForScrollingNode(*overflowNode);
        }

        if (is<ScrollingStateScrollingNode>(*ancestor))
            return computeLayerPositionForScrollingNode(*ancestor);

        if (auto* stickyNode = dynamicDowncast<ScrollingStateStickyNode>(*ancestor))
            offsetFromStickyAncestors += stickyNode->scrollDeltaSinceLastCommit(viewportRect);

        if (is<ScrollingStateFixedNode>(*ancestor)) {
            // FIXME: Do we need scrolling tree nodes at all for nested cases?
            return m_constraints.layerPositionAtLastLayout();
        }
    }
    ASSERT_NOT_REACHED();
    return m_constraints.layerPositionAtLastLayout();
}

FloatPoint ScrollingStateStickyNode::computeClippingLayerPosition(const LayoutRect& viewportRect) const
{
    if (!hasViewportClippingLayer()) {
        ASSERT_NOT_REACHED();
        return { };
    }

    return m_constraints.viewportRelativeLayerPosition(viewportRect);
}

void ScrollingStateStickyNode::reconcileLayerPositionForViewportRect(const LayoutRect& viewportRect, ScrollingLayerPositionAction action)
{
    auto updateLayerPosition = [&](const LayerRepresentation& representation, const FloatPoint& position) {
        if (!representation.representsGraphicsLayer())
            return;

        RefPtr layer = static_cast<GraphicsLayer*>(representation);
        if (!layer)
            return;

        LOG_WITH_STREAM(Compositing, stream << "ScrollingStateStickyNode " << scrollingNodeID() << " reconcileLayerPositionForViewportRect " << action << " position of layer " << layer->primaryLayerID() << " to " << position << " sticky offset " << m_constraints.stickyOffsetAtLastLayout());

        switch (action) {
        case ScrollingLayerPositionAction::Set:
            layer->setPosition(position);
            break;

        case ScrollingLayerPositionAction::SetApproximate:
            layer->setApproximatePosition(position);
            break;

        case ScrollingLayerPositionAction::Sync:
            layer->syncPosition(position);
            break;
        }
    };

    auto anchorLayerPosition = computeAnchorLayerPosition(viewportRect);
    if (hasViewportClippingLayer()) {
        auto clippingLayerPosition = computeClippingLayerPosition(viewportRect);
        updateLayerPosition(layer(), clippingLayerPosition);
        anchorLayerPosition.moveBy(-clippingLayerPosition);
    }
    updateLayerPosition(viewportAnchorLayer(), anchorLayerPosition);
}

bool ScrollingStateStickyNode::hasViewportClippingLayer() const
{
    return m_viewportAnchorLayer && layer() != m_viewportAnchorLayer;
}

FloatSize ScrollingStateStickyNode::scrollDeltaSinceLastCommit(const LayoutRect& viewportRect) const
{
    return computeAnchorLayerPosition(viewportRect) - m_constraints.anchorLayerPositionAtLastLayout();
}

void ScrollingStateStickyNode::dumpProperties(TextStream& ts, OptionSet<ScrollingStateTreeAsTextBehavior> behavior) const
{
    ts << "Sticky node"_s;
    ScrollingStateNode::dumpProperties(ts, behavior);

    if (m_constraints.anchorEdges()) {
        TextStream::GroupScope scope(ts);
        ts << "anchor edges: "_s;
        if (m_constraints.hasAnchorEdge(ViewportConstraints::AnchorEdgeLeft))
            ts << "AnchorEdgeLeft "_s;
        if (m_constraints.hasAnchorEdge(ViewportConstraints::AnchorEdgeRight))
            ts << "AnchorEdgeRight "_s;
        if (m_constraints.hasAnchorEdge(ViewportConstraints::AnchorEdgeTop))
            ts << "AnchorEdgeTop "_s;
        if (m_constraints.hasAnchorEdge(ViewportConstraints::AnchorEdgeBottom))
            ts << "AnchorEdgeBottom"_s;
    }

    if (m_constraints.hasAnchorEdge(ViewportConstraints::AnchorEdgeLeft))
        ts.dumpProperty("left offset"_s, m_constraints.leftOffset());
    if (m_constraints.hasAnchorEdge(ViewportConstraints::AnchorEdgeRight))
        ts.dumpProperty("right offset"_s, m_constraints.rightOffset());
    if (m_constraints.hasAnchorEdge(ViewportConstraints::AnchorEdgeTop))
        ts.dumpProperty("top offset"_s, m_constraints.topOffset());
    if (m_constraints.hasAnchorEdge(ViewportConstraints::AnchorEdgeBottom))
        ts.dumpProperty("bottom offset"_s, m_constraints.bottomOffset());

    ts.dumpProperty("containing block rect"_s, m_constraints.containingBlockRect());

    ts.dumpProperty("sticky box rect"_s, m_constraints.stickyBoxRect());

    ts.dumpProperty("constraining rect"_s, m_constraints.constrainingRectAtLastLayout());

    ts.dumpProperty("sticky offset at last layout"_s, m_constraints.stickyOffsetAtLastLayout());

    ts.dumpProperty("layer position at last layout"_s, m_constraints.layerPositionAtLastLayout());
}

} // namespace WebCore

#endif // ENABLE(ASYNC_SCROLLING)
