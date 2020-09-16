/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

namespace WebCore {

// This class is used to detect when we are painting frequently so that - even in a painting model
// without display lists - we can build and cache portions of display lists and reuse them only when
// animating. Once we transition fully to display lists, we can probably just pull from the previous
// paint's display list if it is still around and get rid of this code.
class PaintFrequencyTracker {
    WTF_MAKE_FAST_ALLOCATED;

public:
    PaintFrequencyTracker() = default;

    void begin(MonotonicTime timestamp)
    {
        static unsigned paintFrequencyPaintCountThreshold = 30;
        static Seconds paintFrequencyTimePerFrameThreshold = 32_ms;
        static Seconds paintFrequencySecondsIdleThreshold = 5_s;

        // Start by assuming the paint frequency is low
        m_paintFrequency = PaintFrequency::Low;

        if (!m_firstPaintTime) {
            // Handle the first time this method is called.
            m_firstPaintTime = timestamp;
        } else if (timestamp - m_lastPaintTime > paintFrequencySecondsIdleThreshold) {
            // It has been 5 seconds since last time we draw this renderer. Reset the state
            // of this object as if, we've just started tracking the paint frequency.
            m_firstPaintTime = timestamp;
            m_totalPaints = 0;
        } else if (m_totalPaints >= paintFrequencyPaintCountThreshold && ((m_lastPaintTime - m_firstPaintTime) / m_totalPaints) <= paintFrequencyTimePerFrameThreshold) {
            // Change the paint frequency to be high only if:
            //  - This renderer has been painted at least 30 times.
            //  - The frame rate to paint this renderer has been at least 31.25 FPS.
            m_paintFrequency = PaintFrequency::High;
        }

        m_lastPaintTime = timestamp;
        ++m_totalPaints;
    }

    void end()
    {
        ASSERT(m_firstPaintTime);
        ASSERT(m_firstPaintTime <= m_lastPaintTime);
    }

    bool paintingFrequently() const { return m_paintFrequency == PaintFrequency::High; }

private:
    MonotonicTime m_firstPaintTime;
    MonotonicTime m_lastPaintTime;
    unsigned m_totalPaints { 0 };

    enum class PaintFrequency : bool { Low, High };
    PaintFrequency m_paintFrequency { PaintFrequency::Low };
};

class SinglePaintFrequencyTracking {
    WTF_MAKE_FAST_ALLOCATED;
public:
    SinglePaintFrequencyTracking(PaintFrequencyTracker& paintFrequencyTracker, MonotonicTime timestamp)
        : m_paintFrequencyTracker(paintFrequencyTracker)
        , m_track(timestamp)
    {
        if (m_track)
            m_paintFrequencyTracker.begin(timestamp);
    }

    ~SinglePaintFrequencyTracking()
    {
        if (m_track)
            m_paintFrequencyTracker.end();
    }

private:
    PaintFrequencyTracker& m_paintFrequencyTracker;
    bool m_track;
};

}
