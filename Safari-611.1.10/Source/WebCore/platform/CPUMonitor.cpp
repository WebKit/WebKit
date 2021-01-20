/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
#include "CPUMonitor.h"

namespace WebCore {

CPUMonitor::CPUMonitor(Seconds checkInterval, ExceededCPULimitHandler&& exceededCPULimitHandler)
    : m_checkInterval(checkInterval)
    , m_exceededCPULimitHandler(WTFMove(exceededCPULimitHandler))
    , m_timer(*this, &CPUMonitor::timerFired)
{
}

void CPUMonitor::setCPULimit(const Optional<double>& cpuLimit)
{
    if (m_cpuLimit == cpuLimit)
        return;

    m_cpuLimit = cpuLimit;
    if (m_cpuLimit) {
        if (!m_timer.isActive()) {
            m_lastCPUTime = CPUTime::get();
            m_timer.startRepeating(m_checkInterval);
        }
    } else
        m_timer.stop();
}

void CPUMonitor::timerFired()
{
    ASSERT(m_cpuLimit);

    if (!m_lastCPUTime) {
        m_lastCPUTime = CPUTime::get();
        return;
    }

    auto cpuTime = CPUTime::get();
    if (!cpuTime)
        return;

    auto cpuUsagePercent = cpuTime.value().percentageCPUUsageSince(m_lastCPUTime.value());
    if (cpuUsagePercent > m_cpuLimit.value() * 100)
        m_exceededCPULimitHandler(cpuUsagePercent / 100.);

    m_lastCPUTime = cpuTime;
}

} // namespace WebCore
