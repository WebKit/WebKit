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
#import "ScrollingTreePluginScrollingNodeRemoteMac.h"
#import <WebCore/EventRegion.h>
#import <WebCore/FrameView.h>
#import <WebCore/LocalFrameView.h>
#import <WebCore/ScrollingThread.h>
#import <WebCore/ScrollingTreeFixedNodeCocoa.h>
#import <WebCore/ScrollingTreeOverflowScrollProxyNode.h>
#import <WebCore/ScrollingTreePositionedNode.h>
#import <WebCore/WebCoreCALayerExtras.h>
#import <pal/spi/cocoa/QuartzCoreSPI.h>
#import <wtf/TZoneMallocInlines.h>
#import <wtf/text/TextStream.h>

namespace WebKit {

WTF_MAKE_TZONE_ALLOCATED_IMPL(RemoteScrollingTreeMac);

using namespace WebCore;

Ref<RemoteScrollingTree> RemoteScrollingTree::create(RemoteScrollingCoordinatorProxy& scrollingCoordinator)
{
    return adoptRef(*new RemoteScrollingTreeMac(scrollingCoordinator));
}

RemoteScrollingTreeMac::RemoteScrollingTreeMac(RemoteScrollingCoordinatorProxy& scrollingCoordinator)
    : RemoteScrollingTree(scrollingCoordinator)
{
    ASSERT(isMainRunLoop());
    ScrollingThread::dispatch([protectedThis = Ref { *this }]() {
        if ([CATransaction respondsToSelector:@selector(setDisableImplicitTransactionMainThreadAssert:)])
            [CATransaction setDisableImplicitTransactionMainThreadAssert:YES];
    });
}

RemoteScrollingTreeMac::~RemoteScrollingTreeMac() = default;

void RemoteScrollingTreeMac::handleWheelEventPhase(ScrollingNodeID nodeID, PlatformWheelEventPhase phase)
{
    auto* targetNode = nodeForID(nodeID);
    if (auto* node = dynamicDowncast<ScrollingTreeScrollingNode>(targetNode))
        node->handleWheelEventPhase(phase);
}

void RemoteScrollingTreeMac::displayDidRefresh(PlatformDisplayID)
{
    ASSERT(ScrollingThread::isCurrentThread());

    Locker locker { m_treeLock };
    serviceScrollAnimations(MonotonicTime::now()); // FIXME: Share timestamp with the rest of the update.
}

void RemoteScrollingTreeMac::scrollingTreeNodeWillStartScroll(ScrollingNodeID nodeID)
{
    RunLoop::main().dispatch([protectedThis = Ref { *this }, this, nodeID] {
        RemoteScrollingTree::scrollingTreeNodeWillStartScroll(nodeID);
    });
}

void RemoteScrollingTreeMac::scrollingTreeNodeDidEndScroll(ScrollingNodeID nodeID)
{
    RunLoop::main().dispatch([protectedThis = Ref { *this }, this, nodeID] {
        RemoteScrollingTree::scrollingTreeNodeDidEndScroll(nodeID);
    });
}

void RemoteScrollingTreeMac::clearNodesWithUserScrollInProgress()
{
    RunLoop::main().dispatch([protectedThis = Ref { *this }, this] {
        RemoteScrollingTree::clearNodesWithUserScrollInProgress();
    });
}

void RemoteScrollingTreeMac::scrollingTreeNodeDidBeginScrollSnapping(ScrollingNodeID nodeID)
{
    RunLoop::main().dispatch([protectedThis = Ref { *this }, this, nodeID] {
        RemoteScrollingTree::scrollingTreeNodeDidBeginScrollSnapping(nodeID);
    });
}

void RemoteScrollingTreeMac::scrollingTreeNodeDidEndScrollSnapping(ScrollingNodeID nodeID)
{
    RunLoop::main().dispatch([protectedThis = Ref { *this }, this, nodeID] {
        RemoteScrollingTree::scrollingTreeNodeDidEndScrollSnapping(nodeID);
    });
}

Ref<ScrollingTreeNode> RemoteScrollingTreeMac::createScrollingTreeNode(ScrollingNodeType nodeType, ScrollingNodeID nodeID)
{
    switch (nodeType) {
    case ScrollingNodeType::MainFrame:
    case ScrollingNodeType::Subframe:
        return ScrollingTreeFrameScrollingNodeRemoteMac::create(*this, nodeType, nodeID);

    case ScrollingNodeType::Overflow:
        return ScrollingTreeOverflowScrollingNodeRemoteMac::create(*this, nodeID);

    case ScrollingNodeType::PluginScrolling:
        return ScrollingTreePluginScrollingNodeRemoteMac::create(*this, nodeID);

    case ScrollingNodeType::FrameHosting:
    case ScrollingNodeType::PluginHosting:
    case ScrollingNodeType::OverflowProxy:
    case ScrollingNodeType::Fixed:
    case ScrollingNodeType::Sticky:
    case ScrollingNodeType::Positioned:
        return RemoteScrollingTree::createScrollingTreeNode(nodeType, nodeID);
    }
    ASSERT_NOT_REACHED();
    return ScrollingTreeFixedNodeCocoa::create(*this, nodeID);
}

void RemoteScrollingTreeMac::didCommitTree()
{
    ASSERT(isMainRunLoop());

    if (m_nodesWithPendingScrollAnimations.isEmpty() && m_nodesWithPendingKeyboardScrollAnimations.isEmpty())
        return;

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
    for (auto& [nodeID, data] : nodesWithPendingScrollAnimations) {
        RefPtr targetNode = dynamicDowncast<ScrollingTreeScrollingNode>(nodeForID(nodeID));
        if (!targetNode)
            continue;

        LOG_WITH_STREAM(Scrolling, stream << "RemoteScrollingTreeMac::startPendingScrollAnimations() for node " << nodeID << " with data " << data);

        if (auto previousData = std::exchange(data.requestedDataBeforeAnimatedScroll, std::nullopt)) {
            auto& [requestType, positionOrDeltaBeforeAnimatedScroll, scrollType, clamping] = *previousData;

            switch (requestType) {
            case ScrollRequestType::PositionUpdate:
            case ScrollRequestType::DeltaUpdate: {
                auto intermediatePosition = RequestedScrollData::computeDestinationPosition(targetNode->currentScrollPosition(), requestType, positionOrDeltaBeforeAnimatedScroll);
                targetNode->scrollTo(intermediatePosition, scrollType, clamping);
                break;
            }
            case ScrollRequestType::CancelAnimatedScroll:
                targetNode->stopAnimatedScroll();
                break;
            }
        }

        auto finalPosition = data.destinationPosition(targetNode->currentScrollPosition());
        targetNode->startAnimatedScrollToPosition(finalPosition);
    }

    auto nodesWithPendingKeyboardScrollAnimations = std::exchange(m_nodesWithPendingKeyboardScrollAnimations, { });
    for (const auto& [key, value] : nodesWithPendingKeyboardScrollAnimations) {
        RefPtr targetNode = dynamicDowncast<ScrollingTreeScrollingNode>(nodeForID(key));
        if (!targetNode)
            continue;

        targetNode->handleKeyboardScrollRequest(value);
    }
}

void RemoteScrollingTreeMac::hasNodeWithAnimatedScrollChanged(bool hasNodeWithAnimatedScroll)
{
    ASSERT(ScrollingThread::isCurrentThread());

    RunLoop::main().dispatch([protectedThis = Ref { *this }, hasNodeWithAnimatedScroll] {
        if (auto* scrollingCoordinatorProxy = protectedThis->scrollingCoordinatorProxy())
            scrollingCoordinatorProxy->hasNodeWithAnimatedScrollChanged(hasNodeWithAnimatedScroll);
    });
}

void RemoteScrollingTreeMac::setRubberBandingInProgressForNode(ScrollingNodeID nodeID, bool isRubberBanding)
{
    ScrollingTree::setRubberBandingInProgressForNode(nodeID, isRubberBanding);

    RunLoop::main().dispatch([protectedThis = Ref { *this }, nodeID, isRubberBanding] {
        if (auto* scrollingCoordinatorProxy = protectedThis->scrollingCoordinatorProxy())
            scrollingCoordinatorProxy->setRubberBandingInProgressForNode(nodeID, isRubberBanding);
    });
}

void RemoteScrollingTreeMac::scrollingTreeNodeDidScroll(ScrollingTreeScrollingNode& node, ScrollingLayerPositionAction action)
{
    ScrollingTree::scrollingTreeNodeDidScroll(node, action);

    std::optional<FloatPoint> layoutViewportOrigin;
    if (auto* scrollingNode = dynamicDowncast<ScrollingTreeFrameScrollingNode>(node))
        layoutViewportOrigin = scrollingNode->layoutViewport().location();

    if (isHandlingProgrammaticScroll())
        return;

    auto scrollUpdate = ScrollUpdate { node.scrollingNodeID(), node.currentScrollPosition(), layoutViewportOrigin, ScrollUpdateType::PositionUpdate, action };
    addPendingScrollUpdate(WTFMove(scrollUpdate));

    // Happens when the this is called as a result of the scrolling tree commmit.
    if (RunLoop::isMain()) {
        if (auto* scrollingCoordinatorProxy = this->scrollingCoordinatorProxy())
            scrollingCoordinatorProxy->scrollingThreadAddedPendingUpdate();
        return;
    }

    RunLoop::main().dispatch([protectedThis = Ref { *this }] {
        if (auto* scrollingCoordinatorProxy = protectedThis->scrollingCoordinatorProxy())
            scrollingCoordinatorProxy->scrollingThreadAddedPendingUpdate();
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

    RunLoop::main().dispatch([protectedThis = Ref { *this }, nodeID = node.scrollingNodeID()] {
        if (auto* scrollingCoordinatorProxy = protectedThis->scrollingCoordinatorProxy())
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

bool RemoteScrollingTreeMac::scrollingTreeNodeRequestsKeyboardScroll(ScrollingNodeID nodeID, const RequestedKeyboardScrollData& request)
{
    ASSERT(m_treeLock.isLocked());
    m_nodesWithPendingKeyboardScrollAnimations.set(nodeID, request);
    return true;
}

void RemoteScrollingTreeMac::currentSnapPointIndicesDidChange(ScrollingNodeID nodeID, std::optional<unsigned> horizontal, std::optional<unsigned> vertical)
{
    RunLoop::main().dispatch([protectedThis = Ref { *this }, nodeID, horizontal, vertical] {
        if (auto* scrollingCoordinatorProxy = protectedThis->scrollingCoordinatorProxy())
            scrollingCoordinatorProxy->currentSnapPointIndicesDidChange(nodeID, horizontal, vertical);
    });
}

void RemoteScrollingTreeMac::reportExposedUnfilledArea(MonotonicTime time, unsigned unfilledArea)
{
    RunLoop::main().dispatch([protectedThis = Ref { *this }, time, unfilledArea] {
        if (auto* scrollingCoordinatorProxy = protectedThis->scrollingCoordinatorProxy())
            scrollingCoordinatorProxy->reportExposedUnfilledArea(time, unfilledArea);
    });
}

void RemoteScrollingTreeMac::reportSynchronousScrollingReasonsChanged(MonotonicTime timestamp, OptionSet<SynchronousScrollingReason> reasons)
{
    RunLoop::main().dispatch([protectedThis = Ref { *this }, timestamp, reasons] {
        if (auto* scrollingCoordinatorProxy = protectedThis->scrollingCoordinatorProxy())
            scrollingCoordinatorProxy->reportSynchronousScrollingReasonsChanged(timestamp, reasons);
    });
}

void RemoteScrollingTreeMac::receivedWheelEventWithPhases(PlatformWheelEventPhase phase, PlatformWheelEventPhase momentumPhase)
{
    RunLoop::main().dispatch([protectedThis = Ref { *this }, phase, momentumPhase] {
        if (auto* scrollingCoordinatorProxy = protectedThis->scrollingCoordinatorProxy())
            scrollingCoordinatorProxy->receivedWheelEventWithPhases(phase, momentumPhase);
    });
}

void RemoteScrollingTreeMac::willSendEventForDefaultHandling(const PlatformWheelEvent&)
{
    ASSERT(ScrollingThread::isCurrentThread());

    Locker locker { m_treeLock };
    m_receivedBeganEventFromMainThread = false;
}

void RemoteScrollingTreeMac::waitForEventDefaultHandlingCompletion(const PlatformWheelEvent& wheelEvent)
{
    ASSERT(ScrollingThread::isCurrentThread());

    if (!wheelEvent.isGestureStart())
        return;

    Locker locker { m_treeLock };

    static constexpr auto maxAllowableEventProcessingDelay = 50_ms;
    auto startTime = MonotonicTime::now();
    auto timeoutTime = startTime + maxAllowableEventProcessingDelay;

    bool receivedEvent = m_waitingForBeganEventCondition.waitUntil(m_treeLock, timeoutTime, [&] {
        assertIsHeld(m_treeLock);
        return m_receivedBeganEventFromMainThread;
    });
    if (!receivedEvent) {
        // Timed out, go asynchronous.
        setGestureState(WheelScrollGestureState::NonBlocking);
    }

    LOG_WITH_STREAM(Scrolling, stream << "RemoteScrollingTreeMac::waitForEventDefaultHandlingCompletion took " << (MonotonicTime::now() - startTime).milliseconds() << "ms - timed out " << !receivedEvent << " gesture state is " << gestureState());
}

void RemoteScrollingTreeMac::receivedEventAfterDefaultHandling(const WebCore::PlatformWheelEvent& wheelEvent, std::optional<WheelScrollGestureState> gestureState)
{
    LOG_WITH_STREAM(Scrolling, stream << "RemoteScrollingTreeMac::receivedEventAfterDefaultHandling - " << wheelEvent << " gestureState " << gestureState);

    ASSERT(isMainRunLoop());

    if (!wheelEvent.isGestureStart())
        return;

    Locker locker { m_treeLock };

    if (m_receivedBeganEventFromMainThread)
        return;

    setGestureState(gestureState);

    m_receivedBeganEventFromMainThread = true;
    m_waitingForBeganEventCondition.notifyOne();
}

WheelEventHandlingResult RemoteScrollingTreeMac::handleWheelEventAfterDefaultHandling(const PlatformWheelEvent& wheelEvent, std::optional<ScrollingNodeID> targetNodeID, std::optional<WheelScrollGestureState> gestureState)
{
    LOG_WITH_STREAM(Scrolling, stream << "RemoteScrollingTreeMac::handleWheelEventAfterDefaultHandling - targetNodeID " << targetNodeID << " gestureState " << gestureState);

    ASSERT(ScrollingThread::isCurrentThread());

    if (!targetNodeID)
        return WheelEventHandlingResult::unhandled();

    Locker locker { m_treeLock };

    bool allowLatching = false;
    OptionSet<WheelEventProcessingSteps> processingSteps;
    if (gestureState.value_or(WheelScrollGestureState::Blocking) == WheelScrollGestureState::NonBlocking) {
        allowLatching = true;
        processingSteps = { WheelEventProcessingSteps::AsyncScrolling, WheelEventProcessingSteps::NonBlockingDOMEventDispatch };
    }

    SetForScope disallowLatchingScope(m_allowLatching, allowLatching);
    RefPtr<ScrollingTreeNode> targetNode = nodeForID(*targetNodeID);
    return handleWheelEventWithNode(wheelEvent, processingSteps, targetNode.get(), EventTargeting::NodeOnly);
}

void RemoteScrollingTreeMac::deferWheelEventTestCompletionForReason(ScrollingNodeID nodeID, WheelEventTestMonitor::DeferReason reason)
{
    if (!isMonitoringWheelEvents())
        return;

    RunLoop::main().dispatch([protectedThis = Ref { *this }, nodeID, reason] {
        if (auto* scrollingCoordinatorProxy = protectedThis->scrollingCoordinatorProxy())
            scrollingCoordinatorProxy->deferWheelEventTestCompletionForReason(nodeID, reason);
    });
}

void RemoteScrollingTreeMac::removeWheelEventTestCompletionDeferralForReason(ScrollingNodeID nodeID, WheelEventTestMonitor::DeferReason reason)
{
    if (!isMonitoringWheelEvents())
        return;

    RunLoop::main().dispatch([protectedThis = Ref { *this }, nodeID, reason] {
        if (auto* scrollingCoordinatorProxy = protectedThis->scrollingCoordinatorProxy())
            scrollingCoordinatorProxy->removeWheelEventTestCompletionDeferralForReason(nodeID, reason);
    });
}

void RemoteScrollingTreeMac::lockLayersForHitTesting()
{
    m_layerHitTestMutex.lock();
}

void RemoteScrollingTreeMac::unlockLayersForHitTesting()
{
    m_layerHitTestMutex.unlock();
}

static std::optional<ScrollingNodeID> scrollingNodeIDForLayer(CALayer *layer)
{
    auto* layerTreeNode = RemoteLayerTreeNode::forCALayer(layer);
    if (!layerTreeNode)
        return std::nullopt;

    return layerTreeNode->scrollingNodeID();
}

static bool isScrolledBy(const ScrollingTree& tree, ScrollingNodeID scrollingNodeID, CALayer *hitLayer)
{
    for (CALayer *layer = hitLayer; layer; layer = [layer superlayer]) {
        auto nodeID = scrollingNodeIDForLayer(layer);
        if (nodeID == scrollingNodeID)
            return true;

        auto* scrollingNode = tree.nodeForID(nodeID);
        if (auto* scollProxyNode = dynamicDowncast<ScrollingTreeOverflowScrollProxyNode>(scrollingNode)) {
            auto actingOverflowScrollingNodeID = scollProxyNode->overflowScrollingNodeID();
            if (actingOverflowScrollingNodeID == scrollingNodeID)
                return true;
        }

        if (auto* positionedNode = dynamicDowncast<ScrollingTreePositionedNode>(scrollingNode)) {
            if (positionedNode->relatedOverflowScrollingNodes().contains(scrollingNodeID))
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
    RefPtr rootScrollingNode = rootNode();
    if (!rootScrollingNode)
        return nullptr;

    ASSERT(m_layerHitTestMutex.isLocked());

    RetainPtr scrolledContentsLayer { static_cast<CALayer*>(rootScrollingNode->scrolledContentsLayer()) };

    auto rootContentsLayerPosition = LocalFrameView::positionForRootContentLayer(rootScrollingNode->currentScrollPosition(), FloatPoint { 0, 0 }, rootScrollingNode->topContentInset(), rootScrollingNode->headerHeight());
    auto pointInContentsLayer = point;
    pointInContentsLayer.moveBy(rootContentsLayerPosition);

    bool hasAnyNonInteractiveScrollingLayers = false;
    auto layersAtPoint = layersAtPointToCheckForScrolling(layerEventRegionContainsPoint, scrollingNodeIDForLayer, scrolledContentsLayer.get(), pointInContentsLayer, hasAnyNonInteractiveScrollingLayers);

    LOG_WITH_STREAM(UIHitTesting, stream << "RemoteScrollingTreeMac " << this << " scrollingNodeForPoint " << point << " (converted to layer point " << pointInContentsLayer << ") found " << layersAtPoint.size() << " layers");
#if !LOG_DISABLED
    for (auto [layer, point] : layersAtPoint)
        LOG_WITH_STREAM(UIHitTesting, stream << " layer " << [layer description] << " scrolling node " << scrollingNodeIDForLayer(layer));
#endif

    if (layersAtPoint.size()) {
        RetainPtr<CALayer> frontmostInteractiveLayer;
        for (size_t i = 0 ; i < layersAtPoint.size() ; i++) {
            auto [layer, point] = layersAtPoint[i];

            if (!layerEventRegionContainsPoint(layer, point))
                continue;

            if (!frontmostInteractiveLayer)
                frontmostInteractiveLayer = layer;

            auto scrollingNodeForLayer = [&] (auto layer, auto point) -> RefPtr<ScrollingTreeNode> {
                UNUSED_PARAM(point);
                auto nodeID = scrollingNodeIDForLayer(layer);
                RefPtr scrollingNode = nodeForID(nodeID);
                if (!is<ScrollingTreeScrollingNode>(scrollingNode))
                    return nullptr;
                ASSERT(frontmostInteractiveLayer);
                if (isScrolledBy(*this, *nodeID, frontmostInteractiveLayer.get())) {
                    LOG_WITH_STREAM(UIHitTesting, stream << "RemoteScrollingTreeMac " << this << " scrollingNodeForPoint " << point << " found scrolling node " << nodeID);
                    return scrollingNode;
                }
                return nullptr;
            };

            if (RefPtr scrollingNode = scrollingNodeForLayer(layer, point))
                return scrollingNode;

            // This layer may be scrolled by some other layer further back which may itself be non-interactive.
            if (hasAnyNonInteractiveScrollingLayers) {
                for (size_t j = i + 1; j < layersAtPoint.size() ; j++) {
                    auto [behindLayer, behindPoint] = layersAtPoint[j];
                    if (RefPtr scrollingNode = scrollingNodeForLayer(behindLayer, behindPoint))
                        return scrollingNode;
                }
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

    ASSERT(m_layerHitTestMutex.isLocked());

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

void RemoteScrollingTreeMac::scrollingTreeNodeScrollbarVisibilityDidChange(ScrollingNodeID nodeID, ScrollbarOrientation orientation, bool isVisible)
{
    RunLoop::main().dispatch([protectedThis = Ref { *this }, nodeID, orientation, isVisible] {
        if (auto* scrollingCoordinatorProxy = protectedThis->scrollingCoordinatorProxy())
            scrollingCoordinatorProxy->scrollingTreeNodeScrollbarVisibilityDidChange(nodeID, orientation, isVisible);
    });
}

void RemoteScrollingTreeMac::scrollingTreeNodeScrollbarMinimumThumbLengthDidChange(ScrollingNodeID nodeID, ScrollbarOrientation orientation, int minimumThumbLength)
{
    RunLoop::main().dispatch([protectedThis = Ref { *this }, nodeID, orientation, minimumThumbLength] {
        if (auto* scrollingCoordinatorProxy = protectedThis->scrollingCoordinatorProxy())
            scrollingCoordinatorProxy->scrollingTreeNodeScrollbarMinimumThumbLengthDidChange(nodeID, orientation, minimumThumbLength);
    });
}

} // namespace WebKit

#endif // PLATFORM(MAC) && ENABLE(UI_SIDE_COMPOSITING)
