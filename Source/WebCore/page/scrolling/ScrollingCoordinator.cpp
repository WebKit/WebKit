/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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

#if ENABLE(THREADED_SCROLLING)

#include "ScrollingCoordinator.h"

#include "Frame.h"
#include "FrameView.h"
#include "IntRect.h"
#include "Page.h"
#include "PlatformWheelEvent.h"
#include "ScrollAnimator.h"
#include "ScrollingThread.h"
#include "ScrollingTree.h"
#include "ScrollingTreeState.h"
#include <wtf/Functional.h>
#include <wtf/MainThread.h>
#include <wtf/PassRefPtr.h>

namespace WebCore {

PassRefPtr<ScrollingCoordinator> ScrollingCoordinator::create(Page* page)
{
    return adoptRef(new ScrollingCoordinator(page));
}

ScrollingCoordinator::ScrollingCoordinator(Page* page)
    : m_page(page)
    , m_scrollingTree(ScrollingTree::create(this))
    , m_scrollingTreeState(ScrollingTreeState::create())
    , m_scrollingTreeStateCommitterTimer(this, &ScrollingCoordinator::scrollingTreeStateCommitterTimerFired)
    , m_didDispatchDidUpdateMainFrameScrollPosition(false)
{
}

ScrollingCoordinator::~ScrollingCoordinator()
{
    ASSERT(!m_page);
    ASSERT(!m_scrollingTree);
}

void ScrollingCoordinator::pageDestroyed()
{
    ASSERT(m_page);
    m_page = 0;

    // Invalidating the scrolling tree will break the reference cycle between the ScrollingCoordinator and ScrollingTree objects.
    ScrollingThread::dispatch(bind(&ScrollingTree::invalidate, m_scrollingTree.release()));
}

ScrollingTree* ScrollingCoordinator::scrollingTree() const
{
    ASSERT(m_scrollingTree);
    return m_scrollingTree.get();
}

bool ScrollingCoordinator::coordinatesScrollingForFrameView(FrameView* frameView) const
{
    ASSERT(isMainThread());
    ASSERT(m_page);

    // We currently only handle the main frame.
    if (frameView->frame() != m_page->mainFrame())
        return false;

    return true;
}

void ScrollingCoordinator::frameViewLayoutUpdated(FrameView* frameView)
{
    ASSERT(isMainThread());
    ASSERT(m_page);

    if (!coordinatesScrollingForFrameView(frameView))
        return;

    m_scrollingTreeState->setViewportRect(IntRect(IntPoint(), frameView->visibleContentRect().size()));
    m_scrollingTreeState->setContentsSize(frameView->contentsSize());
    scheduleTreeStateCommit();
}

void ScrollingCoordinator::syncFrameViewGeometry(FrameView* frameView)
{
    ASSERT(isMainThread());
    ASSERT(m_page);

    if (frameView->frame() != m_page->mainFrame())
        return;

    IntRect visibleContentRect = frameView->visibleContentRect();
    IntSize contentsSize = frameView->contentsSize();

    MutexLocker locker(m_mainFrameGeometryMutex);
    if (m_mainFrameVisibleContentRect == visibleContentRect && m_mainFrameContentsSize == contentsSize)
        return;

    m_mainFrameVisibleContentRect = visibleContentRect;
    m_mainFrameContentsSize = contentsSize;

    // FIXME: Inform the scrolling thread that the frame geometry has changed.
}

bool ScrollingCoordinator::handleWheelEvent(const PlatformWheelEvent& wheelEvent)
{
    // FIXME: Check for wheel event handlers.
    // FIXME: Check if we're over a subframe or overflow div.
    // FIXME: As soon as we've determined that we can handle the wheel event, we should do the
    // bulk of the work on the scrolling thread and return from this function.
    // FIXME: Handle rubberbanding.
    float deltaX = wheelEvent.deltaX();
    float deltaY = wheelEvent.deltaY();

    // Slightly prefer scrolling vertically by applying the = case to deltaY
    if (fabsf(deltaY) >= fabsf(deltaX))
        deltaX = 0;
    else
        deltaY = 0;

    IntSize scrollOffset = IntSize(-deltaX, -deltaY);
    ScrollingThread::dispatch(bind(&ScrollingCoordinator::scrollByOnScrollingThread, this, scrollOffset));
    return true;
}

#if ENABLE(GESTURE_EVENTS)
bool ScrollingCoordinator::handleGestureEvent(const PlatformGestureEvent&)
{
    // FIXME: Implement.
    return false;
}
#endif

void ScrollingCoordinator::didUpdateMainFrameScrollPosition()
{
    ASSERT(isMainThread());

    if (!m_page)
        return;

    IntPoint scrollPosition;

    {
        MutexLocker locker(m_mainFrameGeometryMutex);
        ASSERT(m_didDispatchDidUpdateMainFrameScrollPosition);

        scrollPosition = m_mainFrameScrollPosition;
        m_didDispatchDidUpdateMainFrameScrollPosition = false;
    }

    if (FrameView* frameView = m_page->mainFrame()->view()) {
        frameView->setConstrainsScrollingToContentEdge(false);
        frameView->scrollToOffsetWithoutAnimation(scrollPosition);
        frameView->setConstrainsScrollingToContentEdge(true);
    }
}

void ScrollingCoordinator::scheduleTreeStateCommit()
{
    if (m_scrollingTreeStateCommitterTimer.isActive())
        return;

    if (!m_scrollingTreeState->hasChangedProperties())
        return;

    m_scrollingTreeStateCommitterTimer.startOneShot(0);
}

void ScrollingCoordinator::scrollingTreeStateCommitterTimerFired(Timer<ScrollingCoordinator>*)
{
    commitTreeState();
}

void ScrollingCoordinator::commitTreeStateIfNeeded()
{
    if (!m_scrollingTreeState->hasChangedProperties())
        return;

    commitTreeState();
    m_scrollingTreeStateCommitterTimer.stop();
}

void ScrollingCoordinator::commitTreeState()
{
    ASSERT(m_scrollingTreeState->hasChangedProperties());

    OwnPtr<ScrollingTreeState> treeState = m_scrollingTreeState->commit();
    ScrollingThread::dispatch(bind(&ScrollingTree::commitNewTreeState, m_scrollingTree.get(), treeState.release()));
}

} // namespace WebCore

#endif // ENABLE(THREADED_SCROLLING)
