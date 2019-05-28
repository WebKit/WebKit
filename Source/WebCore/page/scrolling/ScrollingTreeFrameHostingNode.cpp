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
#include "ScrollingTreeFrameHostingNode.h"

#if ENABLE(ASYNC_SCROLLING)

#include "Logging.h"
#include "ScrollingStateFrameHostingNode.h"
#include "ScrollingStateTree.h"
#include "ScrollingTree.h"
#include <wtf/text/TextStream.h>

namespace WebCore {

Ref<ScrollingTreeFrameHostingNode> ScrollingTreeFrameHostingNode::create(ScrollingTree& scrollingTree, ScrollingNodeID nodeID)
{
    return adoptRef(*new ScrollingTreeFrameHostingNode(scrollingTree, nodeID));
}

ScrollingTreeFrameHostingNode::ScrollingTreeFrameHostingNode(ScrollingTree& scrollingTree, ScrollingNodeID nodeID)
    : ScrollingTreeNode(scrollingTree, ScrollingNodeType::FrameHosting, nodeID)
{
    ASSERT(isFrameHostingNode());
}

ScrollingTreeFrameHostingNode::~ScrollingTreeFrameHostingNode() = default;

void ScrollingTreeFrameHostingNode::commitStateBeforeChildren(const ScrollingStateNode& stateNode)
{
    const ScrollingStateFrameHostingNode& frameHostingStateNode = downcast<ScrollingStateFrameHostingNode>(stateNode);

    if (frameHostingStateNode.hasChangedProperty(ScrollingStateFrameHostingNode::ParentRelativeScrollableRect))
        m_parentRelativeScrollableRect = frameHostingStateNode.parentRelativeScrollableRect();
}

void ScrollingTreeFrameHostingNode::applyLayerPositions()
{
}

LayoutPoint ScrollingTreeFrameHostingNode::parentToLocalPoint(LayoutPoint point) const
{
    return point - toLayoutSize(parentRelativeScrollableRect().location());
}

void ScrollingTreeFrameHostingNode::dumpProperties(TextStream& ts, ScrollingStateTreeAsTextBehavior behavior) const
{
    ts << "frame hosting node";
    ScrollingTreeNode::dumpProperties(ts, behavior);

    if (!m_parentRelativeScrollableRect.isEmpty())
        ts.dumpProperty("parent relative scrollable rect", m_parentRelativeScrollableRect);
}

} // namespace WebCore

#endif // ENABLE(ASYNC_SCROLLING)
