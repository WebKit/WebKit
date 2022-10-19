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
#import "ScrollingTreeFixedNodeCocoa.h"

#if ENABLE(ASYNC_SCROLLING)

#import "Logging.h"
#import "ScrollingStateFixedNode.h"
#import "ScrollingThread.h"
#import "WebCoreCALayerExtras.h"
#import <wtf/text/TextStream.h>

namespace WebCore {

Ref<ScrollingTreeFixedNodeCocoa> ScrollingTreeFixedNodeCocoa::create(ScrollingTree& scrollingTree, ScrollingNodeID nodeID)
{
    return adoptRef(*new ScrollingTreeFixedNodeCocoa(scrollingTree, nodeID));
}

ScrollingTreeFixedNodeCocoa::ScrollingTreeFixedNodeCocoa(ScrollingTree& scrollingTree, ScrollingNodeID nodeID)
    : ScrollingTreeFixedNode(scrollingTree, nodeID)
{
}

ScrollingTreeFixedNodeCocoa::~ScrollingTreeFixedNodeCocoa() = default;

void ScrollingTreeFixedNodeCocoa::commitStateBeforeChildren(const ScrollingStateNode& stateNode)
{
    const ScrollingStateFixedNode& fixedStateNode = downcast<ScrollingStateFixedNode>(stateNode);
    if (fixedStateNode.hasChangedProperty(ScrollingStateNode::Property::Layer))
        m_layer = static_cast<CALayer*>(fixedStateNode.layer());

    ScrollingTreeFixedNode::commitStateBeforeChildren(stateNode);
}

void ScrollingTreeFixedNodeCocoa::applyLayerPositions()
{
    auto layerPosition = computeLayerPosition();

    LOG_WITH_STREAM(Scrolling, stream << "ScrollingTreeFixedNode " << scrollingNodeID() << " relatedNodeScrollPositionDidChange: viewportRectAtLastLayout " << m_constraints.viewportRectAtLastLayout() << " last layer pos " << m_constraints.layerPositionAtLastLayout() << " layerPosition " << layerPosition);

#if ENABLE(SCROLLING_THREAD)
    if (ScrollingThread::isCurrentThread()) {
        // Match the behavior of ScrollingTreeFrameScrollingNodeMac::repositionScrollingLayers().
        if (!scrollingTree().isScrollingSynchronizedWithMainThread())
            [m_layer _web_setLayerTopLeftPosition:CGPointZero];
    }
#endif

    [m_layer _web_setLayerTopLeftPosition:layerPosition - m_constraints.alignmentOffset()];
}

void ScrollingTreeFixedNodeCocoa::dumpProperties(TextStream& ts, OptionSet<ScrollingStateTreeAsTextBehavior> behavior) const
{
    ScrollingTreeFixedNode::dumpProperties(ts, behavior);

    if (behavior & ScrollingStateTreeAsTextBehavior::IncludeLayerPositions) {
        FloatRect layerBounds = [m_layer bounds];
        FloatPoint anchorPoint = [m_layer anchorPoint];
        FloatPoint position = [m_layer position];
        FloatPoint layerTopLeft = position - toFloatSize(anchorPoint) * layerBounds.size() + m_constraints.alignmentOffset();
        ts.dumpProperty("layer top left", layerTopLeft);
    }
}

} // namespace WebCore

#endif // ENABLE(ASYNC_SCROLLING)
