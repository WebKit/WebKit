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
static constexpr size_t warmUpSamples = 8;
static constexpr double sustainablePercentile = 0.90;
static constexpr unsigned overloadSamplesToStepDown = 4;
static constexpr unsigned headroomSamplesToStepUp = 30;
static constexpr double budgetToleranceFactor = 0.98;
static constexpr Seconds idleTimeout = 350_ms;

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
    m_frameCosts.clear();
    m_lastPresentTime = std::nullopt;
    m_currentLadderIndex = 0;
    m_consecutiveOverload = 0;
    m_consecutiveHeadroom = 0;
}

void WebGPUFramePacer::recordFrame(Seconds frameCost, MonotonicTime presentTime)
{
    if (m_lastPresentTime && presentTime - *m_lastPresentTime > idleTimeout)
        reset();
    m_lastPresentTime = presentTime;

    if (frameCost <= 0_s || frameCost > idleTimeout)
        return;

    m_frameCosts.append(frameCost);
    if (m_frameCosts.size() > sampleWindowSize)
        m_frameCosts.removeFirst();

    runController();
}

void WebGPUFramePacer::runController()
{
    if (m_frameCosts.size() < warmUpSamples || m_divisorLadder.isEmpty())
        return;

    Vector<Seconds> sorted;
    sorted.appendRange(m_frameCosts.begin(), m_frameCosts.end());
    std::sort(sorted.begin(), sorted.end());
    size_t percentileIndex = std::min(sorted.size() - 1, static_cast<size_t>(sustainablePercentile * sorted.size()));
    double sustainableCost = sorted[percentileIndex].seconds();
    if (sustainableCost <= 0)
        return;

    size_t targetIndex = 0;
    for (size_t i = 0; i < m_divisorLadder.size(); ++i) {
        if (1.0 / m_divisorLadder[i] >= budgetToleranceFactor * sustainableCost) {
            targetIndex = i;
            break;
        }
        targetIndex = i;
    }

    if (!m_currentLadderIndex && targetIndex > m_currentLadderIndex) {
        m_currentLadderIndex = targetIndex;
        m_consecutiveOverload = 0;
        m_consecutiveHeadroom = 0;
        return;
    }

    if (targetIndex > m_currentLadderIndex) {
        m_consecutiveHeadroom = 0;
        if (++m_consecutiveOverload >= overloadSamplesToStepDown) {
            m_currentLadderIndex = targetIndex;
            m_consecutiveOverload = 0;
        }
    } else if (targetIndex < m_currentLadderIndex) {
        m_consecutiveOverload = 0;
        if (++m_consecutiveHeadroom >= headroomSamplesToStepUp) {
            --m_currentLadderIndex;
            m_consecutiveHeadroom = 0;
        }
    } else {
        m_consecutiveOverload = 0;
        m_consecutiveHeadroom = 0;
    }
}

std::optional<FramesPerSecond> WebGPUFramePacer::preferredFramesPerSecond(MonotonicTime now) const
{
    if (m_frameCosts.size() < warmUpSamples || m_divisorLadder.isEmpty())
        return std::nullopt;

    if (m_lastPresentTime && now - *m_lastPresentTime > idleTimeout)
        return std::nullopt;

    if (!m_currentLadderIndex)
        return std::nullopt;

    return m_divisorLadder[m_currentLadderIndex];
}

} // namespace WebCore
