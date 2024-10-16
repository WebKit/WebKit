/*
 * Copyright (C) 2012-2015 Apple Inc. All rights reserved.
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
#include "ScrollingTree.h"

#if ENABLE(ASYNC_SCROLLING)

#include "AnimationFrameRate.h"
#include "EventNames.h"
#include "Logging.h"
#include "PlatformWheelEvent.h"
#include "ScrollingEffectsController.h"
#include "ScrollingStateFrameScrollingNode.h"
#include "ScrollingStateTree.h"
#include "ScrollingTreeFrameHostingNode.h"
#include "ScrollingTreeFrameScrollingNode.h"
#include "ScrollingTreeNode.h"
#include "ScrollingTreeOverflowScrollProxyNode.h"
#include "ScrollingTreeOverflowScrollingNode.h"
#include "ScrollingTreePositionedNode.h"
#include "ScrollingTreeScrollingNode.h"
#include <wtf/SetForScope.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/TextStream.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(ScrollingTree);

using OrphanScrollingNodeMap = UncheckedKeyHashMap<ScrollingNodeID, RefPtr<ScrollingTreeNode>>;

struct CommitTreeState {
    // unvisitedNodes starts with all nodes in the map; we remove nodes as we visit them. At the end, it's the unvisited nodes.
    // We can't use orphanNodes for this, because orphanNodes won't contain descendants of removed nodes.
    HashSet<ScrollingNodeID> unvisitedNodes { };
    // Nodes with non-empty synchronousScrollingReasons.
    HashSet<ScrollingNodeID> synchronousScrollingNodes { };
    // orphanNodes keeps child nodes alive while we rebuild child lists.
    OrphanScrollingNodeMap orphanNodes { };
    // Hosted subtrees needing attaching to scrolling tree after main commit has finished
    Vector<std::pair<LayerHostingContextIdentifier, Vector<std::unique_ptr<ScrollingStateTree>>>> pendingSubtreesNeedingCommit { };
    // Nodes that are descendants of a frame hosting node.
    HashSet<ScrollingNodeID> hostedScrollingNodes { };
    // This has a value when doing a commit for a hosted subtree.
    RefPtr<ScrollingTreeFrameHostingNode> frameHostingNode { };
    // Identifier for the frame associated with this commit.
    FrameIdentifier frameId;
};

ScrollingTree::ScrollingTree()
    : m_gestureState(*this)
{
}

ScrollingTree::~ScrollingTree() = default;

bool ScrollingTree::isUserScrollInProgressAtEventLocation(const PlatformWheelEvent& wheelEvent)
{
    if (!m_rootNode)
        return false;

    // This method is invoked by the event handling thread
    Locker locker { m_treeStateLock };

    if (m_treeState.nodesWithActiveUserScrolls.isEmpty())
        return false;

    FloatPoint position = wheelEvent.position();
    position.move(m_rootNode->viewToContentsOffset(m_treeState.mainFrameScrollPosition));
    if (auto node = scrollingNodeForPoint(position))
        return m_treeState.nodesWithActiveUserScrolls.contains(node->scrollingNodeID());

    return false;
}

OptionSet<WheelEventProcessingSteps> ScrollingTree::computeWheelProcessingSteps(const PlatformWheelEvent& wheelEvent)
{
    if (!m_rootNode)
        return { WheelEventProcessingSteps::AsyncScrolling };

    FloatPoint position = wheelEvent.position();
    position.move(m_rootNode->viewToContentsOffset(m_treeState.mainFrameScrollPosition));

    if (!m_treeState.eventTrackingRegions.isEmpty()) {
        IntPoint roundedPosition = roundedIntPoint(position);

        // Event regions are affected by page scale, so no need to map through scale.
        bool isSynchronousDispatchRegion = m_treeState.eventTrackingRegions.trackingTypeForPoint(EventTrackingRegions::EventType::Wheel, roundedPosition) == TrackingType::Synchronous
            || m_treeState.eventTrackingRegions.trackingTypeForPoint(EventTrackingRegions::EventType::Mousewheel, roundedPosition) == TrackingType::Synchronous;
        LOG_WITH_STREAM(Scrolling, stream << "\nScrollingTree::computeWheelProcessingSteps: wheelEvent " << wheelEvent << " mapped to content point " << position << ", in non-fast region " << isSynchronousDispatchRegion);

        if (isSynchronousDispatchRegion)
            return { WheelEventProcessingSteps::SynchronousScrolling, WheelEventProcessingSteps::BlockingDOMEventDispatch };
    }

#if ENABLE(WHEEL_EVENT_REGIONS)
    auto eventListenerTypes = eventListenerRegionTypesForPoint(position);
    if (eventListenerTypes.contains(EventListenerRegionType::NonPassiveWheel)) {
        if (m_treeState.gestureState.value_or(WheelScrollGestureState::Blocking) == WheelScrollGestureState::NonBlocking) {
            LOG_WITH_STREAM(Scrolling, stream << "ScrollingTree::computeWheelProcessingSteps: wheelEvent - gesture state made event non-blocking");
            return { WheelEventProcessingSteps::AsyncScrolling, WheelEventProcessingSteps::NonBlockingDOMEventDispatch };
        }

        return { WheelEventProcessingSteps::SynchronousScrolling, WheelEventProcessingSteps::BlockingDOMEventDispatch };
    }

    if (eventListenerTypes.contains(EventListenerRegionType::Wheel))
        return { WheelEventProcessingSteps::AsyncScrolling, WheelEventProcessingSteps::NonBlockingDOMEventDispatch };
#endif
    return { WheelEventProcessingSteps::AsyncScrolling };
}

OptionSet<WheelEventProcessingSteps> ScrollingTree::determineWheelEventProcessing(const PlatformWheelEvent& wheelEvent)
{
    // This method is invoked by the event handling thread
    Locker locker { m_treeStateLock };

    auto latchedNodeAndSteps = m_latchingController.latchingDataForEvent(wheelEvent, m_allowLatching);
    if (latchedNodeAndSteps) {
        LOG_WITH_STREAM(ScrollLatching, stream << "ScrollingTree::determineWheelEventProcessing " << wheelEvent << " have latched node " << latchedNodeAndSteps->scrollingNodeID << " steps " << latchedNodeAndSteps->processingSteps << " gesture state " << m_treeState.gestureState);
        return latchedNodeAndSteps->processingSteps;
    }
    if (wheelEvent.isGestureStart() || wheelEvent.isNonGestureEvent())
        m_treeState.gestureState = std::nullopt;

    auto processingSteps = computeWheelProcessingSteps(wheelEvent);

    m_latchingController.receivedWheelEvent(wheelEvent, processingSteps, m_allowLatching);

    LOG_WITH_STREAM(Scrolling, stream << "ScrollingTree::determineWheelEventProcessing: " << wheelEvent << " processingSteps " << processingSteps);

    return processingSteps;
}

WheelEventHandlingResult ScrollingTree::handleWheelEvent(const PlatformWheelEvent& wheelEvent, OptionSet<WheelEventProcessingSteps> processingSteps)
{
    LOG_WITH_STREAM(Scrolling, stream << "\nScrollingTree " << this << " handleWheelEvent " << wheelEvent);

    Locker locker { m_treeLock };

    if (isMonitoringWheelEvents())
        receivedWheelEventWithPhases(wheelEvent.phase(), wheelEvent.momentumPhase());

    m_latchingController.receivedWheelEvent(wheelEvent, processingSteps, m_allowLatching);

    auto result = [&] {
        if (!m_rootNode)
            return WheelEventHandlingResult::unhandled();

        if (!asyncFrameOrOverflowScrollingEnabled()) {
            auto result = m_rootNode->handleWheelEvent(wheelEvent);
            if (result.wasHandled)
                m_gestureState.nodeDidHandleEvent(m_rootNode->scrollingNodeID(), wheelEvent);
            return result;
        }

        if (m_gestureState.handleGestureCancel(wheelEvent)) {
            clearNodesWithUserScrollInProgress();
            return WheelEventHandlingResult::handled();
        }

        m_gestureState.receivedWheelEvent(wheelEvent);

        if (auto latchedNodeAndSteps = m_latchingController.latchingDataForEvent(wheelEvent, m_allowLatching)) {
            LOG_WITH_STREAM(ScrollLatching, stream << "ScrollingTree::handleWheelEvent: has latched node " << latchedNodeAndSteps->scrollingNodeID);
            if (auto* node = dynamicDowncast<ScrollingTreeScrollingNode>(nodeForID(latchedNodeAndSteps->scrollingNodeID))) {
                auto result = node->handleWheelEvent(wheelEvent);
                if (result.wasHandled) {
                    m_latchingController.nodeDidHandleEvent(latchedNodeAndSteps->scrollingNodeID, processingSteps, wheelEvent, m_allowLatching);
                    m_gestureState.nodeDidHandleEvent(latchedNodeAndSteps->scrollingNodeID, wheelEvent);
                }
                return result;
            }
        }

        FloatPoint position = wheelEvent.position();
        {
            Locker locker { m_treeStateLock };
            position.move(m_rootNode->viewToContentsOffset(m_treeState.mainFrameScrollPosition));
        }
        auto node = scrollingNodeForPoint(position);

        LOG_WITH_STREAM(Scrolling, stream << "ScrollingTree::handleWheelEvent found node " << (node ? node->scrollingNodeID() : std::optional<ScrollingNodeID> { }) << " for point " << position);

        return handleWheelEventWithNode(wheelEvent, processingSteps, node.get());
    }();

    static constexpr OptionSet<WheelEventProcessingSteps> mainThreadSteps = { WheelEventProcessingSteps::NonBlockingDOMEventDispatch, WheelEventProcessingSteps::BlockingDOMEventDispatch };
    result.steps.add(processingSteps & mainThreadSteps);
    return result;
}

WheelEventHandlingResult ScrollingTree::handleWheelEventWithNode(const PlatformWheelEvent& wheelEvent, OptionSet<WheelEventProcessingSteps> processingSteps, ScrollingTreeNode* startingNode, EventTargeting eventTargeting)
{
    auto adjustedWheelEvent = wheelEvent;
    RefPtr node = startingNode;
    while (node) {
        if (auto* scrollingNode = dynamicDowncast<ScrollingTreeScrollingNode>(*node)) {
            auto result = scrollingNode->handleWheelEvent(adjustedWheelEvent, eventTargeting);

            if (result.wasHandled) {
                m_latchingController.nodeDidHandleEvent(scrollingNode->scrollingNodeID(), processingSteps, adjustedWheelEvent, m_allowLatching);
                m_gestureState.nodeDidHandleEvent(scrollingNode->scrollingNodeID(), adjustedWheelEvent);
                return result;
            }

            if (result.needsMainThreadProcessing() || eventTargeting != EventTargeting::Propagate)
                return result;
            
            auto scrollPropagationInfo = scrollingNode->computeScrollPropagation(adjustedWheelEvent.delta());
            if (scrollPropagationInfo.shouldBlockScrollPropagation) {
                if (!scrollPropagationInfo.isHandled)
                    return WheelEventHandlingResult::unhandled();
                m_latchingController.nodeDidHandleEvent(scrollingNode->scrollingNodeID(), processingSteps, adjustedWheelEvent, m_allowLatching);
                m_gestureState.nodeDidHandleEvent(scrollingNode->scrollingNodeID(), adjustedWheelEvent);
                return WheelEventHandlingResult::handled();
            }
            adjustedWheelEvent = scrollingNode->eventForPropagation(adjustedWheelEvent);
        }

        if (auto* scrollProxyNode = dynamicDowncast<ScrollingTreeOverflowScrollProxyNode>(*node)) {
            if (auto relatedNode = nodeForID(scrollProxyNode->overflowScrollingNodeID())) {
                node = relatedNode;
                continue;
            }
        }

        node = node->parent();
    }
    return WheelEventHandlingResult::unhandled();
}

RefPtr<ScrollingTreeNode> ScrollingTree::scrollingNodeForPoint(FloatPoint)
{
    ASSERT(asyncFrameOrOverflowScrollingEnabled());
    return m_rootNode;
}

#if ENABLE(WHEEL_EVENT_REGIONS)
OptionSet<EventListenerRegionType> ScrollingTree::eventListenerRegionTypesForPoint(FloatPoint) const
{
    return { };
}
#endif

void ScrollingTree::traverseScrollingTree(VisitorFunction&& visitorFunction)
{
    Locker locker { m_treeLock };
    if (!m_rootNode)
        return;

    auto function = WTFMove(visitorFunction);
    traverseScrollingTreeRecursive(*m_rootNode, function);
}

void ScrollingTree::traverseScrollingTreeRecursive(ScrollingTreeNode& node, const VisitorFunction& visitorFunction)
{
    bool scrolledSinceLastCommit = false;
    std::optional<FloatPoint> scrollPosition;
    if (auto* scrollingNode = dynamicDowncast<ScrollingTreeScrollingNode>(node)) {
        scrollPosition = scrollingNode->currentScrollPosition();
        scrolledSinceLastCommit = scrollingNode->scrolledSinceLastCommit();
    }

    std::optional<FloatPoint> layoutViewportOrigin;
    if (auto* frameScrollingNode = dynamicDowncast<ScrollingTreeFrameScrollingNode>(node))
        layoutViewportOrigin = frameScrollingNode->layoutViewport().location();

    visitorFunction(node.scrollingNodeID(), node.nodeType(), scrollPosition, layoutViewportOrigin, scrolledSinceLastCommit);

    for (auto& child : node.children())
        traverseScrollingTreeRecursive(child.get(), visitorFunction);
}

void ScrollingTree::scrollingTreeNodeDidScroll(ScrollingTreeScrollingNode& node, ScrollingLayerPositionAction)
{
    if (node.isRootNode())
        setMainFrameScrollPosition(node.currentScrollPosition());
}

void ScrollingTree::mainFrameViewportChangedViaDelegatedScrolling(const FloatPoint& scrollPosition, const FloatRect& layoutViewport, double)
{
    LOG_WITH_STREAM(Scrolling, stream << "ScrollingTree::viewportChangedViaDelegatedScrolling - layoutViewport " << layoutViewport);
    
    if (!m_rootNode)
        return;

    m_rootNode->wasScrolledByDelegatedScrolling(scrollPosition, layoutViewport);
}

void ScrollingTree::setOverlayScrollbarsEnabled(bool enabled)
{
    if (enabled == m_overlayScrollbarsEnabled)
        return;
    m_overlayScrollbarsEnabled = enabled;
}

void ScrollingTree::removeNode(ScrollingNodeID nodeID, ScrollingTreeFrameHostingNode* hostingNode)
{
    m_latchingController.nodeWasRemoved(nodeID);
    if (auto node = m_nodeMap.take(nodeID)) {
        {
            Locker locker { m_frameIDMapLock };
            auto nodeList = m_nodeMapPerFrame.find(node->frameIdentifier());
            if (nodeList != m_nodeMapPerFrame.end())
                nodeList->value.remove(nodeID);
        }
        if (hostingNode)
            hostingNode->removeHostedChild(node);
        node->willBeDestroyed();
    }
}

void ScrollingTree::removeFrameHostingNode(LayerHostingContextIdentifier hostingIdentifier)
{
    m_hostedSubtrees.remove(hostingIdentifier);
}

bool ScrollingTree::commitTreeStateInternal(std::unique_ptr<ScrollingStateTree>&& scrollingStateTree, std::optional<LayerHostingContextIdentifier> hostingContextIdentifier)
{
    bool rootStateNodeChanged = scrollingStateTree->hasNewRootStateNode();

    LOG(ScrollingTree, "\nScrollingTree %p commitTreeState", this);

    auto rootNode = scrollingStateTree->rootStateNode();
    CommitTreeState commitState {
        .frameId = scrollingStateTree->rootFrameIdentifier()
    };

    if (hostingContextIdentifier) {
        LOG_WITH_STREAM(Scrolling, stream << "ScrollingTree::commitTreeState - starting hosted tree commit for hosting context ID:  " << *hostingContextIdentifier);
        if (auto scrollingNode = m_hostedSubtrees.get(*hostingContextIdentifier))
            commitState.frameHostingNode = scrollingNode;
        else {
            LOG_WITH_STREAM(Scrolling, stream << "ScrollingTree::commitTreeState - parent not present for hosted tree commit");
            // Add to hosted subtrees needing pairing since the hosting node is not in the scrolling tree yet
            m_hostedSubtreesNeedingPairing.ensure(*hostingContextIdentifier, [] {
                return Vector<std::unique_ptr<ScrollingStateTree>> { };
            }).iterator->value.append(WTFMove(scrollingStateTree));
            return true;
        }
        if (!rootNode) {
            if (commitState.frameHostingNode)
                commitState.frameHostingNode->removeHostedChildren();
            return true;
        }
    }
    if (rootNode && !hostingContextIdentifier
        && (rootStateNodeChanged
            || rootNode->hasChangedProperty(ScrollingStateNode::Property::EventTrackingRegion)
            || rootNode->hasChangedProperty(ScrollingStateNode::Property::ScrolledContentsLayer)
            || rootNode->hasChangedProperty(ScrollingStateNode::Property::AsyncFrameOrOverflowScrollingEnabled)
            || rootNode->hasChangedProperty(ScrollingStateNode::Property::WheelEventGesturesBecomeNonBlocking)
            || rootNode->hasChangedProperty(ScrollingStateNode::Property::ScrollingPerformanceTestingEnabled)
            || rootNode->hasChangedProperty(ScrollingStateNode::Property::IsMonitoringWheelEvents)
            || rootNode->hasChangedProperty(ScrollingStateNode::Property::OverlayScrollbarsEnabled))) {
        Locker locker { m_treeStateLock };

        if (rootStateNodeChanged || rootNode->hasChangedProperty(ScrollingStateNode::Property::ScrolledContentsLayer))
            m_treeState.mainFrameScrollPosition = { };

        if (rootStateNodeChanged || rootNode->hasChangedProperty(ScrollingStateNode::Property::EventTrackingRegion))
            m_treeState.eventTrackingRegions = scrollingStateTree->rootStateNode()->eventTrackingRegions();

        if (rootStateNodeChanged || rootNode->hasChangedProperty(ScrollingStateNode::Property::AsyncFrameOrOverflowScrollingEnabled))
            m_asyncFrameOrOverflowScrollingEnabled = scrollingStateTree->rootStateNode()->asyncFrameOrOverflowScrollingEnabled();

        if (rootStateNodeChanged || rootNode->hasChangedProperty(ScrollingStateNode::Property::WheelEventGesturesBecomeNonBlocking))
            m_wheelEventGesturesBecomeNonBlocking = scrollingStateTree->rootStateNode()->wheelEventGesturesBecomeNonBlocking();

        if (rootStateNodeChanged || rootNode->hasChangedProperty(ScrollingStateNode::Property::ScrollingPerformanceTestingEnabled))
            m_scrollingPerformanceTestingEnabled = scrollingStateTree->rootStateNode()->scrollingPerformanceTestingEnabled();

        if (rootStateNodeChanged || rootNode->hasChangedProperty(ScrollingStateNode::Property::IsMonitoringWheelEvents))
            m_isMonitoringWheelEvents = scrollingStateTree->rootStateNode()->isMonitoringWheelEvents();
        
        if (rootStateNodeChanged || rootNode->hasChangedProperty(ScrollingStateNode::Property::OverlayScrollbarsEnabled))
            setOverlayScrollbarsEnabled(scrollingStateTree->rootStateNode()->overlayScrollbarsEnabled());
    }

    m_overflowRelatedNodesMap.removeIf([this, &commitState](auto& keyAndValue) {
        if (auto* scrollingNode = m_nodeMap.get(keyAndValue.key))
            return scrollingNode->frameIdentifier() == commitState.frameId;
        return false;
    });

    m_activeOverflowScrollProxyNodes.removeIf([&commitState](auto& scrollingNode) {
        return scrollingNode->frameIdentifier() == commitState.frameId;
    });

    m_activePositionedNodes.removeIf([&commitState](auto& scrollingNode) {
        return scrollingNode->frameIdentifier() == commitState.frameId;
    });

    {
        Locker locker { m_frameIDMapLock };
        auto nodesForFrame = m_nodeMapPerFrame.get(commitState.frameId);
        for (auto& nodeID : nodesForFrame)
            commitState.unvisitedNodes.add(nodeID);
    }

    bool succeeded = updateTreeFromStateNodeRecursive(rootNode.get(), commitState);
    if (!succeeded)
        return false;

    propagateSynchronousScrollingReasons(commitState.synchronousScrollingNodes);

    for (auto nodeID : commitState.unvisitedNodes) {
        LOG_WITH_STREAM(Scrolling, stream << "ScrollingTree::commitTreeState - removing unvisited node " << nodeID);
        removeNode(nodeID, commitState.frameHostingNode.get());
    }

    LOG_WITH_STREAM(ScrollingTree, stream << "committed ScrollingTree" << scrollingTreeAsText(debugScrollingStateTreeAsTextBehaviors));

    // Recursively commit subtrees that can now be attatched
    auto subtrees = WTFMove(commitState.pendingSubtreesNeedingCommit);
    for (auto& pair : subtrees) {
        auto hostingID = pair.first;
        LOG_WITH_STREAM(Scrolling, stream << "ScrollingTree::commitTreeState - committing unparented subtrees for hosting context ID: " << hostingID);
        for (auto& subtree : pair.second)
            succeeded &= commitTreeStateInternal(WTFMove(subtree), hostingID);
    }

    didCommitTree();

    return succeeded;
}

bool ScrollingTree::commitTreeState(std::unique_ptr<ScrollingStateTree>&& scrollingStateTree, std::optional<LayerHostingContextIdentifier> hostingContextIdentifier)
{
    SetForScope inCommitTreeState(m_inCommitTreeState, true);
    Locker locker { m_treeLock };
    return commitTreeStateInternal(WTFMove(scrollingStateTree), hostingContextIdentifier);
}

bool ScrollingTree::updateTreeFromStateNodeRecursive(const ScrollingStateNode* stateNode, CommitTreeState& state)
{
    if (!stateNode) {
        removeAllNodes();
        m_rootNode = nullptr;
        return true;
    }

    auto nodeID = stateNode->scrollingNodeID();
    auto parentNodeID = stateNode->parentNodeID();
    if (!parentNodeID && state.frameHostingNode && is<ScrollingStateFrameScrollingNode>(stateNode))
        parentNodeID = state.frameHostingNode->scrollingNodeID();

    auto it = m_nodeMap.find(nodeID);

    RefPtr<ScrollingTreeNode> node;
    if (it != m_nodeMap.end()) {
        node = it->value;
        state.unvisitedNodes.remove(nodeID);
    } else {
        node = createScrollingTreeNode(stateNode->nodeType(), nodeID);
        if (!parentNodeID) {
            // This is the root node. Clear the node map.
            auto* frameScrollingNode = dynamicDowncast<ScrollingTreeFrameScrollingNode>(node.get());
            if (!frameScrollingNode)
                return false;
            m_rootNode = frameScrollingNode;
            removeAllNodes();
        }

        m_nodeMap.set(nodeID, node.get());
        {
            Locker locker { m_frameIDMapLock };
            m_nodeMapPerFrame.ensure(state.frameId, [] { return HashSet<ScrollingNodeID> { }; }).iterator->value.add(node->scrollingNodeID());
        }
        node->setFrameIdentifier(state.frameId);
    }

    if (parentNodeID) {
        auto parentIt = m_nodeMap.find(*parentNodeID);
        ASSERT_WITH_SECURITY_IMPLICATION(parentIt != m_nodeMap.end());
        if (parentIt != m_nodeMap.end()) {
            RefPtr parent = parentIt->value.get();

            RefPtr oldParent = node->parent();
            if (oldParent)
                oldParent->removeChild(*node);

            if (oldParent != parent)
                node->setParent(parent.copyRef());

            parent->appendChild(*node);
        } else {
            // FIXME: Use WeakPtr in m_nodeMap.
            m_nodeMap.remove(nodeID);
        }
    }

    if (!node->commitStateBeforeChildren(*stateNode))
        return false;

    // We don't want to update the children of a hosted subtree as they won't be in the state tree
    if (!node->isRootOfHostedSubtree()) {
        // Move all children into the orphanNodes map. Live ones will get added back as we recurse over children.
        for (auto& childScrollingNode : node->children()) {
            childScrollingNode->setParent(nullptr);
            state.orphanNodes.add(childScrollingNode->scrollingNodeID(), childScrollingNode.ptr());
        }
        node->removeAllChildren();

        // Now update the children if we have any.
        for (auto& child : stateNode->children()) {
            ASSERT(child->parent().get() == stateNode);
            if (!updateTreeFromStateNodeRecursive(child.ptr(), state))
                return false;
        }
    }

    if (!node->commitStateAfterChildren(*stateNode))
        return false;

    node->didCompleteCommitForNode();

#if ENABLE(SCROLLING_THREAD)
    if (auto* scrollingNode = dynamicDowncast<ScrollingTreeScrollingNode>(*node); scrollingNode && !scrollingNode->synchronousScrollingReasons().isEmpty())
        state.synchronousScrollingNodes.add(nodeID);
#endif

    if (RefPtr frameHostingNode = dynamicDowncast<ScrollingTreeFrameHostingNode>(node)) {
        if (auto identifier = frameHostingNode->layerHostingContextIdentifier()) {
            if (m_hostedSubtreesNeedingPairing.contains(*identifier))
                state.pendingSubtreesNeedingCommit.append({ *identifier, m_hostedSubtreesNeedingPairing.take(*identifier) });
        }
    }

    if (RefPtr hostingNodeForCommit = state.frameHostingNode)
        hostingNodeForCommit->addHostedChild(node);

    return true;
}

void ScrollingTree::removeAllNodes()
{
    auto nodes = std::exchange(m_nodeMap, { });
    for (auto iter : nodes)
        iter.value->willBeDestroyed();

    m_nodeMap.clear();
    {
        Locker locker { m_frameIDMapLock };
        m_nodeMapPerFrame.clear();
    }
}

void ScrollingTree::applyLayerPositionsAfterCommit()
{
    // Scrolling tree needs to make adjustments only if the UI side positions have changed.
    if (!m_needsApplyLayerPositionsAfterCommit)
        return;
    m_needsApplyLayerPositionsAfterCommit = false;

    applyLayerPositions();
}

void ScrollingTree::applyLayerPositions()
{
    Locker locker { m_treeLock };

    applyLayerPositionsInternal();
}

void ScrollingTree::applyLayerPositionsInternal()
{
    ASSERT(m_treeLock.isLocked());
    if (!m_rootNode)
        return;

    applyLayerPositionsRecursive(*m_rootNode);
}

void ScrollingTree::applyLayerPositionsRecursive(ScrollingTreeNode& node)
{
    node.applyLayerPositions();

    for (auto& child : node.children())
        applyLayerPositionsRecursive(child.get());
}

ScrollingTreeNode* ScrollingTree::nodeForID(std::optional<ScrollingNodeID> nodeID) const
{
    if (!nodeID)
        return nullptr;

    return m_nodeMap.get(*nodeID);
}

void ScrollingTree::notifyRelatedNodesAfterScrollPositionChange(ScrollingTreeScrollingNode& changedNode)
{
    Vector<ScrollingNodeID> additionalUpdateRoots;
    
    if (is<ScrollingTreeOverflowScrollingNode>(changedNode))
        additionalUpdateRoots = overflowRelatedNodes().get(changedNode.scrollingNodeID());

    notifyRelatedNodesRecursive(changedNode);
    
    for (auto positionedNodeID : additionalUpdateRoots) {
        auto* positionedNode = nodeForID(positionedNodeID);
        if (positionedNode)
            notifyRelatedNodesRecursive(*positionedNode);
    }
}

void ScrollingTree::notifyRelatedNodesRecursive(ScrollingTreeNode& node)
{
    node.applyLayerPositions();

    for (auto& child : node.children()) {
        // Never need to cross frame boundaries, since scroll layer adjustments are isolated to each document.
        if (is<ScrollingTreeFrameScrollingNode>(child))
            continue;

        notifyRelatedNodesRecursive(child.get());
    }
}

std::optional<ScrollingNodeID> ScrollingTree::latchedNodeID() const
{
    return m_latchingController.latchedNodeID();
}

void ScrollingTree::clearLatchedNode()
{
    m_latchingController.clearLatchedNode();
}

float ScrollingTree::mainFrameTopContentInset() const
{
    Locker locker { m_treeStateLock };
    return m_rootNode ? m_rootNode->topContentInset() : 0;
}

FloatPoint ScrollingTree::mainFrameScrollPosition() const
{
    Locker locker { m_treeStateLock };
    return m_rootNode ? m_rootNode->currentScrollPosition() : FloatPoint { };
}

void ScrollingTree::setMainFrameScrollPosition(FloatPoint position)
{
    Locker locker { m_treeStateLock };
    m_treeState.mainFrameScrollPosition = position;
}

OverscrollBehavior ScrollingTree::mainFrameHorizontalOverscrollBehavior() const
{
    Locker locker { m_treeLock };
    return m_rootNode ? m_rootNode->horizontalOverscrollBehavior() : OverscrollBehavior::Auto;
}


ScrollbarWidth ScrollingTree::mainFrameScrollbarWidth() const
{
    Locker locker { m_treeLock };
    return m_rootNode ? m_rootNode->scrollbarWidthStyle() : ScrollbarWidth::Auto;
}

OverscrollBehavior ScrollingTree::mainFrameVerticalOverscrollBehavior() const
{
    Locker locker { m_treeLock };
    return m_rootNode ? m_rootNode->verticalOverscrollBehavior() : OverscrollBehavior::Auto;
}

IntPoint ScrollingTree::mainFrameScrollOrigin() const
{
    Locker locker { m_treeLock };
    return m_rootNode ? m_rootNode->scrollOrigin() : IntPoint();
}

int ScrollingTree::mainFrameHeaderHeight() const
{
    Locker locker { m_treeLock };
    return m_rootNode ? m_rootNode->headerHeight() : 0;
}

int ScrollingTree::mainFrameFooterHeight() const
{
    Locker locker { m_treeLock };
    return m_rootNode ? m_rootNode->footerHeight() : 0;
}

float ScrollingTree::mainFrameScaleFactor() const
{
    Locker locker { m_treeLock };
    return m_rootNode ? m_rootNode->frameScaleFactor() : 1;
}

FloatSize ScrollingTree::totalContentsSize() const
{
    Locker locker { m_treeLock };
    return m_rootNode ? m_rootNode->totalContentsSize() : FloatSize();
}

FloatRect ScrollingTree::layoutViewport() const
{
    Locker locker { m_treeLock };
    return m_rootNode ? m_rootNode->layoutViewport() : FloatRect();
}

void ScrollingTree::viewWillStartLiveResize()
{
    Locker locker { m_treeLock };
    if (m_rootNode)
        m_rootNode->viewWillStartLiveResize();
}

void ScrollingTree::viewWillEndLiveResize()
{
    Locker locker { m_treeLock };
    if (m_rootNode)
        m_rootNode->viewWillEndLiveResize();
}

void ScrollingTree::viewSizeDidChange()
{
    Locker locker { m_treeLock };
    if (m_rootNode)
        m_rootNode->viewSizeDidChange();
}

void ScrollingTree::setGestureState(std::optional<WheelScrollGestureState> gestureState)
{
    Locker locker { m_treeStateLock };
    m_treeState.gestureState = gestureState;
}

std::optional<WheelScrollGestureState> ScrollingTree::gestureState()
{
    Locker locker { m_treeStateLock };
    return m_treeState.gestureState;
}

TrackingType ScrollingTree::eventTrackingTypeForPoint(EventTrackingRegions::EventType eventType, IntPoint p)
{
    Locker locker { m_treeStateLock };
    return m_treeState.eventTrackingRegions.trackingTypeForPoint(eventType, p);
}

// Can be called from the main thread.
bool ScrollingTree::isRubberBandInProgressForNode(std::optional<ScrollingNodeID> nodeID)
{
    if (!nodeID)
        return false;

    Locker locker { m_treeStateLock };
    return m_treeState.nodesWithActiveRubberBanding.contains(*nodeID);
}

void ScrollingTree::setRubberBandingInProgressForNode(ScrollingNodeID nodeID, bool isRubberBanding)
{
    Locker locker { m_treeStateLock };
    if (isRubberBanding)
        m_treeState.nodesWithActiveRubberBanding.add(nodeID);
    else
        m_treeState.nodesWithActiveRubberBanding.remove(nodeID);
}

// Can be called from the main thread.
bool ScrollingTree::isUserScrollInProgressForNode(std::optional<ScrollingNodeID> nodeID)
{
    if (!nodeID)
        return false;

    Locker locker { m_treeStateLock };
    return m_treeState.nodesWithActiveUserScrolls.contains(*nodeID);
}
    
void ScrollingTree::setUserScrollInProgressForNode(ScrollingNodeID nodeID, bool isScrolling)
{
    Locker locker { m_treeStateLock };
    if (isScrolling) {
        m_treeState.nodesWithActiveUserScrolls.add(nodeID);
        scrollingTreeNodeWillStartScroll(nodeID);
    } else {
        m_treeState.nodesWithActiveUserScrolls.remove(nodeID);
        scrollingTreeNodeDidEndScroll(nodeID);
    }
}

void ScrollingTree::clearNodesWithUserScrollInProgress()
{
    Locker locker { m_treeStateLock };
    m_treeState.nodesWithActiveUserScrolls.clear();
}

// Can be called from the main thread.
bool ScrollingTree::isScrollSnapInProgressForNode(std::optional<ScrollingNodeID> nodeID)
{
    if (!nodeID)
        return false;

    Locker locker { m_treeStateLock };
    return m_treeState.nodesWithActiveScrollSnap.contains(*nodeID);
}

void ScrollingTree::setNodeScrollSnapInProgress(ScrollingNodeID nodeID, bool isScrollSnapping)
{
    Locker locker { m_treeStateLock };

    if (isScrollSnapping) {
        if (m_treeState.nodesWithActiveScrollSnap.add(nodeID).isNewEntry)
            scrollingTreeNodeDidBeginScrollSnapping(nodeID);
    } else {
        if (m_treeState.nodesWithActiveScrollSnap.remove(nodeID))
            scrollingTreeNodeDidEndScrollSnapping(nodeID);
    }
}

bool ScrollingTree::isScrollAnimationInProgressForNode(std::optional<ScrollingNodeID> nodeID)
{
    if (!nodeID)
        return false;

    Locker locker { m_treeStateLock };
    return m_treeState.nodesWithActiveScrollAnimations.contains(*nodeID);
}

void ScrollingTree::setScrollAnimationInProgressForNode(std::optional<ScrollingNodeID> nodeID, bool isScrollAnimationInProgress)
{
    if (!nodeID)
        return;

    Locker locker { m_treeStateLock };
    
    bool hadAnyAnimatedScrollingNodes = !m_treeState.nodesWithActiveScrollAnimations.isEmpty();
    
    if (isScrollAnimationInProgress)
        m_treeState.nodesWithActiveScrollAnimations.add(*nodeID);
    else
        m_treeState.nodesWithActiveScrollAnimations.remove(*nodeID);

    bool hasAnyAnimatedScrollingNodes = !m_treeState.nodesWithActiveScrollAnimations.isEmpty();
    if (hasAnyAnimatedScrollingNodes != hadAnyAnimatedScrollingNodes)
        hasNodeWithAnimatedScrollChanged(hasAnyAnimatedScrollingNodes);
}

bool ScrollingTree::hasNodeWithActiveScrollAnimations()
{
    Locker locker { m_treeStateLock };
    return !m_treeState.nodesWithActiveScrollAnimations.isEmpty();
}

HashSet<ScrollingNodeID> ScrollingTree::nodesWithActiveScrollAnimations()
{
    Locker locker { m_treeStateLock };
    return m_treeState.nodesWithActiveScrollAnimations;
}

void ScrollingTree::serviceScrollAnimations(MonotonicTime currentTime)
{
    for (auto nodeID : nodesWithActiveScrollAnimations()) {
        RefPtr targetNode = dynamicDowncast<ScrollingTreeScrollingNode>(nodeForID(nodeID));
        if (!targetNode)
            continue;
        targetNode->serviceScrollAnimation(currentTime);
    }
}

void ScrollingTree::setMainFramePinnedState(RectEdges<bool> edgePinningState)
{
    Locker locker { m_swipeStateLock };

    m_swipeState.mainFramePinnedState = edgePinningState;
}

void ScrollingTree::setClientAllowedMainFrameRubberBandableEdges(RectEdges<bool> clientAllowedRubberBandableEdges)
{
    Locker locker { m_swipeStateLock };

    m_swipeState.clientAllowedRubberBandableEdges = clientAllowedRubberBandableEdges;
}

bool ScrollingTree::clientAllowsMainFrameRubberBandingOnSide(BoxSide side)
{
    Locker locker { m_swipeStateLock };
    return m_swipeState.clientAllowedRubberBandableEdges.at(side);
}

void ScrollingTree::addPendingScrollUpdate(ScrollUpdate&& update)
{
    Locker locker { m_pendingScrollUpdatesLock };
    for (auto& existingUpdate : m_pendingScrollUpdates) {
        if (existingUpdate.canMerge(update)) {
            existingUpdate.merge(WTFMove(update));
            return;
        }
    }

    m_pendingScrollUpdates.append(WTFMove(update));
}

Vector<ScrollUpdate> ScrollingTree::takePendingScrollUpdates()
{
    Locker locker { m_pendingScrollUpdatesLock };
    return std::exchange(m_pendingScrollUpdates, { });
}

bool ScrollingTree::hasPendingScrollUpdates()
{
    Locker locker { m_pendingScrollUpdatesLock };
    return m_pendingScrollUpdates.size();
}

// Can be called from the main thread.
void ScrollingTree::setScrollPinningBehavior(ScrollPinningBehavior pinning)
{
    Locker locker { m_swipeStateLock };
    
    m_swipeState.scrollPinningBehavior = pinning;
}

ScrollPinningBehavior ScrollingTree::scrollPinningBehavior()
{
    Locker locker { m_swipeStateLock };
    
    return m_swipeState.scrollPinningBehavior;
}

bool ScrollingTree::willWheelEventStartSwipeGesture(const PlatformWheelEvent& wheelEvent)
{
    if (wheelEvent.phase() != PlatformWheelEventPhase::Began)
        return false;

    Locker locker { m_swipeStateLock };

    if (wheelEvent.deltaX() > 0 && m_swipeState.mainFramePinnedState.left() && !m_swipeState.clientAllowedRubberBandableEdges.left())
        return true;
    if (wheelEvent.deltaX() < 0 && m_swipeState.mainFramePinnedState.right() && !m_swipeState.clientAllowedRubberBandableEdges.right())
        return true;
    if (wheelEvent.deltaY() > 0 && m_swipeState.mainFramePinnedState.top() && !m_swipeState.clientAllowedRubberBandableEdges.top())
        return true;
    if (wheelEvent.deltaY() < 0 && m_swipeState.mainFramePinnedState.bottom() && !m_swipeState.clientAllowedRubberBandableEdges.bottom())
        return true;

    return false;
}

void ScrollingTree::scrollBySimulatingWheelEventForTesting(ScrollingNodeID nodeID, FloatSize delta)
{
    Locker locker { m_treeLock };

    auto* node = dynamicDowncast<ScrollingTreeScrollingNode>(nodeForID(nodeID));
    if (!node)
        return;
    node->scrollBy(delta);
}

void ScrollingTree::windowScreenDidChange(PlatformDisplayID displayID, std::optional<FramesPerSecond> nominalFramesPerSecond)
{
    Locker locker { m_treeStateLock };
    m_treeState.displayID = displayID;
    m_treeState.nominalFramesPerSecond = nominalFramesPerSecond;
}

PlatformDisplayID ScrollingTree::displayID()
{
    Locker locker { m_treeStateLock };
    return m_treeState.displayID;
}

bool ScrollingTree::hasRecentActivity()
{
    return hasProcessedWheelEventsRecently() || hasNodeWithActiveScrollAnimations();
}

bool ScrollingTree::hasProcessedWheelEventsRecently()
{
    Locker locker { m_lastWheelEventTimeLock };
    constexpr auto activityInterval = 50_ms; // Duration of a few frames so that we stay active for sequence of wheel events.
    
    return (MonotonicTime::now() - m_lastWheelEventTime) < activityInterval;
}

void ScrollingTree::willProcessWheelEvent()
{
    Locker locker { m_lastWheelEventTimeLock };
    m_lastWheelEventTime = MonotonicTime::now();
}

std::optional<FramesPerSecond> ScrollingTree::nominalFramesPerSecond()
{
    Locker locker { m_treeStateLock };
    return m_treeState.nominalFramesPerSecond;
}

bool ScrollingTree::overlayScrollbarsEnabled()
{
    Locker locker { m_treeStateLock };
    return m_overlayScrollbarsEnabled;
}

Seconds ScrollingTree::frameDuration()
{
    auto displayFPS = nominalFramesPerSecond().value_or(FullSpeedFramesPerSecond);
    return 1_s / (double)displayFPS;
}

Seconds ScrollingTree::maxAllowableRenderingUpdateDurationForSynchronization()
{
    constexpr double allowableFrameFraction = 0.5;
    return allowableFrameFraction * frameDuration();
}

void ScrollingTree::addScrollingNodeToHostedSubtreeMap(LayerHostingContextIdentifier identifier, Ref<ScrollingTreeFrameHostingNode> node)
{
    m_hostedSubtrees.set(identifier, node);
}

std::optional<FrameIdentifier> ScrollingTree::frameIDForScrollingNodeID(ScrollingNodeID scrollingNodeID)
{
    Locker locker { m_frameIDMapLock };

    for (auto& pair : m_nodeMapPerFrame) {
        if (pair.value.contains(scrollingNodeID))
            return pair.key;
    }
    return std::nullopt;
}

String ScrollingTree::scrollingTreeAsText(OptionSet<ScrollingStateTreeAsTextBehavior> behavior)
{
    TextStream ts(TextStream::LineMode::MultipleLine);

    {
        TextStream::GroupScope scope(ts);
        ts << "scrolling tree";

        Locker locker { m_treeStateLock };

        if (auto latchedNodeID = m_latchingController.latchedNodeID())
            ts.dumpProperty("latched node", latchedNodeID.value());

        if (!m_treeState.mainFrameScrollPosition.isZero())
            ts.dumpProperty("main frame scroll position", m_treeState.mainFrameScrollPosition);
        
        if (m_rootNode) {
            TextStream::GroupScope scope(ts);
            m_rootNode->dump(ts, behavior | ScrollingStateTreeAsTextBehavior::IncludeLayerPositions);
        }
        
        if (behavior & ScrollingStateTreeAsTextBehavior::IncludeNodeIDs) {
            if (!m_overflowRelatedNodesMap.isEmpty()) {
                TextStream::GroupScope scope(ts);
                ts << "overflow related nodes";
                {
                    TextStream::IndentScope indentScope(ts);
                    for (auto& it : m_overflowRelatedNodesMap)
                        ts << "\n" << indent << it.key << " -> " << it.value;
                }
            }

#if ENABLE(SCROLLING_THREAD)
            if (!m_activeOverflowScrollProxyNodes.isEmpty()) {
                TextStream::GroupScope scope(ts);
                ts << "overflow scroll proxy nodes";
                {
                    TextStream::IndentScope indentScope(ts);
                    for (auto& node : m_activeOverflowScrollProxyNodes)
                        ts << "\n" << indent << node->scrollingNodeID();
                }
            }

            if (!m_activePositionedNodes.isEmpty()) {
                TextStream::GroupScope scope(ts);
                ts << "active positioned nodes";
                {
                    TextStream::IndentScope indentScope(ts);
                    for (const auto& node : m_activePositionedNodes)
                        ts << "\n" << indent << node->scrollingNodeID();
                }
            }
#endif // ENABLE(SCROLLING_THREAD)
        }
    }
    return ts.release();
}

} // namespace WebCore

#endif // ENABLE(ASYNC_SCROLLING)
