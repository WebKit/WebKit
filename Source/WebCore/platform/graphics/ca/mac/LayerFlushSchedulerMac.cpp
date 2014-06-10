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

static const CFIndex layerFlushRunLoopOrder = (CFIndex)RunLoopObserver::WellKnownRunLoopOrders::CoreAnimationCommit - 1;

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

    m_runLoopObserver = RunLoopObserver::create(layerFlushRunLoopOrder, [this]() {
        if (this->isSuspended())
            return;
        this->layerFlushCallback();
    });
}

LayerFlushScheduler::~LayerFlushScheduler()
{
}

void LayerFlushScheduler::layerFlushCallback()
{
    ASSERT(!m_isSuspended);

    AutodrainedPool pool;
    if (m_client->flushLayers())
        invalidate();
}

void LayerFlushScheduler::schedule()
{
    if (m_isSuspended)
        return;

    m_runLoopObserver->schedule(currentRunLoop());
}

void LayerFlushScheduler::invalidate()
{
    m_runLoopObserver->invalidate();
}
    
} // namespace WebCore
