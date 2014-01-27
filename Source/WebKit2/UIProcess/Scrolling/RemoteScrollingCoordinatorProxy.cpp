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

    RemoteLayerTreeDrawingAreaProxy* remoteDrawingArea = static_cast<RemoteLayerTreeDrawingAreaProxy*>(drawingArea);
    return &remoteDrawingArea->remoteLayerTreeHost();
}

void RemoteScrollingCoordinatorProxy::updateScrollingTree(const RemoteScrollingCoordinatorTransaction& transaction)
{
    OwnPtr<ScrollingStateTree> stateTree = const_cast<RemoteScrollingCoordinatorTransaction&>(transaction).scrollingStateTree().release();
    
    const RemoteLayerTreeHost* layerTreeHost = this->layerTreeHost();
    if (!layerTreeHost) {
        ASSERT_NOT_REACHED();
        return;
    }
    
    connectStateNodeLayers(*stateTree, *layerTreeHost);
    m_scrollingTree->commitNewTreeState(stateTree.release());
}

void RemoteScrollingCoordinatorProxy::connectStateNodeLayers(ScrollingStateTree& stateTree, const RemoteLayerTreeHost& layerTreeHost)
{
    for (auto it : stateTree.nodeMap()) {
        ScrollingStateNode* currNode = it.value;
        switch (currNode->nodeType()) {
        case ScrollingNode: {
            ScrollingStateScrollingNode* scrollingStateNode = toScrollingStateScrollingNode(currNode);
            
            if (scrollingStateNode->hasChangedProperty(ScrollingStateNode::ScrollLayer))
                scrollingStateNode->setLayer(layerTreeHost.getLayer(scrollingStateNode->layer()));

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
            if (currNode->hasChangedProperty(ScrollingStateNode::ScrollLayer))
                currNode->setLayer(layerTreeHost.getLayer(currNode->layer()));
            break;
        case StickyNode:
            if (currNode->hasChangedProperty(ScrollingStateNode::ScrollLayer))
                currNode->setLayer(layerTreeHost.getLayer(currNode->layer()));
            break;
        }
    }
}

bool RemoteScrollingCoordinatorProxy::handleWheelEvent(const PlatformWheelEvent& event)
{
    ScrollingTree::EventResult result = m_scrollingTree->tryToHandleWheelEvent(event);
    return result == ScrollingTree::DidHandleEvent; // FIXME: handle other values.
}

void RemoteScrollingCoordinatorProxy::scrollPositionChangedViaDelegatedScrolling(ScrollingNodeID nodeID, const IntPoint& offset)
{
    m_scrollingTree->scrollPositionChangedViaDelegatedScrolling(nodeID, offset);
}

void RemoteScrollingCoordinatorProxy::scrollPositionChanged(WebCore::ScrollingNodeID scrolledNodeID, const WebCore::FloatPoint& newScrollPosition)
{
    m_webPageProxy.send(Messages::RemoteScrollingCoordinator::ScrollPositionChangedForNode(scrolledNodeID, newScrollPosition));
}

} // namespace WebKit

#endif // ENABLE(ASYNC_SCROLLING)
