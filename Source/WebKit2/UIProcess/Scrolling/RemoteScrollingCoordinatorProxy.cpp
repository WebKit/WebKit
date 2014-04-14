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
#include "RemoteScrollingTree.h"
#include "WebPageProxy.h"
#include "WebProcessProxy.h"
#include <WebCore/ScrollingStateTree.h>
#include <WebCore/ScrollingTreeScrollingNode.h>

using namespace WebCore;

namespace WebKit {

RemoteScrollingCoordinatorProxy::RemoteScrollingCoordinatorProxy(WebPageProxy& webPageProxy)
    : m_webPageProxy(webPageProxy)
    , m_scrollingTree(RemoteScrollingTree::create(*this))
    , m_propagatesMainFrameScrolls(true)
{
}

RemoteScrollingCoordinatorProxy::~RemoteScrollingCoordinatorProxy()
{
}

WebCore::ScrollingNodeID RemoteScrollingCoordinatorProxy::rootScrollingNodeID() const
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

void RemoteScrollingCoordinatorProxy::updateScrollingTree(const RemoteScrollingCoordinatorTransaction& transaction, bool& fixedOrStickyLayerChanged)
{
    OwnPtr<ScrollingStateTree> stateTree = const_cast<RemoteScrollingCoordinatorTransaction&>(transaction).scrollingStateTree().release();
    
    const RemoteLayerTreeHost* layerTreeHost = this->layerTreeHost();
    if (!layerTreeHost) {
        ASSERT_NOT_REACHED();
        return;
    }

    connectStateNodeLayers(*stateTree, *layerTreeHost, fixedOrStickyLayerChanged);
    m_scrollingTree->commitNewTreeState(stateTree.release());
}

#if !PLATFORM(IOS)
void RemoteScrollingCoordinatorProxy::connectStateNodeLayers(ScrollingStateTree& stateTree, const RemoteLayerTreeHost& layerTreeHost, bool& fixedOrStickyLayerChanged)
{
    for (auto& currNode : stateTree.nodeMap().values()) {
        switch (currNode->nodeType()) {
        case FrameScrollingNode:
        case OverflowScrollingNode: {
            ScrollingStateScrollingNode* scrollingStateNode = toScrollingStateScrollingNode(currNode);
            
            if (scrollingStateNode->hasChangedProperty(ScrollingStateNode::ScrollLayer))
                scrollingStateNode->setLayer(layerTreeHost.getLayer(scrollingStateNode->layer()));

            if (scrollingStateNode->hasChangedProperty(ScrollingStateScrollingNode::ScrolledContentsLayer))
                scrollingStateNode->setScrolledContentsLayer(layerTreeHost.getLayer(scrollingStateNode->scrolledContentsLayer()));

            if (scrollingStateNode->hasChangedProperty(ScrollingStateScrollingNode::CounterScrollingLayer))
                scrollingStateNode->setCounterScrollingLayer(layerTreeHost.getLayer(scrollingStateNode->counterScrollingLayer()));

            // FIXME: we should never have header and footer layers coming from the WebProcess.
            if (scrollingStateNode->hasChangedProperty(ScrollingStateScrollingNode::HeaderLayer))
                scrollingStateNode->setHeaderLayer(layerTreeHost.getLayer(scrollingStateNode->headerLayer()));

            if (scrollingStateNode->hasChangedProperty(ScrollingStateScrollingNode::FooterLayer))
                scrollingStateNode->setFooterLayer(layerTreeHost.getLayer(scrollingStateNode->footerLayer()));
            break;
        }
        case FixedNode:
            if (currNode->hasChangedProperty(ScrollingStateNode::ScrollLayer)) {
                currNode->setLayer(layerTreeHost.getLayer(currNode->layer()));
                fixedOrStickyLayerChanged = true;
            }
            break;
        case StickyNode:
            if (currNode->hasChangedProperty(ScrollingStateNode::ScrollLayer)) {
                currNode->setLayer(layerTreeHost.getLayer(currNode->layer()));
                fixedOrStickyLayerChanged = true;
            }
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

bool RemoteScrollingCoordinatorProxy::isPointInNonFastScrollableRegion(const WebCore::IntPoint& p) const
{
    return m_scrollingTree->isPointInNonFastScrollableRegion(p);
}

void RemoteScrollingCoordinatorProxy::viewportChangedViaDelegatedScrolling(ScrollingNodeID nodeID, const WebCore::FloatRect& viewportRect, double scale)
{
    m_scrollingTree->viewportChangedViaDelegatedScrolling(nodeID, viewportRect, scale);
}

// This comes from the scrolling tree.
void RemoteScrollingCoordinatorProxy::scrollingTreeNodeDidScroll(WebCore::ScrollingNodeID scrolledNodeID, const WebCore::FloatPoint& newScrollPosition)
{
    // Scroll updates for the main frame are sent via WebPageProxy::updateVisibleContentRects()
    // so don't send them here.
    if (!m_propagatesMainFrameScrolls && scrolledNodeID == rootScrollingNodeID())
        return;

    m_webPageProxy.send(Messages::RemoteScrollingCoordinator::ScrollPositionChangedForNode(scrolledNodeID, newScrollPosition));
}

void RemoteScrollingCoordinatorProxy::scrollingTreeNodeRequestsScroll(ScrollingNodeID scrolledNodeID, const FloatPoint& scrollPosition, bool representsProgrammaticScroll)
{
    if (scrolledNodeID == rootScrollingNodeID())
        m_webPageProxy.requestScroll(scrollPosition, representsProgrammaticScroll);
}

} // namespace WebKit

#endif // ENABLE(ASYNC_SCROLLING)
