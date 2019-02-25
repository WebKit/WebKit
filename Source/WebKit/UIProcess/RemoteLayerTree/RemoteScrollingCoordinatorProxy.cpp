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

#include "config.h"
#include "RemoteScrollingCoordinatorProxy.h"

#if ENABLE(ASYNC_SCROLLING)

#include "ArgumentCoders.h"
#include "RemoteLayerTreeDrawingAreaProxy.h"
#include "RemoteScrollingCoordinator.h"
#include "RemoteScrollingCoordinatorMessages.h"
#include "RemoteScrollingCoordinatorTransaction.h"
#include "WebPageProxy.h"
#include "WebProcessProxy.h"
#include <WebCore/ScrollingStateFrameScrollingNode.h>
#include <WebCore/ScrollingStateOverflowScrollingNode.h>
#include <WebCore/ScrollingStateTree.h>
#include <WebCore/ScrollingTreeScrollingNode.h>

namespace WebKit {
using namespace WebCore;

RemoteScrollingCoordinatorProxy::RemoteScrollingCoordinatorProxy(WebPageProxy& webPageProxy)
    : m_webPageProxy(webPageProxy)
    , m_scrollingTree(RemoteScrollingTree::create(*this))
    , m_requestedScrollInfo(nullptr)
    , m_propagatesMainFrameScrolls(true)
{
}

RemoteScrollingCoordinatorProxy::~RemoteScrollingCoordinatorProxy()
{
}

ScrollingNodeID RemoteScrollingCoordinatorProxy::rootScrollingNodeID() const
{
    if (!m_scrollingTree->rootNode())
        return 0;

    return m_scrollingTree->rootNode()->scrollingNodeID();
}

const RemoteLayerTreeHost* RemoteScrollingCoordinatorProxy::layerTreeHost() const
{
    DrawingAreaProxy* drawingArea = m_webPageProxy.drawingArea();
    if (!is<RemoteLayerTreeDrawingAreaProxy>(drawingArea)) {
        ASSERT_NOT_REACHED();
        return nullptr;
    }

    RemoteLayerTreeDrawingAreaProxy& remoteDrawingArea = downcast<RemoteLayerTreeDrawingAreaProxy>(*drawingArea);
    return &remoteDrawingArea.remoteLayerTreeHost();
}

void RemoteScrollingCoordinatorProxy::commitScrollingTreeState(const RemoteScrollingCoordinatorTransaction& transaction, RequestedScrollInfo& requestedScrollInfo)
{
    m_requestedScrollInfo = &requestedScrollInfo;

    // FIXME: There must be a better idiom for this.
    std::unique_ptr<ScrollingStateTree> stateTree(const_cast<RemoteScrollingCoordinatorTransaction&>(transaction).scrollingStateTree().release());

    const RemoteLayerTreeHost* layerTreeHost = this->layerTreeHost();
    if (!layerTreeHost) {
        ASSERT_NOT_REACHED();
        return;
    }

    connectStateNodeLayers(*stateTree, *layerTreeHost);
    m_scrollingTree->commitTreeState(WTFMove(stateTree));

    m_requestedScrollInfo = nullptr;
}

#if !PLATFORM(IOS_FAMILY)

void RemoteScrollingCoordinatorProxy::connectStateNodeLayers(ScrollingStateTree& stateTree, const RemoteLayerTreeHost& layerTreeHost)
{
    for (auto& currNode : stateTree.nodeMap().values()) {
        if (currNode->hasChangedProperty(ScrollingStateNode::Layer))
            currNode->setLayer(layerTreeHost.layerForID(currNode->layer()));

        switch (currNode->nodeType()) {
        case ScrollingNodeType::MainFrame:
        case ScrollingNodeType::Subframe: {
            ScrollingStateFrameScrollingNode& scrollingStateNode = downcast<ScrollingStateFrameScrollingNode>(*currNode);
            
            if (scrollingStateNode.hasChangedProperty(ScrollingStateScrollingNode::ScrollContainerLayer))
                scrollingStateNode.setScrollContainerLayer(layerTreeHost.layerForID(scrollingStateNode.scrollContainerLayer()));

            if (scrollingStateNode.hasChangedProperty(ScrollingStateScrollingNode::ScrolledContentsLayer))
                scrollingStateNode.setScrolledContentsLayer(layerTreeHost.layerForID(scrollingStateNode.scrolledContentsLayer()));

            if (scrollingStateNode.hasChangedProperty(ScrollingStateFrameScrollingNode::CounterScrollingLayer))
                scrollingStateNode.setCounterScrollingLayer(layerTreeHost.layerForID(scrollingStateNode.counterScrollingLayer()));

            if (scrollingStateNode.hasChangedProperty(ScrollingStateFrameScrollingNode::InsetClipLayer))
                scrollingStateNode.setInsetClipLayer(layerTreeHost.layerForID(scrollingStateNode.insetClipLayer()));

            if (scrollingStateNode.hasChangedProperty(ScrollingStateFrameScrollingNode::ContentShadowLayer))
                scrollingStateNode.setContentShadowLayer(layerTreeHost.layerForID(scrollingStateNode.contentShadowLayer()));

            // FIXME: we should never have header and footer layers coming from the WebProcess.
            if (scrollingStateNode.hasChangedProperty(ScrollingStateFrameScrollingNode::HeaderLayer))
                scrollingStateNode.setHeaderLayer(layerTreeHost.layerForID(scrollingStateNode.headerLayer()));

            if (scrollingStateNode.hasChangedProperty(ScrollingStateFrameScrollingNode::FooterLayer))
                scrollingStateNode.setFooterLayer(layerTreeHost.layerForID(scrollingStateNode.footerLayer()));

            if (scrollingStateNode.hasChangedProperty(ScrollingStateFrameScrollingNode::VerticalScrollbarLayer))
                scrollingStateNode.setVerticalScrollbarLayer(layerTreeHost.layerForID(scrollingStateNode.verticalScrollbarLayer()));

            if (scrollingStateNode.hasChangedProperty(ScrollingStateFrameScrollingNode::HorizontalScrollbarLayer))
                scrollingStateNode.setHorizontalScrollbarLayer(layerTreeHost.layerForID(scrollingStateNode.horizontalScrollbarLayer()));

            if (scrollingStateNode.hasChangedProperty(ScrollingStateFrameScrollingNode::RootContentsLayer))
                scrollingStateNode.setRootContentsLayer(layerTreeHost.layerForID(scrollingStateNode.rootContentsLayer()));
            break;
        }
        case ScrollingNodeType::Overflow: {
            ScrollingStateOverflowScrollingNode& scrollingStateNode = downcast<ScrollingStateOverflowScrollingNode>(*currNode);
            if (scrollingStateNode.hasChangedProperty(ScrollingStateScrollingNode::ScrollContainerLayer))
                scrollingStateNode.setScrollContainerLayer(layerTreeHost.layerForID(scrollingStateNode.scrollContainerLayer()));

            if (scrollingStateNode.hasChangedProperty(ScrollingStateScrollingNode::ScrolledContentsLayer))
                scrollingStateNode.setScrolledContentsLayer(layerTreeHost.layerForID(scrollingStateNode.scrolledContentsLayer()));
            break;
        }
        case ScrollingNodeType::FrameHosting:
        case ScrollingNodeType::Fixed:
        case ScrollingNodeType::Sticky:
            break;
        }
    }
}
#endif

bool RemoteScrollingCoordinatorProxy::handleWheelEvent(const PlatformWheelEvent& event)
{
    ScrollingEventResult result = m_scrollingTree->tryToHandleWheelEvent(event);
    return result == ScrollingEventResult::DidHandleEvent; // FIXME: handle other values.
}

void RemoteScrollingCoordinatorProxy::handleMouseEvent(const WebCore::PlatformMouseEvent& event)
{
    m_scrollingTree->handleMouseEvent(event);
}

TrackingType RemoteScrollingCoordinatorProxy::eventTrackingTypeForPoint(const AtomicString& eventName, IntPoint p) const
{
    return m_scrollingTree->eventTrackingTypeForPoint(eventName, p);
}

void RemoteScrollingCoordinatorProxy::viewportChangedViaDelegatedScrolling(const FloatRect& layoutViewport, double scale)
{
    m_scrollingTree->mainFrameViewportChangedViaDelegatedScrolling(layoutViewport, scale);
}

void RemoteScrollingCoordinatorProxy::currentSnapPointIndicesDidChange(WebCore::ScrollingNodeID nodeID, unsigned horizontal, unsigned vertical)
{
    m_webPageProxy.send(Messages::RemoteScrollingCoordinator::CurrentSnapPointIndicesChangedForNode(nodeID, horizontal, vertical));
}

// This comes from the scrolling tree.
void RemoteScrollingCoordinatorProxy::scrollingTreeNodeDidScroll(ScrollingNodeID scrolledNodeID, const FloatPoint& newScrollPosition, const Optional<FloatPoint>& layoutViewportOrigin, ScrollingLayerPositionAction scrollingLayerPositionAction)
{
    // Scroll updates for the main frame are sent via WebPageProxy::updateVisibleContentRects()
    // so don't send them here.
    if (!m_propagatesMainFrameScrolls && scrolledNodeID == rootScrollingNodeID())
        return;

#if PLATFORM(IOS_FAMILY)
    m_webPageProxy.scrollingNodeScrollViewDidScroll();
#endif
    m_webPageProxy.send(Messages::RemoteScrollingCoordinator::ScrollPositionChangedForNode(scrolledNodeID, newScrollPosition, scrollingLayerPositionAction == ScrollingLayerPositionAction::Sync));
}

void RemoteScrollingCoordinatorProxy::scrollingTreeNodeRequestsScroll(ScrollingNodeID scrolledNodeID, const FloatPoint& scrollPosition, bool representsProgrammaticScroll)
{
    if (scrolledNodeID == rootScrollingNodeID() && m_requestedScrollInfo) {
        m_requestedScrollInfo->requestsScrollPositionUpdate = true;
        m_requestedScrollInfo->requestIsProgrammaticScroll = representsProgrammaticScroll;
        m_requestedScrollInfo->requestedScrollPosition = scrollPosition;
    }
}

String RemoteScrollingCoordinatorProxy::scrollingTreeAsText() const
{
    if (m_scrollingTree)
        return m_scrollingTree->scrollingTreeAsText();
    
    return emptyString();
}

#if ENABLE(POINTER_EVENTS)
Optional<TouchActionData> RemoteScrollingCoordinatorProxy::touchActionDataAtPoint(const IntPoint p) const
{
    return m_scrollingTree->touchActionDataAtPoint(p);
}

Optional<TouchActionData> RemoteScrollingCoordinatorProxy::touchActionDataForScrollNodeID(ScrollingNodeID scrollingNodeID) const
{
    for (auto& touchActionData : m_touchActionDataByTouchIdentifier.values()) {
        if (touchActionData.scrollingNodeID == scrollingNodeID)
            return touchActionData;
    }
    return WTF::nullopt;
}

void RemoteScrollingCoordinatorProxy::setTouchDataForTouchIdentifier(TouchActionData touchActionData, unsigned touchIdentifier)
{
    m_touchActionDataByTouchIdentifier.set(touchIdentifier, touchActionData);
}

void RemoteScrollingCoordinatorProxy::clearTouchDataForTouchIdentifier(unsigned touchIdentifier)
{
    m_touchActionDataByTouchIdentifier.remove(touchIdentifier);
}

#endif

} // namespace WebKit

#endif // ENABLE(ASYNC_SCROLLING)
