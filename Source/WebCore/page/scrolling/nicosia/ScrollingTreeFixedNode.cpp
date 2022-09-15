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
#include "ScrollingTreeFixedNode.h"

#if ENABLE(ASYNC_SCROLLING) && USE(NICOSIA)

#include "Logging.h"
#include "NicosiaPlatformLayer.h"
#include "ScrollingStateFixedNode.h"
#include "ScrollingTree.h"
#include "ScrollingTreeFrameScrollingNode.h"
#include "ScrollingTreeOverflowScrollProxyNode.h"
#include "ScrollingTreeOverflowScrollingNode.h"
#include "ScrollingTreePositionedNode.h"
#include "ScrollingTreeStickyNodeNicosia.h"
#include <wtf/text/TextStream.h>

namespace WebCore {

Ref<ScrollingTreeFixedNode> ScrollingTreeFixedNode::create(ScrollingTree& scrollingTree, ScrollingNodeID nodeID)
{
    return adoptRef(*new ScrollingTreeFixedNode(scrollingTree, nodeID));
}

ScrollingTreeFixedNode::ScrollingTreeFixedNode(ScrollingTree& scrollingTree, ScrollingNodeID nodeID)
    : ScrollingTreeNode(scrollingTree, ScrollingNodeType::Fixed, nodeID)
{
    scrollingTree.fixedOrStickyNodeAdded();
}

ScrollingTreeFixedNode::~ScrollingTreeFixedNode()
{
    scrollingTree().fixedOrStickyNodeRemoved();
}

void ScrollingTreeFixedNode::commitStateBeforeChildren(const ScrollingStateNode& stateNode)
{
    auto& fixedStateNode = downcast<ScrollingStateFixedNode>(stateNode);

    if (fixedStateNode.hasChangedProperty(ScrollingStateNode::Property::Layer)) {
        auto* layer = static_cast<Nicosia::PlatformLayer*>(fixedStateNode.layer());
        m_layer = downcast<Nicosia::CompositionLayer>(layer);
    }

    if (fixedStateNode.hasChangedProperty(ScrollingStateNode::Property::ViewportConstraints))
        m_constraints = fixedStateNode.viewportConstraints();
}

void ScrollingTreeFixedNode::applyLayerPositions()
{
    auto computeLayerPosition = [&] {
        FloatSize overflowScrollDelta;
        ScrollingTreeStickyNodeNicosia* lastStickyNode = nullptr;
        for (auto* ancestor = parent(); ancestor; ancestor = ancestor->parent()) {
            if (is<ScrollingTreeFrameScrollingNode>(*ancestor)) {
                // Fixed nodes are positioned relative to the containing frame scrolling node.
                // We bail out after finding one.
                auto layoutViewport = downcast<ScrollingTreeFrameScrollingNode>(*ancestor).layoutViewport();
                return m_constraints.layerPositionForViewportRect(layoutViewport) - overflowScrollDelta;
            }

            if (is<ScrollingTreeOverflowScrollingNode>(*ancestor)) {
                // To keep the layer still during async scrolling we adjust by how much the position has changed since layout.
                auto& overflowNode = downcast<ScrollingTreeOverflowScrollingNode>(*ancestor);
                overflowScrollDelta -= overflowNode.scrollDeltaSinceLastCommit();
                continue;
            }

            if (is<ScrollingTreeOverflowScrollProxyNode>(*ancestor)) {
                // To keep the layer still during async scrolling we adjust by how much the position has changed since layout.
                auto& overflowNode = downcast<ScrollingTreeOverflowScrollProxyNode>(*ancestor);
                overflowScrollDelta -= overflowNode.scrollDeltaSinceLastCommit();
                continue;
            }

            if (is<ScrollingTreePositionedNode>(*ancestor)) {
                auto& positioningAncestor = downcast<ScrollingTreePositionedNode>(*ancestor);
                // See if sticky node already handled this positioning node.
                // FIXME: Include positioning node information to sticky/fixed node to avoid these tests.
                if (lastStickyNode && lastStickyNode->layer() == positioningAncestor.layer())
                    continue;
                if (positioningAncestor.layer() != m_layer)
                    overflowScrollDelta -= positioningAncestor.scrollDeltaSinceLastCommit();
                continue;
            }

            if (is<ScrollingTreeStickyNode>(*ancestor)) {
                auto& stickyNode = downcast<ScrollingTreeStickyNodeNicosia>(*ancestor);
                overflowScrollDelta += stickyNode.scrollDeltaSinceLastCommit();
                lastStickyNode = &stickyNode;
                continue;
            }

            if (is<ScrollingTreeFixedNode>(*ancestor)) {
                // The ancestor fixed node has already applied the needed corrections to say put.
                return m_constraints.layerPositionAtLastLayout() - overflowScrollDelta;
            }
        }
        ASSERT_NOT_REACHED();
        return FloatPoint();
    };

    auto layerPosition = computeLayerPosition();

    LOG_WITH_STREAM(Scrolling, stream << "ScrollingTreeFixedNode " << scrollingNodeID() << " relatedNodeScrollPositionDidChange: viewportRectAtLastLayout " << m_constraints.viewportRectAtLastLayout() << " last layer pos " << m_constraints.layerPositionAtLastLayout() << " layerPosition " << layerPosition);

    ASSERT(m_layer);
    m_layer->updateState(
        [&layerPosition](Nicosia::CompositionLayer::LayerState& state)
        {
            state.position = layerPosition;
            state.delta.positionChanged = true;
        });
}

void ScrollingTreeFixedNode::dumpProperties(TextStream& ts, OptionSet<ScrollingStateTreeAsTextBehavior> behavior) const
{
    ts << "fixed node";
    ScrollingTreeNode::dumpProperties(ts, behavior);
    ts.dumpProperty("fixed constraints", m_constraints);

    if (behavior & ScrollingStateTreeAsTextBehavior::IncludeLayerPositions) {
        FloatPoint layerTopLeft;
        ASSERT(m_layer);
        m_layer->accessCommitted(
            [this, &layerTopLeft](Nicosia::CompositionLayer::LayerState& state)
            {
                layerTopLeft = state.position - toFloatSize(state.anchorPoint.xy()) * state.size + m_constraints.alignmentOffset();
            });

        ts.dumpProperty("layer top left", layerTopLeft);
    }
}

} // namespace WebCore

#endif // ENABLE(ASYNC_SCROLLING) && USE(NICOSIA)
