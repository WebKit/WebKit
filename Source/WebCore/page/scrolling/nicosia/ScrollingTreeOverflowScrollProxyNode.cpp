/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
 * Copyright (C) 2019 Igalia S.L.
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
#include "ScrollingTreeOverflowScrollProxyNode.h"

#if ENABLE(ASYNC_SCROLLING) && USE(NICOSIA)

#include "Logging.h"
#include "NicosiaPlatformLayer.h"
#include "ScrollingStateOverflowScrollProxyNode.h"
#include "ScrollingStateTree.h"
#include "ScrollingTree.h"
#include "ScrollingTreeOverflowScrollingNode.h"
#include <wtf/text/TextStream.h>

namespace WebCore {

Ref<ScrollingTreeOverflowScrollProxyNode> ScrollingTreeOverflowScrollProxyNode::create(ScrollingTree& scrollingTree, ScrollingNodeID nodeID)
{
    return adoptRef(*new ScrollingTreeOverflowScrollProxyNode(scrollingTree, nodeID));
}

ScrollingTreeOverflowScrollProxyNode::ScrollingTreeOverflowScrollProxyNode(ScrollingTree& scrollingTree, ScrollingNodeID nodeID)
    : ScrollingTreeNode(scrollingTree, ScrollingNodeType::OverflowProxy, nodeID)
{
}

ScrollingTreeOverflowScrollProxyNode::~ScrollingTreeOverflowScrollProxyNode() = default;

void ScrollingTreeOverflowScrollProxyNode::commitStateBeforeChildren(const ScrollingStateNode& stateNode)
{
    const ScrollingStateOverflowScrollProxyNode& overflowProxyStateNode = downcast<ScrollingStateOverflowScrollProxyNode>(stateNode);

    if (overflowProxyStateNode.hasChangedProperty(ScrollingStateNode::Property::Layer)) {
        auto* layer = static_cast<Nicosia::PlatformLayer*>(stateNode.layer());
        m_layer = downcast<Nicosia::CompositionLayer>(layer);
    }

    if (overflowProxyStateNode.hasChangedProperty(ScrollingStateNode::Property::OverflowScrollingNode))
        m_overflowScrollingNodeID = overflowProxyStateNode.overflowScrollingNode();

    if (m_overflowScrollingNodeID) {
        scrollingTree().overflowRelatedNodes().ensure(m_overflowScrollingNodeID,
            [] {
                return Vector<ScrollingNodeID>();
            }).iterator->value.append(scrollingNodeID());

        scrollingTree().activeOverflowScrollProxyNodes().add(*this);
    }
}

void ScrollingTreeOverflowScrollProxyNode::applyLayerPositions()
{
    FloatPoint scrollOffset;
    if (auto* node = scrollingTree().nodeForID(m_overflowScrollingNodeID)) {
        if (is<ScrollingTreeOverflowScrollingNode>(node)) {
            auto& overflowNode = downcast<ScrollingTreeOverflowScrollingNode>(*node);
            scrollOffset = overflowNode.currentScrollOffset();
        }
    }

    LOG_WITH_STREAM(Scrolling, stream << "ScrollingTreeOverflowScrollProxyNode " << scrollingNodeID() << " applyLayerPositions: setting bounds origin to " << scrollOffset);
    m_layer->updateState(
        [&scrollOffset](Nicosia::CompositionLayer::LayerState& state)
        {
            state.boundsOrigin = scrollOffset;
            state.delta.boundsOriginChanged = true;
        });
}

void ScrollingTreeOverflowScrollProxyNode::dumpProperties(TextStream& ts, OptionSet<ScrollingStateTreeAsTextBehavior> behavior) const
{
    ts << "overflow scroll proxy node";
    ScrollingTreeNode::dumpProperties(ts, behavior);

    if (auto* relatedOverflowNode = scrollingTree().nodeForID(m_overflowScrollingNodeID)) {
        auto scrollPosition = downcast<ScrollingTreeOverflowScrollingNode>(*relatedOverflowNode).currentScrollPosition();
        ts.dumpProperty("related overflow scrolling node scroll position", scrollPosition);
    }

    if (behavior & ScrollingStateTreeAsTextBehavior::IncludeNodeIDs)
        ts.dumpProperty("overflow scrolling node", m_overflowScrollingNodeID);
}

} // namespace WebCore

#endif // ENABLE(ASYNC_SCROLLING) && USE(NICOSIA)
