/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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
#include "FrameRateAligner.h"

namespace WebCore {

FrameRateAligner::FrameRateAligner() = default;

FrameRateAligner::~FrameRateAligner() = default;

static ReducedResolutionSeconds idealTimeForNextUpdate(ReducedResolutionSeconds firstUpdateTime, ReducedResolutionSeconds lastUpdateTime, FramesPerSecond frameRate)
{
    ReducedResolutionSeconds interval(1.0 / frameRate);
    auto timeUntilNextUpdate = (lastUpdateTime - firstUpdateTime) % interval;
    return lastUpdateTime + interval - timeUntilNextUpdate;
}

void FrameRateAligner::beginUpdate(ReducedResolutionSeconds timestamp, std::optional<FramesPerSecond> timelineFrameRate)
{
    // We record the timestamp for this new update such that in updateFrameRate()
    // we can compare against it to identify animations that should be sampled
    // for this current update.
    m_timestamp = timestamp;

    const auto nextUpdateTimeEpsilon = 1_ms;
    for (auto& [frameRate, data] : m_frameRates) {
        // If the timeline frame rate is the same as this animation frame rate, then
        // we don't need to compute the next ideal sample time.
        if (timelineFrameRate == frameRate) {
            data.lastUpdateTime = timestamp;
            continue;
        }

        // If the next ideal sample time for this frame rate aligns with the current timestamp
        // or is already behind the current timestamp, we can set the last update time to the
        // current timestamp, which will indicate in updateFrameRate() that animations using
        // this frame rate should be sampled in the current update.
        auto nextUpdateTime = idealTimeForNextUpdate(data.firstUpdateTime, data.lastUpdateTime, frameRate);
        if ((nextUpdateTime - nextUpdateTimeEpsilon) <= timestamp)
            data.lastUpdateTime = timestamp;
    }
}

auto FrameRateAligner::updateFrameRate(FramesPerSecond frameRate) -> ShouldUpdate
{
    auto it = m_frameRates.find(frameRate);

    if (it != m_frameRates.end()) {
        // We're dealing with a frame rate for which we've sampled animations before.
        // If the last update time for this frame rate is the current timestamp, this
        // means we've established in beginUpdate() that animations for this frame rate
        // should be sampled.
        return it->value.lastUpdateTime == m_timestamp ? ShouldUpdate::Yes : ShouldUpdate::No;
    }

    // We're dealing with a frame rate we didn't see in the previous update. In this case,
    // we'll allow animations to be sampled right away.
    // FIXME: We need make sure to reset the last update time to align this new frame rate
    // with other compatible frame rates.
    m_frameRates.set(frameRate, FrameRateData { m_timestamp, m_timestamp });
    return ShouldUpdate::Yes;
}

std::optional<Seconds> FrameRateAligner::timeUntilNextUpdateForFrameRate(FramesPerSecond frameRate, ReducedResolutionSeconds timestamp) const
{
    auto it = m_frameRates.find(frameRate);
    if (it == m_frameRates.end())
        return std::nullopt;

    auto& data = it->value;
    return idealTimeForNextUpdate(data.firstUpdateTime, data.lastUpdateTime, frameRate) - timestamp;
}

std::optional<FramesPerSecond> FrameRateAligner::maximumFrameRate() const
{
    std::optional<FramesPerSecond> maximumFrameRate;
    for (auto frameRate : m_frameRates.keys()) {
        if (!maximumFrameRate || *maximumFrameRate < frameRate)
            maximumFrameRate = frameRate;
    }
    return maximumFrameRate;
}

} // namespace WebCore

