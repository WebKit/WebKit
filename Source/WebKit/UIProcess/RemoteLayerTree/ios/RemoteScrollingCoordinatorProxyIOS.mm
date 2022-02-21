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
#import "ScrollingTreeFrameScrollingNodeRemoteIOS.h"
#import "ScrollingTreeOverflowScrollingNodeIOS.h"
#import "WebPageProxy.h"
#import <UIKit/UIView.h>
#import <WebCore/ScrollSnapOffsetsInfo.h>
#import <WebCore/ScrollTypes.h>
#import <WebCore/ScrollingStateFrameScrollingNode.h>
#import <WebCore/ScrollingStateOverflowScrollProxyNode.h>
#import <WebCore/ScrollingStateOverflowScrollingNode.h>
#import <WebCore/ScrollingStatePositionedNode.h>
#import <WebCore/ScrollingStateTree.h>
#import <WebCore/ScrollingTreeFrameScrollingNode.h>
#import <WebCore/ScrollingTreeOverflowScrollProxyNode.h>
#import <WebCore/ScrollingTreeOverflowScrollingNode.h>
#import <WebCore/ScrollingTreePositionedNode.h>
#import <tuple>

namespace WebKit {
using namespace WebCore;

UIScrollView *RemoteScrollingCoordinatorProxy::scrollViewForScrollingNodeID(ScrollingNodeID nodeID) const
{
    auto* treeNode = m_scrollingTree->nodeForID(nodeID);

    if (is<ScrollingTreeOverflowScrollingNode>(treeNode)) {
        auto* overflowScrollingNode = downcast<ScrollingTreeOverflowScrollingNode>(treeNode);

        // All ScrollingTreeOverflowScrollingNodes are ScrollingTreeOverflowScrollingNodeIOS on iOS.
        return static_cast<ScrollingTreeOverflowScrollingNodeIOS*>(overflowScrollingNode)->scrollView();
    }

    if (is<ScrollingTreeFrameScrollingNode>(treeNode)) {
        auto* frameScrollingNode = downcast<ScrollingTreeFrameScrollingNode>(treeNode);

        // All ScrollingTreeFrameScrollingNodes are ScrollingTreeFrameScrollingNodeRemoteIOS on iOS.
        return static_cast<ScrollingTreeFrameScrollingNodeRemoteIOS*>(frameScrollingNode)->scrollView();
    }

    return nil;
}

void RemoteScrollingCoordinatorProxy::connectStateNodeLayers(ScrollingStateTree& stateTree, const RemoteLayerTreeHost& layerTreeHost)
{
    using PlatformLayerID = GraphicsLayer::PlatformLayerID;

    for (auto& currNode : stateTree.nodeMap().values()) {
        if (currNode->hasChangedProperty(ScrollingStateNode::Property::Layer))
            currNode->setLayer(layerTreeHost.layerForID(PlatformLayerID { currNode->layer() }));
        
        switch (currNode->nodeType()) {
        case ScrollingNodeType::Overflow: {
            ScrollingStateOverflowScrollingNode& scrollingStateNode = downcast<ScrollingStateOverflowScrollingNode>(*currNode);

            if (scrollingStateNode.hasChangedProperty(ScrollingStateNode::Property::ScrollContainerLayer))
                scrollingStateNode.setScrollContainerLayer(layerTreeHost.layerForID(PlatformLayerID { scrollingStateNode.scrollContainerLayer() }));

            if (scrollingStateNode.hasChangedProperty(ScrollingStateNode::Property::ScrolledContentsLayer))
                scrollingStateNode.setScrolledContentsLayer(layerTreeHost.layerForID(PlatformLayerID { scrollingStateNode.scrolledContentsLayer() }));
            break;
        };
        case ScrollingNodeType::MainFrame:
        case ScrollingNodeType::Subframe: {
            ScrollingStateFrameScrollingNode& scrollingStateNode = downcast<ScrollingStateFrameScrollingNode>(*currNode);

            if (scrollingStateNode.hasChangedProperty(ScrollingStateNode::Property::ScrollContainerLayer))
                scrollingStateNode.setScrollContainerLayer(layerTreeHost.layerForID(PlatformLayerID { scrollingStateNode.scrollContainerLayer() }));

            if (scrollingStateNode.hasChangedProperty(ScrollingStateNode::Property::ScrolledContentsLayer))
                scrollingStateNode.setScrolledContentsLayer(layerTreeHost.layerForID(PlatformLayerID { scrollingStateNode.scrolledContentsLayer() }));

            if (scrollingStateNode.hasChangedProperty(ScrollingStateNode::Property::CounterScrollingLayer))
                scrollingStateNode.setCounterScrollingLayer(layerTreeHost.layerForID(PlatformLayerID { scrollingStateNode.counterScrollingLayer() }));

            // FIXME: we should never have header and footer layers coming from the WebProcess.
            if (scrollingStateNode.hasChangedProperty(ScrollingStateNode::Property::HeaderLayer))
                scrollingStateNode.setHeaderLayer(layerTreeHost.layerForID(PlatformLayerID { scrollingStateNode.headerLayer() }));

            if (scrollingStateNode.hasChangedProperty(ScrollingStateNode::Property::FooterLayer))
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

// This is not called for the main scroll view.
void RemoteScrollingCoordinatorProxy::scrollingTreeNodeWillStartScroll(ScrollingNodeID nodeID)
{
    m_webPageProxy.scrollingNodeScrollWillStartScroll();

    m_uiState.addNodeWithActiveUserScroll(nodeID);
    sendUIStateChangedIfNecessary();
}

// This is not called for the main scroll view.
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

void RemoteScrollingCoordinatorProxy::adjustTargetContentOffsetForSnapping(CGSize maxScrollOffsets, CGPoint velocity, CGFloat topInset, CGPoint currentContentOffset, CGPoint* targetContentOffset)
{
    // The bounds checking with maxScrollOffsets is to ensure that we won't interfere with rubber-banding when scrolling to the edge of the page.
    if (shouldSnapForMainFrameScrolling(WebCore::ScrollEventAxis::Horizontal)) {
        float potentialSnapPosition;
        std::tie(potentialSnapPosition, m_currentHorizontalSnapPointIndex) = closestSnapOffsetForMainFrameScrolling(WebCore::ScrollEventAxis::Horizontal, currentContentOffset.x, FloatPoint(*targetContentOffset), velocity.x);
        if (targetContentOffset->x > 0 && targetContentOffset->x < maxScrollOffsets.width)
            targetContentOffset->x = std::min<float>(maxScrollOffsets.width, potentialSnapPosition);
    }

    if (shouldSnapForMainFrameScrolling(WebCore::ScrollEventAxis::Vertical)) {
        float potentialSnapPosition;
        std::tie(potentialSnapPosition, m_currentVerticalSnapPointIndex) = closestSnapOffsetForMainFrameScrolling(WebCore::ScrollEventAxis::Vertical, currentContentOffset.y + topInset, FloatPoint(*targetContentOffset), velocity.y);
        if (m_currentVerticalSnapPointIndex)
            potentialSnapPosition -= topInset;

        if (targetContentOffset->y > 0 && targetContentOffset->y < maxScrollOffsets.height)
            targetContentOffset->y = std::min<float>(maxScrollOffsets.height, potentialSnapPosition);
    }
}

bool RemoteScrollingCoordinatorProxy::shouldSetScrollViewDecelerationRateFast() const
{
    return shouldSnapForMainFrameScrolling(ScrollEventAxis::Horizontal) || shouldSnapForMainFrameScrolling(ScrollEventAxis::Vertical);
}

void RemoteScrollingCoordinatorProxy::setRootNodeIsInUserScroll(bool value)
{
    ScrollingTreeNode* root = m_scrollingTree->rootNode();
    if (!root || !root->isFrameScrollingNode())
        return;

    if (value)
        m_uiState.addNodeWithActiveUserScroll(root->scrollingNodeID());
    else
        m_uiState.removeNodeWithActiveUserScroll(root->scrollingNodeID());

    sendUIStateChangedIfNecessary();
}

bool RemoteScrollingCoordinatorProxy::shouldSnapForMainFrameScrolling(ScrollEventAxis axis) const
{
    ScrollingTreeNode* root = m_scrollingTree->rootNode();
    if (root && root->isFrameScrollingNode()) {
        ScrollingTreeFrameScrollingNode* rootScrollingNode = static_cast<ScrollingTreeFrameScrollingNode*>(root);
        return rootScrollingNode->snapOffsetsInfo().offsetsForAxis(axis).size();
    }
    return false;
}

std::pair<float, std::optional<unsigned>> RemoteScrollingCoordinatorProxy::closestSnapOffsetForMainFrameScrolling(ScrollEventAxis axis, float currentScrollOffset, FloatPoint scrollDestination, float velocity) const
{
    ScrollingTreeNode* root = m_scrollingTree->rootNode();
    ASSERT(root && root->isFrameScrollingNode());
    ScrollingTreeFrameScrollingNode* rootScrollingNode = static_cast<ScrollingTreeFrameScrollingNode*>(root);
    const auto& snapOffsetsInfo = rootScrollingNode->snapOffsetsInfo();

    scrollDestination.scale(1.0 / m_webPageProxy.displayedContentScale());
    float scaledCurrentScrollOffset = currentScrollOffset / m_webPageProxy.displayedContentScale();
    auto [rawClosestSnapOffset, newIndex] = snapOffsetsInfo.closestSnapOffset(axis, rootScrollingNode->layoutViewport().size(), scrollDestination, velocity, scaledCurrentScrollOffset);
    return std::make_pair(rawClosestSnapOffset * m_webPageProxy.displayedContentScale(), newIndex);
}

bool RemoteScrollingCoordinatorProxy::hasActiveSnapPoint() const
{
    ScrollingTreeNode* root = m_scrollingTree->rootNode();
    if (!root)
        return false;

    if (!is<ScrollingTreeFrameScrollingNode>(root))
        return false;

    ScrollingTreeFrameScrollingNode& rootScrollingNode = downcast<ScrollingTreeFrameScrollingNode>(*root);
    const auto& horizontal = rootScrollingNode.snapOffsetsInfo().horizontalSnapOffsets;
    const auto& vertical = rootScrollingNode.snapOffsetsInfo().verticalSnapOffsets;

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
    const auto& horizontal = rootScrollingNode.snapOffsetsInfo().horizontalSnapOffsets;
    const auto& vertical = rootScrollingNode.snapOffsetsInfo().verticalSnapOffsets;

    // The bounds checking with maxScrollOffsets is to ensure that we won't interfere with rubber-banding when scrolling to the edge of the page.
    if (!horizontal.isEmpty() && m_currentHorizontalSnapPointIndex && *m_currentHorizontalSnapPointIndex < horizontal.size())
        activePoint.x = horizontal[*m_currentHorizontalSnapPointIndex].offset * m_webPageProxy.displayedContentScale();

    if (!vertical.isEmpty() && m_currentVerticalSnapPointIndex && *m_currentVerticalSnapPointIndex < vertical.size()) {
        float potentialSnapPosition = vertical[*m_currentVerticalSnapPointIndex].offset * m_webPageProxy.displayedContentScale();
        potentialSnapPosition -= topInset;
        activePoint.y = potentialSnapPosition;
    }

    return activePoint;
}

} // namespace WebKit


#endif // ENABLE(ASYNC_SCROLLING)
#endif // PLATFORM(IOS_FAMILY)
