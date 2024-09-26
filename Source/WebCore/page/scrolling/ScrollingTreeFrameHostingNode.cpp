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
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/TextStream.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(ScrollingTreeFrameHostingNode);

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

bool ScrollingTreeFrameHostingNode::commitStateBeforeChildren(const ScrollingStateNode& stateNode)
{
    auto* state = dynamicDowncast<ScrollingStateFrameHostingNode>(stateNode);
    if (!state)
        return false;

    if (state->hasChangedProperty(ScrollingStateNode::Property::LayerHostingContextIdentifier))
        setLayerHostingContextIdentifier(state->layerHostingContextIdentifier());
    return true;
}

void ScrollingTreeFrameHostingNode::setLayerHostingContextIdentifier(std::optional<LayerHostingContextIdentifier> identifier)
{
    if (m_hostingContext != identifier)
        removeHostedChildren();
    m_hostingContext = identifier;
    if (m_hostingContext)
        scrollingTree().addScrollingNodeToHostedSubtreeMap(*m_hostingContext, *this);
}

void ScrollingTreeFrameHostingNode::removeHostedChildren()
{
    auto hostedChildren = std::exchange(m_hostedChildren, { });
    for (auto& children : hostedChildren)
        scrollingTree().removeNode(children->scrollingNodeID());
}

void ScrollingTreeFrameHostingNode::willBeDestroyed()
{
    if (m_hostingContext)
        scrollingTree().removeFrameHostingNode(*m_hostingContext);
    removeHostedChildren();
}

void ScrollingTreeFrameHostingNode::removeHostedChild(RefPtr<ScrollingTreeNode> node)
{
    if (node) {
        m_hostedChildren.remove(node);
        m_children.removeFirst(node.releaseNonNull());
    }
}

void ScrollingTreeFrameHostingNode::applyLayerPositions()
{
}

void ScrollingTreeFrameHostingNode::dumpProperties(TextStream& ts, OptionSet<ScrollingStateTreeAsTextBehavior> behavior) const
{
    ts << "frame hosting node";
    if (auto hostingContextIdentifier = m_hostingContext) {
        if (behavior & ScrollingStateTreeAsTextBehavior::IncludeNodeIDs)
            ts.dumpProperty("hosting context identifier", *m_hostingContext);
        else
            ts.dumpProperty("has hosting context identifier", "");
    }
    ScrollingTreeNode::dumpProperties(ts, behavior);
}

} // namespace WebCore

#endif // ENABLE(ASYNC_SCROLLING)
