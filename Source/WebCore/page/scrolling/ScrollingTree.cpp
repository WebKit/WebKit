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

ScrollingTree::ScrollingTree() = default;

ScrollingTree::~ScrollingTree() = default;

bool ScrollingTree::shouldHandleWheelEventSynchronously(const PlatformWheelEvent& wheelEvent)
{
    // This method is invoked by the event handling thread
    LockHolder lock(m_treeStateMutex);

    bool shouldSetLatch = wheelEvent.shouldConsiderLatching();
    
    if (hasLatchedNode() && !shouldSetLatch)
        return false;

    if (shouldSetLatch)
        m_treeState.latchedNodeID = 0;
    
    if (!m_treeState.eventTrackingRegions.isEmpty() && m_rootNode) {
        FloatPoint position = wheelEvent.position();
        position.move(m_rootNode->viewToContentsOffset(m_treeState.mainFrameScrollPosition));

        const EventNames& names = eventNames();
        IntPoint roundedPosition = roundedIntPoint(position);

        // Event regions are affected by page scale, so no need to map through scale.
        bool isSynchronousDispatchRegion = m_treeState.eventTrackingRegions.trackingTypeForPoint(names.wheelEvent, roundedPosition) == TrackingType::Synchronous
            || m_treeState.eventTrackingRegions.trackingTypeForPoint(names.mousewheelEvent, roundedPosition) == TrackingType::Synchronous;
        LOG_WITH_STREAM(Scrolling, stream << "ScrollingTree::shouldHandleWheelEventSynchronously: wheelEvent at " << wheelEvent.position() << " mapped to content point " << position << ", in non-fast region " << isSynchronousDispatchRegion);

        if (isSynchronousDispatchRegion)
            return true;
    }
    return false;
}

void ScrollingTree::setOrClearLatchedNode(const PlatformWheelEvent& wheelEvent, ScrollingNodeID nodeID)
{
    if (wheelEvent.shouldConsiderLatching()) {
        LOG_WITH_STREAM(Scrolling, stream << "ScrollingTree " << this << " setOrClearLatchedNode: setting latched node " << nodeID);
        setLatchedNode(nodeID);
    } else if (wheelEvent.shouldResetLatching()) {
        LOG_WITH_STREAM(Scrolling, stream << "ScrollingTree " << this << " setOrClearLatchedNode: clearing latched node (was " << latchedNode() << ")");
        clearLatchedNode();
    }
}

ScrollingEventResult ScrollingTree::handleWheelEvent(const PlatformWheelEvent& wheelEvent)
{
    LOG_WITH_STREAM(Scrolling, stream << "ScrollingTree " << this << " handleWheelEvent (async scrolling enabled: " << asyncFrameOrOverflowScrollingEnabled() << ")");

    LockHolder locker(m_treeMutex);

    if (!asyncFrameOrOverflowScrollingEnabled()) {
        if (m_rootNode)
            m_rootNode->handleWheelEvent(wheelEvent);
        return ScrollingEventResult::DidNotHandleEvent;
    }

    if (hasLatchedNode()) {
        LOG_WITH_STREAM(Scrolling, stream << " has latched node " << latchedNode());
        auto* node = nodeForID(latchedNode());
        if (is<ScrollingTreeScrollingNode>(node))
            return downcast<ScrollingTreeScrollingNode>(*node).handleWheelEvent(wheelEvent);
    }

    if (m_rootNode) {
        FloatPoint position = wheelEvent.position();
        ScrollingTreeNode* node = m_rootNode->scrollingNodeForPoint(LayoutPoint(position));

        LOG_WITH_STREAM(Scrolling, stream << "ScrollingTree::handleWheelEvent found node " << (node ? node->scrollingNodeID() : 0) << " for point " << position << "\n");

        while (node) {
            if (is<ScrollingTreeScrollingNode>(*node)) {
                auto& scrollingNode = downcast<ScrollingTreeScrollingNode>(*node);
                // FIXME: this needs to consult latching logic.
                if (scrollingNode.handleWheelEvent(wheelEvent) == ScrollingEventResult::DidHandleEvent)
                    return ScrollingEventResult::DidHandleEvent;
            }
            node = node->parent();
        }
    }
    return ScrollingEventResult::DidNotHandleEvent;
}

void ScrollingTree::mainFrameViewportChangedViaDelegatedScrolling(const FloatPoint& scrollPosition, const FloatRect& layoutViewport, double)
{
    LOG_WITH_STREAM(Scrolling, stream << "ScrollingTree::viewportChangedViaDelegatedScrolling - layoutViewport " << layoutViewport);
    
    if (!m_rootNode)
        return;

    m_rootNode->wasScrolledByDelegatedScrolling(scrollPosition, layoutViewport);
}

void ScrollingTree::commitTreeState(std::unique_ptr<ScrollingStateTree> scrollingStateTree)
{
    LockHolder locker(m_treeMutex);

    bool rootStateNodeChanged = scrollingStateTree->hasNewRootStateNode();
    
    LOG(Scrolling, "\nScrollingTree %p commitTreeState", this);
    
    auto* rootNode = scrollingStateTree->rootStateNode();
    if (rootNode
        && (rootStateNodeChanged
            || rootNode->hasChangedProperty(ScrollingStateFrameScrollingNode::EventTrackingRegion)
            || rootNode->hasChangedProperty(ScrollingStateScrollingNode::ScrolledContentsLayer)
            || rootNode->hasChangedProperty(ScrollingStateFrameScrollingNode::AsyncFrameOrOverflowScrollingEnabled))) {
        LockHolder lock(m_treeStateMutex);

        if (rootStateNodeChanged || rootNode->hasChangedProperty(ScrollingStateScrollingNode::ScrolledContentsLayer))
            m_treeState.mainFrameScrollPosition = { };

        if (rootStateNodeChanged || rootNode->hasChangedProperty(ScrollingStateFrameScrollingNode::EventTrackingRegion))
            m_treeState.eventTrackingRegions = scrollingStateTree->rootStateNode()->eventTrackingRegions();

        if (rootStateNodeChanged || rootNode->hasChangedProperty(ScrollingStateFrameScrollingNode::AsyncFrameOrOverflowScrollingEnabled))
            m_asyncFrameOrOverflowScrollingEnabled = scrollingStateTree->rootStateNode()->asyncFrameOrOverflowScrollingEnabled();
    }
    
    // unvisitedNodes starts with all nodes in the map; we remove nodes as we visit them. At the end, it's the unvisited nodes.
    // We can't use orphanNodes for this, because orphanNodes won't contain descendants of removed nodes.
    HashSet<ScrollingNodeID> unvisitedNodes;
    for (auto nodeID : m_nodeMap.keys())
        unvisitedNodes.add(nodeID);

    m_overflowRelatedNodesMap.clear();
    m_activeOverflowScrollProxyNodes.clear();
    m_activePositionedNodes.clear();

    // orphanNodes keeps child nodes alive while we rebuild child lists.
    OrphanScrollingNodeMap orphanNodes;
    updateTreeFromStateNode(rootNode, orphanNodes, unvisitedNodes);
    
    for (auto nodeID : unvisitedNodes) {
        if (nodeID == m_treeState.latchedNodeID)
            clearLatchedNode();
        
        LOG(Scrolling, "ScrollingTree::commitTreeState - removing unvisited node %" PRIu64, nodeID);
        m_nodeMap.remove(nodeID);
    }

    LOG_WITH_STREAM(Scrolling, stream << "committed ScrollingTree" << scrollingTreeAsText(ScrollingStateTreeAsTextBehaviorDebug));
}

void ScrollingTree::updateTreeFromStateNode(const ScrollingStateNode* stateNode, OrphanScrollingNodeMap& orphanNodes, HashSet<ScrollingNodeID>& unvisitedNodes)
{
    if (!stateNode) {
        m_nodeMap.clear();
        m_rootNode = nullptr;
        return;
    }
    
    ScrollingNodeID nodeID = stateNode->scrollingNodeID();
    ScrollingNodeID parentNodeID = stateNode->parentNodeID();

    auto it = m_nodeMap.find(nodeID);

    RefPtr<ScrollingTreeNode> node;
    if (it != m_nodeMap.end()) {
        node = it->value;
        unvisitedNodes.remove(nodeID);
    } else {
        node = createScrollingTreeNode(stateNode->nodeType(), nodeID);
        if (!parentNodeID) {
            // This is the root node. Clear the node map.
            ASSERT(stateNode->isFrameScrollingNode());
            m_rootNode = downcast<ScrollingTreeFrameScrollingNode>(node.get());
            m_nodeMap.clear();
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
    if (auto nodeChildren = node->children()) {
        for (auto& childScrollingNode : *nodeChildren) {
            childScrollingNode->setParent(nullptr);
            orphanNodes.add(childScrollingNode->scrollingNodeID(), childScrollingNode.get());
        }
        nodeChildren->clear();
    }

    // Now update the children if we have any.
    if (auto children = stateNode->children()) {
        for (auto& child : *children)
            updateTreeFromStateNode(child.get(), orphanNodes, unvisitedNodes);
    }

    node->commitStateAfterChildren(*stateNode);
}

void ScrollingTree::applyLayerPositionsAfterCommit()
{
    // Scrolling tree needs to make adjustments only if the UI side positions have changed.
    if (!m_wasScrolledByDelegatedScrollingSincePreviousCommit)
        return;
    m_wasScrolledByDelegatedScrollingSincePreviousCommit = false;

    applyLayerPositions();
}

void ScrollingTree::applyLayerPositions()
{
    ASSERT(isMainThread());
    LockHolder locker(m_treeMutex);

    if (!m_rootNode)
        return;

    LOG(Scrolling, "\nScrollingTree %p applyLayerPositions", this);

    applyLayerPositionsRecursive(*m_rootNode);

    LOG(Scrolling, "ScrollingTree %p applyLayerPositions - done\n", this);
}

void ScrollingTree::applyLayerPositionsRecursive(ScrollingTreeNode& currNode)
{
    currNode.applyLayerPositions();

    if (auto children = currNode.children()) {
        for (auto& child : *children)
            applyLayerPositionsRecursive(*child);
    }
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

    if (!node.children())
        return;

    for (auto& child : *node.children()) {
        // Never need to cross frame boundaries, since scroll layer adjustments are isolated to each document.
        if (is<ScrollingTreeFrameScrollingNode>(child))
            continue;

        notifyRelatedNodesRecursive(*child);
    }
}

void ScrollingTree::setAsyncFrameOrOverflowScrollingEnabled(bool enabled)
{
    m_asyncFrameOrOverflowScrollingEnabled = enabled;
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
bool ScrollingTree::isScrollSnapInProgress()
{
    LockHolder lock(m_treeStateMutex);
    return m_treeState.mainFrameIsScrollSnapping;
}
    
void ScrollingTree::setMainFrameIsScrollSnapping(bool isScrollSnapping)
{
    LockHolder locker(m_treeStateMutex);
    m_treeState.mainFrameIsScrollSnapping = isScrollSnapping;
}

void ScrollingTree::setMainFramePinState(bool pinnedToTheLeft, bool pinnedToTheRight, bool pinnedToTheTop, bool pinnedToTheBottom)
{
    LockHolder locker(m_swipeStateMutex);

    m_swipeState.mainFramePinnedToTheLeft = pinnedToTheLeft;
    m_swipeState.mainFramePinnedToTheRight = pinnedToTheRight;
    m_swipeState.mainFramePinnedToTheTop = pinnedToTheTop;
    m_swipeState.mainFramePinnedToTheBottom = pinnedToTheBottom;
}

void ScrollingTree::setCanRubberBandState(bool canRubberBandAtLeft, bool canRubberBandAtRight, bool canRubberBandAtTop, bool canRubberBandAtBottom)
{
    LockHolder locker(m_swipeStateMutex);

    m_swipeState.rubberBandsAtLeft = canRubberBandAtLeft;
    m_swipeState.rubberBandsAtRight = canRubberBandAtRight;
    m_swipeState.rubberBandsAtTop = canRubberBandAtTop;
    m_swipeState.rubberBandsAtBottom = canRubberBandAtBottom;
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

    if (wheelEvent.deltaX() > 0 && m_swipeState.mainFramePinnedToTheLeft && !m_swipeState.rubberBandsAtLeft)
        return true;
    if (wheelEvent.deltaX() < 0 && m_swipeState.mainFramePinnedToTheRight && !m_swipeState.rubberBandsAtRight)
        return true;
    if (wheelEvent.deltaY() > 0 && m_swipeState.mainFramePinnedToTheTop && !m_swipeState.rubberBandsAtTop)
        return true;
    if (wheelEvent.deltaY() < 0 && m_swipeState.mainFramePinnedToTheBottom && !m_swipeState.rubberBandsAtBottom)
        return true;

    return false;
}

void ScrollingTree::setScrollingPerformanceLoggingEnabled(bool flag)
{
    m_scrollingPerformanceLoggingEnabled = flag;
}

bool ScrollingTree::scrollingPerformanceLoggingEnabled()
{
    return m_scrollingPerformanceLoggingEnabled;
}

ScrollingNodeID ScrollingTree::latchedNode()
{
    LockHolder locker(m_treeStateMutex);
    return m_treeState.latchedNodeID;
}

void ScrollingTree::setLatchedNode(ScrollingNodeID node)
{
    LockHolder locker(m_treeStateMutex);
    m_treeState.latchedNodeID = node;
}

void ScrollingTree::clearLatchedNode()
{
    LockHolder locker(m_treeStateMutex);
    m_treeState.latchedNodeID = 0;
}

String ScrollingTree::scrollingTreeAsText(ScrollingStateTreeAsTextBehavior behavior)
{
    TextStream ts(TextStream::LineMode::MultipleLine);

    {
        TextStream::GroupScope scope(ts);
        ts << "scrolling tree";

        LockHolder locker(m_treeStateMutex);

        if (m_treeState.latchedNodeID)
            ts.dumpProperty("latched node", m_treeState.latchedNodeID);

        if (!m_treeState.mainFrameScrollPosition.isZero())
            ts.dumpProperty("main frame scroll position", m_treeState.mainFrameScrollPosition);
        
        if (m_rootNode) {
            TextStream::GroupScope scope(ts);
            m_rootNode->dump(ts, behavior | ScrollingStateTreeAsTextBehaviorIncludeLayerPositions);
        }
        
        if (behavior & ScrollingStateTreeAsTextBehaviorIncludeNodeIDs && !m_overflowRelatedNodesMap.isEmpty()) {
            TextStream::GroupScope scope(ts);
            ts << "overflow related nodes";
            {
                TextStream::IndentScope indentScope(ts);
                for (auto& it : m_overflowRelatedNodesMap)
                    ts << "\n" << indent << it.key << " -> " << it.value;
            }
        }
    }
    return ts.release();
}

} // namespace WebCore

#endif // ENABLE(ASYNC_SCROLLING)
