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
#include "MessageDecoder.h"
#include "MessageEncoder.h"
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

using namespace WebCore;

namespace WebKit {

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
    if (!drawingArea || drawingArea->type() != DrawingAreaTypeRemoteLayerTree) {
        ASSERT_NOT_REACHED();
        return nullptr;
    }

    RemoteLayerTreeDrawingAreaProxy* remoteDrawingArea = toRemoteLayerTreeDrawingAreaProxy(drawingArea);
    return &remoteDrawingArea->remoteLayerTreeHost();
}

void RemoteScrollingCoordinatorProxy::updateScrollingTree(const RemoteScrollingCoordinatorTransaction& transaction, RequestedScrollInfo& requestedScrollInfo)
{
    m_requestedScrollInfo = &requestedScrollInfo;

    OwnPtr<ScrollingStateTree> stateTree = const_cast<RemoteScrollingCoordinatorTransaction&>(transaction).scrollingStateTree().release();
    
    const RemoteLayerTreeHost* layerTreeHost = this->layerTreeHost();
    if (!layerTreeHost) {
        ASSERT_NOT_REACHED();
        return;
    }

    connectStateNodeLayers(*stateTree, *layerTreeHost);
    m_scrollingTree->commitNewTreeState(stateTree.release());

    m_requestedScrollInfo = nullptr;
}

#if !PLATFORM(IOS)
void RemoteScrollingCoordinatorProxy::connectStateNodeLayers(ScrollingStateTree& stateTree, const RemoteLayerTreeHost& layerTreeHost)
{
    for (auto& currNode : stateTree.nodeMap().values()) {
        if (currNode->hasChangedProperty(ScrollingStateNode::ScrollLayer))
            currNode->setLayer(layerTreeHost.getLayer(currNode->layer()));

        switch (currNode->nodeType()) {
        case FrameScrollingNode: {
            ScrollingStateFrameScrollingNode* scrollingStateNode = toScrollingStateFrameScrollingNode(currNode);
            
            if (scrollingStateNode->hasChangedProperty(ScrollingStateFrameScrollingNode::ScrolledContentsLayer))
                scrollingStateNode->setScrolledContentsLayer(layerTreeHost.getLayer(scrollingStateNode->scrolledContentsLayer()));

            if (scrollingStateNode->hasChangedProperty(ScrollingStateFrameScrollingNode::CounterScrollingLayer))
                scrollingStateNode->setCounterScrollingLayer(layerTreeHost.getLayer(scrollingStateNode->counterScrollingLayer()));

            if (scrollingStateNode->hasChangedProperty(ScrollingStateFrameScrollingNode::InsetClipLayer))
                scrollingStateNode->setInsetClipLayer(layerTreeHost.getLayer(scrollingStateNode->insetClipLayer()));

            if (scrollingStateNode->hasChangedProperty(ScrollingStateFrameScrollingNode::ContentShadowLayer))
                scrollingStateNode->setContentShadowLayer(layerTreeHost.getLayer(scrollingStateNode->contentShadowLayer()));

            // FIXME: we should never have header and footer layers coming from the WebProcess.
            if (scrollingStateNode->hasChangedProperty(ScrollingStateFrameScrollingNode::HeaderLayer))
                scrollingStateNode->setHeaderLayer(layerTreeHost.getLayer(scrollingStateNode->headerLayer()));

            if (scrollingStateNode->hasChangedProperty(ScrollingStateFrameScrollingNode::FooterLayer))
                scrollingStateNode->setFooterLayer(layerTreeHost.getLayer(scrollingStateNode->footerLayer()));
            break;
        }
        case OverflowScrollingNode: {
            ScrollingStateOverflowScrollingNode* scrollingStateNode = toScrollingStateOverflowScrollingNode(currNode);

            if (scrollingStateNode->hasChangedProperty(ScrollingStateOverflowScrollingNode::ScrolledContentsLayer))
                scrollingStateNode->setScrolledContentsLayer(layerTreeHost.getLayer(scrollingStateNode->scrolledContentsLayer()));
            break;
        }
        case FixedNode:
        case StickyNode:
            break;
        }
    }
}
#endif

bool RemoteScrollingCoordinatorProxy::handleWheelEvent(const PlatformWheelEvent& event)
{
    ScrollingTree::EventResult result = m_scrollingTree->tryToHandleWheelEvent(event);
    return result == ScrollingTree::DidHandleEvent; // FIXME: handle other values.
}

bool RemoteScrollingCoordinatorProxy::isPointInNonFastScrollableRegion(const IntPoint& p) const
{
    return m_scrollingTree->isPointInNonFastScrollableRegion(p);
}

void RemoteScrollingCoordinatorProxy::viewportChangedViaDelegatedScrolling(ScrollingNodeID nodeID, const FloatRect& fixedPositionRect, double scale)
{
    m_scrollingTree->viewportChangedViaDelegatedScrolling(nodeID, fixedPositionRect, scale);
}

// This comes from the scrolling tree.
void RemoteScrollingCoordinatorProxy::scrollingTreeNodeDidScroll(ScrollingNodeID scrolledNodeID, const FloatPoint& newScrollPosition, SetOrSyncScrollingLayerPosition scrollingLayerPositionAction)
{
    // Scroll updates for the main frame are sent via WebPageProxy::updateVisibleContentRects()
    // so don't send them here.
    if (!m_propagatesMainFrameScrolls && scrolledNodeID == rootScrollingNodeID())
        return;

#if PLATFORM(IOS)
    m_webPageProxy.overflowScrollViewDidScroll();
#endif
    m_webPageProxy.send(Messages::RemoteScrollingCoordinator::ScrollPositionChangedForNode(scrolledNodeID, newScrollPosition, scrollingLayerPositionAction));
}

void RemoteScrollingCoordinatorProxy::scrollingTreeNodeRequestsScroll(ScrollingNodeID scrolledNodeID, const FloatPoint& scrollPosition, bool representsProgrammaticScroll)
{
    if (scrolledNodeID == rootScrollingNodeID() && m_requestedScrollInfo) {
        m_requestedScrollInfo->requestsScrollPositionUpdate = true;
        m_requestedScrollInfo->requestIsProgrammaticScroll = representsProgrammaticScroll;
        m_requestedScrollInfo->requestedScrollPosition = scrollPosition;
    }
}

} // namespace WebKit

#endif // ENABLE(ASYNC_SCROLLING)
