/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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

#if ENABLE(THREADED_SCROLLING)

#include "PlatformWheelEvent.h"
#include "ScrollingCoordinator.h"
#include "ScrollingStateTree.h"
#include "ScrollingThread.h"
#include "ScrollingTreeFixedNode.h"
#include "ScrollingTreeNode.h"
#include "ScrollingTreeScrollingNode.h"
#include "ScrollingTreeStickyNode.h"
#include <wtf/MainThread.h>
#include <wtf/TemporaryChange.h>

namespace WebCore {

PassRefPtr<ScrollingTree> ScrollingTree::create(ScrollingCoordinator* scrollingCoordinator)
{
    return adoptRef(new ScrollingTree(scrollingCoordinator));
}

ScrollingTree::ScrollingTree(ScrollingCoordinator* scrollingCoordinator)
    : m_scrollingCoordinator(scrollingCoordinator)
    , m_rootNode(ScrollingTreeScrollingNode::create(this))
    , m_hasWheelEventHandlers(false)
    , m_canGoBack(false)
    , m_canGoForward(false)
    , m_mainFramePinnedToTheLeft(false)
    , m_mainFramePinnedToTheRight(false)
    , m_scrollingPerformanceLoggingEnabled(false)
    , m_isHandlingProgrammaticScroll(false)
{
}

ScrollingTree::~ScrollingTree()
{
    ASSERT(!m_scrollingCoordinator);
}

ScrollingTree::EventResult ScrollingTree::tryToHandleWheelEvent(const PlatformWheelEvent& wheelEvent)
{
    {
        MutexLocker lock(m_mutex);

        if (m_hasWheelEventHandlers)
            return SendToMainThread;

        if (!m_nonFastScrollableRegion.isEmpty()) {
            // FIXME: This is not correct for non-default scroll origins.
            IntPoint position = wheelEvent.position();
            position.moveBy(m_mainFrameScrollPosition);
            if (m_nonFastScrollableRegion.contains(position))
                return SendToMainThread;
        }
    }

    if (willWheelEventStartSwipeGesture(wheelEvent))
        return DidNotHandleEvent;

    ScrollingThread::dispatch(bind(&ScrollingTree::handleWheelEvent, this, wheelEvent));
    return DidHandleEvent;
}

void ScrollingTree::updateBackForwardState(bool canGoBack, bool canGoForward)
{
    MutexLocker locker(m_swipeStateMutex);

    m_canGoBack = canGoBack;
    m_canGoForward = canGoForward;
}

void ScrollingTree::handleWheelEvent(const PlatformWheelEvent& wheelEvent)
{
    ASSERT(ScrollingThread::isCurrentThread());

    m_rootNode->handleWheelEvent(wheelEvent);
}

static void derefScrollingCoordinator(ScrollingCoordinator* scrollingCoordinator)
{
    ASSERT(isMainThread());

    scrollingCoordinator->deref();
}

void ScrollingTree::invalidate()
{
    // Invalidate is dispatched by the ScrollingCoordinator class on the ScrollingThread
    // to break the reference cycle between ScrollingTree and ScrollingCoordinator when the
    // ScrollingCoordinator's page is destroyed.
    ASSERT(ScrollingThread::isCurrentThread());

    // Since this can potentially be the last reference to the scrolling coordinator,
    // we need to release it on the main thread since it has member variables (such as timers)
    // that expect to be destroyed from the main thread.
    callOnMainThread(bind(derefScrollingCoordinator, m_scrollingCoordinator.release().leakRef()));
}

void ScrollingTree::commitNewTreeState(PassOwnPtr<ScrollingStateTree> scrollingStateTree)
{
    ASSERT(ScrollingThread::isCurrentThread());

    if (scrollingStateTree->rootStateNode()->changedProperties() & (ScrollingStateScrollingNode::WheelEventHandlerCount | ScrollingStateScrollingNode::NonFastScrollableRegion) || scrollingStateTree->rootStateNode()->scrollLayerDidChange()) {
        MutexLocker lock(m_mutex);

        if (scrollingStateTree->rootStateNode()->scrollLayerDidChange())
            m_mainFrameScrollPosition = IntPoint();
        if (scrollingStateTree->rootStateNode()->changedProperties() & ScrollingStateScrollingNode::WheelEventHandlerCount)
            m_hasWheelEventHandlers = scrollingStateTree->rootStateNode()->wheelEventHandlerCount();
        if (scrollingStateTree->rootStateNode()->changedProperties() & ScrollingStateScrollingNode::NonFastScrollableRegion)
            m_nonFastScrollableRegion = scrollingStateTree->rootStateNode()->nonFastScrollableRegion();
    }
    
    TemporaryChange<bool> changeHandlingProgrammaticScroll(m_isHandlingProgrammaticScroll, scrollingStateTree->rootStateNode()->requestedScrollPositionRepresentsProgrammaticScroll());

    removeDestroyedNodes(scrollingStateTree.get());
    updateTreeFromStateNode(scrollingStateTree->rootStateNode());
}

void ScrollingTree::updateTreeFromStateNode(ScrollingStateNode* stateNode)
{
    // This fuction recurses through the ScrollingStateTree and updates the corresponding ScrollingTreeNodes.
    // Find the ScrollingTreeNode associated with the current stateNode using the shared ID and our HashMap.
    ScrollingTreeNodeMap::const_iterator it = m_nodeMap.find(stateNode->scrollingNodeID());

    if (it != m_nodeMap.end()) {
        ScrollingTreeNode* node = it->value;
        node->update(stateNode);
    } else {
        // If the node isn't found, it's either new and needs to be added to the tree, or there is a new ID for our
        // root node.
        if (!stateNode->parent()) {
            // This is the root node.
            m_rootNode->setScrollingNodeID(stateNode->scrollingNodeID());
            m_nodeMap.set(stateNode->scrollingNodeID(), m_rootNode.get());
            m_rootNode->update(stateNode);
        } else {
            OwnPtr<ScrollingTreeNode> newNode;
            if (stateNode->isScrollingNode())
                newNode = ScrollingTreeScrollingNode::create(this);
            else if (stateNode->isFixedNode())
                newNode = ScrollingTreeFixedNode::create(this);
            else if (stateNode->isStickyNode())
                newNode = ScrollingTreeStickyNode::create(this);
            else
                ASSERT_NOT_REACHED();

            ScrollingTreeNode* newNodeRawPtr = newNode.get();
            m_nodeMap.set(stateNode->scrollingNodeID(), newNodeRawPtr);
            ScrollingTreeNodeMap::const_iterator it = m_nodeMap.find(stateNode->parent()->scrollingNodeID());
            ASSERT(it != m_nodeMap.end());
            if (it != m_nodeMap.end()) {
                ScrollingTreeNode* parent = it->value;
                newNode->setParent(parent);
                parent->appendChild(newNode.release());
            }
            newNodeRawPtr->update(stateNode);
        }
    }

    // Now update the children if we have any.
    Vector<OwnPtr<ScrollingStateNode> >* stateNodeChildren = stateNode->children();
    if (!stateNodeChildren)
        return;

    size_t size = stateNodeChildren->size();
    for (size_t i = 0; i < size; ++i)
        updateTreeFromStateNode(stateNodeChildren->at(i).get());
}

void ScrollingTree::removeDestroyedNodes(ScrollingStateTree* stateTree)
{
    const Vector<ScrollingNodeID>& removedNodes = stateTree->removedNodes();
    size_t size = removedNodes.size();
    for (size_t i = 0; i < size; ++i) {
        ScrollingTreeNode* node = m_nodeMap.take(removedNodes[i]);
        // Never destroy the root node. There will be a new root node in the state tree, and we will
        // associate it with our existing root node in updateTreeFromStateNode().
        if (node && node->parent())
            m_rootNode->removeChild(node);
    }
}

void ScrollingTree::setMainFramePinState(bool pinnedToTheLeft, bool pinnedToTheRight)
{
    MutexLocker locker(m_swipeStateMutex);

    m_mainFramePinnedToTheLeft = pinnedToTheLeft;
    m_mainFramePinnedToTheRight = pinnedToTheRight;
}

void ScrollingTree::updateMainFrameScrollPosition(const IntPoint& scrollPosition, SetOrSyncScrollingLayerPosition scrollingLayerPositionAction)
{
    if (!m_scrollingCoordinator)
        return;

    {
        MutexLocker lock(m_mutex);
        m_mainFrameScrollPosition = scrollPosition;
    }

    callOnMainThread(bind(&ScrollingCoordinator::scheduleUpdateMainFrameScrollPosition, m_scrollingCoordinator.get(), scrollPosition, m_isHandlingProgrammaticScroll, scrollingLayerPositionAction));
}

IntPoint ScrollingTree::mainFrameScrollPosition()
{
    MutexLocker lock(m_mutex);
    return m_mainFrameScrollPosition;
}

#if PLATFORM(MAC) || (PLATFORM(CHROMIUM) && OS(DARWIN))
void ScrollingTree::handleWheelEventPhase(PlatformWheelEventPhase phase)
{
    if (!m_scrollingCoordinator)
        return;

    callOnMainThread(bind(&ScrollingCoordinator::handleWheelEventPhase, m_scrollingCoordinator.get(), phase));
}
#endif

bool ScrollingTree::canGoBack()
{
    MutexLocker lock(m_swipeStateMutex);

    return m_canGoBack;
}

bool ScrollingTree::canGoForward()
{
    MutexLocker lock(m_swipeStateMutex);

    return m_canGoForward;
}

bool ScrollingTree::willWheelEventStartSwipeGesture(const PlatformWheelEvent& wheelEvent)
{
    if (wheelEvent.phase() != PlatformWheelEventPhaseBegan)
        return false;
    if (!wheelEvent.deltaX())
        return false;

    MutexLocker lock(m_swipeStateMutex);

    if (wheelEvent.deltaX() > 0 && m_mainFramePinnedToTheLeft && m_canGoBack)
        return true;
    if (wheelEvent.deltaX() < 0 && m_mainFramePinnedToTheRight && m_canGoForward)
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

} // namespace WebCore

#endif // ENABLE(THREADED_SCROLLING)
