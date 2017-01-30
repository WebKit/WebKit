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
#include "PerActivityStateCPUUsageSampler.h"

#include "Logging.h"
#include "WebProcessPool.h"
#include "WebProcessProxy.h"
#include <WebCore/DiagnosticLoggingKeys.h>
#include <wtf/DecimalNumber.h>

namespace WebKit {

static const int64_t microsecondsPerSecond = 1000000;

using namespace WebCore;

static const std::chrono::minutes loggingInterval { 60 };

PerActivityStateCPUUsageSampler::PerActivityStateCPUUsageSampler(WebProcessPool& processPool)
    : m_processPool(processPool)
    , m_loggingTimer(RunLoop::main(), this, &PerActivityStateCPUUsageSampler::loggingTimerFired)
{
    m_lastCPUTime = monotonicallyIncreasingTime();
    m_loggingTimer.startRepeating(loggingInterval);
}

PerActivityStateCPUUsageSampler::~PerActivityStateCPUUsageSampler()
{
}

void PerActivityStateCPUUsageSampler::reportWebContentCPUTime(int64_t cpuTime, ActivityStateForCPUSampling activityState)
{
    auto result = m_cpuTimeInActivityState.add(activityState, cpuTime);
    if (!result.isNewEntry)
        result.iterator->value += cpuTime;
}

static inline String loggingKeyForActivityState(ActivityStateForCPUSampling state)
{
    switch (state) {
    case ActivityStateForCPUSampling::NonVisible:
        return DiagnosticLoggingKeys::nonVisibleStateKey();
    case ActivityStateForCPUSampling::VisibleNonActive:
        return DiagnosticLoggingKeys::visibleNonActiveStateKey();
    case ActivityStateForCPUSampling::VisibleAndActive:
        return DiagnosticLoggingKeys::visibleAndActiveStateKey();
    }
}

void PerActivityStateCPUUsageSampler::loggingTimerFired()
{
    auto* page = pageForLogging();
    if (!page) {
        m_cpuTimeInActivityState.clear();
        return;
    }

    double currentCPUTime = monotonicallyIncreasingTime();
    int64_t cpuTimeDelta = (currentCPUTime - m_lastCPUTime) * microsecondsPerSecond;

    for (auto& pair : m_cpuTimeInActivityState) {
        double cpuUsage = static_cast<double>(pair.value * 100.) / cpuTimeDelta;
        String activityStateKey = loggingKeyForActivityState(pair.key);
        page->logDiagnosticMessageWithValue(DiagnosticLoggingKeys::cpuUsageKey(), activityStateKey, cpuUsage, 2, ShouldSample::No);
        RELEASE_LOG(PerformanceLogging, "WebContent processes used %.1f%% CPU in %s state", cpuUsage, activityStateKey.utf8().data());
    }

    m_cpuTimeInActivityState.clear();
    m_lastCPUTime = currentCPUTime;
}

WebPageProxy* PerActivityStateCPUUsageSampler::pageForLogging() const
{
    for (auto& webProcess : m_processPool.processes()) {
        if (!webProcess->pageCount())
            continue;
        return *webProcess->pages().begin();
    }
    return nullptr;
}

} // namespace WebKit
