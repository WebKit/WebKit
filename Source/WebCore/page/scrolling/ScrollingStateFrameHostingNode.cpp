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
#include "ScrollingStateFrameHostingNode.h"

#if ENABLE(ASYNC_SCROLLING) || USE(COORDINATED_GRAPHICS)

#include "ScrollingStateTree.h"
#include <wtf/text/TextStream.h>

namespace WebCore {

Ref<ScrollingStateFrameHostingNode> ScrollingStateFrameHostingNode::create(ScrollingStateTree& stateTree, ScrollingNodeID nodeID)
{
    return adoptRef(*new ScrollingStateFrameHostingNode(stateTree, nodeID));
}

ScrollingStateFrameHostingNode::ScrollingStateFrameHostingNode(ScrollingStateTree& stateTree, ScrollingNodeID nodeID)
    : ScrollingStateNode(ScrollingNodeType::FrameHosting, stateTree, nodeID)
{
    ASSERT(isFrameHostingNode());
}

ScrollingStateFrameHostingNode::ScrollingStateFrameHostingNode(const ScrollingStateFrameHostingNode& stateNode, ScrollingStateTree& adoptiveTree)
    : ScrollingStateNode(stateNode, adoptiveTree)
    , m_parentRelativeScrollableRect(stateNode.parentRelativeScrollableRect())
{
}

ScrollingStateFrameHostingNode::~ScrollingStateFrameHostingNode() = default;

Ref<ScrollingStateNode> ScrollingStateFrameHostingNode::clone(ScrollingStateTree& adoptiveTree)
{
    return adoptRef(*new ScrollingStateFrameHostingNode(*this, adoptiveTree));
}

void ScrollingStateFrameHostingNode::setAllPropertiesChanged()
{
    setPropertyChangedBit(ParentRelativeScrollableRect);

    ScrollingStateNode::setAllPropertiesChanged();
}

void ScrollingStateFrameHostingNode::setParentRelativeScrollableRect(const LayoutRect& parentRelativeScrollableRect)
{
    if (m_parentRelativeScrollableRect == parentRelativeScrollableRect)
        return;

    m_parentRelativeScrollableRect = parentRelativeScrollableRect;
    setPropertyChanged(ParentRelativeScrollableRect);
}

void ScrollingStateFrameHostingNode::dumpProperties(TextStream& ts, ScrollingStateTreeAsTextBehavior behavior) const
{
    ts << "Frame hosting node";
    ScrollingStateNode::dumpProperties(ts, behavior);

    if (!m_parentRelativeScrollableRect.isEmpty())
        ts.dumpProperty("parent relative scrollable rect", m_parentRelativeScrollableRect);
}

} // namespace WebCore

#endif // ENABLE(ASYNC_SCROLLING) || USE(COORDINATED_GRAPHICS)
