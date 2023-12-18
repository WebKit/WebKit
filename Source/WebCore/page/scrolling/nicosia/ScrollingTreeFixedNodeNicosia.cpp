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
#include "ScrollingTreeFixedNodeNicosia.h"

#if ENABLE(ASYNC_SCROLLING) && USE(NICOSIA)

#include "Logging.h"
#include "ScrollingStateFixedNode.h"
#include <wtf/text/TextStream.h>

namespace WebCore {

Ref<ScrollingTreeFixedNodeNicosia> ScrollingTreeFixedNodeNicosia::create(ScrollingTree& scrollingTree, ScrollingNodeID nodeID)
{
    return adoptRef(*new ScrollingTreeFixedNodeNicosia(scrollingTree, nodeID));
}

ScrollingTreeFixedNodeNicosia::ScrollingTreeFixedNodeNicosia(ScrollingTree& scrollingTree, ScrollingNodeID nodeID)
    : ScrollingTreeFixedNode(scrollingTree, nodeID)
{
}

ScrollingTreeFixedNodeNicosia::~ScrollingTreeFixedNodeNicosia() = default;

bool ScrollingTreeFixedNodeNicosia::commitStateBeforeChildren(const ScrollingStateNode& stateNode)
{
    if (!is<ScrollingStateFixedNode>(stateNode))
        return false;

    auto& fixedStateNode = downcast<ScrollingStateFixedNode>(stateNode);
    if (fixedStateNode.hasChangedProperty(ScrollingStateNode::Property::Layer)) {
        auto* layer = static_cast<Nicosia::PlatformLayer*>(fixedStateNode.layer());
        m_layer = downcast<Nicosia::CompositionLayer>(layer);
    }

    return ScrollingTreeFixedNode::commitStateBeforeChildren(stateNode);
}

void ScrollingTreeFixedNodeNicosia::applyLayerPositions()
{
    auto layerPosition = computeLayerPosition();

    LOG_WITH_STREAM(Scrolling, stream << "ScrollingTreeFixedNode " << scrollingNodeID() << " relatedNodeScrollPositionDidChange: viewportRectAtLastLayout " << m_constraints.viewportRectAtLastLayout() << " last layer pos " << m_constraints.layerPositionAtLastLayout() << " layerPosition " << layerPosition);

    ASSERT(m_layer);
    m_layer->accessPending(
        [&layerPosition](Nicosia::CompositionLayer::LayerState& state)
        {
            state.position = layerPosition;
            state.delta.positionChanged = true;
        });
}

void ScrollingTreeFixedNodeNicosia::dumpProperties(TextStream& ts, OptionSet<ScrollingStateTreeAsTextBehavior> behavior) const
{
    ScrollingTreeFixedNode::dumpProperties(ts, behavior);

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
