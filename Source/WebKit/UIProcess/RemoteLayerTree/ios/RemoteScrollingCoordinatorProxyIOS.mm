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
    auto* treeNode = scrollingTree().nodeForID(nodeID);

    // All ScrollingTreeOverflowScrollingNodes are ScrollingTreeOverflowScrollingNodeIOS on iOS.
    if (RefPtr overflowScrollingNode = dynamicDowncast<ScrollingTreeOverflowScrollingNode>(treeNode))
        return static_cast<ScrollingTreeOverflowScrollingNodeIOS*>(overflowScrollingNode.get())->scrollView();

    // All ScrollingTreeFrameScrollingNodes are ScrollingTreeFrameScrollingNodeRemoteIOS on iOS.
    if (RefPtr frameScrollingNode = dynamicDowncast<ScrollingTreeFrameScrollingNode>(treeNode))
        return static_cast<ScrollingTreeFrameScrollingNodeRemoteIOS*>(frameScrollingNode.get())->scrollView();

    // All ScrollingTreePluginScrollingNodes are ScrollingTreePluginScrollingNodeIOS on iOS.
    if (RefPtr pluginScrollingNode = dynamicDowncast<ScrollingTreePluginScrollingNode>(treeNode))
        return static_cast<ScrollingTreePluginScrollingNodeIOS*>(pluginScrollingNode.get())->scrollView();

    return nil;
}

#if ENABLE(OVERLAY_REGIONS_IN_EVENT_REGION)
void RemoteScrollingCoordinatorProxyIOS::removeDestroyedLayerIDs(const Vector<WebCore::PlatformLayerIdentifier>& destroyedLayers)
{
    for (auto layerID : destroyedLayers) {
        m_fixedScrollingNodesByLayerID.remove(layerID);
        m_scrollingNodesByLayerID.remove(layerID);
    }
}

HashSet<WebCore::PlatformLayerIdentifier> RemoteScrollingCoordinatorProxyIOS::fixedScrollingNodeLayerIDs() const
{
    HashSet<WebCore::PlatformLayerIdentifier> actuallyFixed;
    for (auto& [layerID, scrollingNodeID] : m_fixedScrollingNodesByLayerID) {
        auto* treeNode = scrollingTree().nodeForID(scrollingNodeID);
        if (auto* scrollingNode = dynamicDowncast<ScrollingTreeStickyNodeCocoa>(treeNode)) {
            if (scrollingNode->isCurrentlySticking())
                actuallyFixed.add(layerID);
        } else
            actuallyFixed.add(layerID);
    }
    return actuallyFixed;
}

RemoteScrollingCoordinatorProxyIOS::OverlayRegionCandidatesMap RemoteScrollingCoordinatorProxyIOS::overlayRegionCandidates() const
{
    OverlayRegionCandidatesMap candidates;
    auto& relatedNodesMap = scrollingTree().overflowRelatedNodes();
    for (auto scrollingNodeID : m_scrollingNodesByLayerID.values()) {
        auto* treeNode = scrollingTree().nodeForID(scrollingNodeID);
        if (auto* scrollingNode = dynamicDowncast<ScrollingTreeScrollingNode>(treeNode)) {
            RetainPtr<WKBaseScrollView> scrollView = (WKBaseScrollView *)scrollViewForScrollingNodeID(scrollingNodeID);
            if (scrollView && scrollingNode->snapOffsetsInfo().isEmpty()) {

                HashSet<WebCore::PlatformLayerIdentifier> relatedLayers;
                auto relatedIterator = relatedNodesMap.find(scrollingNodeID);
                if (relatedIterator != relatedNodesMap.end()) {
                    for (auto relatedNodeID : relatedIterator->value) {
                        auto* treeNode = scrollingTree().nodeForID(relatedNodeID);
                        if (RefPtr proxyNode = dynamicDowncast<ScrollingTreeOverflowScrollProxyNode>(treeNode)) {
                            if (RetainPtr layer = proxyNode->layer()) {
                                if (auto layerID = WebKit::RemoteLayerTreeNode::layerID(layer.get()))
                                    relatedLayers.add(*layerID);
                            }
                        }
                    }
                }

                candidates.add(scrollView, relatedLayers);
            }
        }
    }
    return candidates;
}
#endif // ENABLE(OVERLAY_REGIONS_IN_EVENT_REGION)

void RemoteScrollingCoordinatorProxyIOS::connectStateNodeLayers(ScrollingStateTree& stateTree, const RemoteLayerTreeHost& layerTreeHost)
{
    for (auto& currNode : stateTree.nodeMap().values()) {
        if (currNode->hasChangedProperty(ScrollingStateNode::Property::Layer)) {
            auto platformLayerID = currNode->layer().layerID();
            auto remoteLayerTreeNode = layerTreeHost.nodeForID(platformLayerID);
            if (remoteLayerTreeNode)
                currNode->setLayer(remoteLayerTreeNode->layer());
#if ENABLE(OVERLAY_REGIONS_IN_EVENT_REGION)
            if (platformLayerID && (currNode->isFixedNode() || currNode->isStickyNode()))
                m_fixedScrollingNodesByLayerID.add(*platformLayerID, currNode->scrollingNodeID());
            if (platformLayerID && currNode->isScrollingNode())
                m_scrollingNodesByLayerID.add(*platformLayerID, currNode->scrollingNodeID());
#endif
        }

        switch (currNode->nodeType()) {
        case ScrollingNodeType::Overflow: {
            ScrollingStateOverflowScrollingNode& scrollingStateNode = downcast<ScrollingStateOverflowScrollingNode>(currNode).unsafeGet();

            if (scrollingStateNode.hasChangedProperty(ScrollingStateNode::Property::ScrollContainerLayer)) {
                auto platformLayerID = scrollingStateNode.scrollContainerLayer().layerID();
                auto remoteLayerTreeNode = layerTreeHost.nodeForID(platformLayerID);
                if (remoteLayerTreeNode)
                    scrollingStateNode.setScrollContainerLayer(remoteLayerTreeNode->layer());
            }

            if (scrollingStateNode.hasChangedProperty(ScrollingStateNode::Property::ScrolledContentsLayer))
                scrollingStateNode.setScrolledContentsLayer(layerTreeHost.layerForID(scrollingStateNode.scrolledContentsLayer().layerID()));
            break;
        };
        case ScrollingNodeType::MainFrame:
        case ScrollingNodeType::Subframe: {
            ScrollingStateFrameScrollingNode& scrollingStateNode = downcast<ScrollingStateFrameScrollingNode>(currNode).unsafeGet();

            if (scrollingStateNode.hasChangedProperty(ScrollingStateNode::Property::ScrollContainerLayer)) {
                auto platformLayerID = scrollingStateNode.scrollContainerLayer().layerID();
                auto remoteLayerTreeNode = layerTreeHost.nodeForID(platformLayerID);
                if (remoteLayerTreeNode)
                    scrollingStateNode.setScrollContainerLayer(remoteLayerTreeNode->layer());
            }

            if (scrollingStateNode.hasChangedProperty(ScrollingStateNode::Property::ScrolledContentsLayer))
                scrollingStateNode.setScrolledContentsLayer(layerTreeHost.layerForID(scrollingStateNode.scrolledContentsLayer().layerID()));

            if (scrollingStateNode.hasChangedProperty(ScrollingStateNode::Property::CounterScrollingLayer))
                scrollingStateNode.setCounterScrollingLayer(layerTreeHost.layerForID(scrollingStateNode.counterScrollingLayer().layerID()));

            // FIXME: we should never have header and footer layers coming from the WebProcess.
            if (scrollingStateNode.hasChangedProperty(ScrollingStateNode::Property::HeaderLayer))
                scrollingStateNode.setHeaderLayer(layerTreeHost.layerForID(scrollingStateNode.headerLayer().layerID()));

            if (scrollingStateNode.hasChangedProperty(ScrollingStateNode::Property::FooterLayer))
                scrollingStateNode.setFooterLayer(layerTreeHost.layerForID(scrollingStateNode.footerLayer().layerID()));
            break;
        }
        case ScrollingNodeType::PluginScrolling: {
            ScrollingStatePluginScrollingNode& scrollingStateNode = downcast<ScrollingStatePluginScrollingNode>(currNode).unsafeGet();

            if (scrollingStateNode.hasChangedProperty(ScrollingStateNode::Property::ScrollContainerLayer)) {
                auto platformLayerID = scrollingStateNode.scrollContainerLayer().layerID();
                auto remoteLayerTreeNode = layerTreeHost.nodeForID(platformLayerID);
                if (remoteLayerTreeNode)
                    scrollingStateNode.setScrollContainerLayer(remoteLayerTreeNode->layer());
            }

            if (scrollingStateNode.hasChangedProperty(ScrollingStateNode::Property::ScrolledContentsLayer))
                scrollingStateNode.setScrolledContentsLayer(layerTreeHost.layerForID(scrollingStateNode.scrolledContentsLayer().layerID()));
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
    return page->computeLayoutViewportRect(page->unobscuredContentRect(), page->unobscuredContentRectRespectingInputViewBounds(), webPageProxy().layoutViewportRect(),
        page->displayedContentScale(), LayoutViewportConstraint::Unconstrained);
}

void RemoteScrollingCoordinatorProxyIOS::scrollingTreeNodeWillStartPanGesture(ScrollingNodeID nodeID)
{
    protectedWebPageProxy()->scrollingNodeScrollViewWillStartPanGesture(nodeID);
}

// This is not called for the main scroll view.
void RemoteScrollingCoordinatorProxyIOS::scrollingTreeNodeWillStartScroll(ScrollingNodeID nodeID)
{
    protectedWebPageProxy()->scrollingNodeScrollWillStartScroll(nodeID);

    m_uiState.addNodeWithActiveUserScroll(nodeID);
    sendUIStateChangedIfNecessary();
}

// This is not called for the main scroll view.
void RemoteScrollingCoordinatorProxyIOS::scrollingTreeNodeDidEndScroll(ScrollingNodeID nodeID)
{
    protectedWebPageProxy()->scrollingNodeScrollDidEndScroll(nodeID);

    m_uiState.removeNodeWithActiveUserScroll(nodeID);
    sendUIStateChangedIfNecessary();
}

void RemoteScrollingCoordinatorProxyIOS::establishLayerTreeScrollingRelations(const RemoteLayerTreeHost& remoteLayerTreeHost)
{
    for (auto layerID : m_layersWithScrollingRelations) {
        if (auto* layerNode = remoteLayerTreeHost.nodeForID(layerID)) {
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
            auto* node = scrollingTree().nodeForID(overflowNodeID);
            auto* overflowNode = dynamicDowncast<ScrollingTreeOverflowScrollingNode>(node);
            MESSAGE_CHECK(overflowNode);
            auto layerID = RemoteLayerTreeNode::layerID(static_cast<CALayer*>(overflowNode->scrollContainerLayer()));
            MESSAGE_CHECK(layerID);
            stationaryScrollContainerIDs.append(*layerID);
        }

        if (auto* layerNode = RemoteLayerTreeNode::forCALayer(positionedNode->layer())) {
            layerNode->setStationaryScrollContainerIDs(WTFMove(stationaryScrollContainerIDs));
            m_layersWithScrollingRelations.add(layerNode->layerID());
        }
    }

    for (auto& scrollProxyNode : scrollingTree().activeOverflowScrollProxyNodes()) {
        auto* node = scrollingTree().nodeForID(scrollProxyNode->overflowScrollingNodeID());
        auto* overflowNode = dynamicDowncast<ScrollingTreeOverflowScrollingNode>(node);
        MESSAGE_CHECK(overflowNode);

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
        FloatPoint projectedOffset { *targetContentOffset };
        projectedOffset.move(0, topInset);
        std::tie(potentialSnapPosition, m_currentVerticalSnapPointIndex) = closestSnapOffsetForMainFrameScrolling(WebCore::ScrollEventAxis::Vertical, currentContentOffset.y + topInset, WTFMove(projectedOffset), velocity.y);
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
    auto* rootNode = scrollingTree().rootNode();
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
    auto* rootNode = scrollingTree().rootNode();
    if (rootNode)
        return rootNode->snapOffsetsInfo().offsetsForAxis(axis).size();

    return false;
}

std::pair<float, std::optional<unsigned>> RemoteScrollingCoordinatorProxyIOS::closestSnapOffsetForMainFrameScrolling(ScrollEventAxis axis, float currentScrollOffset, FloatPoint scrollDestination, float velocity) const
{
    auto* rootNode = scrollingTree().rootNode();
    const auto& snapOffsetsInfo = rootNode->snapOffsetsInfo();

    auto zoomScale = [webPageProxy().cocoaView() scrollView].zoomScale;
    scrollDestination.scale(1.0 / zoomScale);
    float scaledCurrentScrollOffset = currentScrollOffset / zoomScale;
    auto [rawClosestSnapOffset, newIndex] = snapOffsetsInfo.closestSnapOffset(axis, rootNode->layoutViewport().size(), scrollDestination, velocity, scaledCurrentScrollOffset);
    return std::make_pair(rawClosestSnapOffset * zoomScale, newIndex);
}

bool RemoteScrollingCoordinatorProxyIOS::hasActiveSnapPoint() const
{
    auto* rootNode = scrollingTree().rootNode();
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

    auto* rootNode = scrollingTree().rootNode();
    if (!rootNode)
        return CGPointZero;

    const auto& horizontal = rootNode->snapOffsetsInfo().horizontalSnapOffsets;
    const auto& vertical = rootNode->snapOffsetsInfo().verticalSnapOffsets;
    auto zoomScale = [webPageProxy().cocoaView() scrollView].zoomScale;

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
    updateAnimations();
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
        drawingAreaIOS().scheduleDisplayRefreshCallbacksForAnimation();
}

void RemoteScrollingCoordinatorProxyIOS::progressBasedTimelinesWereUpdatedForNode(const WebCore::ScrollingTreeScrollingNode& node)
{
    if (scrollingTree().hasTimelineForNode(node))
        drawingAreaIOS().scheduleDisplayRefreshCallbacksForAnimation();
}

void RemoteScrollingCoordinatorProxyIOS::animationsWereRemovedFromNode(RemoteLayerTreeNode& node)
{
    m_animatedNodeLayerIDs.remove(node.layerID());
    if (m_animatedNodeLayerIDs.isEmpty())
        drawingAreaIOS().pauseDisplayRefreshCallbacksForAnimation();
}

void RemoteScrollingCoordinatorProxyIOS::updateTimelineRegistration(WebCore::ProcessIdentifier processIdentifier, const HashSet<Ref<WebCore::AcceleratedTimeline>>& timelineRepresentations, MonotonicTime now)
{
    scrollingTree().updateTimelineRegistration(processIdentifier, timelineRepresentations);
    if (!m_monotonicTimelineRegistry)
        m_monotonicTimelineRegistry = makeUnique<RemoteMonotonicTimelineRegistry>();
    m_monotonicTimelineRegistry->update(processIdentifier, timelineRepresentations, now);
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

void RemoteScrollingCoordinatorProxyIOS::updateAnimations()
{
    // FIXME: Rather than using 'now' at the point this is called, we
    // should probably be using the timestamp of the (next?) display
    // link update or vblank refresh.
    if (m_monotonicTimelineRegistry)
        m_monotonicTimelineRegistry->advanceCurrentTime(MonotonicTime::now());

    auto& layerTreeHost = drawingAreaIOS().remoteLayerTreeHost();

    auto animatedNodeLayerIDs = std::exchange(m_animatedNodeLayerIDs, { });
    for (auto animatedNodeLayerID : animatedNodeLayerIDs) {
        auto* animatedNode = layerTreeHost.nodeForID(animatedNodeLayerID);
        auto* animationStack = animatedNode->animationStack();
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
