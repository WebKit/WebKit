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
    , m_didDispatchDidUpdateMainFrameScrollPosition(false)
{
}

ScrollingCoordinator::~ScrollingCoordinator()
{
    ASSERT(!m_page);
}

void ScrollingCoordinator::pageDestroyed()
{
    ASSERT(m_page);
    m_page = 0;
}

void ScrollingCoordinator::syncFrameGeometry(Frame* frame)
{
    ASSERT(isMainThread());
    ASSERT(m_page);

    if (frame != m_page->mainFrame())
        return;

    IntRect visibleContentRect = frame->view()->visibleContentRect();
    IntSize contentsSize = frame->view()->contentsSize();

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
    dispatchOnScrollingThread(bind(&ScrollingCoordinator::scrollByOnScrollingThread, this, scrollOffset));
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

    if (FrameView* frameView = m_page->mainFrame()->view())
        frameView->setScrollOffset(scrollPosition);
}

} // namespace WebCore

#endif // ENABLE(THREADED_SCROLLING)
