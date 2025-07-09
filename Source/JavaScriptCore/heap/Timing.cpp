/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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
#include "Timing.h"

#include <wtf/CPUTime.h>

namespace JSC {

CPUTimingScope::CPUTimingScope(const std::function<void(Seconds)>& notifier)
    : m_notifier(notifier)
{
    m_before = CPUTime::forCurrentThreadSlow();
}
CPUTimingScope::CPUTimingScope(CPUTimingScope&& other)
    : m_before(other.m_before)
    , m_notifier(other.m_notifier)
    , m_cancelled(other.m_cancelled)
{
    other.m_cancelled = true;
}
CPUTimingScope& CPUTimingScope::operator=(CPUTimingScope&& other) noexcept {
    if (this != &other) {
        m_before = other.m_before;
        m_notifier = other.m_notifier;
        m_cancelled = other.m_cancelled;

        other.m_cancelled = true;
    }
    return *this;
}
CPUTimingScope::~CPUTimingScope()
{
    if (m_cancelled)
        return;
    Seconds after = CPUTime::forCurrentThreadSlow();
    Seconds duration = after - m_before;
    m_notifier(duration);
}

CPUTimingScope ParallelCPUTimer::synchronousScope()
{
    ASSERT(!m_inParrallelMainThreadScope);
    ASSERT(!m_inSychronousThreadScope);
    m_inSychronousThreadScope = true;
    return CPUTimingScope([&] (Seconds duration) {
        m_duration += duration;
        m_inSychronousThreadScope = false;
    });
}
CPUTimingScope ParallelCPUTimer::parallelMainScope()
{
    ASSERT(!m_inParrallelMainThreadScope);
    m_inParrallelMainThreadScope = true;
    Seconds durationBefore = m_parallelDuration.load();
    return CPUTimingScope([&] (Seconds duration) {
        if (m_inSychronousThreadScope)
            m_duration -= duration; // don't double count this time since we're inside a synchronousScope
        parallelDurationFetchMax(durationBefore + duration);
        m_inParrallelMainThreadScope = false;
    });
}
CPUTimingScope ParallelCPUTimer::parallelHelperScope()
{
    Seconds durationBefore = m_parallelDuration.load();
    return CPUTimingScope([&] (Seconds duration) {
        parallelDurationFetchMax(durationBefore + duration);
    });
}
Seconds ParallelCPUTimer::read()
{
    accumulateParallelThreads();
    return m_duration;
}
void ParallelCPUTimer::clear()
{
    m_parallelDuration.store(0_s);
    m_duration = 0_s;
}
void ParallelCPUTimer::accumulateParallelThreads()
{
    m_duration += m_parallelDuration.exchange(0_s);
}
void ParallelCPUTimer::parallelDurationFetchMax(Seconds duration)
{
    while (true) {
        auto current = m_parallelDuration.load();
        if (current >= duration)
            return;
        if (m_parallelDuration.compareExchangeWeakRelaxed(current, duration))
            return;
    }
}

} // namespace JSC
