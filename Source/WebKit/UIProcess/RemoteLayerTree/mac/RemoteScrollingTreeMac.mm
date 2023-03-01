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
#import <WebCore/ScrollingThread.h>
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

void RemoteScrollingTreeMac::displayDidRefresh(PlatformDisplayID)
{
    ASSERT(ScrollingThread::isCurrentThread());

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

void RemoteScrollingTreeMac::handleMouseEvent(const PlatformMouseEvent& event)
{
    if (!rootNode())
        return;
    static_cast<ScrollingTreeFrameScrollingNodeRemoteMac&>(*rootNode()).handleMouseEvent(event);
}

void RemoteScrollingTreeMac::didCommitTree()
{
    ASSERT(isMainRunLoop());
    ScrollingThread::dispatch([protectedThis = Ref { *this }]() {
        Locker treeLocker { protectedThis->m_treeLock };
        protectedThis->startPendingScrollAnimations();
    });
}

void RemoteScrollingTreeMac::startPendingScrollAnimations()
{
    ASSERT(ScrollingThread::isCurrentThread());
    ASSERT(m_treeLock.isLocked());

    auto nodesWithPendingScrollAnimations = std::exchange(m_nodesWithPendingScrollAnimations, { });

    LOG_WITH_STREAM(Scrolling, stream << "RemoteScrollingTreeMac::startPendingScrollAnimations() - " << nodesWithPendingScrollAnimations.size() << " nodes with pending animations");

    for (const auto& it : nodesWithPendingScrollAnimations) {
        RefPtr targetNode = nodeForID(it.key);
        if (!is<ScrollingTreeScrollingNode>(targetNode))
            continue;

        downcast<ScrollingTreeScrollingNode>(*targetNode).startAnimatedScrollToPosition(it.value.scrollPosition);
    }
}

void RemoteScrollingTreeMac::hasNodeWithAnimatedScrollChanged(bool hasNodeWithAnimatedScroll)
{
    ASSERT(ScrollingThread::isCurrentThread());

    RunLoop::main().dispatch([strongThis = Ref { *this }, hasNodeWithAnimatedScroll] {
        if (auto* scrollingCoordinatorProxy = strongThis->scrollingCoordinatorProxy())
            scrollingCoordinatorProxy->hasNodeWithAnimatedScrollChanged(hasNodeWithAnimatedScroll);
    });
}

void RemoteScrollingTreeMac::scrollingTreeNodeDidScroll(ScrollingTreeScrollingNode& node, ScrollingLayerPositionAction action)
{
    std::optional<FloatPoint> layoutViewportOrigin;
    if (is<ScrollingTreeFrameScrollingNode>(node))
        layoutViewportOrigin = downcast<ScrollingTreeFrameScrollingNode>(node).layoutViewport().location();

    auto nodeID = node.scrollingNodeID();
    auto scrollPosition = node.currentScrollPosition();

    // Happens when the this is called as a result of the scrolling tree commmit.
    if (RunLoop::isMain()) {
        if (auto* scrollingCoordinatorProxy = this->scrollingCoordinatorProxy())
            scrollingCoordinatorProxy->scrollingTreeNodeDidScroll(nodeID, scrollPosition, layoutViewportOrigin, action);
        return;
    }

    RunLoop::main().dispatch([strongThis = Ref { *this }, nodeID, scrollPosition, layoutViewportOrigin, action] {
        if (auto* scrollingCoordinatorProxy = strongThis->scrollingCoordinatorProxy())
            scrollingCoordinatorProxy->scrollingTreeNodeDidScroll(nodeID, scrollPosition, layoutViewportOrigin, action);
    });
}

void RemoteScrollingTreeMac::scrollingTreeNodeDidStopAnimatedScroll(ScrollingTreeScrollingNode& node)
{
    // Happens when the this is called as a result of the scrolling tree commmit.
    if (RunLoop::isMain()) {
        if (auto* scrollingCoordinatorProxy = this->scrollingCoordinatorProxy())
            scrollingCoordinatorProxy->scrollingTreeNodeDidStopAnimatedScroll(node.scrollingNodeID());
        return;
    }

    ASSERT(ScrollingThread::isCurrentThread());

    RunLoop::main().dispatch([strongThis = Ref { *this }, nodeID = node.scrollingNodeID()] {
        if (auto* scrollingCoordinatorProxy = strongThis->scrollingCoordinatorProxy())
            scrollingCoordinatorProxy->scrollingTreeNodeDidStopAnimatedScroll(nodeID);
    });
}

bool RemoteScrollingTreeMac::scrollingTreeNodeRequestsScroll(ScrollingNodeID nodeID, const RequestedScrollData& request)
{
    if (request.animated == ScrollIsAnimated::Yes) {
        ASSERT(m_treeLock.isLocked());
        m_nodesWithPendingScrollAnimations.set(nodeID, request);
        return true;
    }
    return false;
}

void RemoteScrollingTreeMac::currentSnapPointIndicesDidChange(ScrollingNodeID nodeID, std::optional<unsigned> horizontal, std::optional<unsigned> vertical)
{
    RunLoop::main().dispatch([strongThis = Ref { *this }, nodeID, horizontal, vertical] {
        if (auto* scrollingCoordinatorProxy = strongThis->scrollingCoordinatorProxy())
            scrollingCoordinatorProxy->currentSnapPointIndicesDidChange(nodeID, horizontal, vertical);
    });
}

void RemoteScrollingTreeMac::reportExposedUnfilledArea(MonotonicTime time, unsigned unfilledArea)
{
    RunLoop::main().dispatch([strongThis = Ref { *this }, time, unfilledArea] {
        if (auto* scrollingCoordinatorProxy = strongThis->scrollingCoordinatorProxy())
            scrollingCoordinatorProxy->reportExposedUnfilledArea(time, unfilledArea);
    });
}

void RemoteScrollingTreeMac::reportSynchronousScrollingReasonsChanged(MonotonicTime timestamp, OptionSet<SynchronousScrollingReason> reasons)
{
    RunLoop::main().dispatch([strongThis = Ref { *this }, timestamp, reasons] {
        if (auto* scrollingCoordinatorProxy = strongThis->scrollingCoordinatorProxy())
            scrollingCoordinatorProxy->reportSynchronousScrollingReasonsChanged(timestamp, reasons);
    });
}

void RemoteScrollingTreeMac::receivedWheelEventWithPhases(PlatformWheelEventPhase phase, PlatformWheelEventPhase momentumPhase)
{
    RunLoop::main().dispatch([strongThis = Ref { *this }, phase, momentumPhase] {
        if (auto* scrollingCoordinatorProxy = strongThis->scrollingCoordinatorProxy())
            scrollingCoordinatorProxy->receivedWheelEventWithPhases(phase, momentumPhase);
    });
}

void RemoteScrollingTreeMac::deferWheelEventTestCompletionForReason(ScrollingNodeID nodeID, WheelEventTestMonitor::DeferReason reason)
{
    RunLoop::main().dispatch([strongThis = Ref { *this }, nodeID, reason] {
        if (auto* scrollingCoordinatorProxy = strongThis->scrollingCoordinatorProxy())
            scrollingCoordinatorProxy->deferWheelEventTestCompletionForReason(nodeID, reason);
    });
}

void RemoteScrollingTreeMac::removeWheelEventTestCompletionDeferralForReason(ScrollingNodeID nodeID, WheelEventTestMonitor::DeferReason reason)
{
    RunLoop::main().dispatch([strongThis = Ref { *this }, nodeID, reason] {
        if (auto* scrollingCoordinatorProxy = strongThis->scrollingCoordinatorProxy())
            scrollingCoordinatorProxy->removeWheelEventTestCompletionDeferralForReason(nodeID, reason);
    });
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
