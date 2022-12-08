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

#import "config.h"
#import "RemoteScrollingTreeMac.h"

#if PLATFORM(MAC) && ENABLE(UI_SIDE_COMPOSITING)

#import "Logging.h"
#import "RemoteLayerTreeNode.h"
#import "RemoteScrollingCoordinatorProxy.h"
#import "ScrollingTreeFrameScrollingNodeRemoteMac.h"
#import "ScrollingTreeOverflowScrollingNodeRemoteMac.h"
#import <WebCore/ScrollingTreeFixedNodeCocoa.h>
#import <WebCore/ScrollingTreeOverflowScrollProxyNode.h>
#import <WebCore/ScrollingTreePositionedNode.h>
#import <WebCore/WebCoreCALayerExtras.h>
#import <wtf/text/TextStream.h>

namespace WebKit {
using namespace WebCore;

Ref<RemoteScrollingTree> RemoteScrollingTree::create(RemoteScrollingCoordinatorProxy& scrollingCoordinator)
{
    return adoptRef(*new RemoteScrollingTreeMac(scrollingCoordinator));
}

RemoteScrollingTreeMac::RemoteScrollingTreeMac(RemoteScrollingCoordinatorProxy& scrollingCoordinator)
    : RemoteScrollingTree(scrollingCoordinator)
{
}

RemoteScrollingTreeMac::~RemoteScrollingTreeMac() = default;

void RemoteScrollingTreeMac::handleWheelEventPhase(ScrollingNodeID, PlatformWheelEventPhase)
{
    // FIXME: Is this needed?
}

void RemoteScrollingTreeMac::hasNodeWithAnimatedScrollChanged(bool hasNodeWithAnimatedScroll)
{
    m_scrollingCoordinatorProxy.hasNodeWithAnimatedScrollChanged(hasNodeWithAnimatedScroll);
}

void RemoteScrollingTreeMac::displayDidRefresh(PlatformDisplayID)
{
    Locker locker { m_treeLock };
    serviceScrollAnimations(MonotonicTime::now()); // FIXME: Share timestamp with the rest of the update.
}

Ref<ScrollingTreeNode> RemoteScrollingTreeMac::createScrollingTreeNode(ScrollingNodeType nodeType, ScrollingNodeID nodeID)
{
    switch (nodeType) {
    case ScrollingNodeType::MainFrame:
    case ScrollingNodeType::Subframe:
        return ScrollingTreeFrameScrollingNodeRemoteMac::create(*this, nodeType, nodeID);

    case ScrollingNodeType::Overflow:
        return ScrollingTreeOverflowScrollingNodeRemoteMac::create(*this, nodeID);

    case ScrollingNodeType::FrameHosting:
    case ScrollingNodeType::OverflowProxy:
    case ScrollingNodeType::Fixed:
    case ScrollingNodeType::Sticky:
    case ScrollingNodeType::Positioned:
        return RemoteScrollingTree::createScrollingTreeNode(nodeType, nodeID);
    }
    ASSERT_NOT_REACHED();
    return ScrollingTreeFixedNodeCocoa::create(*this, nodeID);
}

void RemoteScrollingTreeMac::handleMouseEvent(const WebCore::PlatformMouseEvent& event)
{
    if (!rootNode())
        return;
    static_cast<ScrollingTreeFrameScrollingNodeRemoteMac&>(*rootNode()).handleMouseEvent(event);
}

static ScrollingNodeID scrollingNodeIDForLayer(CALayer *layer)
{
    auto* layerTreeNode = RemoteLayerTreeNode::forCALayer(layer);
    if (!layerTreeNode)
        return 0;

    return layerTreeNode->scrollingNodeID();
}

static bool isScrolledBy(const ScrollingTree& tree, ScrollingNodeID scrollingNodeID, CALayer *hitLayer)
{
    for (CALayer *layer = hitLayer; layer; layer = [layer superlayer]) {
        auto nodeID = scrollingNodeIDForLayer(layer);
        if (nodeID == scrollingNodeID)
            return true;

        auto* scrollingNode = tree.nodeForID(nodeID);
        if (is<ScrollingTreeOverflowScrollProxyNode>(scrollingNode)) {
            ScrollingNodeID actingOverflowScrollingNodeID = downcast<ScrollingTreeOverflowScrollProxyNode>(*scrollingNode).overflowScrollingNodeID();
            if (actingOverflowScrollingNodeID == scrollingNodeID)
                return true;
        }

        if (is<ScrollingTreePositionedNode>(scrollingNode)) {
            if (downcast<ScrollingTreePositionedNode>(*scrollingNode).relatedOverflowScrollingNodes().contains(scrollingNodeID))
                return false;
        }
    }

    return false;
}

static const EventRegion* eventRegionForLayer(CALayer *layer)
{
    auto* layerTreeNode = RemoteLayerTreeNode::forCALayer(layer);
    if (!layerTreeNode)
        return nullptr;

    return &layerTreeNode->eventRegion();
}

static bool layerEventRegionContainsPoint(CALayer *layer, CGPoint localPoint)
{
    auto* eventRegion = eventRegionForLayer(layer);
    if (!eventRegion)
        return false;

    // Scrolling changes boundsOrigin on the scroll container layer, but we computed its event region ignoring scroll position, so factor out bounds origin.
    FloatPoint boundsOrigin = layer.bounds.origin;
    FloatPoint originRelativePoint = localPoint - toFloatSize(boundsOrigin);
    return eventRegion->contains(roundedIntPoint(originRelativePoint));
}

RefPtr<ScrollingTreeNode> RemoteScrollingTreeMac::scrollingNodeForPoint(FloatPoint point)
{
    auto* rootScrollingNode = rootNode();
    if (!rootScrollingNode)
        return nullptr;

    RetainPtr scrolledContentsLayer { static_cast<CALayer*>(rootScrollingNode->scrolledContentsLayer()) };
    auto scrollPosition = rootScrollingNode->currentScrollPosition();
    auto pointInContentsLayer = point;
    pointInContentsLayer.moveBy(scrollPosition);

    Vector<LayerAndPoint, 16> layersAtPoint;
    collectDescendantLayersAtPoint(layersAtPoint, scrolledContentsLayer.get(), pointInContentsLayer, layerEventRegionContainsPoint);

    LOG_WITH_STREAM(UIHitTesting, stream << "RemoteScrollingTreeMac " << this << " scrollingNodeForPoint " << point << " found " << layersAtPoint.size() << " layers");
#if !LOG_DISABLED
    for (auto [layer, point] : WTF::makeReversedRange(layersAtPoint))
        LOG_WITH_STREAM(UIHitTesting, stream << " layer " << [layer description] << " scrolling node " << scrollingNodeIDForLayer(layer));
#endif

    if (layersAtPoint.size()) {
        auto* frontmostLayer = layersAtPoint.last().first;
        for (auto [layer, point] : WTF::makeReversedRange(layersAtPoint)) {
            auto nodeID = scrollingNodeIDForLayer(layer);

            auto* scrollingNode = nodeForID(nodeID);
            if (!is<ScrollingTreeScrollingNode>(scrollingNode))
                continue;

            if (isScrolledBy(*this, nodeID, frontmostLayer)) {
                LOG_WITH_STREAM(UIHitTesting, stream << "RemoteScrollingTreeMac " << this << " scrollingNodeForPoint " << point << " found scrolling node " << nodeID);
                return scrollingNode;
            }
        }
    }

    LOG_WITH_STREAM(UIHitTesting, stream << "RemoteScrollingTreeMac " << this << " scrollingNodeForPoint " << point << " found no scrollable layers; using root node");
    return rootScrollingNode;
}

#if ENABLE(WHEEL_EVENT_REGIONS)
OptionSet<EventListenerRegionType> RemoteScrollingTreeMac::eventListenerRegionTypesForPoint(FloatPoint point) const
{
    auto* rootScrollingNode = rootNode();
    if (!rootScrollingNode)
        return { };

    auto rootContentsLayer = static_cast<ScrollingTreeFrameScrollingNodeMac*>(rootScrollingNode)->rootContentsLayer();

    Vector<LayerAndPoint, 16> layersAtPoint;
    collectDescendantLayersAtPoint(layersAtPoint, rootContentsLayer.get(), point, layerEventRegionContainsPoint);

    if (layersAtPoint.isEmpty())
        return { };

    auto [hitLayer, localPoint] = layersAtPoint.last();
    if (!hitLayer)
        return { };

    auto* eventRegion = eventRegionForLayer(hitLayer);
    if (!eventRegion)
        return { };

    return eventRegion->eventListenerRegionTypesForPoint(roundedIntPoint(localPoint));
}
#endif

} // namespace WebKit

#endif // PLATFORM(MAC) && ENABLE(UI_SIDE_COMPOSITING)
