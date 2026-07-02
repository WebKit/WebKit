/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
#include "SessionHistoryTraversalQueue.h"

#include "WebPageProxy.h"
#include <utility>
#include <wtf/CheckedArithmetic.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebKit {

WTF_MAKE_TZONE_ALLOCATED_IMPL(SessionHistoryTraversalQueue);

SessionHistoryTraversalQueue::SessionHistoryTraversalQueue(WebPageProxy& page)
    : m_pageProxy(page)
    , m_flushTimer(RunLoop::mainSingleton(), "SessionHistoryTraversalQueue::FlushTimer"_s, this, &SessionHistoryTraversalQueue::flush)
{
}

SessionHistoryTraversalQueue::~SessionHistoryTraversalQueue()
{
    m_flushTimer.stop();
}

void SessionHistoryTraversalQueue::enqueueDelta(int32_t delta)
{
    if (m_hasPending) {
        auto sum = WTF::CheckedInt32 { m_pendingDelta } + delta;
        if (sum.hasOverflowed()) {
            cancel();
            return;
        }
        m_pendingDelta = sum.value();
    } else {
        m_pendingDelta = delta;
        m_hasPending = true;
        m_flushTimer.startOneShot(0_s);
    }
}

void SessionHistoryTraversalQueue::cancel()
{
    m_flushTimer.stop();
    m_pendingDelta = 0;
    m_hasPending = false;
}

void SessionHistoryTraversalQueue::flush()
{
    int32_t delta = std::exchange(m_pendingDelta, 0);
    m_hasPending = false;

    if (!delta)
        return;

    Ref pageProxy = m_pageProxy.get();
    pageProxy->goToBackForwardItemAtIndex(delta);
}

} // namespace WebKit
