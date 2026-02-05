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

#if PLATFORM(IOS_FAMILY)

#import "RemoteLayerTreeDrawingAreaProxyIOS.h"
#import "RemoteLayerTreeHost.h"
#import "RemoteLayerTreeNode.h"
#import "ScrollingTreeFrameScrollingNodeRemoteIOS.h"
#import "ScrollingTreeOverflowScrollingNodeIOS.h"
#import "ScrollingTreePluginScrollingNodeIOS.h"
#import "WKBaseScrollView.h"
#import "WebPageProxy.h"
#import "WebProcessProxy.h"
#import <WebCore/LocalFrameView.h>
#import <WebCore/ScrollSnapOffsetsInfo.h>
#import <WebCore/ScrollTypes.h>
#import <WebCore/ScrollingStateFrameScrollingNode.h>
#import <WebCore/ScrollingStateOverflowScrollProxyNode.h>
#import <WebCore/ScrollingStateOverflowScrollingNode.h>
#import <WebCore/ScrollingStatePluginScrollingNode.h>
#import <WebCore/ScrollingStatePositionedNode.h>
#import <WebCore/ScrollingStateStickyNode.h>
#import <WebCore/ScrollingStateTree.h>
#import <WebCore/ScrollingTreeFrameScrollingNode.h>
#import <WebCore/ScrollingTreeOverflowScrollProxyNode.h>
#import <WebCore/ScrollingTreeOverflowScrollingNode.h>
#import <WebCore/ScrollingTreePluginScrollingNode.h>
#import <WebCore/ScrollingTreePositionedNode.h>
#import <WebCore/ScrollingTreeStickyNodeCocoa.h>
#import <tuple>
#import <wtf/TZoneMallocInlines.h>

#if ENABLE(OVERLAY_REGIONS_IN_EVENT_REGION)
#import "WKWebViewIOS.h"
#endif

namespace WebKit {

WTF_MAKE_TZONE_ALLOCATED_IMPL(RemoteScrollingCoordinatorProxyIOS);

using namespace WebCore;

#define MESSAGE_CHECK(assertion) MESSAGE_CHECK_BASE(assertion, webPageProxy().legacyMainFrameProcess().connection())

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

UIScrollView *RemoteScrollingCoordinatorProxyIOS::scrollViewForScrollingNodeID(std::optional<ScrollingNodeID> nodeID) const
{
    RefPtr treeNode = scrollingTree().nodeForID(nodeID);

    if (RefPtr overflowScrollingNode = dynamicDowncast<ScrollingTreeOverflowScrollingNodeIOS>(treeNode))
        return overflowScrollingNode->scrollView();

    if (RefPtr frameScrollingNode = dynamicDowncast<ScrollingTreeFrameScrollingNodeRemoteIOS>(treeNode))
        return frameScrollingNode->scrollView();

    if (RefPtr pluginScrollingNode = dynamicDowncast<ScrollingTreePluginScrollingNodeIOS>(treeNode))
        return pluginScrollingNode->scrollView();

    return nil;
}

#if ENABLE(OVERLAY_REGIONS_IN_EVENT_REGION)

void RemoteScrollingCoordinatorProxyIOS::updateOverlayRegions(const Vector<WebCore::PlatformLayerIdentifier>& destroyedLayers)
{
    for (auto layerID : destroyedLayers) {
        m_fixedScrollingNodesByLayerID.remove(layerID);
        if (m_scrollingNodesByLayerID.remove(layerID))
            m_needsOverlayRegionScrollViewSelection = true;
    }

    selectOverlayRegionScrollViewIfNeeded();
    updateOverlayRegionLayers();
}

void RemoteScrollingCoordinatorProxyIOS::overlayRegionsEnabledChanged()
{
    m_needsOverlayRegionScrollViewSelection = true;
    updateOverlayRegions();
}

void RemoteScrollingCoordinatorProxyIOS::selectOverlayRegionScrollViewIfNeeded()
{
    if (!m_needsOverlayRegionScrollViewSelection)
        return;

    m_needsOverlayRegionScrollViewSelection = false;

    Ref page = webPageProxy();

    RetainPtr<WKBaseScrollView> newSelectedScrollView;

    if (page->preferences().overlayRegionsEnabled()) {
        RetainPtr cocoaView = page->cocoaView();
        if (!cocoaView)
            return;

        auto scrollViewCanHaveOverlayRegions = [](WKBaseScrollView *scrollView, WKBaseScrollView *mainScrollView) -> bool {
            if (![scrollView _hasEnoughContentForOverlayRegions])
                return false;

            if (scrollView == mainScrollView)
                return true;

            auto mainScrollViewArea = mainScrollView.bounds.size.width * mainScrollView.bounds.size.height;
            auto scrollViewArea = scrollView.bounds.size.width * scrollView.bounds.size.height;
            return scrollViewArea > mainScrollViewArea / 2;
        };

        RetainPtr mainScrollView = (WKBaseScrollView *)[cocoaView scrollView];

        RefPtr rootNode = scrollingTree().rootNode();
        if (scrollViewCanHaveOverlayRegions(mainScrollView.get(), mainScrollView.get()) && rootNode && rootNode->snapOffsetsInfo().isEmpty())
            newSelectedScrollView = mainScrollView;
        else {
            Vector<RetainPtr<WKBaseScrollView>> candidateScrollViews;
            for (auto scrollingNodeID : m_scrollingNodesByLayerID.values()) {
                RefPtr treeNode = scrollingTree().nodeForID(scrollingNodeID);
                if (RefPtr scrollingNode = dynamicDowncast<ScrollingTreeScrollingNode>(treeNode)) {
                    RetainPtr<WKBaseScrollView> scrollView = (WKBaseScrollView *)scrollViewForScrollingNodeID(scrollingNodeID);
                    if (scrollView && scrollingNode->snapOffsetsInfo().isEmpty())
                        candidateScrollViews.append(scrollView);
                }
            }

            std::ranges::sort(candidateScrollViews, [](auto& first, auto& second) {
                auto firstFrame = [first frame];
                auto secondFrame = [second frame];
                return firstFrame.size.width * firstFrame.size.height
                    > secondFrame.size.width * secondFrame.size.height;
            });

            for (auto scrollView : candidateScrollViews) {
                if (scrollViewCanHaveOverlayRegions(scrollView.get(), mainScrollView.get())) {
                    newSelectedScrollView = scrollView;
                    break;
                }
            }
        }
    }

    if (newSelectedScrollView == m_selectedOverlayRegionScrollView)
        return;

    if (m_selectedOverlayRegionScrollView) {
        [m_selectedOverlayRegionScrollView _updateOverlayRegionsBehavior:NO];
        m_lastOverlayRegionRects.clear();
    }

    if (newSelectedScrollView) {
        [newSelectedScrollView _updateOverlayRegionsBehavior:YES];

        HashSet<WebCore::PlatformLayerIdentifier> relatedLayers;
        auto& relatedNodesMap = scrollingTree().overflowRelatedNodes();

        for (auto [layerID, scrollingNodeID] : m_scrollingNodesByLayerID) {
            if ((WKBaseScrollView *)scrollViewForScrollingNodeID(scrollingNodeID) == newSelectedScrollView.get()) {
                auto relatedIterator = relatedNodesMap.find(scrollingNodeID);
                if (relatedIterator != relatedNodesMap.end()) {
                    for (auto relatedNodeID : relatedIterator->value) {
                        RefPtr treeNode = scrollingTree().nodeForID(relatedNodeID);
                        if (RefPtr proxyNode = dynamicDowncast<ScrollingTreeOverflowScrollProxyNode>(treeNode)) {
                            if (RetainPtr layer = proxyNode->layer()) {
                                if (auto layerID = WebKit::RemoteLayerTreeNode::layerID(layer.get()))
                                    relatedLayers.add(*layerID);
                            }
                        }
                    }
                }
                break;
            }
        }

        if (!relatedLayers.isEmpty()) {
            auto& layerTreeHost = drawingAreaIOS().remoteLayerTreeHost();
            [newSelectedScrollView _associateRelatedLayersForOverlayRegions:relatedLayers with:layerTreeHost];
        }
    }

    m_selectedOverlayRegionScrollView = newSelectedScrollView;
}

static bool overlayDrawsAboveScrollView(const HashSet<RetainPtr<UIView>>& overlayAncestors, const HashSet<RetainPtr<UIView>>& targetScrollViewAncestors)
{
    auto commonAncestors = overlayAncestors.intersectionWith(targetScrollViewAncestors);
    if (commonAncestors.isEmpty())
        return false;

    for (RetainPtr ancestor : commonAncestors) {
        bool foundOverlaySubtree = false;
        for (UIView* subview in [ancestor subviews]) {
            if (commonAncestors.contains(subview))
                break;

            if (overlayAncestors.contains(subview))
                foundOverlaySubtree = true;
            if (targetScrollViewAncestors.contains(subview))
                return !foundOverlaySubtree;
        }
    }

    return false;
}

void RemoteScrollingCoordinatorProxyIOS::updateOverlayRegionLayers()
{
    Ref page = webPageProxy();
    if (!page->preferences().overlayRegionsEnabled())
        return;

    if (!m_selectedOverlayRegionScrollView) {
        m_lastOverlayRegionRects.clear();
        return;
    }

    RetainPtr webView = page->cocoaView();
    RetainPtr targetScrollView = m_selectedOverlayRegionScrollView;
    BOOL stable = webView && [webView _isInStableState:targetScrollView.get()];
    if (!stable)
        return;

    auto& layerTreeHost = drawingAreaIOS().remoteLayerTreeHost();

    HashSet<WebCore::IntRect> overlayRegionRects;

    HashSet<RetainPtr<UIView>> targetScrollViewAncestors;
    auto getTargetScrollViewAncestors = [&]() {
        if (!targetScrollViewAncestors.isEmpty())
            return targetScrollViewAncestors;

        for (RetainPtr<UIView> scrollViewAncestor = [targetScrollView superview]; scrollViewAncestor; scrollViewAncestor = [scrollViewAncestor superview]) {
            targetScrollViewAncestors.add(scrollViewAncestor);
            if ([scrollViewAncestor isKindOfClass:[WKWebView class]])
                break;
        }
        return targetScrollViewAncestors;
    };

    std::function<void(RefPtr<RemoteLayerTreeNode>, RetainPtr<WKBaseScrollView>)> traverseAndAddRects = [&](RefPtr<RemoteLayerTreeNode> node, RetainPtr<WKBaseScrollView> enclosingScrollView) {
        if (!node)
            return;

        RetainPtr overlayView = node->uiView();
        if (!overlayView)
            return;
        if ([overlayView isKindOfClass:[WKBaseScrollView class]])
            return;

        auto traverse = [&]() {
            for (CALayer *sublayer in node->layer().sublayers)
                traverseAndAddRects(WebKit::RemoteLayerTreeNode::forCALayer(sublayer), enclosingScrollView);
        };

        // Simply traverse containers without event regions.
        if (node->eventRegion().region().isEmpty()) {
            traverse();
            return;
        }

        bool needsClipping = true;
        auto clippedRegion = node->eventRegion().region();

        if (!enclosingScrollView) {
            HashSet<RetainPtr<UIView>> overlayAncestors;
            for (RetainPtr<UIView> overlayAncestor = overlayView; overlayAncestor; overlayAncestor = [overlayAncestor superview]) {
                overlayAncestors.add(overlayAncestor);
                if (overlayAncestor.get().clipsToBounds || overlayAncestor.get().layer.mask)
                    clippedRegion.intersect(WebCore::enclosingIntRect([overlayAncestor convertRect:overlayAncestor.get().bounds toView:overlayView.get()]));
                if (RetainPtr scrollView = dynamic_objc_cast<WKBaseScrollView>(overlayAncestor.get())) {
                    enclosingScrollView = scrollView;
                    break;
                }
            }
            needsClipping = false;

            if (!enclosingScrollView)
                return;

            if (enclosingScrollView != targetScrollView) {
                // Ignore this subtree.
                if (!overlayDrawsAboveScrollView(overlayAncestors, getTargetScrollViewAncestors()))
                    return;
            }
        }

        if (needsClipping) {
            for (RetainPtr<UIView> overlayAncestor = overlayView; overlayAncestor; overlayAncestor = [overlayAncestor superview]) {
                if (overlayAncestor.get().clipsToBounds || overlayAncestor.get().layer.mask)
                    clippedRegion.intersect(WebCore::enclosingIntRect([overlayAncestor convertRect:overlayAncestor.get().bounds toView:overlayView.get()]));
                if (overlayAncestor == enclosingScrollView)
                    break;
            }
        }

        CGRect frame = [targetScrollView frame];
        RetainPtr toView = [targetScrollView superview];
        for (auto regionRect : clippedRegion.rects()) {
            CGRect rect = [overlayView convertRect:regionRect toView:toView.get()];
            CGRect offsetRect = CGRectOffset(rect, -frame.origin.x, -frame.origin.y);
            overlayRegionRects.add(WebCore::enclosingIntRect(offsetRect));
        }

        traverse();
    };

    for (auto& [layerID, scrollingNodeID] : m_fixedScrollingNodesByLayerID) {
        RefPtr treeNode = scrollingTree().nodeForID(scrollingNodeID);
        if (RefPtr scrollingNode = dynamicDowncast<ScrollingTreeStickyNodeCocoa>(treeNode)) {
            if (!scrollingNode->isCurrentlySticking())
                continue;
        }

        traverseAndAddRects(layerTreeHost.nodeForID(layerID), nullptr);
    }

    if (overlayRegionRects != m_lastOverlayRegionRects) {
        m_lastOverlayRegionRects = overlayRegionRects;
        [targetScrollView _updateOverlayRegionRects:overlayRegionRects whileStable:stable];
    }
}
#endif // ENABLE(OVERLAY_REGIONS_IN_EVENT_REGION)

void RemoteScrollingCoordinatorProxyIOS::connectStateNodeLayers(ScrollingStateTree& stateTree, const RemoteLayerTreeHost& layerTreeHost)
{
    for (auto& currNode : stateTree.nodeMap().values()) {
        if (currNode->hasChangedProperty(ScrollingStateNode::Property::Layer)) {
            auto platformLayerID = currNode->layer().layerID();
            RefPtr remoteLayerTreeNode = layerTreeHost.nodeForID(platformLayerID);
            if (remoteLayerTreeNode)
                currNode->setLayer(remoteLayerTreeNode->layer());
#if ENABLE(OVERLAY_REGIONS_IN_EVENT_REGION)
            if (platformLayerID && (currNode->isFixedNode() || currNode->isStickyNode()))
                m_fixedScrollingNodesByLayerID.add(*platformLayerID, currNode->scrollingNodeID());
            if (platformLayerID && currNode->isScrollingNode()) {
                m_scrollingNodesByLayerID.add(*platformLayerID, currNode->scrollingNodeID());
                m_needsOverlayRegionScrollViewSelection = true;
            }
#endif
        }

#if ENABLE(OVERLAY_REGIONS_IN_EVENT_REGION)
        if (currNode->hasChangedProperty(ScrollingStateNode::Property::SnapOffsetsInfo)
            || currNode->hasChangedProperty(ScrollingStateNode::Property::TotalContentsSize)
            || currNode->hasChangedProperty(ScrollingStateNode::Property::ScrollContainerLayer)
            || currNode->hasChangedProperty(ScrollingStateNode::Property::ScrolledContentsLayer))
            m_needsOverlayRegionScrollViewSelection = true;
#endif

        switch (currNode->nodeType()) {
        case ScrollingNodeType::Overflow: {
            Ref scrollingStateNode = downcast<ScrollingStateOverflowScrollingNode>(currNode.get());

            if (scrollingStateNode->hasChangedProperty(ScrollingStateNode::Property::ScrollContainerLayer)) {
                auto platformLayerID = scrollingStateNode->scrollContainerLayer().layerID();
                RefPtr remoteLayerTreeNode = layerTreeHost.nodeForID(platformLayerID);
                if (remoteLayerTreeNode)
                    scrollingStateNode->setScrollContainerLayer(remoteLayerTreeNode->layer());
            }

            if (scrollingStateNode->hasChangedProperty(ScrollingStateNode::Property::ScrolledContentsLayer))
                scrollingStateNode->setScrolledContentsLayer(layerTreeHost.layerForID(scrollingStateNode->scrolledContentsLayer().layerID()));
            break;
        };
        case ScrollingNodeType::MainFrame:
        case ScrollingNodeType::Subframe: {
            Ref scrollingStateNode = downcast<ScrollingStateFrameScrollingNode>(currNode.get());

            if (scrollingStateNode->hasChangedProperty(ScrollingStateNode::Property::ScrollContainerLayer)) {
                auto platformLayerID = scrollingStateNode->scrollContainerLayer().layerID();
                RefPtr remoteLayerTreeNode = layerTreeHost.nodeForID(platformLayerID);
                if (remoteLayerTreeNode)
                    scrollingStateNode->setScrollContainerLayer(remoteLayerTreeNode->layer());
            }

            if (scrollingStateNode->hasChangedProperty(ScrollingStateNode::Property::ScrolledContentsLayer))
                scrollingStateNode->setScrolledContentsLayer(layerTreeHost.layerForID(scrollingStateNode->scrolledContentsLayer().layerID()));

            if (scrollingStateNode->hasChangedProperty(ScrollingStateNode::Property::CounterScrollingLayer))
                scrollingStateNode->setCounterScrollingLayer(layerTreeHost.layerForID(scrollingStateNode->counterScrollingLayer().layerID()));

            // FIXME: we should never have header and footer layers coming from the WebProcess.
            if (scrollingStateNode->hasChangedProperty(ScrollingStateNode::Property::HeaderLayer))
                scrollingStateNode->setHeaderLayer(layerTreeHost.layerForID(scrollingStateNode->headerLayer().layerID()));

            if (scrollingStateNode->hasChangedProperty(ScrollingStateNode::Property::FooterLayer))
                scrollingStateNode->setFooterLayer(layerTreeHost.layerForID(scrollingStateNode->footerLayer().layerID()));
            break;
        }
        case ScrollingNodeType::PluginScrolling: {
            Ref scrollingStateNode = downcast<ScrollingStatePluginScrollingNode>(currNode.get());

            if (scrollingStateNode->hasChangedProperty(ScrollingStateNode::Property::ScrollContainerLayer)) {
                auto platformLayerID = scrollingStateNode->scrollContainerLayer().layerID();
                RefPtr remoteLayerTreeNode = layerTreeHost.nodeForID(platformLayerID);
                if (remoteLayerTreeNode)
                    scrollingStateNode->setScrollContainerLayer(remoteLayerTreeNode->layer());
            }

            if (scrollingStateNode->hasChangedProperty(ScrollingStateNode::Property::ScrolledContentsLayer))
                scrollingStateNode->setScrolledContentsLayer(layerTreeHost.layerForID(scrollingStateNode->scrolledContentsLayer().layerID()));
            break;
        }
        case ScrollingNodeType::OverflowProxy:
        case ScrollingNodeType::FrameHosting:
        case ScrollingNodeType::PluginHosting:
        case ScrollingNodeType::Fixed:
        case ScrollingNodeType::Sticky: {
            if (RefPtr stickyStateNode = dynamicDowncast<ScrollingStateStickyNode>(currNode)) {
                if (stickyStateNode->hasChangedProperty(ScrollingStateNode::Property::ViewportAnchorLayer))
                    stickyStateNode->setViewportAnchorLayer(layerTreeHost.layerForID(stickyStateNode->viewportAnchorLayer().layerID()));
            }
            break;
        }
        case ScrollingNodeType::Positioned:
            break;
        }
    }
}

FloatRect RemoteScrollingCoordinatorProxyIOS::currentLayoutViewport() const
{
    // FIXME: does this give a different value to the last value pushed onto us?
    Ref page = webPageProxy();
    return page->computeLayoutViewportRect(page->unobscuredContentRect(), page->unobscuredContentRectRespectingInputViewBounds(), protect(webPageProxy())->layoutViewportRect(),
        page->displayedContentScale(), LayoutViewportConstraint::Unconstrained);
}

void RemoteScrollingCoordinatorProxyIOS::scrollingTreeNodeWillStartPanGesture(ScrollingNodeID nodeID)
{
    protect(webPageProxy())->scrollingNodeScrollViewWillStartPanGesture(nodeID);
}

// This is not called for the main scroll view.
void RemoteScrollingCoordinatorProxyIOS::scrollingTreeNodeWillStartScroll(ScrollingNodeID nodeID)
{
    protect(webPageProxy())->scrollingNodeScrollWillStartScroll(nodeID);

    m_uiState.addNodeWithActiveUserScroll(nodeID);
    sendUIStateChangedIfNecessary();
}

// This is not called for the main scroll view.
void RemoteScrollingCoordinatorProxyIOS::scrollingTreeNodeDidEndScroll(ScrollingNodeID nodeID)
{
    protect(webPageProxy())->scrollingNodeScrollDidEndScroll(nodeID);

    m_uiState.removeNodeWithActiveUserScroll(nodeID);
    sendUIStateChangedIfNecessary();
}

void RemoteScrollingCoordinatorProxyIOS::establishLayerTreeScrollingRelations(const RemoteLayerTreeHost& remoteLayerTreeHost)
{
    for (auto layerID : m_layersWithScrollingRelations) {
        if (RefPtr layerNode = remoteLayerTreeHost.nodeForID(layerID)) {
            layerNode->setActingScrollContainerID(std::nullopt);
            layerNode->setStationaryScrollContainerIDs({ });
        }
    }
    m_layersWithScrollingRelations.clear();

    // Usually a scroll view scrolls its descendant layers. In some positioning cases it also controls non-descendants, or doesn't control a descendant.
    // To do overlap hit testing correctly we tell layers about such relations.

    for (auto& positionedNode : scrollingTree().activePositionedNodes()) {
        Vector<PlatformLayerIdentifier> stationaryScrollContainerIDs;

        for (auto overflowNodeID : positionedNode->relatedOverflowScrollingNodes()) {
            RefPtr node = scrollingTree().nodeForID(overflowNodeID);
            RefPtr overflowNode = dynamicDowncast<ScrollingTreeOverflowScrollingNode>(node.get());
            MESSAGE_CHECK(overflowNode);
            auto layerID = RemoteLayerTreeNode::layerID(static_cast<CALayer*>(overflowNode->scrollContainerLayer()));
            MESSAGE_CHECK(layerID);
            stationaryScrollContainerIDs.append(*layerID);
        }

        if (RefPtr layerNode = RemoteLayerTreeNode::forCALayer(positionedNode->layer())) {
            layerNode->setStationaryScrollContainerIDs(WTF::move(stationaryScrollContainerIDs));
            m_layersWithScrollingRelations.add(layerNode->layerID());
        }
    }

    for (auto& scrollProxyNode : scrollingTree().activeOverflowScrollProxyNodes()) {
        RefPtr node = scrollingTree().nodeForID(scrollProxyNode->overflowScrollingNodeID());
        RefPtr overflowNode = dynamicDowncast<ScrollingTreeOverflowScrollingNode>(node.get());
        MESSAGE_CHECK(overflowNode);

        if (RefPtr layerNode = RemoteLayerTreeNode::forCALayer(scrollProxyNode->layer())) {
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
        FloatPoint projectedOffset { *targetContentOffset };
        projectedOffset.move(0, topInset);
        std::tie(potentialSnapPosition, m_currentVerticalSnapPointIndex) = closestSnapOffsetForMainFrameScrolling(WebCore::ScrollEventAxis::Vertical, currentContentOffset.y + topInset, WTF::move(projectedOffset), velocity.y);
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
    // FIXME: Locking
    RefPtr rootNode = scrollingTree().rootNode();
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
    RefPtr rootNode = scrollingTree().rootNode();
    if (rootNode)
        return rootNode->snapOffsetsInfo().offsetsForAxis(axis).size();

    return false;
}

std::pair<float, std::optional<unsigned>> RemoteScrollingCoordinatorProxyIOS::closestSnapOffsetForMainFrameScrolling(ScrollEventAxis axis, float currentScrollOffset, FloatPoint scrollDestination, float velocity) const
{
    RefPtr rootNode = scrollingTree().rootNode();
    const auto& snapOffsetsInfo = rootNode->snapOffsetsInfo();

    auto zoomScale = [protect(webPageProxy())->cocoaView() scrollView].zoomScale;
    scrollDestination.scale(1.0 / zoomScale);
    float scaledCurrentScrollOffset = currentScrollOffset / zoomScale;
    auto [rawClosestSnapOffset, newIndex] = snapOffsetsInfo.closestSnapOffset(axis, rootNode->layoutViewport().size(), scrollDestination, velocity, scaledCurrentScrollOffset);
    return std::make_pair(rawClosestSnapOffset * zoomScale, newIndex);
}

bool RemoteScrollingCoordinatorProxyIOS::hasActiveSnapPoint() const
{
    RefPtr rootNode = scrollingTree().rootNode();
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

    RefPtr rootNode = scrollingTree().rootNode();
    if (!rootNode)
        return CGPointZero;

    const auto& horizontal = rootNode->snapOffsetsInfo().horizontalSnapOffsets;
    const auto& vertical = rootNode->snapOffsetsInfo().verticalSnapOffsets;
    auto zoomScale = [protect(webPageProxy())->cocoaView() scrollView].zoomScale;

    // The bounds checking with maxScrollOffsets is to ensure that we won't interfere with rubber-banding when scrolling to the edge of the page.
    if (!horizontal.isEmpty() && m_currentHorizontalSnapPointIndex && *m_currentHorizontalSnapPointIndex < horizontal.size())
        activePoint.x = horizontal[*m_currentHorizontalSnapPointIndex].offset * zoomScale;

    if (!vertical.isEmpty() && m_currentVerticalSnapPointIndex && *m_currentVerticalSnapPointIndex < vertical.size()) {
        float potentialSnapPosition = vertical[*m_currentVerticalSnapPointIndex].offset * zoomScale;
        potentialSnapPosition -= topInset;
        activePoint.y = potentialSnapPosition;
    }

    return activePoint;
}

void RemoteScrollingCoordinatorProxyIOS::displayDidRefresh(PlatformDisplayID displayID)
{
#if ENABLE(THREADED_ANIMATIONS)
    updateTimeDependentAnimationStacks();
#endif
}

RemoteLayerTreeDrawingAreaProxyIOS& RemoteScrollingCoordinatorProxyIOS::drawingAreaIOS() const
{
    return *downcast<RemoteLayerTreeDrawingAreaProxyIOS>(webPageProxy().drawingArea());
}

#if ENABLE(THREADED_ANIMATIONS)
void RemoteScrollingCoordinatorProxyIOS::animationsWereAddedToNode(RemoteLayerTreeNode& node)
{
    m_animatedNodeLayerIDs.add(node.layerID());
    if (m_monotonicTimelineRegistry && !m_monotonicTimelineRegistry->isEmpty())
        protect(drawingAreaIOS())->scheduleDisplayRefreshCallbacksForMonotonicAnimations();
}

void RemoteScrollingCoordinatorProxyIOS::progressBasedTimelinesWereUpdatedForNode(const WebCore::ScrollingTreeScrollingNode& node)
{
    updateAnimationStacksDependentOnScrollingNode(node);
}

void RemoteScrollingCoordinatorProxyIOS::animationsWereRemovedFromNode(RemoteLayerTreeNode& node)
{
    m_animatedNodeLayerIDs.remove(node.layerID());
    if (m_animatedNodeLayerIDs.isEmpty() || !m_monotonicTimelineRegistry || m_monotonicTimelineRegistry->isEmpty())
        protect(drawingAreaIOS())->pauseDisplayRefreshCallbacksForMonotonicAnimations();
}

void RemoteScrollingCoordinatorProxyIOS::updateTimelinesRegistration(WebCore::ProcessIdentifier processIdentifier, const WebCore::AcceleratedTimelinesUpdate& timelinesUpdate, MonotonicTime now)
{
    scrollingTree().updateTimelinesRegistration(processIdentifier, timelinesUpdate);
    if (!m_monotonicTimelineRegistry)
        m_monotonicTimelineRegistry = makeUnique<RemoteMonotonicTimelineRegistry>();
    m_monotonicTimelineRegistry->update(processIdentifier, timelinesUpdate, now);
    if (m_monotonicTimelineRegistry->isEmpty())
        m_monotonicTimelineRegistry = nullptr;
}

RefPtr<const RemoteAnimationTimeline> RemoteScrollingCoordinatorProxyIOS::timeline(const TimelineID& timelineID) const
{
    if (m_monotonicTimelineRegistry) {
        if (RefPtr timeline = m_monotonicTimelineRegistry->get(timelineID))
            return timeline;
    }
    return scrollingTree().timeline(timelineID);
}

HashSet<Ref<RemoteProgressBasedTimeline>> RemoteScrollingCoordinatorProxyIOS::timelinesForScrollingNodeIDForTesting(WebCore::ScrollingNodeID scrollingNodeID) const
{
    return scrollingTree().timelinesForScrollingNodeIDForTesting(scrollingNodeID);
}

void RemoteScrollingCoordinatorProxyIOS::updateTimeDependentAnimationStacks()
{
    if (!m_monotonicTimelineRegistry)
        return;

    // FIXME: Rather than using 'now' at the point this is called, we
    // should probably be using the timestamp of the (next?) display
    // link update or vblank refresh.
    m_monotonicTimelineRegistry->advanceCurrentTime(MonotonicTime::now());

    updateAnimationStacks([](auto& animationStack) {
        return animationStack.isTimeDependent();
    });
}

void RemoteScrollingCoordinatorProxyIOS::updateAnimationStacksDependentOnScrollingNode(const WebCore::ScrollingTreeScrollingNode& node)
{
    auto scrollingNodeID = node.scrollingNodeID();
    updateAnimationStacks([scrollingNodeID](auto& animationStack) {
        return animationStack.isDependentOnScrollingNodeWithID(scrollingNodeID);
    });
}

void RemoteScrollingCoordinatorProxyIOS::updateAnimationStacks(NOESCAPE const Function<bool(const RemoteAnimationStack&)>& predicate)
{
    auto& layerTreeHost = drawingAreaIOS().remoteLayerTreeHost();

    auto animatedNodeLayerIDs = std::exchange(m_animatedNodeLayerIDs, { });
    for (auto animatedNodeLayerID : animatedNodeLayerIDs) {
        RefPtr animatedNode = layerTreeHost.nodeForID(animatedNodeLayerID);
        RefPtr animationStack = animatedNode->animationStack();
        ASSERT(animationStack);
        if (predicate(*animationStack))
            animationStack->applyEffectsFromMainThread(animatedNode->layer(), animatedNode->backdropRootIsOpaque());

        // We can clear the effect stack if it's empty, but the previous
        // call to applyEffects() is important so that the base values
        // were re-applied.
        if (!animationStack->isEmpty())
            m_animatedNodeLayerIDs.add(animatedNodeLayerID);
    }
}
#endif

#undef MESSAGE_CHECK

} // namespace WebKit

#endif // PLATFORM(IOS_FAMILY)
