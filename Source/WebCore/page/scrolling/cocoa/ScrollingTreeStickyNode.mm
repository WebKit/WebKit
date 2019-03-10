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
#import "ScrollingTreeStickyNode.h"

#if ENABLE(ASYNC_SCROLLING)

#import "Logging.h"
#import "ScrollingStateStickyNode.h"
#import "ScrollingTree.h"
#import "ScrollingTreeFrameScrollingNode.h"
#import "ScrollingTreeOverflowScrollingNode.h"
#import "WebCoreCALayerExtras.h"
#import <wtf/text/TextStream.h>

namespace WebCore {

Ref<ScrollingTreeStickyNode> ScrollingTreeStickyNode::create(ScrollingTree& scrollingTree, ScrollingNodeID nodeID)
{
    return adoptRef(*new ScrollingTreeStickyNode(scrollingTree, nodeID));
}

ScrollingTreeStickyNode::ScrollingTreeStickyNode(ScrollingTree& scrollingTree, ScrollingNodeID nodeID)
    : ScrollingTreeNode(scrollingTree, ScrollingNodeType::Sticky, nodeID)
{
    scrollingTree.fixedOrStickyNodeAdded();
}

ScrollingTreeStickyNode::~ScrollingTreeStickyNode()
{
    scrollingTree().fixedOrStickyNodeRemoved();
}

void ScrollingTreeStickyNode::commitStateBeforeChildren(const ScrollingStateNode& stateNode)
{
    const ScrollingStateStickyNode& stickyStateNode = downcast<ScrollingStateStickyNode>(stateNode);

    if (stickyStateNode.hasChangedProperty(ScrollingStateNode::Layer))
        m_layer = stickyStateNode.layer();

    if (stateNode.hasChangedProperty(ScrollingStateStickyNode::ViewportConstraints))
        m_constraints = stickyStateNode.viewportConstraints();
}

void ScrollingTreeStickyNode::applyLayerPositions(const FloatRect& layoutViewport, FloatSize& cumulativeDelta)
{
    FloatRect constrainingRect;

    auto* enclosingScrollingNode = enclosingScrollingNodeIncludingSelf();
    if (is<ScrollingTreeOverflowScrollingNode>(enclosingScrollingNode))
        constrainingRect = FloatRect(downcast<ScrollingTreeOverflowScrollingNode>(*enclosingScrollingNode).currentScrollPosition(), m_constraints.constrainingRectAtLastLayout().size());
    else if (is<ScrollingTreeFrameScrollingNode>(enclosingScrollingNode))
        constrainingRect = layoutViewport;
    else
        return;

    LOG_WITH_STREAM(Scrolling, stream << "ScrollingTreeStickyNode " << scrollingNodeID() << " relatedNodeScrollPositionDidChange: new viewport " << layoutViewport << " constrainingRectAtLastLayout " << m_constraints.constrainingRectAtLastLayout() << " last layer pos " << m_constraints.layerPositionAtLastLayout());

    FloatPoint layerPosition = m_constraints.layerPositionForConstrainingRect(constrainingRect) - m_constraints.alignmentOffset();
    [m_layer _web_setLayerTopLeftPosition:layerPosition];

    cumulativeDelta += layerPosition - m_constraints.layerPositionAtLastLayout();
}

void ScrollingTreeStickyNode::dumpProperties(TextStream& ts, ScrollingStateTreeAsTextBehavior behavior) const
{
    ts << "sticky node";

    ScrollingTreeNode::dumpProperties(ts, behavior);
    ts.dumpProperty("sticky constraints", m_constraints);

    if (behavior & ScrollingStateTreeAsTextBehaviorIncludeLayerPositions) {
        FloatRect layerBounds = [m_layer bounds];
        FloatPoint anchorPoint = [m_layer anchorPoint];
        FloatPoint position = [m_layer position];
        FloatPoint layerTopLeft = position - toFloatSize(anchorPoint) * layerBounds.size() + m_constraints.alignmentOffset();
        ts.dumpProperty("layer top left", layerTopLeft);
    }
}

} // namespace WebCore

#endif // ENABLE(ASYNC_SCROLLING)
