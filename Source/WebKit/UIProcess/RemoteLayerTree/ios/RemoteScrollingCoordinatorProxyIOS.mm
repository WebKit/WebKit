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
#import "ScrollingTreeOverflowScrollingNodeIOS.h"
#import "WebPageProxy.h"
#import <UIKit/UIView.h>
#import <WebCore/ScrollingStateFrameScrollingNode.h>
#import <WebCore/ScrollingStateOverflowScrollProxyNode.h>
#import <WebCore/ScrollingStateOverflowScrollingNode.h>
#import <WebCore/ScrollingStatePositionedNode.h>
#import <WebCore/ScrollingStateTree.h>

#if ENABLE(CSS_SCROLL_SNAP)
#import <WebCore/AxisScrollSnapOffsets.h>
#import <WebCore/ScrollSnapOffsetsInfo.h>
#import <WebCore/ScrollTypes.h>
#import <WebCore/ScrollingTreeFrameScrollingNode.h>
#import <WebCore/ScrollingTreeOverflowScrollProxyNode.h>
#import <WebCore/ScrollingTreeOverflowScrollingNode.h>
#import <WebCore/ScrollingTreePositionedNode.h>
#endif

namespace WebKit {
using namespace WebCore;

UIScrollView *RemoteScrollingCoordinatorProxy::scrollViewForScrollingNodeID(ScrollingNodeID nodeID) const
{
    auto* treeNode = m_scrollingTree->nodeForID(nodeID);
    if (!is<ScrollingTreeOverflowScrollingNode>(treeNode))
        return nil;

    auto* scrollingNode = downcast<ScrollingTreeOverflowScrollingNode>(treeNode);
    // All ScrollingTreeOverflowScrollingNodes are ScrollingTreeOverflowScrollingNodeIOS on iOS.
    return static_cast<ScrollingTreeOverflowScrollingNodeIOS*>(scrollingNode)->scrollView();
}

void RemoteScrollingCoordinatorProxy::connectStateNodeLayers(ScrollingStateTree& stateTree, const RemoteLayerTreeHost& layerTreeHost)
{
    using PlatformLayerID = GraphicsLayer::PlatformLayerID;

    for (auto& currNode : stateTree.nodeMap().values()) {
        if (currNode->hasChangedProperty(ScrollingStateNode::Layer))
            currNode->setLayer(layerTreeHost.layerForID(PlatformLayerID { currNode->layer() }));
        
        switch (currNode->nodeType()) {
        case ScrollingNodeType::Overflow: {
            ScrollingStateOverflowScrollingNode& scrollingStateNode = downcast<ScrollingStateOverflowScrollingNode>(*currNode);

            if (scrollingStateNode.hasChangedProperty(ScrollingStateScrollingNode::ScrollContainerLayer))
                scrollingStateNode.setScrollContainerLayer(layerTreeHost.layerForID(PlatformLayerID { scrollingStateNode.scrollContainerLayer() }));

            if (scrollingStateNode.hasChangedProperty(ScrollingStateScrollingNode::ScrolledContentsLayer))
                scrollingStateNode.setScrolledContentsLayer(layerTreeHost.layerForID(PlatformLayerID { scrollingStateNode.scrolledContentsLayer() }));
            break;
        };
        case ScrollingNodeType::MainFrame:
        case ScrollingNodeType::Subframe: {
            ScrollingStateFrameScrollingNode& scrollingStateNode = downcast<ScrollingStateFrameScrollingNode>(*currNode);

            if (scrollingStateNode.hasChangedProperty(ScrollingStateScrollingNode::ScrollContainerLayer))
                scrollingStateNode.setScrollContainerLayer(layerTreeHost.layerForID(PlatformLayerID { scrollingStateNode.scrollContainerLayer() }));

            if (scrollingStateNode.hasChangedProperty(ScrollingStateScrollingNode::ScrolledContentsLayer))
                scrollingStateNode.setScrolledContentsLayer(layerTreeHost.layerForID(PlatformLayerID { scrollingStateNode.scrolledContentsLayer() }));

            if (scrollingStateNode.hasChangedProperty(ScrollingStateFrameScrollingNode::CounterScrollingLayer))
                scrollingStateNode.setCounterScrollingLayer(layerTreeHost.layerForID(PlatformLayerID { scrollingStateNode.counterScrollingLayer() }));

            // FIXME: we should never have header and footer layers coming from the WebProcess.
            if (scrollingStateNode.hasChangedProperty(ScrollingStateFrameScrollingNode::HeaderLayer))
                scrollingStateNode.setHeaderLayer(layerTreeHost.layerForID(PlatformLayerID { scrollingStateNode.headerLayer() }));

            if (scrollingStateNode.hasChangedProperty(ScrollingStateFrameScrollingNode::FooterLayer))
                scrollingStateNode.setFooterLayer(layerTreeHost.layerForID(PlatformLayerID { scrollingStateNode.footerLayer() }));
            break;
        }
        case ScrollingNodeType::OverflowProxy:
        case ScrollingNodeType::FrameHosting:
        case ScrollingNodeType::Fixed:
        case ScrollingNodeType::Sticky:
        case ScrollingNodeType::Positioned:
            break;
        }
    }
}

FloatRect RemoteScrollingCoordinatorProxy::currentLayoutViewport() const
{
    // FIXME: does this give a different value to the last value pushed onto us?
    return m_webPageProxy.computeLayoutViewportRect(m_webPageProxy.unobscuredContentRect(), m_webPageProxy.unobscuredContentRectRespectingInputViewBounds(), m_webPageProxy.layoutViewportRect(),
        m_webPageProxy.displayedContentScale(), FrameView::LayoutViewportConstraint::Unconstrained);
}

void RemoteScrollingCoordinatorProxy::scrollingTreeNodeWillStartPanGesture(ScrollingNodeID)
{
    m_webPageProxy.scrollingNodeScrollViewWillStartPanGesture();
}

void RemoteScrollingCoordinatorProxy::scrollingTreeNodeWillStartScroll(ScrollingNodeID nodeID)
{
    m_webPageProxy.scrollingNodeScrollWillStartScroll();

    m_uiState.addNodeWithActiveUserScroll(nodeID);
    sendUIStateChangedIfNecessary();
}

void RemoteScrollingCoordinatorProxy::scrollingTreeNodeDidEndScroll(ScrollingNodeID nodeID)
{
    m_webPageProxy.scrollingNodeScrollDidEndScroll();

    m_uiState.removeNodeWithActiveUserScroll(nodeID);
    sendUIStateChangedIfNecessary();
}

void RemoteScrollingCoordinatorProxy::establishLayerTreeScrollingRelations(const RemoteLayerTreeHost& remoteLayerTreeHost)
{
    for (auto layerID : m_layersWithScrollingRelations) {
        if (auto* layerNode = remoteLayerTreeHost.nodeForID(layerID)) {
            layerNode->setActingScrollContainerID(0);
            layerNode->setStationaryScrollContainerIDs({ });
        }
    }
    m_layersWithScrollingRelations.clear();

    // Usually a scroll view scrolls its descendant layers. In some positioning cases it also controls non-descendants, or doesn't control a descendant.
    // To do overlap hit testing correctly we tell layers about such relations.
    
    for (auto& positionedNode : m_scrollingTree->activePositionedNodes()) {
        Vector<GraphicsLayer::PlatformLayerID> stationaryScrollContainerIDs;

        for (auto overflowNodeID : positionedNode->relatedOverflowScrollingNodes()) {
            auto* overflowNode = downcast<ScrollingTreeOverflowScrollingNode>(m_scrollingTree->nodeForID(overflowNodeID));
            if (!overflowNode)
                continue;
            stationaryScrollContainerIDs.append(RemoteLayerTreeNode::layerID(static_cast<CALayer*>(overflowNode->scrollContainerLayer())));
        }

        if (auto* layerNode = RemoteLayerTreeNode::forCALayer(positionedNode->layer())) {
            layerNode->setStationaryScrollContainerIDs(WTFMove(stationaryScrollContainerIDs));
            m_layersWithScrollingRelations.add(layerNode->layerID());
        }
    }

    for (auto& scrollProxyNode : m_scrollingTree->activeOverflowScrollProxyNodes()) {
        auto* overflowNode = downcast<ScrollingTreeOverflowScrollingNode>(m_scrollingTree->nodeForID(scrollProxyNode->overflowScrollingNodeID()));
        if (!overflowNode)
            continue;
        if (auto* layerNode = RemoteLayerTreeNode::forCALayer(scrollProxyNode->layer())) {
            layerNode->setActingScrollContainerID(RemoteLayerTreeNode::layerID(static_cast<CALayer*>(overflowNode->scrollContainerLayer())));
            m_layersWithScrollingRelations.add(layerNode->layerID());
        }
    }
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
        ScrollingTreeFrameScrollingNode* rootScrollingNode = static_cast<ScrollingTreeFrameScrollingNode*>(root);
        const Vector<float>& snapOffsets = axis == ScrollEventAxis::Horizontal ? rootScrollingNode->horizontalSnapOffsets() : rootScrollingNode->verticalSnapOffsets();
        unsigned currentIndex = axis == ScrollEventAxis::Horizontal ? m_currentHorizontalSnapPointIndex : m_currentVerticalSnapPointIndex;
        return snapOffsets.size() && (currentIndex < snapOffsets.size() || currentIndex == invalidSnapOffsetIndex);
    }
    return false;
}

float RemoteScrollingCoordinatorProxy::closestSnapOffsetForMainFrameScrolling(ScrollEventAxis axis, float scrollDestination, float velocity, unsigned& currentIndex) const
{
    ScrollingTreeNode* root = m_scrollingTree->rootNode();
    ASSERT(root && root->isFrameScrollingNode());
    ScrollingTreeFrameScrollingNode* rootScrollingNode = static_cast<ScrollingTreeFrameScrollingNode*>(root);
    const Vector<float>& snapOffsets = axis == ScrollEventAxis::Horizontal ? rootScrollingNode->horizontalSnapOffsets() : rootScrollingNode->verticalSnapOffsets();
    const Vector<ScrollOffsetRange<float>>& snapOffsetRanges = axis == ScrollEventAxis::Horizontal ? rootScrollingNode->horizontalSnapOffsetRanges() : rootScrollingNode->verticalSnapOffsetRanges();

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

    ScrollingTreeFrameScrollingNode& rootScrollingNode = downcast<ScrollingTreeFrameScrollingNode>(*root);
    const Vector<float>& horizontal = rootScrollingNode.horizontalSnapOffsets();
    const Vector<float>& vertical = rootScrollingNode.verticalSnapOffsets();

    if (horizontal.isEmpty() && vertical.isEmpty())
        return false;

    if ((!horizontal.isEmpty() && m_currentHorizontalSnapPointIndex >= horizontal.size())
        || (!vertical.isEmpty() && m_currentVerticalSnapPointIndex >= vertical.size())) {
        return false;
    }
    
    return true;
}
    
CGPoint RemoteScrollingCoordinatorProxy::nearestActiveContentInsetAdjustedSnapOffset(CGFloat topInset, const CGPoint& currentPoint) const
{
    CGPoint activePoint = currentPoint;

    ScrollingTreeNode* root = m_scrollingTree->rootNode();
    ASSERT(root && is<ScrollingTreeFrameScrollingNode>(root));
    ScrollingTreeFrameScrollingNode& rootScrollingNode = downcast<ScrollingTreeFrameScrollingNode>(*root);
    const Vector<float>& horizontal = rootScrollingNode.horizontalSnapOffsets();
    const Vector<float>& vertical = rootScrollingNode.verticalSnapOffsets();

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
