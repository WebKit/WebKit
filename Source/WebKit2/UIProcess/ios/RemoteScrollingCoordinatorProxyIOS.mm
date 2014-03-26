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

#if PLATFORM(IOS)
#if ENABLE(ASYNC_SCROLLING)

#include "LayerRepresentation.h"
#include "RemoteLayerTreeHost.h"
#include <WebCore/ScrollingStateTree.h>
#include <UIKit/UIView.h>

using namespace WebCore;

namespace WebKit {

static LayerRepresentation layerRepresentationFromLayerOrView(LayerOrView *layerOrView)
{
    return LayerRepresentation(layerOrView.layer);
}

void RemoteScrollingCoordinatorProxy::connectStateNodeLayers(ScrollingStateTree& stateTree, const RemoteLayerTreeHost& layerTreeHost, bool& fixedOrStickyLayerChanged)
{
    for (auto& currNode : stateTree.nodeMap().values()) {
        switch (currNode->nodeType()) {
        case FrameScrollingNode:
        case OverflowScrollingNode: {
            ScrollingStateScrollingNode* scrollingStateNode = toScrollingStateScrollingNode(currNode);
            
            if (scrollingStateNode->hasChangedProperty(ScrollingStateNode::ScrollLayer))
                scrollingStateNode->setLayer(layerRepresentationFromLayerOrView(layerTreeHost.getLayer(scrollingStateNode->layer())));

            if (scrollingStateNode->hasChangedProperty(ScrollingStateScrollingNode::ScrolledContentsLayer))
                scrollingStateNode->setScrolledContentsLayer(layerRepresentationFromLayerOrView(layerTreeHost.getLayer(scrollingStateNode->scrolledContentsLayer())));

            if (scrollingStateNode->hasChangedProperty(ScrollingStateScrollingNode::CounterScrollingLayer))
                scrollingStateNode->setCounterScrollingLayer(layerRepresentationFromLayerOrView(layerTreeHost.getLayer(scrollingStateNode->counterScrollingLayer())));

            // FIXME: we should never have header and footer layers coming from the WebProcess.
            if (scrollingStateNode->hasChangedProperty(ScrollingStateScrollingNode::HeaderLayer))
                scrollingStateNode->setHeaderLayer(layerRepresentationFromLayerOrView(layerTreeHost.getLayer(scrollingStateNode->headerLayer())));

            if (scrollingStateNode->hasChangedProperty(ScrollingStateScrollingNode::FooterLayer))
                scrollingStateNode->setFooterLayer(layerRepresentationFromLayerOrView(layerTreeHost.getLayer(scrollingStateNode->footerLayer())));
            break;
        }
        case FixedNode:
            if (currNode->hasChangedProperty(ScrollingStateNode::ScrollLayer)) {
                currNode->setLayer(layerRepresentationFromLayerOrView(layerTreeHost.getLayer(currNode->layer())));
                fixedOrStickyLayerChanged = true;
            }
            break;
        case StickyNode:
            if (currNode->hasChangedProperty(ScrollingStateNode::ScrollLayer)) {
                currNode->setLayer(layerRepresentationFromLayerOrView(layerTreeHost.getLayer(currNode->layer())));
                fixedOrStickyLayerChanged = true;
            }
            break;
        }
    }
}

} // namespace WebKit


#endif // ENABLE(ASYNC_SCROLLING)
#endif // PLATFORM(IOS)
