/*
 * Copyright (C) 2022-2025 Apple Inc. All rights reserved.
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
#import "RemoteScrollingCoordinatorProxyMac.h"

#if PLATFORM(MAC) && ENABLE(UI_SIDE_COMPOSITING)

#import "RemoteLayerTreeDrawingAreaProxy.h"
#import "RemoteLayerTreeEventDispatcher.h"
#import "WebPageProxy.h"
#import <WebCore/PerformanceLoggingClient.h>
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
#import <WebCore/WheelEventDeltaFilter.h>
#import <wtf/TZoneMallocInlines.h>

namespace WebKit {

WTF_MAKE_TZONE_ALLOCATED_IMPL(RemoteScrollingCoordinatorProxyMac);

using namespace WebCore;

RemoteScrollingCoordinatorProxyMac::RemoteScrollingCoordinatorProxyMac(WebPageProxy& webPageProxy)
    : RemoteScrollingCoordinatorProxy(webPageProxy)
    , m_eventDispatcher(RemoteLayerTreeEventDispatcher::create(*this, webPageProxy.webPageIDInMainFrameProcess()))
{
    m_eventDispatcher->setScrollingTree(&scrollingTree());
}

RemoteScrollingCoordinatorProxyMac::~RemoteScrollingCoordinatorProxyMac()
{
    m_eventDispatcher->invalidate();
}

void RemoteScrollingCoordinatorProxyMac::cacheWheelEventScrollingAccelerationCurve(const NativeWebWheelEvent& nativeWheelEvent)
{
    m_eventDispatcher->cacheWheelEventScrollingAccelerationCurve(nativeWheelEvent);
}

void RemoteScrollingCoordinatorProxyMac::handleWheelEvent(const WebWheelEvent& wheelEvent, RectEdges<WebCore::RubberBandingBehavior> rubberBandableEdges)
{
    m_eventDispatcher->handleWheelEvent(wheelEvent, rubberBandableEdges);
}

void RemoteScrollingCoordinatorProxyMac::wheelEventHandlingCompleted(const PlatformWheelEvent& wheelEvent, std::optional<ScrollingNodeID> scrollingNodeID, std::optional<WheelScrollGestureState> gestureState, bool wasHandled)
{
    m_eventDispatcher->wheelEventHandlingCompleted(wheelEvent, scrollingNodeID, gestureState, wasHandled);
}

bool RemoteScrollingCoordinatorProxyMac::scrollingTreeNodeRequestsScroll(ScrollingNodeID, const RequestedScrollData&)
{
    // Unlike iOS, we handle scrolling requests for the main frame in the same way we handle them for subscrollers.
    return false;
}

bool RemoteScrollingCoordinatorProxyMac::scrollingTreeNodeRequestsKeyboardScroll(ScrollingNodeID, const RequestedKeyboardScrollData&)
{
    // Unlike iOS, we handle scrolling requests for the main frame in the same way we handle them for subscrollers.
    return false;
}

void RemoteScrollingCoordinatorProxyMac::hasNodeWithAnimatedScrollChanged(bool hasAnimatedScrolls)
{
    m_eventDispatcher->hasNodeWithAnimatedScrollChanged(hasAnimatedScrolls);
}

void RemoteScrollingCoordinatorProxyMac::setRubberBandingInProgressForNode(ScrollingNodeID nodeID, bool isRubberBanding)
{
    if (isRubberBanding) {
        if (scrollingTree().scrollingPerformanceTestingEnabled()) {
#if ENABLE(THREADED_ANIMATIONS)
            m_eventDispatcher->didStartRubberbanding();
#endif
            protectedWebPageProxy()->logScrollingEvent(static_cast<uint32_t>(PerformanceLoggingClient::ScrollingEvent::StartedRubberbanding), MonotonicTime::now(), 0);
        }
        m_uiState.addNodeWithActiveRubberband(nodeID);
    } else
        m_uiState.removeNodeWithActiveRubberband(nodeID);

    sendUIStateChangedIfNecessary();
}

void RemoteScrollingCoordinatorProxyMac::clearNodesWithUserScrollInProgress()
{
    m_uiState.clearNodesWithActiveUserScroll();
    sendUIStateChangedIfNecessary();
}

void RemoteScrollingCoordinatorProxyMac::scrollingTreeNodeWillStartScroll(ScrollingNodeID nodeID)
{
    m_uiState.addNodeWithActiveUserScroll(nodeID);
    sendUIStateChangedIfNecessary();
}

void RemoteScrollingCoordinatorProxyMac::scrollingTreeNodeDidEndScroll(ScrollingNodeID nodeID)
{
    m_uiState.removeNodeWithActiveUserScroll(nodeID);
    sendUIStateChangedIfNecessary();
}

void RemoteScrollingCoordinatorProxyMac::scrollingTreeNodeDidBeginScrollSnapping(ScrollingNodeID nodeID)
{
    m_uiState.addNodeWithActiveScrollSnap(nodeID);
    sendUIStateChangedIfNecessary();
}

void RemoteScrollingCoordinatorProxyMac::scrollingTreeNodeDidEndScrollSnapping(ScrollingNodeID nodeID)
{
    m_uiState.removeNodeWithActiveScrollSnap(nodeID);
    sendUIStateChangedIfNecessary();
}

void RemoteScrollingCoordinatorProxyMac::connectStateNodeLayers(ScrollingStateTree& stateTree, const RemoteLayerTreeHost& layerTreeHost)
{
    for (auto& currNode : stateTree.nodeMap().values()) {
        if (currNode->hasChangedProperty(ScrollingStateNode::Property::Layer))
            currNode->setLayer(layerTreeHost.layerForID(currNode->layer().layerID()));

        switch (currNode->nodeType()) {
        case ScrollingNodeType::MainFrame:
        case ScrollingNodeType::Subframe: {
            Ref scrollingStateNode = downcast<ScrollingStateFrameScrollingNode>(currNode);
            
            if (scrollingStateNode->hasChangedProperty(ScrollingStateNode::Property::ScrollContainerLayer))
                scrollingStateNode->setScrollContainerLayer(layerTreeHost.layerForID(scrollingStateNode->scrollContainerLayer().layerID()));

            if (scrollingStateNode->hasChangedProperty(ScrollingStateNode::Property::ScrolledContentsLayer))
                scrollingStateNode->setScrolledContentsLayer(layerTreeHost.layerForID(scrollingStateNode->scrolledContentsLayer().layerID()));

            if (scrollingStateNode->hasChangedProperty(ScrollingStateNode::Property::CounterScrollingLayer))
                scrollingStateNode->setCounterScrollingLayer(layerTreeHost.layerForID(scrollingStateNode->counterScrollingLayer().layerID()));

            if (scrollingStateNode->hasChangedProperty(ScrollingStateNode::Property::InsetClipLayer))
                scrollingStateNode->setInsetClipLayer(layerTreeHost.layerForID(scrollingStateNode->insetClipLayer().layerID()));

            if (scrollingStateNode->hasChangedProperty(ScrollingStateNode::Property::ContentShadowLayer))
                scrollingStateNode->setContentShadowLayer(layerTreeHost.layerForID(scrollingStateNode->contentShadowLayer().layerID()));

            // FIXME: we should never have header and footer layers coming from the WebProcess.
            if (scrollingStateNode->hasChangedProperty(ScrollingStateNode::Property::HeaderLayer))
                scrollingStateNode->setHeaderLayer(layerTreeHost.layerForID(scrollingStateNode->headerLayer().layerID()));

            if (scrollingStateNode->hasChangedProperty(ScrollingStateNode::Property::FooterLayer))
                scrollingStateNode->setFooterLayer(layerTreeHost.layerForID(scrollingStateNode->footerLayer().layerID()));

            if (scrollingStateNode->hasChangedProperty(ScrollingStateNode::Property::VerticalScrollbarLayer))
                scrollingStateNode->setVerticalScrollbarLayer(layerTreeHost.layerForID(scrollingStateNode->verticalScrollbarLayer().layerID()));

            if (scrollingStateNode->hasChangedProperty(ScrollingStateNode::Property::HorizontalScrollbarLayer))
                scrollingStateNode->setHorizontalScrollbarLayer(layerTreeHost.layerForID(scrollingStateNode->horizontalScrollbarLayer().layerID()));

            if (scrollingStateNode->hasChangedProperty(ScrollingStateNode::Property::RootContentsLayer))
                scrollingStateNode->setRootContentsLayer(layerTreeHost.layerForID(scrollingStateNode->rootContentsLayer().layerID()));
            break;
        }
        case ScrollingNodeType::Overflow: {
            Ref scrollingStateNode = downcast<ScrollingStateOverflowScrollingNode>(currNode);
            if (scrollingStateNode->hasChangedProperty(ScrollingStateNode::Property::ScrollContainerLayer))
                scrollingStateNode->setScrollContainerLayer(layerTreeHost.layerForID(scrollingStateNode->scrollContainerLayer().layerID()));

            if (scrollingStateNode->hasChangedProperty(ScrollingStateNode::Property::ScrolledContentsLayer))
                scrollingStateNode->setScrolledContentsLayer(layerTreeHost.layerForID(scrollingStateNode->scrolledContentsLayer().layerID()));

            if (scrollingStateNode->hasChangedProperty(ScrollingStateNode::Property::VerticalScrollbarLayer))
                scrollingStateNode->setVerticalScrollbarLayer(layerTreeHost.layerForID(scrollingStateNode->verticalScrollbarLayer().layerID()));

            if (scrollingStateNode->hasChangedProperty(ScrollingStateNode::Property::HorizontalScrollbarLayer))
                scrollingStateNode->setHorizontalScrollbarLayer(layerTreeHost.layerForID(scrollingStateNode->horizontalScrollbarLayer().layerID()));
            break;
        }
        case ScrollingNodeType::PluginScrolling: {
            Ref scrollingStateNode = downcast<ScrollingStatePluginScrollingNode>(currNode);
            if (scrollingStateNode->hasChangedProperty(ScrollingStateNode::Property::ScrollContainerLayer))
                scrollingStateNode->setScrollContainerLayer(layerTreeHost.layerForID(scrollingStateNode->scrollContainerLayer().layerID()));

            if (scrollingStateNode->hasChangedProperty(ScrollingStateNode::Property::ScrolledContentsLayer))
                scrollingStateNode->setScrolledContentsLayer(layerTreeHost.layerForID(scrollingStateNode->scrolledContentsLayer().layerID()));

            if (scrollingStateNode->hasChangedProperty(ScrollingStateNode::Property::VerticalScrollbarLayer))
                scrollingStateNode->setVerticalScrollbarLayer(layerTreeHost.layerForID(scrollingStateNode->verticalScrollbarLayer().layerID()));

            if (scrollingStateNode->hasChangedProperty(ScrollingStateNode::Property::HorizontalScrollbarLayer))
                scrollingStateNode->setHorizontalScrollbarLayer(layerTreeHost.layerForID(scrollingStateNode->horizontalScrollbarLayer().layerID()));
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

void RemoteScrollingCoordinatorProxyMac::establishLayerTreeScrollingRelations(const RemoteLayerTreeHost&)
{
}

void RemoteScrollingCoordinatorProxyMac::displayDidRefresh(PlatformDisplayID displayID)
{
    m_eventDispatcher->mainThreadDisplayDidRefresh(displayID);
}

void RemoteScrollingCoordinatorProxyMac::windowScreenDidChange(PlatformDisplayID displayID, std::optional<FramesPerSecond> nominalFramesPerSecond)
{
    m_eventDispatcher->windowScreenDidChange(displayID, nominalFramesPerSecond);
}

void RemoteScrollingCoordinatorProxyMac::windowScreenWillChange()
{
    m_eventDispatcher->windowScreenWillChange();
}

void RemoteScrollingCoordinatorProxyMac::willCommitLayerAndScrollingTrees()
{
    scrollingTree().lockLayersForHitTesting();
#if ENABLE(THREADED_ANIMATIONS)
    m_eventDispatcher->lockForAnimationChanges();
#endif
}

void RemoteScrollingCoordinatorProxyMac::didCommitLayerAndScrollingTrees()
{
    scrollingTree().unlockLayersForHitTesting();
#if ENABLE(THREADED_ANIMATIONS)
    m_eventDispatcher->unlockForAnimationChanges();
#endif
}

void RemoteScrollingCoordinatorProxyMac::applyScrollingTreeLayerPositionsAfterCommit()
{
    RemoteScrollingCoordinatorProxy::applyScrollingTreeLayerPositionsAfterCommit();
    m_eventDispatcher->renderingUpdateComplete();
}

#if ENABLE(THREADED_ANIMATIONS)
void RemoteScrollingCoordinatorProxyMac::animationsWereAddedToNode(RemoteLayerTreeNode& node)
{
    m_eventDispatcher->animationsWereAddedToNode(node);
}

void RemoteScrollingCoordinatorProxyMac::animationsWereRemovedFromNode(RemoteLayerTreeNode& node)
{
    m_eventDispatcher->animationsWereRemovedFromNode(node);
}

void RemoteScrollingCoordinatorProxyMac::updateTimelineRegistration(WebCore::ProcessIdentifier processIdentifier, const HashSet<Ref<WebCore::AcceleratedTimeline>>& timelineRepresentations, MonotonicTime now)
{
    m_eventDispatcher->updateTimelineRegistration(processIdentifier, timelineRepresentations, now);
}

RefPtr<const RemoteAnimationTimeline> RemoteScrollingCoordinatorProxyMac::timeline(const TimelineID& timelineID) const
{
    return m_eventDispatcher->timeline(timelineID);
}

RefPtr<const RemoteAnimationStack> RemoteScrollingCoordinatorProxyMac::animationStackForNodeWithIDForTesting(WebCore::PlatformLayerIdentifier layerID) const
{
    m_eventDispatcher->lockForAnimationChanges();
    RefPtr animationStack = m_eventDispatcher->animationStackForNodeWithIDForTesting(layerID);
    m_eventDispatcher->unlockForAnimationChanges();
    return animationStack;
}
#endif

} // namespace WebKit

#endif // PLATFORM(MAC) && ENABLE(UI_SIDE_COMPOSITING)
