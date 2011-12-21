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
#import <QuartzCore/QuartzCore.h>
#import <wtf/Functional.h>
#import <wtf/MainThread.h>
#import <wtf/RetainPtr.h>
#import <wtf/StdLibExtras.h>
#import <wtf/Vector.h>

namespace WebCore {

class ScrollingThread {
public:
    ScrollingThread()
        : m_threadIdentifier(0)
    {
    }

    bool isCurrentThread() const;
    void dispatch(const Function<void()>&);

private:
    void createThreadIfNeeded();

    static void* threadCallback(void* scrollingThread);
    void threadBody();

    static void threadRunLoopSourceCallback(void* scrollingThread);
    void threadRunLoopSourceCallback();

    ThreadIdentifier m_threadIdentifier;

    ThreadCondition m_initializeRunLoopCondition;
    Mutex m_initializeRunLoopConditionMutex;
    
    RetainPtr<CFRunLoopRef> m_threadRunLoop;
    RetainPtr<CFRunLoopSourceRef> m_threadRunLoopSource;

    Mutex m_functionsMutex;
    Vector<Function<void()> > m_functions;
};

bool ScrollingThread::isCurrentThread() const
{
    if (!m_threadIdentifier)
        return false;

    return currentThread() == m_threadIdentifier;
}

void ScrollingThread::createThreadIfNeeded()
{
    if (m_threadIdentifier)
        return;

    m_threadIdentifier = createThread(threadCallback, this, "WebCore: Scrolling");

    // Wait for the thread to initialize the run loop.
    {
        MutexLocker locker(m_initializeRunLoopConditionMutex);

        while (!m_threadRunLoop)
            m_initializeRunLoopCondition.wait(m_initializeRunLoopConditionMutex);
    }
}

void* ScrollingThread::threadCallback(void* scrollingThread)
{
    static_cast<ScrollingThread*>(scrollingThread)->threadBody();

    return 0;
}

void ScrollingThread::threadBody()
{
    ASSERT(isCurrentThread());

    // Initialize the run loop.
    {
        MutexLocker locker(m_initializeRunLoopConditionMutex);

        m_threadRunLoop = CFRunLoopGetCurrent();

        CFRunLoopSourceContext context = { 0, this, 0, 0, 0, 0, 0, 0, 0, threadRunLoopSourceCallback };
        m_threadRunLoopSource = adoptCF(CFRunLoopSourceCreate(0, 0, &context));
        CFRunLoopAddSource(CFRunLoopGetCurrent(), m_threadRunLoopSource.get(), kCFRunLoopDefaultMode);

        m_initializeRunLoopCondition.broadcast();
    }

    CFRunLoopRun();
}

void ScrollingThread::threadRunLoopSourceCallback(void* scrollingThread)
{
    static_cast<ScrollingThread*>(scrollingThread)->threadRunLoopSourceCallback();
}

void ScrollingThread::threadRunLoopSourceCallback()
{
    ASSERT(isCurrentThread());

    Vector<Function<void ()> > functions;

    {
        MutexLocker locker(m_functionsMutex);
        m_functions.swap(functions);
    }

    for (size_t i = 0; i < functions.size(); ++i)
        functions[i]();
}

void ScrollingThread::dispatch(const Function<void()>& function)
{
    createThreadIfNeeded();

    {
        MutexLocker locker(m_functionsMutex);
        m_functions.append(function);
    }

    CFRunLoopSourceSignal(m_threadRunLoopSource.get());
    CFRunLoopWakeUp(m_threadRunLoop.get());
}

static ScrollingThread& scrollingThread()
{
    DEFINE_STATIC_LOCAL(ScrollingThread, scrollingThread, ());
    return scrollingThread;
}

void ScrollingCoordinator::setFrameViewScrollLayer(FrameView* frameView, const GraphicsLayer* scrollLayer)
{
    ASSERT(isMainThread());
    ASSERT(m_page);

    if (frameView->frame() != m_page->mainFrame())
        return;

    MutexLocker locker(m_mainFrameGeometryMutex);
    m_mainFrameScrollLayer = scrollLayer->platformLayer();

    // FIXME: Inform the scrolling thread?
}

bool ScrollingCoordinator::isScrollingThread()
{
    return scrollingThread().isCurrentThread();
}

void ScrollingCoordinator::dispatchOnScrollingThread(const Function<void()>& function)
{
    return scrollingThread().dispatch(function);
}

void ScrollingCoordinator::scrollByOnScrollingThread(const IntSize& offset)
{
    ASSERT(isScrollingThread());

    MutexLocker locker(m_mainFrameGeometryMutex);

    // FIXME: Should we cache the scroll position as well or always get it from the layer?
    IntPoint scrollPosition = IntPoint([m_mainFrameScrollLayer.get() position]);
    scrollPosition = -scrollPosition;

    scrollPosition += offset;
    scrollPosition.clampNegativeToZero();

    IntPoint maximumScrollPosition = IntPoint(m_mainFrameContentsSize.width() - m_mainFrameVisibleContentRect.width(), m_mainFrameContentsSize.height() - m_mainFrameVisibleContentRect.height());
    scrollPosition = scrollPosition.shrunkTo(maximumScrollPosition);

    updateMainFrameScrollLayerPositionOnScrollingThread(-scrollPosition);

    m_mainFrameScrollPosition = scrollPosition;
    if (!m_didDispatchDidUpdateMainFrameScrollPosition) {
        callOnMainThread(bind(&ScrollingCoordinator::didUpdateMainFrameScrollPosition, this));
        m_didDispatchDidUpdateMainFrameScrollPosition = true;
    }
}

void ScrollingCoordinator::updateMainFrameScrollLayerPositionOnScrollingThread(const FloatPoint& scrollLayerPosition)
{
    ASSERT(isScrollingThread());
    ASSERT(!m_mainFrameGeometryMutex.tryLock());

    [CATransaction begin];
    [CATransaction setDisableActions:YES];
    [m_mainFrameScrollLayer.get() setPosition:scrollLayerPosition];
    [CATransaction commit];
}

} // namespace WebCore

#endif // ENABLE(THREADED_SCROLLING)
