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

#include "EventNames.h"
#include "Logging.h"
#include "PlatformWheelEvent.h"
#include "ScrollingStateFrameScrollingNode.h"
#include "ScrollingStateTree.h"
#include "ScrollingTreeFrameScrollingNode.h"
#include "ScrollingTreeNode.h"
#include "ScrollingTreeOverflowScrollProxyNode.h"
#include "ScrollingTreeOverflowScrollingNode.h"
#include "ScrollingTreePositionedNode.h"
#include "ScrollingTreeScrollingNode.h"
#include <wtf/SetForScope.h>
#include <wtf/text/TextStream.h>

namespace WebCore {

using OrphanScrollingNodeMap = HashMap<ScrollingNodeID, RefPtr<ScrollingTreeNode>>;

struct CommitTreeState {
    // unvisitedNodes starts with all nodes in the map; we remove nodes as we visit them. At the end, it's the unvisited nodes.
    // We can't use orphanNodes for this, because orphanNodes won't contain descendants of removed nodes.
    HashSet<ScrollingNodeID> unvisitedNodes;
    // Nodes with non-empty synchronousScrollingReasons.
    HashSet<ScrollingNodeID> synchronousScrollingNodes;
    // orphanNodes keeps child nodes alive while we rebuild child lists.
    OrphanScrollingNodeMap orphanNodes;
};

ScrollingTree::ScrollingTree()
    : m_gestureState(*this)
{
}

ScrollingTree::~ScrollingTree() = default;

OptionSet<WheelEventProcessingSteps> ScrollingTree::determineWheelEventProcessing(const PlatformWheelEvent& wheelEvent)
{
    // This method is invoked by the event handling thread
    LockHolder lock(m_treeStateMutex);

    LOG_WITH_STREAM(ScrollLatching, stream << "ScrollingTree::shouldHandleWheelEventSynchronously " << wheelEvent << " have latched node " << m_latchingController.latchedNodeForEvent(wheelEvent, m_allowLatching));
    if (m_latchingController.latchedNodeForEvent(wheelEvent, m_allowLatching))
        return { WheelEventProcessingSteps::ScrollingThread };

    m_latchingController.receivedWheelEvent(wheelEvent, m_allowLatching);

    if (!m_treeState.eventTrackingRegions.isEmpty() && m_rootNode) {
        FloatPoint position = wheelEvent.position();
        position.move(m_rootNode->viewToContentsOffset(m_treeState.mainFrameScrollPosition));

        const EventNames& names = eventNames();
        IntPoint roundedPosition = roundedIntPoint(position);

        // Event regions are affected by page scale, so no need to map through scale.
        bool isSynchronousDispatchRegion = m_treeState.eventTrackingRegions.trackingTypeForPoint(names.wheelEvent, roundedPosition) == TrackingType::Synchronous
            || m_treeState.eventTrackingRegions.trackingTypeForPoint(names.mousewheelEvent, roundedPosition) == TrackingType::Synchronous;
        LOG_WITH_STREAM(Scrolling, stream << "\n\nScrollingTree::shouldHandleWheelEventSynchronously: wheelEvent " << wheelEvent << " mapped to content point " << position << ", in non-fast region " << isSynchronousDispatchRegion);

        if (isSynchronousDispatchRegion)
            return { WheelEventProcessingSteps::MainThreadForScrolling, WheelEventProcessingSteps::MainThreadForDOMEventDispatch };
    }
    return { WheelEventProcessingSteps::ScrollingThread };
}

WheelEventHandlingResult ScrollingTree::handleWheelEvent(const PlatformWheelEvent& wheelEvent)
{
    LOG_WITH_STREAM(Scrolling, stream << "\nScrollingTree " << this << " handleWheelEvent " << wheelEvent);

    LockHolder locker(m_treeMutex);

    if (isMonitoringWheelEvents())
        receivedWheelEvent(wheelEvent);

    m_latchingController.receivedWheelEvent(wheelEvent, m_allowLatching);

    auto result = [&] {
        if (!m_rootNode)
            return WheelEventHandlingResult::unhandled();

        if (!asyncFrameOrOverflowScrollingEnabled())
            return m_rootNode->handleWheelEvent(wheelEvent);

        if (m_gestureState.handleGestureCancel(wheelEvent)) {
            clearNodesWithUserScrollInProgress();
            return WheelEventHandlingResult::handled();
        }

        m_gestureState.receivedWheelEvent(wheelEvent);

        if (auto latchedNodeID = m_latchingController.latchedNodeForEvent(wheelEvent, m_allowLatching)) {
            LOG_WITH_STREAM(ScrollLatching, stream << "ScrollingTree::handleWheelEvent: has latched node " << latchedNodeID);
            auto* node = nodeForID(*latchedNodeID);
            if (is<ScrollingTreeScrollingNode>(node)) {
                auto result = downcast<ScrollingTreeScrollingNode>(*node).handleWheelEvent(wheelEvent);
                if (result.wasHandled) {
                    m_latchingController.nodeDidHandleEvent(*latchedNodeID, wheelEvent, m_allowLatching);
                    m_gestureState.nodeDidHandleEvent(*latchedNodeID, wheelEvent);
                }
                return result;
            }
        }

        FloatPoint position = wheelEvent.position();
        position.move(m_rootNode->viewToContentsOffset(m_treeState.mainFrameScrollPosition));
        auto node = scrollingNodeForPoint(position);

        LOG_WITH_STREAM(Scrolling, stream << "ScrollingTree::handleWheelEvent found node " << (node ? node->scrollingNodeID() : 0) << " for point " << position);

        return handleWheelEventWithNode(wheelEvent, node.get());
    }();

    return result;
}

WheelEventHandlingResult ScrollingTree::handleWheelEventWithNode(const PlatformWheelEvent& wheelEvent, ScrollingTreeNode* node)
{
    while (node) {
        if (is<ScrollingTreeScrollingNode>(*node)) {
            auto& scrollingNode = downcast<ScrollingTreeScrollingNode>(*node);
            auto result = scrollingNode.handleWheelEvent(wheelEvent);

            if (result.wasHandled) {
                m_latchingController.nodeDidHandleEvent(scrollingNode.scrollingNodeID(), wheelEvent, m_allowLatching);
                m_gestureState.nodeDidHandleEvent(scrollingNode.scrollingNodeID(), wheelEvent);
                return result;
            }

            if (result.needsMainThreadProcessing())
                return result;
        }

        if (is<ScrollingTreeOverflowScrollProxyNode>(*node)) {
            if (auto relatedNode = nodeForID(downcast<ScrollingTreeOverflowScrollProxyNode>(*node).overflowScrollingNodeID())) {
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
    LockHolder locker(m_treeMutex);
    if (!m_rootNode)
        return;

    auto function = WTFMove(visitorFunction);
    traverseScrollingTreeRecursive(*m_rootNode, function);
}

void ScrollingTree::traverseScrollingTreeRecursive(ScrollingTreeNode& node, const VisitorFunction& visitorFunction)
{
    bool scrolledSinceLastCommit = false;
    Optional<FloatPoint> scrollPosition;
    if (is<ScrollingTreeScrollingNode>(node)) {
        auto& scrollingNode = downcast<ScrollingTreeScrollingNode>(node);
        scrollPosition = scrollingNode.currentScrollPosition();
        scrolledSinceLastCommit = scrollingNode.scrolledSinceLastCommit();
    }

    Optional<FloatPoint> layoutViewportOrigin;
    if (is<ScrollingTreeFrameScrollingNode>(node))
        layoutViewportOrigin = downcast<ScrollingTreeFrameScrollingNode>(node).layoutViewport().location();

    visitorFunction(node.scrollingNodeID(), node.nodeType(), scrollPosition, layoutViewportOrigin, scrolledSinceLastCommit);

    for (auto& child : node.children())
        traverseScrollingTreeRecursive(child.get(), visitorFunction);
}

void ScrollingTree::mainFrameViewportChangedViaDelegatedScrolling(const FloatPoint& scrollPosition, const FloatRect& layoutViewport, double)
{
    LOG_WITH_STREAM(Scrolling, stream << "ScrollingTree::viewportChangedViaDelegatedScrolling - layoutViewport " << layoutViewport);
    
    if (!m_rootNode)
        return;

    m_rootNode->wasScrolledByDelegatedScrolling(scrollPosition, layoutViewport);
}

void ScrollingTree::commitTreeState(std::unique_ptr<ScrollingStateTree>&& scrollingStateTree)
{
    SetForScope<bool> inCommitTreeState(m_inCommitTreeState, true);
    LockHolder locker(m_treeMutex);

    bool rootStateNodeChanged = scrollingStateTree->hasNewRootStateNode();

    LOG(ScrollingTree, "\nScrollingTree %p commitTreeState", this);

    auto* rootNode = scrollingStateTree->rootStateNode();
    if (rootNode
        && (rootStateNodeChanged
            || rootNode->hasChangedProperty(ScrollingStateFrameScrollingNode::EventTrackingRegion)
            || rootNode->hasChangedProperty(ScrollingStateScrollingNode::ScrolledContentsLayer)
            || rootNode->hasChangedProperty(ScrollingStateFrameScrollingNode::AsyncFrameOrOverflowScrollingEnabled)
            || rootNode->hasChangedProperty(ScrollingStateFrameScrollingNode::IsMonitoringWheelEvents))) {
        LockHolder lock(m_treeStateMutex);

        if (rootStateNodeChanged || rootNode->hasChangedProperty(ScrollingStateScrollingNode::ScrolledContentsLayer))
            m_treeState.mainFrameScrollPosition = { };

        if (rootStateNodeChanged || rootNode->hasChangedProperty(ScrollingStateFrameScrollingNode::EventTrackingRegion))
            m_treeState.eventTrackingRegions = scrollingStateTree->rootStateNode()->eventTrackingRegions();

        if (rootStateNodeChanged || rootNode->hasChangedProperty(ScrollingStateFrameScrollingNode::AsyncFrameOrOverflowScrollingEnabled))
            m_asyncFrameOrOverflowScrollingEnabled = scrollingStateTree->rootStateNode()->asyncFrameOrOverflowScrollingEnabled();

        if (rootStateNodeChanged || rootNode->hasChangedProperty(ScrollingStateFrameScrollingNode::IsMonitoringWheelEvents))
            m_isMonitoringWheelEvents = scrollingStateTree->rootStateNode()->isMonitoringWheelEvents();
    }

    m_overflowRelatedNodesMap.clear();
    m_activeOverflowScrollProxyNodes.clear();
    m_activePositionedNodes.clear();

    CommitTreeState commitState;
    for (auto nodeID : m_nodeMap.keys())
        commitState.unvisitedNodes.add(nodeID);

    updateTreeFromStateNodeRecursive(rootNode, commitState);
    propagateSynchronousScrollingReasons(commitState.synchronousScrollingNodes);

    for (auto nodeID : commitState.unvisitedNodes) {
        LOG_WITH_STREAM(Scrolling, stream << "ScrollingTree::commitTreeState - removing unvisited node " << nodeID);

        m_latchingController.nodeWasRemoved(nodeID);

        if (auto node = m_nodeMap.take(nodeID))
            node->willBeDestroyed();
    }

    LOG_WITH_STREAM(ScrollingTree, stream << "committed ScrollingTree" << scrollingTreeAsText(ScrollingStateTreeAsTextBehaviorDebug));
}

void ScrollingTree::updateTreeFromStateNodeRecursive(const ScrollingStateNode* stateNode, CommitTreeState& state)
{
    if (!stateNode) {
        removeAllNodes();
        m_rootNode = nullptr;
        return;
    }
    
    ScrollingNodeID nodeID = stateNode->scrollingNodeID();
    ScrollingNodeID parentNodeID = stateNode->parentNodeID();

    auto it = m_nodeMap.find(nodeID);

    RefPtr<ScrollingTreeNode> node;
    if (it != m_nodeMap.end()) {
        node = it->value;
        state.unvisitedNodes.remove(nodeID);
    } else {
        node = createScrollingTreeNode(stateNode->nodeType(), nodeID);
        if (!parentNodeID) {
            // This is the root node. Clear the node map.
            ASSERT(stateNode->isFrameScrollingNode());
            m_rootNode = downcast<ScrollingTreeFrameScrollingNode>(node.get());
            removeAllNodes();
        } 
        m_nodeMap.set(nodeID, node.get());
    }

    if (parentNodeID) {
        auto parentIt = m_nodeMap.find(parentNodeID);
        ASSERT_WITH_SECURITY_IMPLICATION(parentIt != m_nodeMap.end());
        if (parentIt != m_nodeMap.end()) {
            auto* parent = parentIt->value.get();

            auto* oldParent = node->parent();
            if (oldParent)
                oldParent->removeChild(*node);

            if (oldParent != parent)
                node->setParent(parent);

            parent->appendChild(*node);
        } else {
            // FIXME: Use WeakPtr in m_nodeMap.
            m_nodeMap.remove(nodeID);
        }
    }

    node->commitStateBeforeChildren(*stateNode);
    
    // Move all children into the orphanNodes map. Live ones will get added back as we recurse over children.
    for (auto& childScrollingNode : node->children()) {
        childScrollingNode->setParent(nullptr);
        state.orphanNodes.add(childScrollingNode->scrollingNodeID(), childScrollingNode.ptr());
    }
    node->removeAllChildren();

    // Now update the children if we have any.
    if (auto children = stateNode->children()) {
        for (auto& child : *children)
            updateTreeFromStateNodeRecursive(child.get(), state);
    }

    node->commitStateAfterChildren(*stateNode);
    node->didCompleteCommitForNode();

#if ENABLE(SCROLLING_THREAD)
    if (is<ScrollingTreeScrollingNode>(*node) && !downcast<ScrollingTreeScrollingNode>(*node).synchronousScrollingReasons().isEmpty())
        state.synchronousScrollingNodes.add(nodeID);
#endif
}

void ScrollingTree::removeAllNodes()
{
    for (auto iter : m_nodeMap)
        iter.value->willBeDestroyed();

    m_nodeMap.clear();
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
    ASSERT(isMainThread());
    LockHolder locker(m_treeMutex);

    applyLayerPositionsInternal();
}

void ScrollingTree::applyLayerPositionsInternal()
{
    ASSERT(m_treeMutex.isLocked());
    if (!m_rootNode)
        return;

    LOG(Scrolling, "\nScrollingTree %p applyLayerPositions (main thread %d)", this, isMainThread());
    applyLayerPositionsRecursive(*m_rootNode);
}

void ScrollingTree::applyLayerPositionsRecursive(ScrollingTreeNode& node)
{
    node.applyLayerPositions();

    for (auto& child : node.children())
        applyLayerPositionsRecursive(child.get());
}

ScrollingTreeNode* ScrollingTree::nodeForID(ScrollingNodeID nodeID) const
{
    if (!nodeID)
        return nullptr;

    return m_nodeMap.get(nodeID);
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

Optional<ScrollingNodeID> ScrollingTree::latchedNodeID() const
{
    return m_latchingController.latchedNodeID();
}

void ScrollingTree::clearLatchedNode()
{
    m_latchingController.clearLatchedNode();
}

void ScrollingTree::setAsyncFrameOrOverflowScrollingEnabled(bool enabled)
{
    m_asyncFrameOrOverflowScrollingEnabled = enabled;
}

FloatPoint ScrollingTree::mainFrameScrollPosition() const
{
    ASSERT(m_treeStateMutex.isLocked());
    return m_treeState.mainFrameScrollPosition;
}

void ScrollingTree::setMainFrameScrollPosition(FloatPoint position)
{
    LockHolder lock(m_treeStateMutex);
    m_treeState.mainFrameScrollPosition = position;
}

TrackingType ScrollingTree::eventTrackingTypeForPoint(const AtomString& eventName, IntPoint p)
{
    LockHolder lock(m_treeStateMutex);
    return m_treeState.eventTrackingRegions.trackingTypeForPoint(eventName, p);
}

// Can be called from the main thread.
bool ScrollingTree::isRubberBandInProgress()
{
    LockHolder lock(m_treeStateMutex);
    return m_treeState.mainFrameIsRubberBanding;
}

void ScrollingTree::setMainFrameIsRubberBanding(bool isRubberBanding)
{
    LockHolder locker(m_treeStateMutex);
    m_treeState.mainFrameIsRubberBanding = isRubberBanding;
}

// Can be called from the main thread.
bool ScrollingTree::isUserScrollInProgressForNode(ScrollingNodeID nodeID)
{
    if (!nodeID)
        return false;

    LockHolder lock(m_treeStateMutex);
    return m_treeState.nodesWithActiveUserScrolls.contains(nodeID);
}
    
void ScrollingTree::setUserScrollInProgressForNode(ScrollingNodeID nodeID, bool isScrolling)
{
    ASSERT(nodeID);
    LockHolder locker(m_treeStateMutex);
    if (isScrolling)
        m_treeState.nodesWithActiveUserScrolls.add(nodeID);
    else
        m_treeState.nodesWithActiveUserScrolls.remove(nodeID);
}

void ScrollingTree::clearNodesWithUserScrollInProgress()
{
    LockHolder locker(m_treeStateMutex);
    m_treeState.nodesWithActiveUserScrolls.clear();
}

// Can be called from the main thread.
bool ScrollingTree::isScrollSnapInProgressForNode(ScrollingNodeID nodeID)
{
    if (!nodeID)
        return false;

    LockHolder lock(m_treeStateMutex);
    return m_treeState.nodesWithActiveScrollSnap.contains(nodeID);
}
    
void ScrollingTree::setNodeScrollSnapInProgress(ScrollingNodeID nodeID, bool isScrollSnapping)
{
    ASSERT(nodeID);
    LockHolder locker(m_treeStateMutex);
    if (isScrollSnapping)
        m_treeState.nodesWithActiveScrollSnap.add(nodeID);
    else
        m_treeState.nodesWithActiveScrollSnap.remove(nodeID);
}

void ScrollingTree::setMainFramePinnedState(RectEdges<bool> edgePinningState)
{
    LockHolder locker(m_swipeStateMutex);

    m_swipeState.mainFramePinnedState = edgePinningState;
}

void ScrollingTree::setMainFrameCanRubberBand(RectEdges<bool> canRubberBand)
{
    LockHolder locker(m_swipeStateMutex);

    m_swipeState.canRubberBand = canRubberBand;
}

bool ScrollingTree::mainFrameCanRubberBandInDirection(ScrollDirection direction)
{
    LockHolder locker(m_swipeStateMutex);

    switch (direction) {
    case ScrollUp: return m_swipeState.canRubberBand.top();
    case ScrollDown: return m_swipeState.canRubberBand.bottom();
    case ScrollLeft: return m_swipeState.canRubberBand.left();
    case ScrollRight: return m_swipeState.canRubberBand.right();
    };

    return false;
}

// Can be called from the main thread.
void ScrollingTree::setScrollPinningBehavior(ScrollPinningBehavior pinning)
{
    LockHolder locker(m_swipeStateMutex);
    
    m_swipeState.scrollPinningBehavior = pinning;
}

ScrollPinningBehavior ScrollingTree::scrollPinningBehavior()
{
    LockHolder lock(m_swipeStateMutex);
    
    return m_swipeState.scrollPinningBehavior;
}

bool ScrollingTree::willWheelEventStartSwipeGesture(const PlatformWheelEvent& wheelEvent)
{
    if (wheelEvent.phase() != PlatformWheelEventPhaseBegan)
        return false;

    LockHolder lock(m_swipeStateMutex);

    if (wheelEvent.deltaX() > 0 && m_swipeState.mainFramePinnedState.left() && !m_swipeState.canRubberBand.left())
        return true;
    if (wheelEvent.deltaX() < 0 && m_swipeState.mainFramePinnedState.right() && !m_swipeState.canRubberBand.right())
        return true;
    if (wheelEvent.deltaY() > 0 && m_swipeState.mainFramePinnedState.top() && !m_swipeState.canRubberBand.top())
        return true;
    if (wheelEvent.deltaY() < 0 && m_swipeState.mainFramePinnedState.bottom() && !m_swipeState.canRubberBand.bottom())
        return true;

    return false;
}

void ScrollingTree::scrollBySimulatingWheelEventForTesting(ScrollingNodeID nodeID, FloatSize delta)
{
    LockHolder locker(m_treeMutex);

    auto* node = nodeForID(nodeID);
    if (!is<ScrollingTreeScrollingNode>(node))
        return;

    downcast<ScrollingTreeScrollingNode>(*node).scrollBy(delta);
}

void ScrollingTree::windowScreenDidChange(PlatformDisplayID displayID, Optional<unsigned> nominalFramesPerSecond)
{
    LockHolder locker(m_treeStateMutex);
    m_treeState.displayID = displayID;
    m_treeState.nominalFramesPerSecond = nominalFramesPerSecond;
}

PlatformDisplayID ScrollingTree::displayID()
{
    LockHolder locker(m_treeStateMutex);
    return m_treeState.displayID;
}

bool ScrollingTree::hasProcessedWheelEventsRecently()
{
    LockHolder locker(m_lastWheelEventTimeMutex);
    constexpr auto activityInterval = 50_ms; // Duration of a few frames so that we stay active for sequence of wheel events.
    
    return (MonotonicTime::now() - m_lastWheelEventTime) < activityInterval;
}

void ScrollingTree::willProcessWheelEvent()
{
    LockHolder locker(m_lastWheelEventTimeMutex);
    m_lastWheelEventTime = MonotonicTime::now();
}

Optional<unsigned> ScrollingTree::nominalFramesPerSecond()
{
    LockHolder locker(m_treeStateMutex);
    return m_treeState.nominalFramesPerSecond;
}

void ScrollingTree::setScrollingPerformanceLoggingEnabled(bool flag)
{
    m_scrollingPerformanceLoggingEnabled = flag;
}

bool ScrollingTree::scrollingPerformanceLoggingEnabled()
{
    return m_scrollingPerformanceLoggingEnabled;
}

String ScrollingTree::scrollingTreeAsText(ScrollingStateTreeAsTextBehavior behavior)
{
    TextStream ts(TextStream::LineMode::MultipleLine);

    {
        TextStream::GroupScope scope(ts);
        ts << "scrolling tree";

        LockHolder locker(m_treeStateMutex);

        if (auto latchedNodeID = m_latchingController.latchedNodeID())
            ts.dumpProperty("latched node", latchedNodeID.value());

        if (!m_treeState.mainFrameScrollPosition.isZero())
            ts.dumpProperty("main frame scroll position", m_treeState.mainFrameScrollPosition);
        
        if (m_rootNode) {
            TextStream::GroupScope scope(ts);
            m_rootNode->dump(ts, behavior | ScrollingStateTreeAsTextBehaviorIncludeLayerPositions);
        }
        
        if (behavior & ScrollingStateTreeAsTextBehaviorIncludeNodeIDs) {
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
