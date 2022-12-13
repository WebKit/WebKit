/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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
#import "WebPageProxy.h"
#import <WebCore/ScrollingStateFrameScrollingNode.h>
#import <WebCore/ScrollingStateOverflowScrollProxyNode.h>
#import <WebCore/ScrollingStateOverflowScrollingNode.h>
#import <WebCore/ScrollingStatePositionedNode.h>
#import <WebCore/ScrollingStateTree.h>
#import <WebCore/ScrollingTreeFrameScrollingNode.h>
#import <WebCore/ScrollingTreeOverflowScrollProxyNode.h>
#import <WebCore/ScrollingTreeOverflowScrollingNode.h>
#import <WebCore/ScrollingTreePositionedNode.h>
#import <WebCore/WheelEventDeltaFilter.h>

namespace WebKit {
using namespace WebCore;

RemoteScrollingCoordinatorProxyMac::RemoteScrollingCoordinatorProxyMac(WebPageProxy& webPageProxy)
    : RemoteScrollingCoordinatorProxy(webPageProxy)
    , m_recentWheelEventDeltaFilter(WheelEventDeltaFilter::create())
{
}

PlatformWheelEvent RemoteScrollingCoordinatorProxyMac::filteredWheelEvent(const WebCore::PlatformWheelEvent& wheelEvent)
{
    m_recentWheelEventDeltaFilter->updateFromEvent(wheelEvent);

    auto filteredEvent = wheelEvent;
    if (WheelEventDeltaFilter::shouldApplyFilteringForEvent(wheelEvent))
        filteredEvent = m_recentWheelEventDeltaFilter->eventCopyWithFilteredDeltas(wheelEvent);
    else if (WheelEventDeltaFilter::shouldIncludeVelocityForEvent(wheelEvent))
        filteredEvent = m_recentWheelEventDeltaFilter->eventCopyWithVelocity(wheelEvent);

    return filteredEvent;
}

void RemoteScrollingCoordinatorProxyMac::didReceiveWheelEvent(bool /* wasHandled */)
{
    scrollingTree()->applyLayerPositions();
}

void RemoteScrollingCoordinatorProxyMac::displayDidRefresh(PlatformDisplayID displayID)
{
    RemoteScrollingCoordinatorProxy::displayDidRefresh(displayID);
    scrollingTree()->applyLayerPositions();
}

bool RemoteScrollingCoordinatorProxyMac::scrollingTreeNodeRequestsScroll(WebCore::ScrollingNodeID, const WebCore::RequestedScrollData&)
{
    // Unlike iOS, we handle scrolling requests for the main frame in the same way we handle them for subscrollers.
    return false;
}

void RemoteScrollingCoordinatorProxyMac::hasNodeWithAnimatedScrollChanged(bool hasAnimatedScrolls)
{
    auto* drawingArea = dynamicDowncast<RemoteLayerTreeDrawingAreaProxy>(webPageProxy().drawingArea());
    if (!drawingArea)
        return;

    drawingArea->setDisplayLinkWantsFullSpeedUpdates(hasAnimatedScrolls);
}

void RemoteScrollingCoordinatorProxyMac::connectStateNodeLayers(ScrollingStateTree& stateTree, const RemoteLayerTreeHost& layerTreeHost)
{
    using PlatformLayerID = GraphicsLayer::PlatformLayerID;

    for (auto& currNode : stateTree.nodeMap().values()) {
        if (currNode->hasChangedProperty(ScrollingStateNode::Property::Layer))
            currNode->setLayer(layerTreeHost.layerForID(PlatformLayerID { currNode->layer() }));

        switch (currNode->nodeType()) {
        case ScrollingNodeType::MainFrame:
        case ScrollingNodeType::Subframe: {
            ScrollingStateFrameScrollingNode& scrollingStateNode = downcast<ScrollingStateFrameScrollingNode>(*currNode);
            
            if (scrollingStateNode.hasChangedProperty(ScrollingStateNode::Property::ScrollContainerLayer))
                scrollingStateNode.setScrollContainerLayer(layerTreeHost.layerForID(PlatformLayerID { scrollingStateNode.scrollContainerLayer() }));

            if (scrollingStateNode.hasChangedProperty(ScrollingStateNode::Property::ScrolledContentsLayer))
                scrollingStateNode.setScrolledContentsLayer(layerTreeHost.layerForID(PlatformLayerID { scrollingStateNode.scrolledContentsLayer() }));

            if (scrollingStateNode.hasChangedProperty(ScrollingStateNode::Property::CounterScrollingLayer))
                scrollingStateNode.setCounterScrollingLayer(layerTreeHost.layerForID(PlatformLayerID { scrollingStateNode.counterScrollingLayer() }));

            if (scrollingStateNode.hasChangedProperty(ScrollingStateNode::Property::InsetClipLayer))
                scrollingStateNode.setInsetClipLayer(layerTreeHost.layerForID(PlatformLayerID { scrollingStateNode.insetClipLayer() }));

            if (scrollingStateNode.hasChangedProperty(ScrollingStateNode::Property::ContentShadowLayer))
                scrollingStateNode.setContentShadowLayer(layerTreeHost.layerForID(PlatformLayerID { scrollingStateNode.contentShadowLayer() }));

            // FIXME: we should never have header and footer layers coming from the WebProcess.
            if (scrollingStateNode.hasChangedProperty(ScrollingStateNode::Property::HeaderLayer))
                scrollingStateNode.setHeaderLayer(layerTreeHost.layerForID(PlatformLayerID { scrollingStateNode.headerLayer() }));

            if (scrollingStateNode.hasChangedProperty(ScrollingStateNode::Property::FooterLayer))
                scrollingStateNode.setFooterLayer(layerTreeHost.layerForID(PlatformLayerID { scrollingStateNode.footerLayer() }));

            if (scrollingStateNode.hasChangedProperty(ScrollingStateNode::Property::VerticalScrollbarLayer))
                scrollingStateNode.setVerticalScrollbarLayer(layerTreeHost.layerForID(PlatformLayerID { scrollingStateNode.verticalScrollbarLayer() }));

            if (scrollingStateNode.hasChangedProperty(ScrollingStateNode::Property::HorizontalScrollbarLayer))
                scrollingStateNode.setHorizontalScrollbarLayer(layerTreeHost.layerForID(PlatformLayerID { scrollingStateNode.horizontalScrollbarLayer() }));

            if (scrollingStateNode.hasChangedProperty(ScrollingStateNode::Property::RootContentsLayer))
                scrollingStateNode.setRootContentsLayer(layerTreeHost.layerForID(PlatformLayerID { scrollingStateNode.rootContentsLayer() }));
            break;
        }
        case ScrollingNodeType::Overflow: {
            ScrollingStateOverflowScrollingNode& scrollingStateNode = downcast<ScrollingStateOverflowScrollingNode>(*currNode);
            if (scrollingStateNode.hasChangedProperty(ScrollingStateNode::Property::ScrollContainerLayer))
                scrollingStateNode.setScrollContainerLayer(layerTreeHost.layerForID(PlatformLayerID { scrollingStateNode.scrollContainerLayer() }));

            if (scrollingStateNode.hasChangedProperty(ScrollingStateNode::Property::ScrolledContentsLayer))
                scrollingStateNode.setScrolledContentsLayer(layerTreeHost.layerForID(PlatformLayerID { scrollingStateNode.scrolledContentsLayer() }));

            if (scrollingStateNode.hasChangedProperty(ScrollingStateNode::Property::VerticalScrollbarLayer))
                scrollingStateNode.setVerticalScrollbarLayer(layerTreeHost.layerForID(PlatformLayerID { scrollingStateNode.verticalScrollbarLayer() }));

            if (scrollingStateNode.hasChangedProperty(ScrollingStateNode::Property::HorizontalScrollbarLayer))
                scrollingStateNode.setHorizontalScrollbarLayer(layerTreeHost.layerForID(PlatformLayerID { scrollingStateNode.horizontalScrollbarLayer() }));
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

void RemoteScrollingCoordinatorProxyMac::establishLayerTreeScrollingRelations(const RemoteLayerTreeHost&)
{
}

} // namespace WebKit

#endif // PLATFORM(MAC) && ENABLE(UI_SIDE_COMPOSITING)
