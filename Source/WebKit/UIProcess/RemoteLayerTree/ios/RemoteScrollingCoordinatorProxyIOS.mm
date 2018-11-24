/*
 * Copyright (C) 2014-2015 Apple Inc. All rights reserved.
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

#if PLATFORM(IOS_FAMILY)
#if ENABLE(ASYNC_SCROLLING)

#import "RemoteLayerTreeHost.h"
#import "RemoteLayerTreeNode.h"
#import "WebPageProxy.h"
#import <UIKit/UIView.h>
#import <WebCore/ScrollingStateFrameScrollingNode.h>
#import <WebCore/ScrollingStateOverflowScrollingNode.h>
#import <WebCore/ScrollingStateTree.h>

#if ENABLE(CSS_SCROLL_SNAP)
#import <WebCore/AxisScrollSnapOffsets.h>
#import <WebCore/ScrollSnapOffsetsInfo.h>
#import <WebCore/ScrollTypes.h>
#import <WebCore/ScrollingTreeFrameScrollingNode.h>
#endif

namespace WebKit {
using namespace WebCore;

void RemoteScrollingCoordinatorProxy::connectStateNodeLayers(ScrollingStateTree& stateTree, const RemoteLayerTreeHost& layerTreeHost)
{
    for (auto& currNode : stateTree.nodeMap().values()) {
        switch (currNode->nodeType()) {
        case OverflowScrollingNode: {
            ScrollingStateOverflowScrollingNode& scrollingStateNode = downcast<ScrollingStateOverflowScrollingNode>(*currNode);
            
            if (scrollingStateNode.hasChangedProperty(ScrollingStateNode::ScrollLayer))
                scrollingStateNode.setLayer(layerTreeHost.layerForID(scrollingStateNode.layer()));
            
            if (scrollingStateNode.hasChangedProperty(ScrollingStateScrollingNode::ScrolledContentsLayer))
                scrollingStateNode.setScrolledContentsLayer(layerTreeHost.layerForID(scrollingStateNode.scrolledContentsLayer()));
            break;
        };
        case MainFrameScrollingNode:
        case SubframeScrollingNode: {
            ScrollingStateFrameScrollingNode& scrollingStateNode = downcast<ScrollingStateFrameScrollingNode>(*currNode);
            
            if (scrollingStateNode.hasChangedProperty(ScrollingStateNode::ScrollLayer))
                scrollingStateNode.setLayer(layerTreeHost.layerForID(scrollingStateNode.layer()));

            if (scrollingStateNode.hasChangedProperty(ScrollingStateFrameScrollingNode::CounterScrollingLayer))
                scrollingStateNode.setCounterScrollingLayer(layerTreeHost.layerForID(scrollingStateNode.counterScrollingLayer()));

            // FIXME: we should never have header and footer layers coming from the WebProcess.
            if (scrollingStateNode.hasChangedProperty(ScrollingStateFrameScrollingNode::HeaderLayer))
                scrollingStateNode.setHeaderLayer(layerTreeHost.layerForID(scrollingStateNode.headerLayer()));

            if (scrollingStateNode.hasChangedProperty(ScrollingStateFrameScrollingNode::FooterLayer))
                scrollingStateNode.setFooterLayer(layerTreeHost.layerForID(scrollingStateNode.footerLayer()));
            break;
        }
        case FixedNode:
        case StickyNode:
            if (currNode->hasChangedProperty(ScrollingStateNode::ScrollLayer))
                currNode->setLayer(layerTreeHost.layerForID(currNode->layer()));
            break;
        }
    }
}

FloatRect RemoteScrollingCoordinatorProxy::customFixedPositionRect() const
{
    return m_webPageProxy.computeCustomFixedPositionRect(m_webPageProxy.unobscuredContentRect(), m_webPageProxy.unobscuredContentRectRespectingInputViewBounds(), m_webPageProxy.customFixedPositionRect(),
        m_webPageProxy.displayedContentScale(), FrameView::LayoutViewportConstraint::Unconstrained, visualViewportEnabled());
}

void RemoteScrollingCoordinatorProxy::scrollingTreeNodeWillStartPanGesture()
{
    m_webPageProxy.scrollingNodeScrollViewWillStartPanGesture();
}

void RemoteScrollingCoordinatorProxy::scrollingTreeNodeWillStartScroll()
{
    m_webPageProxy.scrollingNodeScrollWillStartScroll();
}
    
void RemoteScrollingCoordinatorProxy::scrollingTreeNodeDidEndScroll()
{
    m_webPageProxy.scrollingNodeScrollDidEndScroll();
}

#if ENABLE(CSS_SCROLL_SNAP)
void RemoteScrollingCoordinatorProxy::adjustTargetContentOffsetForSnapping(CGSize maxScrollOffsets, CGPoint velocity, CGFloat topInset, CGPoint* targetContentOffset)
{
    // The bounds checking with maxScrollOffsets is to ensure that we won't interfere with rubber-banding when scrolling to the edge of the page.
    if (shouldSnapForMainFrameScrolling(WebCore::ScrollEventAxis::Horizontal)) {
        float potentialSnapPosition = closestSnapOffsetForMainFrameScrolling(WebCore::ScrollEventAxis::Horizontal, targetContentOffset->x, velocity.x, m_currentHorizontalSnapPointIndex);
        if (targetContentOffset->x > 0 && targetContentOffset->x < maxScrollOffsets.width)
            targetContentOffset->x = std::min<float>(maxScrollOffsets.width, potentialSnapPosition);
    }

    if (shouldSnapForMainFrameScrolling(WebCore::ScrollEventAxis::Vertical)) {
        float potentialSnapPosition = closestSnapOffsetForMainFrameScrolling(WebCore::ScrollEventAxis::Vertical, targetContentOffset->y, velocity.y, m_currentVerticalSnapPointIndex);
        if (m_currentVerticalSnapPointIndex != invalidSnapOffsetIndex)
            potentialSnapPosition -= topInset;

        if (targetContentOffset->y > 0 && targetContentOffset->y < maxScrollOffsets.height)
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
        unsigned currentIndex = axis == ScrollEventAxis::Horizontal ? m_currentHorizontalSnapPointIndex : m_currentVerticalSnapPointIndex;
        return snapOffsets.size() && (currentIndex < snapOffsets.size() || currentIndex == invalidSnapOffsetIndex);
    }
    return false;
}

float RemoteScrollingCoordinatorProxy::closestSnapOffsetForMainFrameScrolling(ScrollEventAxis axis, float scrollDestination, float velocity, unsigned& currentIndex) const
{
    ScrollingTreeNode* root = m_scrollingTree->rootNode();
    ASSERT(root && root->isFrameScrollingNode());
    ScrollingTreeFrameScrollingNode* rootFrame = static_cast<ScrollingTreeFrameScrollingNode*>(root);
    const Vector<float>& snapOffsets = axis == ScrollEventAxis::Horizontal ? rootFrame->horizontalSnapOffsets() : rootFrame->verticalSnapOffsets();
    const Vector<ScrollOffsetRange<float>>& snapOffsetRanges = axis == ScrollEventAxis::Horizontal ? rootFrame->horizontalSnapOffsetRanges() : rootFrame->verticalSnapOffsetRanges();

    float scaledScrollDestination = scrollDestination / m_webPageProxy.displayedContentScale();
    float rawClosestSnapOffset = closestSnapOffset(snapOffsets, snapOffsetRanges, scaledScrollDestination, velocity, currentIndex);
    return rawClosestSnapOffset * m_webPageProxy.displayedContentScale();
}

bool RemoteScrollingCoordinatorProxy::hasActiveSnapPoint() const
{
    ScrollingTreeNode* root = m_scrollingTree->rootNode();
    if (!root)
        return false;

    if (!is<ScrollingTreeFrameScrollingNode>(root))
        return false;

    ScrollingTreeFrameScrollingNode& rootFrame = downcast<ScrollingTreeFrameScrollingNode>(*root);
    const Vector<float>& horizontal = rootFrame.horizontalSnapOffsets();
    const Vector<float>& vertical = rootFrame.verticalSnapOffsets();

    if (horizontal.isEmpty() && vertical.isEmpty())
        return false;

    if ((!horizontal.isEmpty() && m_currentHorizontalSnapPointIndex >= horizontal.size())
        || (!vertical.isEmpty() && m_currentVerticalSnapPointIndex >= vertical.size())) {
        return false;
    }
    
    return true;
}
    
CGPoint RemoteScrollingCoordinatorProxy::nearestActiveContentInsetAdjustedSnapPoint(CGFloat topInset, const CGPoint& currentPoint) const
{
    CGPoint activePoint = currentPoint;

    ScrollingTreeNode* root = m_scrollingTree->rootNode();
    ASSERT(root && is<ScrollingTreeFrameScrollingNode>(root));
    ScrollingTreeFrameScrollingNode& rootFrame = downcast<ScrollingTreeFrameScrollingNode>(*root);
    const Vector<float>& horizontal = rootFrame.horizontalSnapOffsets();
    const Vector<float>& vertical = rootFrame.verticalSnapOffsets();

    // The bounds checking with maxScrollOffsets is to ensure that we won't interfere with rubber-banding when scrolling to the edge of the page.
    if (!horizontal.isEmpty() && m_currentHorizontalSnapPointIndex < horizontal.size())
        activePoint.x = horizontal[m_currentHorizontalSnapPointIndex] * m_webPageProxy.displayedContentScale();

    if (!vertical.isEmpty() && m_currentVerticalSnapPointIndex < vertical.size()) {
        float potentialSnapPosition = vertical[m_currentVerticalSnapPointIndex] * m_webPageProxy.displayedContentScale();
        potentialSnapPosition -= topInset;
        activePoint.y = potentialSnapPosition;
    }

    return activePoint;
}

#endif

} // namespace WebKit


#endif // ENABLE(ASYNC_SCROLLING)
#endif // PLATFORM(IOS_FAMILY)
