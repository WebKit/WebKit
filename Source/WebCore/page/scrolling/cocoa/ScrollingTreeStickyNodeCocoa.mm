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
#import "ScrollingTreeStickyNodeCocoa.h"

#import "Logging.h"
#import "ScrollingStateStickyNode.h"
#import "ScrollingThread.h"
#import "ScrollingTree.h"
#import "WebCoreCALayerExtras.h"
#import <wtf/TZoneMalloc.h>
#import <wtf/text/TextStream.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(ScrollingTreeStickyNodeCocoa);

Ref<ScrollingTreeStickyNodeCocoa> ScrollingTreeStickyNodeCocoa::create(ScrollingTree& scrollingTree, ScrollingNodeID nodeID)
{
    return adoptRef(*new ScrollingTreeStickyNodeCocoa(scrollingTree, nodeID));
}

ScrollingTreeStickyNodeCocoa::ScrollingTreeStickyNodeCocoa(ScrollingTree& scrollingTree, ScrollingNodeID nodeID)
    : ScrollingTreeStickyNode(scrollingTree, nodeID)
{
}

bool ScrollingTreeStickyNodeCocoa::commitStateBeforeChildren(const ScrollingStateNode& stateNode)
{
    if (stateNode.hasChangedProperty(ScrollingStateNode::Property::Layer))
        m_layer = static_cast<CALayer *>(stateNode.layer());

    if (stateNode.hasChangedProperty(ScrollingStateNode::Property::ViewportAnchorLayer)) {
        if (RefPtr stickyStateNode = dynamicDowncast<ScrollingStateStickyNode>(stateNode))
            m_viewportAnchorLayer = static_cast<CALayer *>(stickyStateNode->viewportAnchorLayer());
    }

    return ScrollingTreeStickyNode::commitStateBeforeChildren(stateNode);
}

void ScrollingTreeStickyNodeCocoa::applyLayerPositions()
{
#if ENABLE(SCROLLING_THREAD)
    if (ScrollingThread::isCurrentThread()) {
        // Match the behavior of ScrollingTreeFrameScrollingNodeMac::repositionScrollingLayers().
        if (!scrollingTree()->isScrollingSynchronizedWithMainThread()) {
            [m_layer _web_setLayerTopLeftPosition:CGPointZero];
            if (hasViewportClippingLayer())
                [m_viewportAnchorLayer _web_setLayerTopLeftPosition:CGPointZero];
        }
    }
#endif

    auto [constrainingRect, anchorLayerPosition] = computeConstrainingRectAndAnchorLayerPosition();
    LOG_WITH_STREAM(Scrolling, stream << "ScrollingTreeStickyNodeCocoa::applyLayerPositions() " << scrollingNodeID() << " constrainingRectAtLastLayout " << m_constraints.constrainingRectAtLastLayout() << " last layer pos " << m_constraints.layerPositionAtLastLayout());
    if (hasViewportClippingLayer()) {
        auto clippingLayerPosition = computeClippingLayerPosition();
        [m_layer _web_setLayerTopLeftPosition:clippingLayerPosition];
        LOG_WITH_STREAM(Scrolling, stream << "ScrollingTreeStickyNodeCocoa::applyLayerPositions() " << scrollingNodeID() << " clippingLayerPosition " << clippingLayerPosition);
        anchorLayerPosition.moveBy(-clippingLayerPosition);
    }
    LOG_WITH_STREAM(Scrolling, stream << "ScrollingTreeStickyNodeCocoa::applyLayerPositions() " << scrollingNodeID() << " clippingLayerPosition " << anchorLayerPosition);
    [m_viewportAnchorLayer _web_setLayerTopLeftPosition:anchorLayerPosition - m_constraints.alignmentOffset()];

    if (constrainingRect)
        setIsSticking(isCurrentlySticking(*constrainingRect));
}

void ScrollingTreeStickyNodeCocoa::setIsSticking(bool isSticking)
{
    if (m_isSticking == isSticking)
        return;

    if (std::exchange(m_isSticking, isSticking))
        return;

    RefPtr scrollingTree = this->scrollingTree();
    if (!scrollingTree)
        return;

    ensureOnMainRunLoop([scrollingTree = WTFMove(scrollingTree), nodeID = scrollingNodeID()] {
        scrollingTree->stickyScrollingTreeNodeBeganSticking(nodeID);
    });
}

bool ScrollingTreeStickyNodeCocoa::hasViewportClippingLayer() const
{
    return m_viewportAnchorLayer && m_layer != m_viewportAnchorLayer;
}

FloatPoint ScrollingTreeStickyNodeCocoa::layerTopLeft() const
{
    FloatRect layerBounds = [m_layer bounds];
    FloatPoint anchorPoint = [m_layer anchorPoint];
    FloatPoint position = [m_layer position];
    return position - toFloatSize(anchorPoint) * layerBounds.size() + m_constraints.alignmentOffset();
}

} // namespace WebCore
