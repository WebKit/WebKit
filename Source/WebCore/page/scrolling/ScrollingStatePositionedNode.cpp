/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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
#include "ScrollingStatePositionedNode.h"

#include "GraphicsLayer.h"
#include "Logging.h"
#include "ScrollingStateTree.h"
#include <wtf/text/TextStream.h>

#if ENABLE(ASYNC_SCROLLING)

namespace WebCore {

ScrollingStatePositionedNode::ScrollingStatePositionedNode(ScrollingNodeID nodeID, Vector<Ref<ScrollingStateNode>>&& children, OptionSet<ScrollingStateNodeProperty> changedProperties, std::optional<PlatformLayerIdentifier> layerID, Vector<ScrollingNodeID>&& relatedOverflowScrollingNodes, AbsolutePositionConstraints&& constraints)
    : ScrollingStateNode(ScrollingNodeType::Positioned, nodeID, WTFMove(children), changedProperties, layerID)
    , m_relatedOverflowScrollingNodes(WTFMove(relatedOverflowScrollingNodes))
    , m_constraints(WTFMove(constraints))
{
}

ScrollingStatePositionedNode::ScrollingStatePositionedNode(ScrollingStateTree& tree, ScrollingNodeID nodeID)
    : ScrollingStateNode(ScrollingNodeType::Positioned, tree, nodeID)
{
}

ScrollingStatePositionedNode::ScrollingStatePositionedNode(const ScrollingStatePositionedNode& node, ScrollingStateTree& adoptiveTree)
    : ScrollingStateNode(node, adoptiveTree)
    , m_relatedOverflowScrollingNodes(node.relatedOverflowScrollingNodes())
    , m_constraints(node.layoutConstraints())
{
}

ScrollingStatePositionedNode::~ScrollingStatePositionedNode() = default;

Ref<ScrollingStateNode> ScrollingStatePositionedNode::clone(ScrollingStateTree& adoptiveTree)
{
    return adoptRef(*new ScrollingStatePositionedNode(*this, adoptiveTree));
}

OptionSet<ScrollingStateNode::Property> ScrollingStatePositionedNode::applicableProperties() const
{
    constexpr OptionSet<Property> nodeProperties = { Property::RelatedOverflowScrollingNodes, Property::LayoutConstraintData };

    auto properties = ScrollingStateNode::applicableProperties();
    properties.add(nodeProperties);
    return properties;
}

void ScrollingStatePositionedNode::setRelatedOverflowScrollingNodes(Vector<ScrollingNodeID>&& nodes)
{
    if (nodes == m_relatedOverflowScrollingNodes)
        return;

    m_relatedOverflowScrollingNodes = WTFMove(nodes);
    setPropertyChanged(Property::RelatedOverflowScrollingNodes);
}

void ScrollingStatePositionedNode::updateConstraints(const AbsolutePositionConstraints& constraints)
{
    if (m_constraints == constraints)
        return;

    LOG_WITH_STREAM(Scrolling, stream << "ScrollingStatePositionedNode " << scrollingNodeID() << " updateConstraints " << constraints);

    m_constraints = constraints;
    setPropertyChanged(Property::LayoutConstraintData);
}

void ScrollingStatePositionedNode::dumpProperties(TextStream& ts, OptionSet<ScrollingStateTreeAsTextBehavior> behavior) const
{
    ts << "Positioned node";
    ScrollingStateNode::dumpProperties(ts, behavior);

    ts.dumpProperty("layout constraints", m_constraints);
    ts.dumpProperty("related overflow nodes", m_relatedOverflowScrollingNodes.size());

    if (behavior & ScrollingStateTreeAsTextBehavior::IncludeNodeIDs) {
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
