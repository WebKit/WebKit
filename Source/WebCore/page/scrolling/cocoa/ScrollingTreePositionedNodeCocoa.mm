/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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

#import "config.h"
#import "ScrollingTreePositionedNodeCocoa.h"

#if ENABLE(ASYNC_SCROLLING)

#import "Logging.h"
#import "ScrollingStatePositionedNode.h"
#import "WebCoreCALayerExtras.h"

namespace WebCore {

Ref<ScrollingTreePositionedNodeCocoa> ScrollingTreePositionedNodeCocoa::create(ScrollingTree& scrollingTree, ScrollingNodeID nodeID)
{
    return adoptRef(*new ScrollingTreePositionedNodeCocoa(scrollingTree, nodeID));
}

ScrollingTreePositionedNodeCocoa::ScrollingTreePositionedNodeCocoa(ScrollingTree& scrollingTree, ScrollingNodeID nodeID)
    : ScrollingTreePositionedNode(scrollingTree, nodeID)
{
}

ScrollingTreePositionedNodeCocoa::~ScrollingTreePositionedNodeCocoa() = default;

void ScrollingTreePositionedNodeCocoa::commitStateBeforeChildren(const ScrollingStateNode& stateNode)
{
    const ScrollingStatePositionedNode& positionedStateNode = downcast<ScrollingStatePositionedNode>(stateNode);
    if (positionedStateNode.hasChangedProperty(ScrollingStateNode::Property::Layer))
        m_layer = static_cast<CALayer*>(positionedStateNode.layer());

    ScrollingTreePositionedNode::commitStateBeforeChildren(stateNode);
}

void ScrollingTreePositionedNodeCocoa::applyLayerPositions()
{
    auto delta = scrollDeltaSinceLastCommit();
    auto layerPosition = m_constraints.layerPositionAtLastLayout() - delta;

    LOG_WITH_STREAM(Scrolling, stream << "ScrollingTreePositionedNode " << scrollingNodeID() << " applyLayerPositions: overflow delta " << delta << " moving layer to " << layerPosition);

    [m_layer _web_setLayerTopLeftPosition:layerPosition - m_constraints.alignmentOffset()];
}

} // namespace WebCore

#endif // ENABLE(ASYNC_SCROLLING)
