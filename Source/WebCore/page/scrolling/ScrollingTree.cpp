/*
 * Copyright (C) 2012, 2013, 2014 Apple Inc. All rights reserved.
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

#include "PlatformWheelEvent.h"
#include "ScrollingStateTree.h"
#include "ScrollingTreeNode.h"
#include "ScrollingTreeScrollingNode.h"
#include <wtf/TemporaryChange.h>

namespace WebCore {

ScrollingTree::ScrollingTree()
    : m_hasWheelEventHandlers(false)
    , m_rubberBandsAtLeft(true)
    , m_rubberBandsAtRight(true)
    , m_rubberBandsAtTop(true)
    , m_rubberBandsAtBottom(true)
    , m_mainFramePinnedToTheLeft(false)
    , m_mainFramePinnedToTheRight(false)
    , m_mainFramePinnedToTheTop(false)
    , m_mainFramePinnedToTheBottom(false)
    , m_mainFrameIsRubberBanding(false)
    , m_scrollPinningBehavior(DoNotPin)
    , m_latchedNode(0)
    , m_scrollingPerformanceLoggingEnabled(false)
    , m_isHandlingProgrammaticScroll(false)
{
}

ScrollingTree::~ScrollingTree()
{
}

bool ScrollingTree::shouldHandleWheelEventSynchronously(const PlatformWheelEvent& wheelEvent)
{
    // This method is invoked by the event handling thread
    MutexLocker lock(m_mutex);

    if (m_hasWheelEventHandlers)
        return true;

    bool shouldSetLatch = wheelEvent.shouldConsiderLatching();
    
    if (hasLatchedNode() && !shouldSetLatch)
        return false;

    if (shouldSetLatch)
        m_latchedNode = 0;
    
    if (!m_nonFastScrollableRegion.isEmpty()) {
        // FIXME: This is not correct for non-default scroll origins.
        FloatPoint position = wheelEvent.position();
        position.moveBy(m_mainFrameScrollPosition);
        if (m_nonFastScrollableRegion.contains(roundedIntPoint(position)))
            return true;
    }
    return false;
}

void ScrollingTree::setOrClearLatchedNode(const PlatformWheelEvent& wheelEvent, ScrollingNodeID nodeID)
{
    if (wheelEvent.shouldConsiderLatching())
        setLatchedNode(nodeID);
    else if (wheelEvent.shouldResetLatching())
        clearLatchedNode();
}

void ScrollingTree::handleWheelEvent(const PlatformWheelEvent& wheelEvent)
{
    if (m_rootNode)
        m_rootNode->handleWheelEvent(wheelEvent);
}

void ScrollingTree::viewportChangedViaDelegatedScrolling(ScrollingNodeID nodeID, const WebCore::FloatRect& viewportRect, double scale)
{
    ScrollingTreeNode* node = nodeForID(nodeID);
    if (!node)
        return;

    if (node->nodeType() != ScrollingNode)
        return;

    toScrollingTreeScrollingNode(node)->updateForViewport(viewportRect, scale);
}

void ScrollingTree::commitNewTreeState(PassOwnPtr<ScrollingStateTree> scrollingStateTree)
{
    bool rootStateNodeChanged = scrollingStateTree->hasNewRootStateNode();
    
    ScrollingStateScrollingNode* rootNode = scrollingStateTree->rootStateNode();
    if (rootNode
        && (rootStateNodeChanged
            || rootNode->hasChangedProperty(ScrollingStateScrollingNode::WheelEventHandlerCount)
            || rootNode->hasChangedProperty(ScrollingStateScrollingNode::NonFastScrollableRegion)
            || rootNode->hasChangedProperty(ScrollingStateNode::ScrollLayer))) {
        MutexLocker lock(m_mutex);

        if (rootStateNodeChanged || rootNode->hasChangedProperty(ScrollingStateNode::ScrollLayer))
            m_mainFrameScrollPosition = FloatPoint();
        if (rootStateNodeChanged || rootNode->hasChangedProperty(ScrollingStateScrollingNode::WheelEventHandlerCount))
            m_hasWheelEventHandlers = scrollingStateTree->rootStateNode()->wheelEventHandlerCount();
        if (rootStateNodeChanged || rootNode->hasChangedProperty(ScrollingStateScrollingNode::NonFastScrollableRegion))
            m_nonFastScrollableRegion = scrollingStateTree->rootStateNode()->nonFastScrollableRegion();
    }
    
    bool scrollRequestIsProgammatic = rootNode ? rootNode->requestedScrollPositionRepresentsProgrammaticScroll() : false;
    TemporaryChange<bool> changeHandlingProgrammaticScroll(m_isHandlingProgrammaticScroll, scrollRequestIsProgammatic);

    removeDestroyedNodes(*scrollingStateTree);
    updateTreeFromStateNode(rootNode);
}

void ScrollingTree::updateTreeFromStateNode(const ScrollingStateNode* stateNode)
{
    if (!stateNode) {
        m_nodeMap.clear();
        m_rootNode = nullptr;
        return;
    }
    
    // This fuction recurses through the ScrollingStateTree and updates the corresponding ScrollingTreeNodes.
    // Find the ScrollingTreeNode associated with the current stateNode using the shared ID and our HashMap.
    ScrollingTreeNodeMap::const_iterator it = m_nodeMap.find(stateNode->scrollingNodeID());

    ScrollingTreeNode* node;
    if (it != m_nodeMap.end()) {
        node = it->value;
        node->updateBeforeChildren(*stateNode);
    } else {
        // If the node isn't found, it's either new and needs to be added to the tree, or there is a new ID for our
        // root node.
        ScrollingNodeID nodeID = stateNode->scrollingNodeID();
        if (!stateNode->parent()) {
            // This is the root node. Nuke the node map.
            m_nodeMap.clear();

            m_rootNode = static_pointer_cast<ScrollingTreeScrollingNode>(createNode(ScrollingNode, nodeID));
            m_nodeMap.set(nodeID, m_rootNode.get());
            m_rootNode->updateBeforeChildren(*stateNode);
            node = m_rootNode.get();
        } else {
            OwnPtr<ScrollingTreeNode> newNode = createNode(stateNode->nodeType(), nodeID);
            node = newNode.get();
            m_nodeMap.set(nodeID, node);
            ScrollingTreeNodeMap::const_iterator it = m_nodeMap.find(stateNode->parent()->scrollingNodeID());
            ASSERT(it != m_nodeMap.end());
            if (it != m_nodeMap.end()) {
                ScrollingTreeNode* parent = it->value;
                newNode->setParent(parent);
                parent->appendChild(newNode.release());
            }
            node->updateBeforeChildren(*stateNode);
        }
    }

    // Now update the children if we have any.
    Vector<OwnPtr<ScrollingStateNode>>* stateNodeChildren = stateNode->children();
    if (stateNodeChildren) {
        size_t size = stateNodeChildren->size();
        for (size_t i = 0; i < size; ++i)
            updateTreeFromStateNode(stateNodeChildren->at(i).get());
    }
    node->updateAfterChildren(*stateNode);
}

void ScrollingTree::removeDestroyedNodes(const ScrollingStateTree& stateTree)
{
    for (const auto& removedNode : stateTree.removedNodes()) {
        ScrollingTreeNode* node = m_nodeMap.take(removedNode);
        if (!node)
            continue;

        if (node->scrollingNodeID() == m_latchedNode)
            clearLatchedNode();

        // Never destroy the root node. There will be a new root node in the state tree, and we will
        // associate it with our existing root node in updateTreeFromStateNode().
        if (node->parent())
            m_rootNode->removeChild(node);
    }
}

ScrollingTreeNode* ScrollingTree::nodeForID(ScrollingNodeID nodeID) const
{
    if (!nodeID)
        return nullptr;

    return m_nodeMap.get(nodeID);
}

void ScrollingTree::setMainFramePinState(bool pinnedToTheLeft, bool pinnedToTheRight, bool pinnedToTheTop, bool pinnedToTheBottom)
{
    MutexLocker locker(m_swipeStateMutex);

    m_mainFramePinnedToTheLeft = pinnedToTheLeft;
    m_mainFramePinnedToTheRight = pinnedToTheRight;
    m_mainFramePinnedToTheTop = pinnedToTheTop;
    m_mainFramePinnedToTheBottom = pinnedToTheBottom;
}

FloatPoint ScrollingTree::mainFrameScrollPosition()
{
    MutexLocker lock(m_mutex);
    return m_mainFrameScrollPosition;
}

void ScrollingTree::setMainFrameScrollPosition(FloatPoint position)
{
    MutexLocker lock(m_mutex);
    m_mainFrameScrollPosition = position;
}

bool ScrollingTree::isPointInNonFastScrollableRegion(IntPoint p)
{
    MutexLocker lock(m_mutex);
    
    return m_nonFastScrollableRegion.contains(p);
}

bool ScrollingTree::isRubberBandInProgress()
{
    MutexLocker lock(m_mutex);    

    return m_mainFrameIsRubberBanding;
}

void ScrollingTree::setMainFrameIsRubberBanding(bool isRubberBanding)
{
    MutexLocker locker(m_mutex);

    m_mainFrameIsRubberBanding = isRubberBanding;
}

void ScrollingTree::setCanRubberBandState(bool canRubberBandAtLeft, bool canRubberBandAtRight, bool canRubberBandAtTop, bool canRubberBandAtBottom)
{
    MutexLocker locker(m_swipeStateMutex);

    m_rubberBandsAtLeft = canRubberBandAtLeft;
    m_rubberBandsAtRight = canRubberBandAtRight;
    m_rubberBandsAtTop = canRubberBandAtTop;
    m_rubberBandsAtBottom = canRubberBandAtBottom;
}

bool ScrollingTree::rubberBandsAtLeft()
{
    MutexLocker lock(m_swipeStateMutex);

    return m_rubberBandsAtLeft;
}

bool ScrollingTree::rubberBandsAtRight()
{
    MutexLocker lock(m_swipeStateMutex);

    return m_rubberBandsAtRight;
}

bool ScrollingTree::rubberBandsAtBottom()
{
    MutexLocker lock(m_swipeStateMutex);

    return m_rubberBandsAtBottom;
}

bool ScrollingTree::rubberBandsAtTop()
{
    MutexLocker lock(m_swipeStateMutex);

    return m_rubberBandsAtTop;
}

bool ScrollingTree::isHandlingProgrammaticScroll()
{
    return m_isHandlingProgrammaticScroll;
}

void ScrollingTree::setScrollPinningBehavior(ScrollPinningBehavior pinning)
{
    MutexLocker locker(m_swipeStateMutex);
    
    m_scrollPinningBehavior = pinning;
}

ScrollPinningBehavior ScrollingTree::scrollPinningBehavior()
{
    MutexLocker lock(m_swipeStateMutex);
    
    return m_scrollPinningBehavior;
}

bool ScrollingTree::willWheelEventStartSwipeGesture(const PlatformWheelEvent& wheelEvent)
{
    if (wheelEvent.phase() != PlatformWheelEventPhaseBegan)
        return false;

    MutexLocker lock(m_swipeStateMutex);

    if (wheelEvent.deltaX() > 0 && m_mainFramePinnedToTheLeft && !m_rubberBandsAtLeft)
        return true;
    if (wheelEvent.deltaX() < 0 && m_mainFramePinnedToTheRight && !m_rubberBandsAtRight)
        return true;
    if (wheelEvent.deltaY() > 0 && m_mainFramePinnedToTheTop && !m_rubberBandsAtTop)
        return true;
    if (wheelEvent.deltaY() < 0 && m_mainFramePinnedToTheBottom && !m_rubberBandsAtBottom)
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
    MutexLocker locker(m_mutex);
    return m_latchedNode;
}

void ScrollingTree::setLatchedNode(ScrollingNodeID node)
{
    MutexLocker locker(m_mutex);
    m_latchedNode = node;
}

void ScrollingTree::clearLatchedNode()
{
    MutexLocker locker(m_mutex);
    m_latchedNode = 0;
}

} // namespace WebCore

#endif // ENABLE(ASYNC_SCROLLING)
