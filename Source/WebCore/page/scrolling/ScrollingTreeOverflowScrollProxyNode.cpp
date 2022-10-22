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
#include "ScrollingTreeOverflowScrollProxyNode.h"

#if ENABLE(ASYNC_SCROLLING)

#include "ScrollingStateOverflowScrollProxyNode.h"
#include "ScrollingStateTree.h"
#include "ScrollingTree.h"
#include "ScrollingTreeOverflowScrollingNode.h"

namespace WebCore {

ScrollingTreeOverflowScrollProxyNode::ScrollingTreeOverflowScrollProxyNode(ScrollingTree& scrollingTree, ScrollingNodeID nodeID)
    : ScrollingTreeNode(scrollingTree, ScrollingNodeType::OverflowProxy, nodeID)
{
}

ScrollingTreeOverflowScrollProxyNode::~ScrollingTreeOverflowScrollProxyNode() = default;

void ScrollingTreeOverflowScrollProxyNode::commitStateBeforeChildren(const ScrollingStateNode& stateNode)
{
    const ScrollingStateOverflowScrollProxyNode& overflowProxyStateNode = downcast<ScrollingStateOverflowScrollProxyNode>(stateNode);

    if (overflowProxyStateNode.hasChangedProperty(ScrollingStateNode::Property::OverflowScrollingNode))
        m_overflowScrollingNodeID = overflowProxyStateNode.overflowScrollingNode();

    if (m_overflowScrollingNodeID) {
        auto& relatedNodes = scrollingTree().overflowRelatedNodes();
        relatedNodes.ensure(m_overflowScrollingNodeID, [] {
            return Vector<ScrollingNodeID>();
        }).iterator->value.append(scrollingNodeID());

        scrollingTree().activeOverflowScrollProxyNodes().add(*this);
    }
}

FloatSize ScrollingTreeOverflowScrollProxyNode::scrollDeltaSinceLastCommit() const
{
    if (auto* node = scrollingTree().nodeForID(m_overflowScrollingNodeID)) {
        if (is<ScrollingTreeOverflowScrollingNode>(node))
            return downcast<ScrollingTreeOverflowScrollingNode>(*node).scrollDeltaSinceLastCommit();
    }

    return { };
}

FloatPoint ScrollingTreeOverflowScrollProxyNode::computeLayerPosition() const
{
    FloatPoint scrollOffset;
    if (auto* node = scrollingTree().nodeForID(m_overflowScrollingNodeID)) {
        if (is<ScrollingTreeOverflowScrollingNode>(node)) {
            auto& overflowNode = downcast<ScrollingTreeOverflowScrollingNode>(*node);
            scrollOffset = overflowNode.currentScrollOffset();
        }
    }
    return scrollOffset;
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

#endif // ENABLE(ASYNC_SCROLLING)
