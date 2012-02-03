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

#import "ScrollingCoordinator.h"

#import "FrameView.h"
#import "Page.h"
#import "ScrollingThread.h"
#import "ScrollingTreeState.h"
#import <QuartzCore/QuartzCore.h>
#import <wtf/Functional.h>
#import <wtf/MainThread.h>
#import <wtf/RetainPtr.h>
#import <wtf/StdLibExtras.h>
#import <wtf/Vector.h>

namespace WebCore {

void ScrollingCoordinator::frameViewHorizontalScrollbarLayerDidChange(FrameView* frameView, GraphicsLayer*)
{
    ASSERT(isMainThread());
    ASSERT(m_page);

    if (frameView->frame() != m_page->mainFrame())
        return;

    // FIXME: Implement.
}

void ScrollingCoordinator::frameViewVerticalScrollbarLayerDidChange(FrameView* frameView, GraphicsLayer*)
{
    ASSERT(isMainThread());
    ASSERT(m_page);
    
    if (frameView->frame() != m_page->mainFrame())
        return;

    // FIXME: Implement.
}

void ScrollingCoordinator::frameViewScrollLayerDidChange(FrameView* frameView, const GraphicsLayer* scrollLayer)
{
    ASSERT(isMainThread());
    ASSERT(m_page);

    if (frameView->frame() != m_page->mainFrame())
        return;

    m_scrollingTreeState->setScrollLayer(scrollLayer);
    scheduleTreeStateCommit();

    MutexLocker locker(m_mainFrameGeometryMutex);
    m_mainFrameScrollLayer = scrollLayer->platformLayer();

    // FIXME: Inform the scrolling thread?
}

#define ENABLE_FREE_SCROLLING 0

void ScrollingCoordinator::scrollByOnScrollingThread(const IntSize& offset)
{
    ASSERT(ScrollingThread::isCurrentThread());

    MutexLocker locker(m_mainFrameGeometryMutex);

    // FIXME: Should we cache the scroll position as well or always get it from the layer?
    IntPoint scrollPosition = IntPoint([m_mainFrameScrollLayer.get() position]);
    scrollPosition = -scrollPosition;

    scrollPosition += offset;

#if !ENABLE_FREE_SCROLLING
    scrollPosition.clampNegativeToZero();

    IntPoint maximumScrollPosition = IntPoint(m_mainFrameContentsSize.width() - m_mainFrameVisibleContentRect.width(), m_mainFrameContentsSize.height() - m_mainFrameVisibleContentRect.height());
    scrollPosition = scrollPosition.shrunkTo(maximumScrollPosition);
#endif

    updateMainFrameScrollLayerPositionOnScrollingThread(-scrollPosition);

    m_mainFrameScrollPosition = scrollPosition;
    if (!m_didDispatchDidUpdateMainFrameScrollPosition) {
        callOnMainThread(bind(&ScrollingCoordinator::didUpdateMainFrameScrollPosition, this));
        m_didDispatchDidUpdateMainFrameScrollPosition = true;
    }
}

void ScrollingCoordinator::updateMainFrameScrollLayerPositionOnScrollingThread(const FloatPoint& scrollLayerPosition)
{
    ASSERT(ScrollingThread::isCurrentThread());
    ASSERT(!m_mainFrameGeometryMutex.tryLock());

    [CATransaction begin];
    [CATransaction setDisableActions:YES];
    [m_mainFrameScrollLayer.get() setPosition:scrollLayerPosition];
    [CATransaction commit];
}

} // namespace WebCore

#endif // ENABLE(THREADED_SCROLLING)
