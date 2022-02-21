/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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
#import "ScrollingMomentumCalculatorMac.h"

#if PLATFORM(MAC)

#import <pal/spi/mac/NSScrollingMomentumCalculatorSPI.h>

namespace WebCore {

static bool gEnablePlatformMomentumScrollingPrediction = true;

std::unique_ptr<ScrollingMomentumCalculator> ScrollingMomentumCalculator::create(const ScrollExtents& scrollExtents, const FloatPoint& initialOffset, const FloatSize& initialDelta, const FloatSize& initialVelocity)
{
    return makeUnique<ScrollingMomentumCalculatorMac>(scrollExtents, initialOffset, initialDelta, initialVelocity);
}

void ScrollingMomentumCalculator::setPlatformMomentumScrollingPredictionEnabled(bool enabled)
{
    gEnablePlatformMomentumScrollingPrediction = enabled;
}

ScrollingMomentumCalculatorMac::ScrollingMomentumCalculatorMac(const ScrollExtents& scrollExtents, const FloatPoint& initialOffset, const FloatSize& initialDelta, const FloatSize& initialVelocity)
    : ScrollingMomentumCalculator(scrollExtents, initialOffset, initialDelta, initialVelocity)
{
    m_initialDestinationOffset = predictedDestinationOffset();
    // We could compute m_requiresMomentumScrolling here, based on whether initialDelta is non-zero or we are in a rubber-banded state.
}

FloatPoint ScrollingMomentumCalculatorMac::scrollOffsetAfterElapsedTime(Seconds elapsedTime)
{
    if (!requiresMomentumScrolling())
        return destinationScrollOffset();

    return [ensurePlatformMomentumCalculator() positionAfterDuration:elapsedTime.value()];
}

FloatPoint ScrollingMomentumCalculatorMac::predictedDestinationOffset()
{
    ensurePlatformMomentumCalculator();

    if (!gEnablePlatformMomentumScrollingPrediction) {
        auto nonPlatformPredictedOffset = ScrollingMomentumCalculator::predictedDestinationOffset();
        // We need to make sure the _NSScrollingMomentumCalculator has the same idea of what offset we're shooting for.
        if (nonPlatformPredictedOffset != m_initialDestinationOffset)
            setMomentumCalculatorDestinationOffset(nonPlatformPredictedOffset);

        return nonPlatformPredictedOffset;
    }

    return m_initialDestinationOffset;
}

void ScrollingMomentumCalculatorMac::destinationScrollOffsetDidChange()
{
    setMomentumCalculatorDestinationOffset(destinationScrollOffset());
}

void ScrollingMomentumCalculatorMac::setMomentumCalculatorDestinationOffset(FloatPoint scrollOffset)
{
    _NSScrollingMomentumCalculator *calculator = ensurePlatformMomentumCalculator();
    calculator.destinationOrigin = scrollOffset;
    [calculator calculateToReachDestination];
}

Seconds ScrollingMomentumCalculatorMac::animationDuration()
{
    if (!requiresMomentumScrolling())
        return 0_s;

    return Seconds([ensurePlatformMomentumCalculator() durationUntilStop]);
}

bool ScrollingMomentumCalculatorMac::requiresMomentumScrolling()
{
    if (m_requiresMomentumScrolling == std::nullopt)
        m_requiresMomentumScrolling = m_initialScrollOffset != destinationScrollOffset() || m_initialVelocity.area();
    return m_requiresMomentumScrolling.value();
}

_NSScrollingMomentumCalculator *ScrollingMomentumCalculatorMac::ensurePlatformMomentumCalculator()
{
    if (m_platformMomentumCalculator)
        return m_platformMomentumCalculator.get();

    NSPoint origin = m_initialScrollOffset;
    NSRect contentFrame = NSMakeRect(0, 0, m_scrollExtents.contentsSize.width(), m_scrollExtents.contentsSize.height());
    NSPoint velocity = NSMakePoint(m_initialVelocity.width(), m_initialVelocity.height());
    m_platformMomentumCalculator = adoptNS([[_NSScrollingMomentumCalculator alloc] initWithInitialOrigin:origin velocity:velocity documentFrame:contentFrame constrainedClippingOrigin:NSZeroPoint clippingSize:m_scrollExtents.viewportSize tolerance:NSMakeSize(1, 1)]);
    m_initialDestinationOffset = [m_platformMomentumCalculator destinationOrigin];
    return m_platformMomentumCalculator.get();
}

} // namespace WebCore

#endif // PLATFORM(MAC)
