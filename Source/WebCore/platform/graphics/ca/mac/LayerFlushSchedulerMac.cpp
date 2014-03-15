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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"

#include "LayerFlushScheduler.h"

#include <wtf/AutodrainedPool.h>

#if PLATFORM(IOS)
#include "RuntimeApplicationChecksIOS.h"
#include <CoreFoundation/CFBundle.h>
#include <WebCore/WebCoreThread.h>
#endif

namespace WebCore {

static const CFIndex CoreAnimationRunLoopOrder = 2000000;
static const CFIndex LayerFlushRunLoopOrder = CoreAnimationRunLoopOrder - 1;

static CFRunLoopRef currentRunLoop()
{
#if PLATFORM(IOS)
    // A race condition during WebView deallocation can lead to a crash if the layer sync run loop
    // observer is added to the main run loop <rdar://problem/9798550>. However, for responsiveness,
    // we still allow this, see <rdar://problem/7403328>. Since the race condition and subsequent
    // crash are especially troublesome for iBooks, we never allow the observer to be added to the
    // main run loop in iBooks.
    if (applicationIsIBooksOnIOS())
        return WebThreadRunLoop();
#endif
    return CFRunLoopGetCurrent();
}

LayerFlushScheduler::LayerFlushScheduler(LayerFlushSchedulerClient* client)
    : m_isSuspended(false)
    , m_client(client)
{
    ASSERT_ARG(client, client);
}

LayerFlushScheduler::~LayerFlushScheduler()
{
    ASSERT(!m_runLoopObserver);
}

void LayerFlushScheduler::runLoopObserverCallback(CFRunLoopObserverRef, CFRunLoopActivity, void* context)
{
    static_cast<LayerFlushScheduler*>(context)->runLoopObserverCallback();
}

void LayerFlushScheduler::runLoopObserverCallback()
{
    ASSERT(m_runLoopObserver);
    ASSERT(!m_isSuspended);

    AutodrainedPool pool;
    if (m_client->flushLayers())
        invalidate();
}

void LayerFlushScheduler::schedule()
{
    if (m_isSuspended)
        return;

    CFRunLoopRef runLoop = currentRunLoop();

    // Make sure we wake up the loop or the observer could be delayed until some other source fires.
    CFRunLoopWakeUp(runLoop);

    if (m_runLoopObserver)
        return;

    CFRunLoopObserverContext context = { 0, this, 0, 0, 0 };
    m_runLoopObserver = adoptCF(CFRunLoopObserverCreate(0, kCFRunLoopBeforeWaiting | kCFRunLoopExit, true, LayerFlushRunLoopOrder, runLoopObserverCallback, &context));

    CFRunLoopAddObserver(runLoop, m_runLoopObserver.get(), kCFRunLoopCommonModes);
}

void LayerFlushScheduler::invalidate()
{
    if (m_runLoopObserver) {
        CFRunLoopObserverInvalidate(m_runLoopObserver.get());
        m_runLoopObserver = nullptr;   
    }
}
    
} // namespace WebCore
