/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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
#include "DOMTimerHoldingTank.h"

#if PLATFORM(IOS_FAMILY)

#include <wtf/HashSet.h>

namespace WebCore {

#if PLATFORM(IOS_SIMULATOR)
constexpr Seconds maximumHoldTimeLimit { 50_ms };
#else
constexpr Seconds maximumHoldTimeLimit { 32_ms };
#endif

bool DeferDOMTimersForScope::s_isDeferring { false };

DOMTimerHoldingTank::DOMTimerHoldingTank()
    : m_exceededMaximumHoldTimer { *this, &DOMTimerHoldingTank::removeAll }
{
}

DOMTimerHoldingTank::~DOMTimerHoldingTank() = default;

void DOMTimerHoldingTank::add(const DOMTimer& timer)
{
    m_timers.add(&timer);
    if (!m_exceededMaximumHoldTimer.isActive())
        m_exceededMaximumHoldTimer.startOneShot(maximumHoldTimeLimit);
}

void DOMTimerHoldingTank::remove(const DOMTimer& timer)
{
    stopExceededMaximumHoldTimer();
    m_timers.remove(&timer);
}

bool DOMTimerHoldingTank::contains(const DOMTimer& timer)
{
    return m_timers.contains(&timer);
}

void DOMTimerHoldingTank::removeAll()
{
    stopExceededMaximumHoldTimer();
    m_timers.clear();
}

inline void DOMTimerHoldingTank::stopExceededMaximumHoldTimer()
{
    if (m_exceededMaximumHoldTimer.isActive())
        m_exceededMaximumHoldTimer.stop();
}

} // namespace WebCore

#endif // PLATFORM(IOS_FAMILY)
