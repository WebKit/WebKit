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
#include "ScrollingThread.h"
#include "ScrollingTreeNode.h"
#include "ScrollingTreeState.h"
#include <wtf/MainThread.h>

namespace WebCore {

PassRefPtr<ScrollingTree> ScrollingTree::create(ScrollingCoordinator* scrollingCoordinator)
{
    return adoptRef(new ScrollingTree(scrollingCoordinator));
}

ScrollingTree::ScrollingTree(ScrollingCoordinator* scrollingCoordinator)
    : m_scrollingCoordinator(scrollingCoordinator)
    , m_rootNode(ScrollingTreeNode::create(this))
    , m_hasWheelEventHandlers(false)
    , m_canGoBack(false)
    , m_canGoForward(false)
    , m_mainFramePinnedToTheLeft(false)
    , m_mainFramePinnedToTheRight(false)
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

void ScrollingTree::commitNewTreeState(PassOwnPtr<ScrollingTreeState> scrollingTreeState)
{
    ASSERT(ScrollingThread::isCurrentThread());

    if (scrollingTreeState->changedProperties() & (ScrollingTreeState::WheelEventHandlerCount | ScrollingTreeState::NonFastScrollableRegion | ScrollingTreeState::ScrollLayer)) {
        MutexLocker lock(m_mutex);

        if (scrollingTreeState->changedProperties() & ScrollingTreeState::ScrollLayer)
            m_mainFrameScrollPosition = IntPoint();
        if (scrollingTreeState->changedProperties() & ScrollingTreeState::WheelEventHandlerCount)
            m_hasWheelEventHandlers = scrollingTreeState->wheelEventHandlerCount();
        if (scrollingTreeState->changedProperties() & ScrollingTreeState::NonFastScrollableRegion)
            m_nonFastScrollableRegion = scrollingTreeState->nonFastScrollableRegion();
    }

    m_rootNode->update(scrollingTreeState.get());

    updateDebugRootLayer();
}

void ScrollingTree::setMainFramePinState(bool pinnedToTheLeft, bool pinnedToTheRight)
{
    MutexLocker locker(m_swipeStateMutex);

    m_mainFramePinnedToTheLeft = pinnedToTheLeft;
    m_mainFramePinnedToTheRight = pinnedToTheRight;
}

void ScrollingTree::updateMainFrameScrollPosition(const IntPoint& scrollPosition)
{
    if (!m_scrollingCoordinator)
        return;

    {
        MutexLocker lock(m_mutex);
        m_mainFrameScrollPosition = scrollPosition;
    }

    callOnMainThread(bind(&ScrollingCoordinator::updateMainFrameScrollPosition, m_scrollingCoordinator.get(), scrollPosition));
}

IntPoint ScrollingTree::mainFrameScrollPosition()
{
    MutexLocker lock(m_mutex);
    return m_mainFrameScrollPosition;
}

void ScrollingTree::updateMainFrameScrollPositionAndScrollLayerPosition(const IntPoint& scrollPosition)
{
    if (!m_scrollingCoordinator)
        return;

    {
        MutexLocker lock(m_mutex);
        m_mainFrameScrollPosition = scrollPosition;
    }

    callOnMainThread(bind(&ScrollingCoordinator::updateMainFrameScrollPositionAndScrollLayerPosition, m_scrollingCoordinator.get()));
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

} // namespace WebCore

#endif // ENABLE(THREADED_SCROLLING)
