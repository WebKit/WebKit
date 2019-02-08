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

#pragma once

#include "ActivityState.h"
#include "Timer.h"
#include <wtf/CPUTime.h>
#include <wtf/Optional.h>

namespace WebCore {

class Page;

class PerformanceMonitor {
    WTF_MAKE_FAST_ALLOCATED;
public:
    explicit PerformanceMonitor(Page&);

    void didStartProvisionalLoad();
    void didFinishLoad();
    void activityStateChanged(OptionSet<ActivityState::Flag> oldState, OptionSet<ActivityState::Flag> newState);

private:
    void measurePostLoadCPUUsage();
    void measurePostBackgroundingCPUUsage();
    void measurePerActivityStateCPUUsage();
    void measureCPUUsageInActivityState(ActivityStateForCPUSampling);
    void measurePostLoadMemoryUsage();
    void measurePostBackgroundingMemoryUsage();
    void processMayBecomeInactiveTimerFired();
    static void updateProcessStateForMemoryPressure();

    Page& m_page;

    Timer m_postPageLoadCPUUsageTimer;
    Optional<CPUTime> m_postLoadCPUTime;
    Timer m_postBackgroundingCPUUsageTimer;
    Optional<CPUTime> m_postBackgroundingCPUTime;
    Timer m_perActivityStateCPUUsageTimer;
    Optional<CPUTime> m_perActivityStateCPUTime;

    Timer m_postPageLoadMemoryUsageTimer;
    Timer m_postBackgroundingMemoryUsageTimer;

    Timer m_processMayBecomeInactiveTimer;
    bool m_processMayBecomeInactive { true };
};

}
