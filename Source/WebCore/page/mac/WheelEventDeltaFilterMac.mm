/*
 * Copyright (C) 2015-2021 Apple Inc. All rights reserved.
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

#if PLATFORM(MAC)
#import "WheelEventDeltaFilterMac.h"

#import "FloatPoint.h"
#import "Logging.h"
#import "PlatformWheelEvent.h"
#import <pal/spi/mac/NSScrollingInputFilterSPI.h>
#import <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(WheelEventDeltaFilterMac);

WheelEventDeltaFilterMac::WheelEventDeltaFilterMac()
    : WheelEventDeltaFilter()
    , m_predominantAxisFilter(adoptNS([[_NSScrollingPredominantAxisFilter alloc] init]))
    , m_initialWallTime(WallTime::now())
{
}

void WheelEventDeltaFilterMac::updateFromEvent(const PlatformWheelEvent& event)
{
    if (event.isMomentumEvent()) {
        if (event.momentumPhase() == PlatformWheelEventPhase::Began)
            updateCurrentVelocityFromEvent(event);
        m_lastIOHIDEventTimestamp = event.ioHIDEventTimestamp();
        return;
    }

    switch (event.phase()) {
    case PlatformWheelEventPhase::WillBegin:
    case PlatformWheelEventPhase::None:
    case PlatformWheelEventPhase::Ended:
        break;

    case PlatformWheelEventPhase::Began:
        reset();
        updateCurrentVelocityFromEvent(event);
        break;

    case PlatformWheelEventPhase::Changed:
        updateCurrentVelocityFromEvent(event);
        break;

    case PlatformWheelEventPhase::MayBegin:
    case PlatformWheelEventPhase::Cancelled:
    case PlatformWheelEventPhase::Stationary:
        reset();
        break;
    }

    m_lastIOHIDEventTimestamp = event.ioHIDEventTimestamp();
}

void WheelEventDeltaFilterMac::updateCurrentVelocityFromEvent(const PlatformWheelEvent& event)
{
    // The absolute value of timestamp doesn't matter; the filter looks at deltas from the previous event.
    auto timestamp = event.timestamp() - m_initialWallTime;

    NSPoint filteredDeltaResult;
    NSPoint filteredVelocityResult;

    [m_predominantAxisFilter filterInputDelta:toFloatPoint(event.delta()) timestamp:timestamp.seconds() outputDelta:&filteredDeltaResult velocity:&filteredVelocityResult];
    auto axisFilteredVelocity = toFloatSize(filteredVelocityResult);
    m_currentFilteredDelta = toFloatSize(filteredDeltaResult);

    // Use a 1ms minimum to avoid divide by zero. The usual cadence of these events matches screen refresh rate.
    auto deltaFromLastEvent = std::max(event.ioHIDEventTimestamp() - m_lastIOHIDEventTimestamp, 1_ms);
    m_currentFilteredVelocity = event.delta() / deltaFromLastEvent.seconds();

    // Apply the axis-locking that m_predominantAxisFilter does.
    if (!axisFilteredVelocity.width())
        m_currentFilteredVelocity.setWidth(0);
    if (!axisFilteredVelocity.height())
        m_currentFilteredVelocity.setHeight(0);

    LOG(ScrollAnimations, "WheelEventDeltaFilterMac::updateFromEvent: _NSScrollingPredominantAxisFilter velocity %.2f, %2f, IOHIDEvent velocity %.2f,%.2f",
        axisFilteredVelocity.width(), axisFilteredVelocity.height(), m_currentFilteredVelocity.width(), m_currentFilteredVelocity.height());
}

void WheelEventDeltaFilterMac::reset()
{
    [m_predominantAxisFilter reset];
    m_currentFilteredVelocity = { };
    m_currentFilteredDelta = { };
    m_lastIOHIDEventTimestamp = { };
}

}

#endif /* PLATFORM(MAC) */
