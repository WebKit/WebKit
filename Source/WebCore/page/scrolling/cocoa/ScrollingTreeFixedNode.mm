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
#import "ScrollingTreeFixedNode.h"

#if ENABLE(ASYNC_SCROLLING)

#import "Logging.h"
#import "ScrollingStateFixedNode.h"
#import "ScrollingTree.h"
#import <QuartzCore/CALayer.h>
#import <wtf/text/TextStream.h>

namespace WebCore {

Ref<ScrollingTreeFixedNode> ScrollingTreeFixedNode::create(ScrollingTree& scrollingTree, ScrollingNodeID nodeID)
{
    return adoptRef(*new ScrollingTreeFixedNode(scrollingTree, nodeID));
}

ScrollingTreeFixedNode::ScrollingTreeFixedNode(ScrollingTree& scrollingTree, ScrollingNodeID nodeID)
    : ScrollingTreeNode(scrollingTree, ScrollingNodeType::Fixed, nodeID)
{
    scrollingTree.fixedOrStickyNodeAdded();
}

ScrollingTreeFixedNode::~ScrollingTreeFixedNode()
{
    scrollingTree().fixedOrStickyNodeRemoved();
}

void ScrollingTreeFixedNode::commitStateBeforeChildren(const ScrollingStateNode& stateNode)
{
    const ScrollingStateFixedNode& fixedStateNode = downcast<ScrollingStateFixedNode>(stateNode);

    if (fixedStateNode.hasChangedProperty(ScrollingStateNode::Layer))
        m_layer = fixedStateNode.layer();

    if (stateNode.hasChangedProperty(ScrollingStateFixedNode::ViewportConstraints))
        m_constraints = fixedStateNode.viewportConstraints();
}

namespace ScrollingTreeFixedNodeInternal {
static inline CGPoint operator*(CGPoint& a, const CGSize& b)
{
    return CGPointMake(a.x * b.width, a.y * b.height);
}
}

void ScrollingTreeFixedNode::updateLayersAfterAncestorChange(const ScrollingTreeNode& changedNode, const FloatRect& fixedPositionRect, const FloatSize& cumulativeDelta)
{
    using namespace ScrollingTreeFixedNodeInternal;
    FloatPoint layerPosition = m_constraints.layerPositionForViewportRect(fixedPositionRect);

    LOG_WITH_STREAM(Scrolling, stream << "ScrollingTreeFixedNode " << scrollingNodeID() << " updateLayersAfterAncestorChange: new viewport " << fixedPositionRect << " viewportRectAtLastLayout " << m_constraints.viewportRectAtLastLayout() << " last layer pos " << m_constraints.layerPositionAtLastLayout() << " new offset from top " << (fixedPositionRect.y() - layerPosition.y()));

    layerPosition -= cumulativeDelta;

    CGRect layerBounds = [m_layer bounds];
    CGPoint anchorPoint = [m_layer anchorPoint];
    CGPoint newPosition = layerPosition - m_constraints.alignmentOffset() + anchorPoint * layerBounds.size;

    if (isnan(newPosition.x) || isnan(newPosition.y)) {
        WTFLogAlways("Attempt to call [CALayer setPosition] with NaN: newPosition=(%f, %f) layerPosition=(%f, %f) alignmentOffset=(%f, %f)",
            newPosition.x, newPosition.y, layerPosition.x(), layerPosition.y(),
            m_constraints.alignmentOffset().width(), m_constraints.alignmentOffset().height());
        ASSERT_NOT_REACHED();
        return;
    }

    [m_layer setPosition:newPosition];

    if (!m_children)
        return;

    FloatSize newDelta = layerPosition - m_constraints.layerPositionAtLastLayout() + cumulativeDelta;

    for (auto& child : *m_children)
        child->updateLayersAfterAncestorChange(changedNode, fixedPositionRect, newDelta);
}

void ScrollingTreeFixedNode::dumpProperties(TextStream& ts, ScrollingStateTreeAsTextBehavior behavior) const
{
    ts << "fixed node";
    ScrollingTreeNode::dumpProperties(ts, behavior);
    ts.dumpProperty("fixed constraints", m_constraints);
    
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
