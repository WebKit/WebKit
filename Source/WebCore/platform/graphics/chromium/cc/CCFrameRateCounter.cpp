/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#if USE(ACCELERATED_COMPOSITING)
#include "CCFrameRateCounter.h"

#include "CCProxy.h"
#include <wtf/CurrentTime.h>

namespace WebCore {

const double CCFrameRateCounter::kFrameTooFast = 1.0 / 70.0; // measured in seconds
const double CCFrameRateCounter::kFrameTooSlow = 1.0 / 12.0;

// safeMod works on -1, returning m-1 in that case.
static inline int safeMod(int number, int modulus)
{
    return (number + modulus) % modulus;
}

inline int CCFrameRateCounter::frameIndex(int frame) const
{
    return safeMod(frame, kTimeStampHistorySize);
}

CCFrameRateCounter::CCFrameRateCounter()
    : m_currentFrameNumber(1)
{
    m_timeStampHistory[0] = currentTime();
    m_timeStampHistory[1] = m_timeStampHistory[0];
    for (int i = 2; i < kTimeStampHistorySize; i++)
        m_timeStampHistory[i] = 0;
}

void CCFrameRateCounter::markBeginningOfFrame(double timestamp)
{
    m_timeStampHistory[frameIndex(m_currentFrameNumber)] = timestamp;
}

void CCFrameRateCounter::markEndOfFrame()
{
    m_currentFrameNumber += 1;
}

bool CCFrameRateCounter::isBadFrameInterval(double intervalBetweenConsecutiveFrames) const
{
    bool schedulerAllowsDoubleFrames = !CCProxy::hasImplThread();
    bool intervalTooFast = schedulerAllowsDoubleFrames && intervalBetweenConsecutiveFrames < kFrameTooFast;
    bool intervalTooSlow = intervalBetweenConsecutiveFrames > kFrameTooSlow;
    return intervalTooFast || intervalTooSlow;
}

bool CCFrameRateCounter::isBadFrame(int frameNumber) const
{
    double delta = m_timeStampHistory[frameIndex(frameNumber)] -
            m_timeStampHistory[frameIndex(frameNumber - 1)];
    return isBadFrameInterval(delta);
}

void CCFrameRateCounter::getAverageFPSAndStandardDeviation(double& averageFPS, double& standardDeviation) const
{
    int frame = m_currentFrameNumber - 1;
    averageFPS = 0;
    int averageFPSCount = 0;
    double fpsVarianceNumerator = 0;

    // Walk backwards through the samples looking for a run of good frame
    // timings from which to compute the mean and standard deviation.
    //
    // Slow frames occur just because the user is inactive, and should be
    // ignored. Fast frames are ignored if the scheduler is in single-thread
    // mode in order to represent the true frame rate in spite of the fact that
    // the first few swapbuffers happen instantly which skews the statistics
    // too much for short lived animations.
    //
    // isBadFrame encapsulates the frame too slow/frame too fast logic.
    while (1) {
        if (!isBadFrame(frame)) {
            averageFPSCount++;
            double secForLastFrame = m_timeStampHistory[frameIndex(frame)] -
                                     m_timeStampHistory[frameIndex(frame - 1)];
            double x = 1.0 / secForLastFrame;
            double deltaFromAverage = x - averageFPS;
            // Change with caution - numerics. http://en.wikipedia.org/wiki/Standard_deviation
            averageFPS = averageFPS + deltaFromAverage / averageFPSCount;
            fpsVarianceNumerator = fpsVarianceNumerator + deltaFromAverage * (x - averageFPS);
        }
        if (averageFPSCount && isBadFrame(frame)) {
            // We've gathered a run of good samples, so stop.
            break;
        }
        --frame;
        if (frameIndex(frame) == frameIndex(m_currentFrameNumber) || frame < 0) {
            // We've gone through all available historical data, so stop.
            break;
        }
    }

    standardDeviation = sqrt(fpsVarianceNumerator / averageFPSCount);
}

double CCFrameRateCounter::timeStampOfRecentFrame(int n)
{
    ASSERT(n >= 0 && n < kTimeStampHistorySize);
    int desiredIndex = (frameIndex(m_currentFrameNumber) + n) % kTimeStampHistorySize;
    return m_timeStampHistory[desiredIndex];
}

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)
