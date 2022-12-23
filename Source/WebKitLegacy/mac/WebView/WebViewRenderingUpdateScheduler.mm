/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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


#import "WebViewRenderingUpdateScheduler.h"

#import "WebViewInternal.h"
#import <pal/spi/cocoa/QuartzCoreSPI.h>

#if PLATFORM(IOS_FAMILY)
#import <WebCore/RuntimeApplicationChecks.h>
#import <WebCore/WebCoreThread.h>
#import <WebCore/WebCoreThreadInternal.h>
#endif

WebViewRenderingUpdateScheduler::WebViewRenderingUpdateScheduler(WebView* webView)
    : m_webView(webView)
{
    ASSERT_ARG(webView, webView);

    m_renderingUpdateRunLoopObserver = makeUnique<WebCore::RunLoopObserver>(static_cast<CFIndex>(WebCore::RunLoopObserver::WellKnownRunLoopOrders::RenderingUpdate), [this]() {
        this->renderingUpdateRunLoopObserverCallback();
    });

    m_postRenderingUpdateRunLoopObserver = makeUnique<WebCore::RunLoopObserver>(static_cast<CFIndex>(WebCore::RunLoopObserver::WellKnownRunLoopOrders::PostRenderingUpdate), [this]() {
        this->postRenderingUpdateCallback();
    });
}

WebViewRenderingUpdateScheduler::~WebViewRenderingUpdateScheduler() = default;

void WebViewRenderingUpdateScheduler::scheduleRenderingUpdate()
{
    if (m_insideCallback)
        m_rescheduledInsideCallback = true;

    m_renderingUpdateRunLoopObserver->schedule();
}

void WebViewRenderingUpdateScheduler::invalidate()
{
    m_webView = nullptr;
    m_renderingUpdateRunLoopObserver->invalidate();
    m_postRenderingUpdateRunLoopObserver->invalidate();
}

void WebViewRenderingUpdateScheduler::didCompleteRenderingUpdateDisplay()
{
    m_haveRegisteredCommitHandlers = false;
    schedulePostRenderingUpdate();
}

void WebViewRenderingUpdateScheduler::schedulePostRenderingUpdate()
{
    m_postRenderingUpdateRunLoopObserver->schedule();
}

void WebViewRenderingUpdateScheduler::registerCACommitHandlers()
{
    if (m_haveRegisteredCommitHandlers)
        return;

    WebView* webView = m_webView;
    [CATransaction addCommitHandler:^{
        [webView _willStartRenderingUpdateDisplay];
    } forPhase:kCATransactionPhasePreLayout];

    [CATransaction addCommitHandler:^{
        [webView _didCompleteRenderingUpdateDisplay];
    } forPhase:kCATransactionPhasePostCommit];
    
    m_haveRegisteredCommitHandlers = true;
}

void WebViewRenderingUpdateScheduler::renderingUpdateRunLoopObserverCallback()
{
#if PLATFORM(IOS_FAMILY)
    // Normally the layer flush callback happens before the web lock auto-unlock observer runs.
    // However if the flush is rescheduled from the callback it may get pushed past it, to the next cycle.
    WebThreadLock();
#endif

    SetForScope insideCallbackScope(m_insideCallback, true);
    m_rescheduledInsideCallback = false;

    updateRendering();
    registerCACommitHandlers();

    if (!m_rescheduledInsideCallback)
        m_renderingUpdateRunLoopObserver->invalidate();
}

void WebViewRenderingUpdateScheduler::postRenderingUpdateCallback()
{
    @autoreleasepool {
        [m_webView _didCompleteRenderingFrame];
        m_postRenderingUpdateRunLoopObserver->invalidate();
    }
}

/*
    Note: Much of the following is obsolete.
    
    The order of events with compositing updates is this:

   Start of runloop                                        End of runloop
        |                                                       |
      --|-------------------------------------------------------|--
           ^         ^                                        ^
           |         |                                        |
    NSWindow update, |                                     CA commit
     NSView drawing  |
        flush        |
                layerSyncRunLoopObserverCallBack

    To avoid flashing, we have to ensure that compositing changes (rendered via
    the CoreAnimation rendering display link) appear on screen at the same time
    as content painted into the window via the normal WebCore rendering path.

    CoreAnimation will commit any layer changes at the end of the runloop via
    its "CA commit" observer. Those changes can then appear onscreen at any time
    when the display link fires, which can result in unsynchronized rendering.

    To fix this, the GraphicsLayerCA code in WebCore does not change the CA
    layer tree during style changes and layout; it stores up all changes and
    commits them via flushCompositingState(). There are then two situations in
    which we can call flushCompositingState():

    1. When painting. FrameView::paintContents() makes a call to flushCompositingState().

    2. When style changes/layout have made changes to the layer tree which do not
       result in painting. In this case we need a run loop observer to do a
       flushCompositingState() at an appropriate time. The observer will keep firing
       until the time is right (essentially when there are no more pending layouts).
*/

void WebViewRenderingUpdateScheduler::updateRendering()
{
    @autoreleasepool {
#if PLATFORM(MAC)
        NSWindow *window = [m_webView window];
#endif // PLATFORM(MAC)

        [m_webView _updateRendering];

#if PLATFORM(MAC)
        // AppKit may have disabled screen updates, thinking an upcoming window flush will re-enable them.
        // In case setNeedsDisplayInRect() has prevented the window from needing to be flushed, re-enable screen
        // updates here.
        ALLOW_DEPRECATED_DECLARATIONS_BEGIN
        if (![window isFlushWindowDisabled])
            [window _enableScreenUpdatesIfNeeded];
        ALLOW_DEPRECATED_DECLARATIONS_END
#endif
    }
}
