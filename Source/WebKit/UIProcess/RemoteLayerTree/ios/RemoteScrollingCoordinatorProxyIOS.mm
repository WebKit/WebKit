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
#import "RemoteScrollingCoordinatorProxyIOS.h"

#if PLATFORM(IOS_FAMILY) && ENABLE(ASYNC_SCROLLING)

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

RemoteScrollingCoordinatorProxyIOS::RemoteScrollingCoordinatorProxyIOS(WebPageProxy& webPageProxy)
    : RemoteScrollingCoordinatorProxy(webPageProxy)
{
}

OptionSet<TouchAction> RemoteScrollingCoordinatorProxyIOS::activeTouchActionsForTouchIdentifier(unsigned touchIdentifier) const
{
    auto iterator = m_touchActionsByTouchIdentifier.find(touchIdentifier);
    if (iterator == m_touchActionsByTouchIdentifier.end())
        return { };
    return iterator->value;
}

void RemoteScrollingCoordinatorProxyIOS::setTouchActionsForTouchIdentifier(OptionSet<TouchAction> touchActions, unsigned touchIdentifier)
{
    m_touchActionsByTouchIdentifier.set(touchIdentifier, touchActions);
}

void RemoteScrollingCoordinatorProxyIOS::clearTouchActionsForTouchIdentifier(unsigned touchIdentifier)
{
    m_touchActionsByTouchIdentifier.remove(touchIdentifier);
}

UIScrollView *RemoteScrollingCoordinatorProxyIOS::scrollViewForScrollingNodeID(ScrollingNodeID nodeID) const
{
    auto* treeNode = scrollingTree()->nodeForID(nodeID);

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

#if ENABLE(OVERLAY_REGIONS_IN_EVENT_REGION)
void RemoteScrollingCoordinatorProxyIOS::removeFixedScrollingNodeLayerIDs(const Vector<WebCore::GraphicsLayer::PlatformLayerID>& destroyedLayers)
{
    for (auto layerID : destroyedLayers)
        m_fixedScrollingNodeLayerIDs.remove(layerID);
}
#endif // ENABLE(OVERLAY_REGIONS_IN_EVENT_REGION)

void RemoteScrollingCoordinatorProxyIOS::connectStateNodeLayers(ScrollingStateTree& stateTree, const RemoteLayerTreeHost& layerTreeHost)
{
    using PlatformLayerID = GraphicsLayer::PlatformLayerID;

    for (auto& currNode : stateTree.nodeMap().values()) {
        if (currNode->hasChangedProperty(ScrollingStateNode::Property::Layer)) {
            auto platformLayerID = PlatformLayerID { currNode->layer() };
            currNode->setLayer(layerTreeHost.layerForID(platformLayerID));
#if ENABLE(OVERLAY_REGIONS_IN_EVENT_REGION)
            if (platformLayerID && (currNode->isFixedNode() || currNode->isStickyNode()))
                m_fixedScrollingNodeLayerIDs.add(platformLayerID);
#endif
        }
        
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

FloatRect RemoteScrollingCoordinatorProxyIOS::currentLayoutViewport() const
{
    // FIXME: does this give a different value to the last value pushed onto us?
    return webPageProxy().computeLayoutViewportRect(webPageProxy().unobscuredContentRect(), webPageProxy().unobscuredContentRectRespectingInputViewBounds(), webPageProxy().layoutViewportRect(),
        webPageProxy().displayedContentScale(), FrameView::LayoutViewportConstraint::Unconstrained);
}

void RemoteScrollingCoordinatorProxyIOS::scrollingTreeNodeWillStartPanGesture(ScrollingNodeID nodeID)
{
    webPageProxy().scrollingNodeScrollViewWillStartPanGesture(nodeID);
}

// This is not called for the main scroll view.
void RemoteScrollingCoordinatorProxyIOS::scrollingTreeNodeWillStartScroll(ScrollingNodeID nodeID)
{
    webPageProxy().scrollingNodeScrollWillStartScroll(nodeID);

    m_uiState.addNodeWithActiveUserScroll(nodeID);
    sendUIStateChangedIfNecessary();
}

// This is not called for the main scroll view.
void RemoteScrollingCoordinatorProxyIOS::scrollingTreeNodeDidEndScroll(ScrollingNodeID nodeID)
{
    webPageProxy().scrollingNodeScrollDidEndScroll(nodeID);

    m_uiState.removeNodeWithActiveUserScroll(nodeID);
    sendUIStateChangedIfNecessary();
}

void RemoteScrollingCoordinatorProxyIOS::establishLayerTreeScrollingRelations(const RemoteLayerTreeHost& remoteLayerTreeHost)
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
    
    for (auto& positionedNode : scrollingTree()->activePositionedNodes()) {
        Vector<GraphicsLayer::PlatformLayerID> stationaryScrollContainerIDs;

        for (auto overflowNodeID : positionedNode->relatedOverflowScrollingNodes()) {
            auto* overflowNode = downcast<ScrollingTreeOverflowScrollingNode>(scrollingTree()->nodeForID(overflowNodeID));
            if (!overflowNode)
                continue;
            stationaryScrollContainerIDs.append(RemoteLayerTreeNode::layerID(static_cast<CALayer*>(overflowNode->scrollContainerLayer())));
        }

        if (auto* layerNode = RemoteLayerTreeNode::forCALayer(positionedNode->layer())) {
            layerNode->setStationaryScrollContainerIDs(WTFMove(stationaryScrollContainerIDs));
            m_layersWithScrollingRelations.add(layerNode->layerID());
        }
    }

    for (auto& scrollProxyNode : scrollingTree()->activeOverflowScrollProxyNodes()) {
        auto* overflowNode = downcast<ScrollingTreeOverflowScrollingNode>(scrollingTree()->nodeForID(scrollProxyNode->overflowScrollingNodeID()));
        if (!overflowNode)
            continue;
        if (auto* layerNode = RemoteLayerTreeNode::forCALayer(scrollProxyNode->layer())) {
            layerNode->setActingScrollContainerID(RemoteLayerTreeNode::layerID(static_cast<CALayer*>(overflowNode->scrollContainerLayer())));
            m_layersWithScrollingRelations.add(layerNode->layerID());
        }
    }
}

void RemoteScrollingCoordinatorProxyIOS::adjustTargetContentOffsetForSnapping(CGSize maxScrollOffsets, CGPoint velocity, CGFloat topInset, CGPoint currentContentOffset, CGPoint* targetContentOffset)
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

bool RemoteScrollingCoordinatorProxyIOS::shouldSetScrollViewDecelerationRateFast() const
{
    return shouldSnapForMainFrameScrolling(ScrollEventAxis::Horizontal) || shouldSnapForMainFrameScrolling(ScrollEventAxis::Vertical);
}

void RemoteScrollingCoordinatorProxyIOS::setRootNodeIsInUserScroll(bool value)
{
    auto* rootNode = scrollingTree()->rootNode();
    if (!rootNode)
        return;

    if (value)
        m_uiState.addNodeWithActiveUserScroll(rootNode->scrollingNodeID());
    else
        m_uiState.removeNodeWithActiveUserScroll(rootNode->scrollingNodeID());

    sendUIStateChangedIfNecessary();
}

bool RemoteScrollingCoordinatorProxyIOS::shouldSnapForMainFrameScrolling(ScrollEventAxis axis) const
{
    auto* rootNode = scrollingTree()->rootNode();
    if (rootNode)
        return rootNode->snapOffsetsInfo().offsetsForAxis(axis).size();

    return false;
}

std::pair<float, std::optional<unsigned>> RemoteScrollingCoordinatorProxyIOS::closestSnapOffsetForMainFrameScrolling(ScrollEventAxis axis, float currentScrollOffset, FloatPoint scrollDestination, float velocity) const
{
    auto* rootNode = scrollingTree()->rootNode();
    const auto& snapOffsetsInfo = rootNode->snapOffsetsInfo();

    scrollDestination.scale(1.0 / webPageProxy().displayedContentScale());
    float scaledCurrentScrollOffset = currentScrollOffset / webPageProxy().displayedContentScale();
    auto [rawClosestSnapOffset, newIndex] = snapOffsetsInfo.closestSnapOffset(axis, rootNode->layoutViewport().size(), scrollDestination, velocity, scaledCurrentScrollOffset);
    return std::make_pair(rawClosestSnapOffset * webPageProxy().displayedContentScale(), newIndex);
}

bool RemoteScrollingCoordinatorProxyIOS::hasActiveSnapPoint() const
{
    auto* rootNode = scrollingTree()->rootNode();
    if (!rootNode)
        return false;

    const auto& horizontal = rootNode->snapOffsetsInfo().horizontalSnapOffsets;
    const auto& vertical = rootNode->snapOffsetsInfo().verticalSnapOffsets;

    if (horizontal.isEmpty() && vertical.isEmpty())
        return false;

    if ((!horizontal.isEmpty() && m_currentHorizontalSnapPointIndex >= horizontal.size())
        || (!vertical.isEmpty() && m_currentVerticalSnapPointIndex >= vertical.size())) {
        return false;
    }
    
    return true;
}
    
CGPoint RemoteScrollingCoordinatorProxyIOS::nearestActiveContentInsetAdjustedSnapOffset(CGFloat topInset, const CGPoint& currentPoint) const
{
    CGPoint activePoint = currentPoint;

    auto* rootNode = scrollingTree()->rootNode();
    if (!rootNode)
        return CGPointZero;

    const auto& horizontal = rootNode->snapOffsetsInfo().horizontalSnapOffsets;
    const auto& vertical = rootNode->snapOffsetsInfo().verticalSnapOffsets;

    // The bounds checking with maxScrollOffsets is to ensure that we won't interfere with rubber-banding when scrolling to the edge of the page.
    if (!horizontal.isEmpty() && m_currentHorizontalSnapPointIndex && *m_currentHorizontalSnapPointIndex < horizontal.size())
        activePoint.x = horizontal[*m_currentHorizontalSnapPointIndex].offset * webPageProxy().displayedContentScale();

    if (!vertical.isEmpty() && m_currentVerticalSnapPointIndex && *m_currentVerticalSnapPointIndex < vertical.size()) {
        float potentialSnapPosition = vertical[*m_currentVerticalSnapPointIndex].offset * webPageProxy().displayedContentScale();
        potentialSnapPosition -= topInset;
        activePoint.y = potentialSnapPosition;
    }

    return activePoint;
}

} // namespace WebKit

#endif // PLATFORM(IOS_FAMILY) && ENABLE(ASYNC_SCROLLING)
