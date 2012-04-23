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

#ifndef CCFrameRateCounter_h
#define CCFrameRateCounter_h

#if USE(ACCELERATED_COMPOSITING)

#include <wtf/Noncopyable.h>
#include <wtf/PassOwnPtr.h>

namespace WebCore {

// This class maintains a history of timestamps, and provides functionality to
// intelligently compute average frames per second (and standard deviation).
class CCFrameRateCounter {
    WTF_MAKE_NONCOPYABLE(CCFrameRateCounter);
public:
    static PassOwnPtr<CCFrameRateCounter> create()
    {
        return adoptPtr(new CCFrameRateCounter());
    }

    void markBeginningOfFrame(double timestamp);
    void markEndOfFrame();
    int currentFrameNumber() const { return m_currentFrameNumber; }
    void getAverageFPSAndStandardDeviation(double& averageFPS, double& standardDeviation) const;
    int timeStampHistorySize() const { return kTimeStampHistorySize; }

    // n = 0 returns the oldest frame retained in the history,
    // while n = timeStampHistorySize() - 1 returns the timestamp most recent frame.
    double timeStampOfRecentFrame(int /* n */);

    // This is a heuristic that can be used to ignore frames in a reasonable way. Returns
    // true if the given frame interval is too fast or too slow, based on constant thresholds.
    bool isBadFrameInterval(double intervalBetweenConsecutiveFrames) const;

private:
    CCFrameRateCounter();

    int frameIndex(int frameNumber) const;
    bool isBadFrame(int frameNumber) const;

    // Two thresholds (measured in seconds) that describe what is considered to be a "no-op frame" that should not be counted.
    // - if the frame is too fast, then given our compositor implementation, the frame probably was a no-op and did not draw.
    // - if the frame is too slow, then there is probably not animating content, so we should not pollute the average.
    static const double kFrameTooFast;
    static const double kFrameTooSlow;

    static const int kTimeStampHistorySize = 120;

    int m_currentFrameNumber;
    double m_timeStampHistory[kTimeStampHistorySize];
};

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)

#endif
