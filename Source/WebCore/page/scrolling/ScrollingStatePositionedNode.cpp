/*
 * Copyright (C) 2019-2026 Apple Inc. All rights reserved.
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
#include <wtf/DataRef.h>
#include <wtf/Ref.h>
#include <wtf/RefCounted.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/TextStream.h>

#if ENABLE(ASYNC_SCROLLING)

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(ScrollingStatePositionedNode);

ScrollingStatePositionedNode::ScrollingStatePositionedNode(ScrollingNodeID nodeID, Vector<Ref<ScrollingStateNode>>&& children, OptionSet<ScrollingStateNodeProperty> changedProperties, std::optional<PlatformLayerIdentifier> layerID, Vector<ScrollingNodeID>&& relatedOverflowScrollingNodes, AbsolutePositionConstraints&& constraints)
    : ScrollingStateNode(ScrollingNodeType::Positioned, nodeID, WTF::move(children), changedProperties, layerID)
{
    SUPPRESS_UNCOUNTED_LOCAL auto& layout = m_staticLayoutData.access();
    layout.relatedOverflowScrollingNodes = WTF::move(relatedOverflowScrollingNodes);
    layout.constraints = WTF::move(constraints);
}

ScrollingStatePositionedNode::ScrollingStatePositionedNode(ScrollingStateTree& tree, ScrollingNodeID nodeID)
    : ScrollingStateNode(ScrollingNodeType::Positioned, tree, nodeID)
{
}

ScrollingStatePositionedNode::ScrollingStatePositionedNode(const ScrollingStatePositionedNode& node, ScrollingStateTree& adoptiveTree)
    : ScrollingStateNode(node, adoptiveTree)
    , m_staticLayoutData(node.m_staticLayoutData)
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

bool ScrollingStatePositionedNode::hasUnchangedGroupsAs(const ScrollingStateNode& other) const
{
    if (!ScrollingStateNode::hasUnchangedGroupsAs(other))
        return false;
    auto& otherPositioned = downcast<ScrollingStatePositionedNode>(other);
    return m_staticLayoutData.ptr() == otherPositioned.m_staticLayoutData.ptr();
}

void ScrollingStatePositionedNode::setRelatedOverflowScrollingNodes(Vector<ScrollingNodeID>&& nodes)
{
    if (nodes == m_staticLayoutData->relatedOverflowScrollingNodes)
        return;

    m_staticLayoutData.access().relatedOverflowScrollingNodes = WTF::move(nodes);
    setPropertyChanged(Property::RelatedOverflowScrollingNodes);
}

void ScrollingStatePositionedNode::updateConstraints(const AbsolutePositionConstraints& constraints)
{
    if (m_staticLayoutData->constraints == constraints)
        return;

    LOG_WITH_STREAM(Scrolling, stream << "ScrollingStatePositionedNode " << scrollingNodeID() << " updateConstraints " << constraints);

    m_staticLayoutData.access().constraints = constraints;
    setPropertyChanged(Property::LayoutConstraintData);
}

void ScrollingStatePositionedNode::dumpProperties(TextStream& ts, OptionSet<ScrollingStateTreeAsTextBehavior> behavior) const
{
    ts << "Positioned node"_s;
    ScrollingStateNode::dumpProperties(ts, behavior);

    SUPPRESS_UNCOUNTED_LOCAL auto& layout = m_staticLayoutData.get();
    ts.dumpProperty("layout constraints"_s, layout.constraints);
    ts.dumpProperty("related overflow nodes"_s, layout.relatedOverflowScrollingNodes.size());

    if (behavior & ScrollingStateTreeAsTextBehavior::IncludeNodeIDs) {
        if (!layout.relatedOverflowScrollingNodes.isEmpty()) {
            TextStream::GroupScope scope(ts);
            ts << "overflow nodes"_s;
            for (auto nodeID : layout.relatedOverflowScrollingNodes)
                ts << '\n' << indent << "nodeID "_s << nodeID;
        }
    }
}

} // namespace WebCore

#endif // ENABLE(ASYNC_SCROLLING)
