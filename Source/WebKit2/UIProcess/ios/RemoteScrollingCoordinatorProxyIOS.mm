/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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
#import "RemoteScrollingCoordinatorProxy.h"

#if PLATFORM(IOS)
#if ENABLE(ASYNC_SCROLLING)

#import "LayerRepresentation.h"
#import "RemoteLayerTreeHost.h"
#import "WebPageProxy.h"
#import <UIKit/UIView.h>
#import <WebCore/ScrollingStateFrameScrollingNode.h>
#import <WebCore/ScrollingStateOverflowScrollingNode.h>
#import <WebCore/ScrollingStateTree.h>

#if ENABLE(CSS_SCROLL_SNAP)
#import <WebCore/AxisScrollSnapOffsets.h>
#import <WebCore/ScrollTypes.h>
#import <WebCore/ScrollingTreeFrameScrollingNode.h>
#endif

using namespace WebCore;

namespace WebKit {

static LayerRepresentation layerRepresentationFromLayerOrView(LayerOrView *layerOrView)
{
    return LayerRepresentation(layerOrView.layer);
}

void RemoteScrollingCoordinatorProxy::connectStateNodeLayers(ScrollingStateTree& stateTree, const RemoteLayerTreeHost& layerTreeHost)
{
    for (auto& currNode : stateTree.nodeMap().values()) {
        switch (currNode->nodeType()) {
        case OverflowScrollingNode: {
            ScrollingStateOverflowScrollingNode& scrollingStateNode = downcast<ScrollingStateOverflowScrollingNode>(*currNode);
            
            if (scrollingStateNode.hasChangedProperty(ScrollingStateNode::ScrollLayer))
                scrollingStateNode.setLayer(layerRepresentationFromLayerOrView(layerTreeHost.getLayer(scrollingStateNode.layer())));
            
            if (scrollingStateNode.hasChangedProperty(ScrollingStateOverflowScrollingNode::ScrolledContentsLayer))
                scrollingStateNode.setScrolledContentsLayer(layerRepresentationFromLayerOrView(layerTreeHost.getLayer(scrollingStateNode.scrolledContentsLayer())));
            break;
        };
        case FrameScrollingNode: {
            ScrollingStateFrameScrollingNode& scrollingStateNode = downcast<ScrollingStateFrameScrollingNode>(*currNode);
            
            if (scrollingStateNode.hasChangedProperty(ScrollingStateNode::ScrollLayer))
                scrollingStateNode.setLayer(layerRepresentationFromLayerOrView(layerTreeHost.getLayer(scrollingStateNode.layer())));

            if (scrollingStateNode.hasChangedProperty(ScrollingStateFrameScrollingNode::CounterScrollingLayer))
                scrollingStateNode.setCounterScrollingLayer(layerRepresentationFromLayerOrView(layerTreeHost.getLayer(scrollingStateNode.counterScrollingLayer())));

            // FIXME: we should never have header and footer layers coming from the WebProcess.
            if (scrollingStateNode.hasChangedProperty(ScrollingStateFrameScrollingNode::HeaderLayer))
                scrollingStateNode.setHeaderLayer(layerRepresentationFromLayerOrView(layerTreeHost.getLayer(scrollingStateNode.headerLayer())));

            if (scrollingStateNode.hasChangedProperty(ScrollingStateFrameScrollingNode::FooterLayer))
                scrollingStateNode.setFooterLayer(layerRepresentationFromLayerOrView(layerTreeHost.getLayer(scrollingStateNode.footerLayer())));
            break;
        }
        case FixedNode:
        case StickyNode:
            if (currNode->hasChangedProperty(ScrollingStateNode::ScrollLayer))
                currNode->setLayer(layerRepresentationFromLayerOrView(layerTreeHost.getLayer(currNode->layer())));
            break;
        }
    }
}

FloatRect RemoteScrollingCoordinatorProxy::customFixedPositionRect() const
{
    return m_webPageProxy.computeCustomFixedPositionRect(m_webPageProxy.unobscuredContentRect(), m_webPageProxy.displayedContentScale());
}

void RemoteScrollingCoordinatorProxy::scrollingTreeNodeWillStartPanGesture()
{
    m_webPageProxy.overflowScrollViewWillStartPanGesture();
}

void RemoteScrollingCoordinatorProxy::scrollingTreeNodeWillStartScroll()
{
    m_webPageProxy.overflowScrollWillStartScroll();
}
    
void RemoteScrollingCoordinatorProxy::scrollingTreeNodeDidEndScroll()
{
    m_webPageProxy.overflowScrollDidEndScroll();
}

#if ENABLE(CSS_SCROLL_SNAP)
void RemoteScrollingCoordinatorProxy::adjustTargetContentOffsetForSnapping(CGSize maxScrollOffsets, CGPoint velocity, CGPoint* targetContentOffset) const
{
    // The bounds checking with maxScrollOffsets is to ensure that we won't interfere with rubber-banding when scrolling to the edge of the page.
    if (shouldSnapForMainFrameScrolling(WebCore::ScrollEventAxis::Horizontal) && targetContentOffset->x > 0 && targetContentOffset->x < maxScrollOffsets.width) {
        float potentialSnapPosition = closestSnapOffsetForMainFrameScrolling(WebCore::ScrollEventAxis::Horizontal, targetContentOffset->x, velocity.x);
        targetContentOffset->x = std::min<float>(maxScrollOffsets.width, potentialSnapPosition);
    }
    // FIXME: We need to account for how the top navigation bar changes in size.
    if (shouldSnapForMainFrameScrolling(WebCore::ScrollEventAxis::Vertical) && targetContentOffset->y > 0 && targetContentOffset->y < maxScrollOffsets.height) {
        float potentialSnapPosition = closestSnapOffsetForMainFrameScrolling(WebCore::ScrollEventAxis::Vertical, targetContentOffset->y, velocity.y);
        targetContentOffset->y = std::min<float>(maxScrollOffsets.height, potentialSnapPosition);
    }
}

bool RemoteScrollingCoordinatorProxy::shouldSetScrollViewDecelerationRateFast() const
{
    return shouldSnapForMainFrameScrolling(ScrollEventAxis::Horizontal) || shouldSnapForMainFrameScrolling(ScrollEventAxis::Vertical);
}

bool RemoteScrollingCoordinatorProxy::shouldSnapForMainFrameScrolling(ScrollEventAxis axis) const
{
    ScrollingTreeNode* root = m_scrollingTree->rootNode();
    if (root && root->isFrameScrollingNode()) {
        ScrollingTreeFrameScrollingNode* rootFrame = static_cast<ScrollingTreeFrameScrollingNode*>(root);
        const Vector<float>& snapOffsets = axis == ScrollEventAxis::Horizontal ? rootFrame->horizontalSnapOffsets() : rootFrame->verticalSnapOffsets();
        return snapOffsets.size() > 0;
    }
    return false;
}

float RemoteScrollingCoordinatorProxy::closestSnapOffsetForMainFrameScrolling(ScrollEventAxis axis, float scrollDestination, float velocity) const
{
    ScrollingTreeNode* root = m_scrollingTree->rootNode();
    ASSERT(root && root->isFrameScrollingNode());
    ScrollingTreeFrameScrollingNode* rootFrame = static_cast<ScrollingTreeFrameScrollingNode*>(root);
    const Vector<float>& snapOffsets = axis == ScrollEventAxis::Horizontal ? rootFrame->horizontalSnapOffsets() : rootFrame->verticalSnapOffsets();

    float scaledScrollDestination = scrollDestination / m_webPageProxy.displayedContentScale();
    float rawClosestSnapOffset = closestSnapOffset<float, float>(snapOffsets, scaledScrollDestination, velocity);
    return rawClosestSnapOffset * m_webPageProxy.displayedContentScale();
}
#endif

} // namespace WebKit


#endif // ENABLE(ASYNC_SCROLLING)
#endif // PLATFORM(IOS)
