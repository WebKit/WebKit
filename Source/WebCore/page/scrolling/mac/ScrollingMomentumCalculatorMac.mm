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

#include "config.h"
#include "ScrollingMomentumCalculatorMac.h"

#if HAVE(NSSCROLLING_FILTERS)

#include "NSScrollingMomentumCalculatorSPI.h"

namespace WebCore {

std::unique_ptr<ScrollingMomentumCalculator> ScrollingMomentumCalculator::create(const FloatSize& viewportSize, const FloatSize& contentSize, const FloatPoint& initialOffset, const FloatPoint& targetOffset, const FloatSize& initialDelta, const FloatSize& initialVelocity)
{
    return std::make_unique<ScrollingMomentumCalculatorMac>(viewportSize, contentSize, initialOffset, targetOffset, initialDelta, initialVelocity);
}

ScrollingMomentumCalculatorMac::ScrollingMomentumCalculatorMac(const FloatSize& viewportSize, const FloatSize& contentSize, const FloatPoint& initialOffset, const FloatPoint& targetOffset, const FloatSize& initialDelta, const FloatSize& initialVelocity)
    : ScrollingMomentumCalculator(viewportSize, contentSize, initialOffset, targetOffset, initialDelta, initialVelocity)
    , m_requiresMomentumScrolling(initialOffset != targetOffset || initialVelocity.area())
{
}

FloatPoint ScrollingMomentumCalculatorMac::scrollOffsetAfterElapsedTime(Seconds elapsedTime)
{
    if (!m_requiresMomentumScrolling)
        return { m_targetScrollOffset.width(), m_targetScrollOffset.height() };

    return [ensurePlatformMomentumCalculator() positionAfterDuration:elapsedTime.value()];
}

Seconds ScrollingMomentumCalculatorMac::animationDuration()
{
    if (!m_requiresMomentumScrolling)
        return 0_s;

    return Seconds([ensurePlatformMomentumCalculator() durationUntilStop]);
}

_NSScrollingMomentumCalculator *ScrollingMomentumCalculatorMac::ensurePlatformMomentumCalculator()
{
    if (m_platformMomentumCalculator)
        return m_platformMomentumCalculator.get();

    NSPoint origin = NSMakePoint(m_initialScrollOffset.width(), m_initialScrollOffset.height());
    NSRect contentFrame = NSMakeRect(0, 0, m_contentSize.width(), m_contentSize.height());
    NSPoint velocity = NSMakePoint(m_initialVelocity.width(), m_initialVelocity.height());
    m_platformMomentumCalculator = adoptNS([[_NSScrollingMomentumCalculator alloc] initWithInitialOrigin:origin velocity:velocity documentFrame:contentFrame constrainedClippingOrigin:NSZeroPoint clippingSize:m_viewportSize tolerance:NSMakeSize(1, 1)]);
    [m_platformMomentumCalculator setDestinationOrigin:NSMakePoint(m_targetScrollOffset.width(), m_targetScrollOffset.height())];
#if __MAC_OS_X_VERSION_MIN_REQUIRED >= 101200
    [m_platformMomentumCalculator calculateToReachDestination];
#endif
    return m_platformMomentumCalculator.get();
}

} // namespace WebCore

#endif // HAVE(NSSCROLLING_FILTERS)
