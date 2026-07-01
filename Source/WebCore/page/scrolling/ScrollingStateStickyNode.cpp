/*
 * Copyright (C) 2012-2026 Apple Inc. All rights reserved.
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
#include <wtf/DataRef.h>
#include <wtf/Ref.h>
#include <wtf/RefCounted.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/TextStream.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(ScrollingStateStickyNode);

ScrollingStateStickyNode::ScrollingStateStickyNode(ScrollingNodeID nodeID, Vector<Ref<ScrollingStateNode>>&& children, OptionSet<ScrollingStateNodeProperty> changedProperties, std::optional<PlatformLayerIdentifier> layerID, StickyPositionViewportConstraints&& constraints, LayerRepresentation&& viewportAnchorLayer)
    : ScrollingStateNode(ScrollingNodeType::Sticky, nodeID, WTF::move(children), changedProperties, layerID)
{
    m_staticLayoutData.access().constraints = WTF::move(constraints);
    m_staticConfigData.access().viewportAnchorLayer = WTF::move(viewportAnchorLayer);
}

ScrollingStateStickyNode::ScrollingStateStickyNode(ScrollingStateTree& tree, ScrollingNodeID nodeID)
    : ScrollingStateNode(ScrollingNodeType::Sticky, tree, nodeID)
{
}

ScrollingStateStickyNode::ScrollingStateStickyNode(const ScrollingStateStickyNode& node, ScrollingStateTree& adoptiveTree)
    : ScrollingStateNode(node, adoptiveTree)
    , m_staticLayoutData(node.m_staticLayoutData)
    , m_staticConfigData(node.m_staticConfigData)
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

bool ScrollingStateStickyNode::hasUnchangedGroupsAs(const ScrollingStateNode& other) const
{
    if (!ScrollingStateNode::hasUnchangedGroupsAs(other))
        return false;
    auto& otherSticky = downcast<ScrollingStateStickyNode>(other);
    if (m_staticLayoutData.ptr() != otherSticky.m_staticLayoutData.ptr())
        return false;
    if (m_staticConfigData.ptr() != otherSticky.m_staticConfigData.ptr())
        return false;
    return true;
}

void ScrollingStateStickyNode::clearLayerFieldsForUnchangedProperties()
{
    if (hasChangedProperty(Property::ViewportAnchorLayer) || !m_staticConfigData->viewportAnchorLayer)
        return;
    m_staticConfigData.access().viewportAnchorLayer = { };
}

#if ASSERT_ENABLED
void ScrollingStateStickyNode::verifyClearedLayerFieldsForUnchangedProperties() const
{
    ASSERT(hasChangedProperty(Property::ViewportAnchorLayer) || !m_staticConfigData->viewportAnchorLayer);
}
#endif
void ScrollingStateStickyNode::setViewportAnchorLayer(const LayerRepresentation& layer)
{
    if (layer == m_staticConfigData->viewportAnchorLayer)
        return;

    m_staticConfigData.access().viewportAnchorLayer = layer;
    setPropertyChanged(Property::ViewportAnchorLayer);
}

void ScrollingStateStickyNode::updateConstraints(const StickyPositionViewportConstraints& constraints)
{
    if (m_staticLayoutData->constraints == constraints)
        return;

    LOG_WITH_STREAM(Scrolling, stream << "ScrollingStateStickyNode " << scrollingNodeID() << " updateConstraints with constraining rect " << constraints.constrainingRectAtLastLayout() << " sticky offset " << constraints.stickyOffsetAtLastLayout() << " layer pos at last layout " << constraints.layerPositionAtLastLayout());

    m_staticLayoutData.access().constraints = constraints;
    setPropertyChanged(Property::ViewportConstraints);
}

FloatPoint ScrollingStateStickyNode::computeAnchorLayerPosition(const LayoutRect& viewportRect) const
{
    auto& constraints = m_staticLayoutData->constraints;
    // This logic follows ScrollingTreeStickyNode::computeConstrainingRectAndAnchorLayerPosition().
    FloatSize offsetFromStickyAncestors;
    auto computeLayerPositionForScrollingNode = [&](ScrollingStateNode& scrollingStateNode) {
        FloatRect constrainingRect;
        if (is<ScrollingStateFrameScrollingNode>(scrollingStateNode))
            constrainingRect = viewportRect;
        else if (auto* overflowScrollingNode = dynamicDowncast<ScrollingStateOverflowScrollingNode>(scrollingStateNode))
            constrainingRect = FloatRect(overflowScrollingNode->scrollPosition(), constraints.constrainingRectAtLastLayout().size());

        constrainingRect.move(offsetFromStickyAncestors);
        return constraints.anchorLayerPositionForConstrainingRect(constrainingRect);
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
            return constraints.layerPositionAtLastLayout();
        }
    }
    ASSERT_NOT_REACHED();
    return constraints.layerPositionAtLastLayout();
}

FloatPoint ScrollingStateStickyNode::computeClippingLayerPosition(const LayoutRect& viewportRect) const
{
    if (!hasViewportClippingLayer()) {
        ASSERT_NOT_REACHED();
        return { };
    }

    return m_staticLayoutData->constraints.viewportRelativeLayerPosition(viewportRect);
}

void ScrollingStateStickyNode::reconcileLayerPositionForViewportRect(const LayoutRect& viewportRect, ScrollingLayerPositionAction action)
{
    auto updateLayerPosition = [&](const LayerRepresentation& representation, const FloatPoint& position) {
        if (!representation.representsGraphicsLayer())
            return;

        RefPtr layer = static_cast<GraphicsLayer*>(representation);
        if (!layer)
            return;

        LOG_WITH_STREAM(Compositing, stream << "ScrollingStateStickyNode " << scrollingNodeID() << " reconcileLayerPositionForViewportRect " << action << " position of layer " << layer->primaryLayerID() << " to " << position << " sticky offset " << m_staticLayoutData->constraints.stickyOffsetAtLastLayout());

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
    auto& anchor = m_staticConfigData->viewportAnchorLayer;
    return anchor && layer() != anchor;
}

FloatSize ScrollingStateStickyNode::scrollDeltaSinceLastCommit(const LayoutRect& viewportRect) const
{
    return computeAnchorLayerPosition(viewportRect) - m_staticLayoutData->constraints.anchorLayerPositionAtLastLayout();
}

void ScrollingStateStickyNode::dumpProperties(TextStream& ts, OptionSet<ScrollingStateTreeAsTextBehavior> behavior) const
{
    ts << "Sticky node"_s;
    ScrollingStateNode::dumpProperties(ts, behavior);

    auto& constraints = m_staticLayoutData->constraints;

    if (constraints.anchorEdges()) {
        TextStream::GroupScope scope(ts);
        ts << "anchor edges: "_s;
        if (constraints.hasAnchorEdge(ViewportConstraints::AnchorEdgeLeft))
            ts << "AnchorEdgeLeft "_s;
        if (constraints.hasAnchorEdge(ViewportConstraints::AnchorEdgeRight))
            ts << "AnchorEdgeRight "_s;
        if (constraints.hasAnchorEdge(ViewportConstraints::AnchorEdgeTop))
            ts << "AnchorEdgeTop "_s;
        if (constraints.hasAnchorEdge(ViewportConstraints::AnchorEdgeBottom))
            ts << "AnchorEdgeBottom"_s;
    }

    if (constraints.hasAnchorEdge(ViewportConstraints::AnchorEdgeLeft))
        ts.dumpProperty("left offset"_s, constraints.leftOffset());
    if (constraints.hasAnchorEdge(ViewportConstraints::AnchorEdgeRight))
        ts.dumpProperty("right offset"_s, constraints.rightOffset());
    if (constraints.hasAnchorEdge(ViewportConstraints::AnchorEdgeTop))
        ts.dumpProperty("top offset"_s, constraints.topOffset());
    if (constraints.hasAnchorEdge(ViewportConstraints::AnchorEdgeBottom))
        ts.dumpProperty("bottom offset"_s, constraints.bottomOffset());

    ts.dumpProperty("containing block rect"_s, constraints.containingBlockRect());

    ts.dumpProperty("sticky box rect"_s, constraints.stickyBoxRect());

    ts.dumpProperty("constraining rect"_s, constraints.constrainingRectAtLastLayout());

    ts.dumpProperty("sticky offset at last layout"_s, constraints.stickyOffsetAtLastLayout());

    ts.dumpProperty("layer position at last layout"_s, constraints.layerPositionAtLastLayout());
}

} // namespace WebCore

#endif // ENABLE(ASYNC_SCROLLING)
