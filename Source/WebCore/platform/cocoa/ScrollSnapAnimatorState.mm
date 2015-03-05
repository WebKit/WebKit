/*
 * Copyright (C) 2014-2015 Apple Inc. All rights reserved.
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
#include "ScrollSnapAnimatorState.h"

#if ENABLE(CSS_SCROLL_SNAP)

namespace WebCore {

ScrollSnapAnimatorState::ScrollSnapAnimatorState(ScrollEventAxis axis, const Vector<LayoutUnit>& snapOffsets)
    : m_snapOffsets(snapOffsets)
    , m_axis(axis)
    , m_currentState(ScrollSnapState::DestinationReached)
    , m_initialOffset(0)
    , m_targetOffset(0)
    , m_beginTrackingWheelDeltaOffset(0)
    , m_numWheelDeltasTracked(0)
    , m_glideMagnitude(0)
    , m_glidePhaseShift(0)
    , m_glideInitialWheelDelta(0)
    , m_shouldOverrideWheelEvent(false)
{
}

void ScrollSnapAnimatorState::pushInitialWheelDelta(float wheelDelta)
{
    if (m_numWheelDeltasTracked < wheelDeltaWindowSize)
        m_wheelDeltaWindow[m_numWheelDeltasTracked++] = wheelDelta;
}

float ScrollSnapAnimatorState::averageInitialWheelDelta() const
{
    if (!m_numWheelDeltasTracked)
        return 0;

    float sum = 0;
    for (int i = 0; i < m_numWheelDeltasTracked; i++)
        sum += m_wheelDeltaWindow[i];

    return sum / m_numWheelDeltasTracked;
}

void ScrollSnapAnimatorState::clearInitialWheelDeltaWindow()
{
    for (int i = 0; i < m_numWheelDeltasTracked; i++)
        m_wheelDeltaWindow[i] = 0;

    m_numWheelDeltasTracked = 0;
}

} // namespace WebCore

#endif // CSS_SCROLL_SNAP
