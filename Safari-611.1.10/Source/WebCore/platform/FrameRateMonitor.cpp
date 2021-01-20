/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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
#include "FrameRateMonitor.h"

namespace WebCore {

static constexpr Seconds MinimumAverageDuration = 1_s;
static constexpr Seconds MaxQueueDuration = 2_s;
static constexpr unsigned MaxFrameDelayCount = 3;

void FrameRateMonitor::update()
{
    ++m_frameCount;

    auto frameTime = MonotonicTime::now().secondsSinceEpoch().value();
    auto lastFrameTime = m_observedFrameTimeStamps.isEmpty() ? frameTime : m_observedFrameTimeStamps.last();

    if (m_observedFrameRate) {
        auto maxDelay = MaxFrameDelayCount / m_observedFrameRate;
        if ((frameTime - lastFrameTime) > maxDelay)
            m_lateFrameCallback({ MonotonicTime::fromRawSeconds(frameTime), MonotonicTime::fromRawSeconds(lastFrameTime) });
    }
    m_observedFrameTimeStamps.append(frameTime);
    m_observedFrameTimeStamps.removeAllMatching([&](auto time) {
        return time <= frameTime - MaxQueueDuration.value();
    });

    auto queueDuration = m_observedFrameTimeStamps.last() - m_observedFrameTimeStamps.first();
    if (queueDuration > MinimumAverageDuration.value())
        m_observedFrameRate = (m_observedFrameTimeStamps.size() / queueDuration);
}

}
