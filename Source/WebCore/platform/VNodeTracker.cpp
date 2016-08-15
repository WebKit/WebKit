/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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
#include "VNodeTracker.h"

#include "Logging.h"
#include <wtf/NeverDestroyed.h>

namespace WebCore {

VNodeTracker& VNodeTracker::singleton()
{
    static NeverDestroyed<VNodeTracker> vnodeTracker;
    return vnodeTracker;
}

VNodeTracker::VNodeTracker()
    : m_vnodeCounter([this](RefCounterEvent event) { if (event == RefCounterEvent::Increment) checkPressureState(); })
    , m_pressureWarningTimer(*this, &VNodeTracker::pressureWarningTimerFired)
    , m_lastWarningTime(std::chrono::steady_clock::now())
{
    platformInitialize();

    LOG(MemoryPressure, "Using following vnode limits for this process: soft=%u, hard=%u", m_softVNodeLimit, m_hardVNodeLimit);
}

void VNodeTracker::checkPressureState()
{
    ASSERT(m_pressureHandler);

    if (m_vnodeCounter.value() <= m_softVNodeLimit)
        return;

    if (!m_pressureWarningTimer.isActive())
        m_pressureWarningTimer.startOneShot(nextPressureWarningInterval());
}

void VNodeTracker::pressureWarningTimerFired()
{
    if (m_vnodeCounter.value() <= m_softVNodeLimit)
        return;

    m_lastWarningTime = std::chrono::steady_clock::now();
    unsigned vnodeCount = m_vnodeCounter.value();
    auto critical = vnodeCount > m_hardVNodeLimit ? Critical::Yes : Critical::No;
    m_pressureHandler(critical);
    LOG(MemoryPressure, "vnode pressure handler freed %lu vnodes out of %u (critical pressure: %s)", vnodeCount - m_vnodeCounter.value(), vnodeCount, critical == Critical::Yes ? "Yes" : "No");
}

std::chrono::milliseconds VNodeTracker::nextPressureWarningInterval() const
{
    // We run the vnode pressure handler every 30 seconds at most.
    static const auto minimumWarningInterval = std::chrono::seconds { 30 };
    auto timeSinceLastWarning = std::chrono::steady_clock::now() - m_lastWarningTime;
    if (timeSinceLastWarning < minimumWarningInterval)
        return std::chrono::duration_cast<std::chrono::milliseconds>(minimumWarningInterval - timeSinceLastWarning);
    return std::chrono::milliseconds { 0 };
}

#if !PLATFORM(COCOA)

void VNodeTracker::platformInitialize()
{
}

#endif

} // namespace WebCore
