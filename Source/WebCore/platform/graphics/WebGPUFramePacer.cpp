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
#include "WebGPUFramePacer.h"

#include <algorithm>

namespace WebCore {

static constexpr size_t sampleWindowSize = 16;
static constexpr size_t minimumStallFreeSamplesToStepUp = 2;
static constexpr unsigned stallsToStepDown = 4;
static constexpr double budgetToleranceFactor = 0.98;
static constexpr Seconds idleTimeout = 350_ms;
// Below this a stall is scheduling jitter around the wait, not the GPU keeping the present waiting.
static constexpr Seconds significantPresentStall = 1_ms;
static constexpr Seconds stallFreeProbeInterval = 500_ms;

// The median rather than the extreme, so one frame unlike its neighbours decides nothing.
static Seconds medianCost(const Deque<Seconds>& costs)
{
    ASSERT(!costs.isEmpty());
    Vector<Seconds> sorted;
    sorted.appendRange(costs.begin(), costs.end());
    std::sort(sorted.begin(), sorted.end());
    return sorted[sorted.size() / 2];
}

WebGPUFramePacer::WebGPUFramePacer() = default;

void WebGPUFramePacer::setDisplayNominalFramesPerSecond(FramesPerSecond nominal)
{
    if (nominal == m_displayNominalFramesPerSecond)
        return;
    m_displayNominalFramesPerSecond = nominal;
    rebuildDivisorLadder();
    reset();
}

void WebGPUFramePacer::rebuildDivisorLadder()
{
    m_divisorLadder.clear();
    if (!m_displayNominalFramesPerSecond)
        return;
    for (unsigned divisor = 1; divisor <= m_displayNominalFramesPerSecond; ++divisor) {
        if (m_displayNominalFramesPerSecond % divisor)
            continue;
        m_divisorLadder.append(m_displayNominalFramesPerSecond / divisor);
    }
}

void WebGPUFramePacer::reset()
{
    m_stallFreeFrameCosts.clear();
    m_stallingFrameCosts.clear();
    m_lastPresentTime = std::nullopt;
    m_lastRateDecisionTime = std::nullopt;
    m_currentLadderIndex = 0;
    m_consecutiveStalls = 0;
}

void WebGPUFramePacer::recordFrame(Seconds frameCost, Seconds presentStall, MonotonicTime presentTime)
{
    if (m_lastPresentTime && presentTime - *m_lastPresentTime > idleTimeout)
        reset();
    m_lastPresentTime = presentTime;

    if (m_divisorLadder.isEmpty())
        return;

    if (presentStall >= significantPresentStall) {
        // A frame costing more than the presentation interval only matters once it backs presents up.
        m_lastRateDecisionTime = presentTime;
        m_stallFreeFrameCosts.clear();
        if (frameCost > 0_s && frameCost <= idleTimeout) {
            m_stallingFrameCosts.append(frameCost);
            if (m_stallingFrameCosts.size() > sampleWindowSize)
                m_stallingFrameCosts.removeFirst();
        }
        if (++m_consecutiveStalls >= stallsToStepDown)
            stepDownForStalls();
        return;
    }

    m_consecutiveStalls = 0;
    m_stallingFrameCosts.clear();

    if (frameCost <= 0_s || frameCost > idleTimeout)
        return;

    m_stallFreeFrameCosts.append(frameCost);
    if (m_stallFreeFrameCosts.size() > sampleWindowSize)
        m_stallFreeFrameCosts.removeFirst();

    stepUpIfStallFree(presentTime);
}

size_t WebGPUFramePacer::ladderIndexForCost(Seconds cost) const
{
    ASSERT(!m_divisorLadder.isEmpty());
    for (size_t i = 0; i < m_divisorLadder.size(); ++i) {
        if (1.0 / m_divisorLadder[i] >= budgetToleranceFactor * cost.seconds())
            return i;
    }
    return m_divisorLadder.size() - 1;
}

void WebGPUFramePacer::stepDownForStalls()
{
    m_consecutiveStalls = 0;
    if (m_stallingFrameCosts.isEmpty())
        return;

    size_t targetIndex = ladderIndexForCost(medianCost(m_stallingFrameCosts));
    if (targetIndex <= m_currentLadderIndex)
        return;

    m_currentLadderIndex = targetIndex;
}

void WebGPUFramePacer::stepUpIfStallFree(MonotonicTime presentTime)
{
    if (!m_currentLadderIndex || m_stallFreeFrameCosts.size() < minimumStallFreeSamplesToStepUp)
        return;
    if (m_lastRateDecisionTime && presentTime - *m_lastRateDecisionTime < stallFreeProbeInterval)
        return;

    // Requiring the cost to allow a faster rung too, or a workload sitting exactly at its current rung
    // would probe up and stall back down forever.
    size_t targetIndex = ladderIndexForCost(medianCost(m_stallFreeFrameCosts));
    if (targetIndex >= m_currentLadderIndex)
        return;

    m_currentLadderIndex = targetIndex;
    m_lastRateDecisionTime = presentTime;
    m_stallFreeFrameCosts.clear();
}

std::optional<FramesPerSecond> WebGPUFramePacer::preferredFramesPerSecond(MonotonicTime now) const
{
    if (!m_currentLadderIndex || m_divisorLadder.isEmpty())
        return std::nullopt;

    if (m_lastPresentTime && now - *m_lastPresentTime > idleTimeout)
        return std::nullopt;

    return m_divisorLadder[m_currentLadderIndex];
}

} // namespace WebCore
