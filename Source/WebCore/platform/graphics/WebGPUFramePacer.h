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

#pragma once

#include <WebCore/AnimationFrameRate.h>
#include <wtf/Deque.h>
#include <wtf/MonotonicTime.h>
#include <wtf/Vector.h>

namespace WebCore {

class WebGPUFramePacer {
public:
    WEBCORE_EXPORT WebGPUFramePacer();

    WEBCORE_EXPORT void setDisplayNominalFramesPerSecond(FramesPerSecond);

    WEBCORE_EXPORT void recordFrame(Seconds frameCost, Seconds presentStall, MonotonicTime presentTime);

    WEBCORE_EXPORT std::optional<FramesPerSecond> preferredFramesPerSecond(MonotonicTime now) const;

    WEBCORE_EXPORT void reset();

private:
    void rebuildDivisorLadder();
    size_t ladderIndexForCost(Seconds) const;
    void stepDownForStalls();
    void stepUpIfStallFree(MonotonicTime presentTime);

    FramesPerSecond m_displayNominalFramesPerSecond { 0 };
    Vector<FramesPerSecond> m_divisorLadder;
    // Only stalling frames can lower the rate; only stall-free frames can raise it.
    Deque<Seconds> m_stallFreeFrameCosts;
    Deque<Seconds> m_stallingFrameCosts;
    std::optional<MonotonicTime> m_lastPresentTime;
    // The step-up probe measures its stall-free interval from here.
    std::optional<MonotonicTime> m_lastRateDecisionTime;
    size_t m_currentLadderIndex { 0 };
    unsigned m_consecutiveStalls { 0 };
};

} // namespace WebCore
