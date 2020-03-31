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
#include "ScrollingTreePositionedNode.h"

#if ENABLE(ASYNC_SCROLLING) && USE(NICOSIA)

#include "Logging.h"
#include "NicosiaPlatformLayer.h"
#include "ScrollingStatePositionedNode.h"
#include "ScrollingTree.h"
#include "ScrollingTreeOverflowScrollingNode.h"
#include <wtf/text/TextStream.h>

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

    if (positionedStateNode.hasChangedProperty(ScrollingStateNode::Layer)) {
        auto* layer = static_cast<Nicosia::PlatformLayer*>(positionedStateNode.layer());
        m_layer = downcast<Nicosia::CompositionLayer>(layer);
    }

    if (positionedStateNode.hasChangedProperty(ScrollingStatePositionedNode::RelatedOverflowScrollingNodes))
        m_relatedOverflowScrollingNodes = positionedStateNode.relatedOverflowScrollingNodes();

    if (positionedStateNode.hasChangedProperty(ScrollingStatePositionedNode::LayoutConstraintData))
        m_constraints = positionedStateNode.layoutConstraints();

    if (!m_relatedOverflowScrollingNodes.isEmpty())
        scrollingTree().activePositionedNodes().add(*this);
}

void ScrollingTreePositionedNode::applyLayerPositions()
{
    FloatSize delta;
    for (auto nodeID : m_relatedOverflowScrollingNodes) {
        if (auto* node = scrollingTree().nodeForID(nodeID)) {
            if (is<ScrollingTreeOverflowScrollingNode>(node)) {
                auto& overflowNode = downcast<ScrollingTreeOverflowScrollingNode>(*node);
                delta += overflowNode.scrollDeltaSinceLastCommit();
            }
        }
    }

    FloatPoint layerPosition = m_constraints.layerPositionAtLastLayout() + delta;

    LOG_WITH_STREAM(Scrolling, stream << "ScrollingTreePositionedNode " << scrollingNodeID() << " applyLayerPositions: overflow delta " << delta << " moving layer to " << layerPosition);

    layerPosition -= m_constraints.alignmentOffset();

    ASSERT(m_layer);
    m_layer->updateState(
        [&layerPosition](Nicosia::CompositionLayer::LayerState& state)
        {
            state.position = layerPosition;
            state.delta.positionChanged = true;
        });
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

#endif // ENABLE(ASYNC_SCROLLING) && USE(NICOSIA)
