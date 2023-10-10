/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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

#import "config.h"
#import "WKVelocityTrackingScrollView.h"

#if PLATFORM(IOS_FAMILY)

#import <wtf/ApproximateTime.h>
#import <wtf/FixedVector.h>

template <size_t windowSize>
struct ScrollingDeltaWindow {
public:
    static constexpr auto maxDeltaDuration = 100_ms;

    void update(CGPoint contentOffset)
    {
        auto currentTime = ApproximateTime::now();
        auto deltaDuration = currentTime - m_lastTimestamp;
        if (deltaDuration > maxDeltaDuration)
            reset();
        else {
            m_deltas[m_lastIndex] = {
                CGSizeMake(contentOffset.x - m_lastOffset.x, contentOffset.y - m_lastOffset.y),
                deltaDuration
            };
            m_lastIndex = ++m_lastIndex % windowSize;
        }
        m_lastTimestamp = currentTime;
        m_lastOffset = contentOffset;
    }

    void reset()
    {
        for (auto& delta : m_deltas)
            delta = { CGSizeZero, 0_ms };
    }

    CGSize averageVelocity() const
    {
        if (ApproximateTime::now() - m_lastTimestamp > maxDeltaDuration)
            return CGSizeZero;

        auto cumulativeDelta = CGSizeZero;
        CGFloat numberOfDeltas = 0;
        for (auto [delta, duration] : m_deltas) {
            if (!duration)
                continue;

            cumulativeDelta.width += delta.width / duration.seconds();
            cumulativeDelta.height += delta.height / duration.seconds();
            numberOfDeltas += 1;
        }

        if (!numberOfDeltas)
            return CGSizeZero;

        cumulativeDelta.width /= numberOfDeltas;
        cumulativeDelta.height /= numberOfDeltas;
        return cumulativeDelta;
    }

private:
    std::array<std::pair<CGSize, Seconds>, windowSize> m_deltas;
    size_t m_lastIndex { 0 };
    ApproximateTime m_lastTimestamp;
    CGPoint m_lastOffset { CGPointZero };
};

@implementation WKVelocityTrackingScrollView {
    ScrollingDeltaWindow<3> _scrollingDeltaWindow;
}

- (void)updateInteractiveScrollVelocity
{
    if (!self.tracking && !self.decelerating)
        return;

    _scrollingDeltaWindow.update(self.contentOffset);
}

- (CGSize)interactiveScrollVelocityInPointsPerSecond
{
    return _scrollingDeltaWindow.averageVelocity();
}

@end

#endif // PLATFORM(IOS_FAMILY)
