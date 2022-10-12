/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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
#include "WebWheelEvent.h"

#include "WebCoreArgumentCoders.h"

namespace WebKit {

using namespace WebCore;

WebWheelEvent::WebWheelEvent(WebEvent&& event, const IntPoint& position, const IntPoint& globalPosition, const FloatSize& delta, const FloatSize& wheelTicks, Granularity granularity)
    : WebEvent(WTFMove(event))
    , m_position(position)
    , m_globalPosition(globalPosition)
    , m_delta(delta)
    , m_wheelTicks(wheelTicks)
    , m_granularity(granularity)
{
    ASSERT(isWheelEventType(type()));
}

#if PLATFORM(COCOA)
WebWheelEvent::WebWheelEvent(WebEvent&& event, const IntPoint& position, const IntPoint& globalPosition, const FloatSize& delta, const FloatSize& wheelTicks, Granularity granularity, bool directionInvertedFromDevice, Phase phase, Phase momentumPhase, bool hasPreciseScrollingDeltas, uint32_t scrollCount, const WebCore::FloatSize& unacceleratedScrollingDelta, WallTime ioHIDEventTimestamp, std::optional<WebCore::FloatSize> rawPlatformDelta, MomentumEndType momentumEndType)
    : WebEvent(WTFMove(event))
    , m_position(position)
    , m_globalPosition(globalPosition)
    , m_delta(delta)
    , m_wheelTicks(wheelTicks)
    , m_granularity(granularity)
    , m_phase(phase)
    , m_momentumPhase(momentumPhase)
    , m_momentumEndType(momentumEndType)
    , m_directionInvertedFromDevice(directionInvertedFromDevice)
    , m_hasPreciseScrollingDeltas(hasPreciseScrollingDeltas)
    , m_ioHIDEventTimestamp(ioHIDEventTimestamp)
    , m_rawPlatformDelta(rawPlatformDelta)
    , m_scrollCount(scrollCount)
    , m_unacceleratedScrollingDelta(unacceleratedScrollingDelta)
{
    ASSERT(isWheelEventType(type()));
}
#elif PLATFORM(GTK) || USE(LIBWPE)
WebWheelEvent::WebWheelEvent(WebEvent&& event, const IntPoint& position, const IntPoint& globalPosition, const FloatSize& delta, const FloatSize& wheelTicks, Granularity granularity, Phase phase, Phase momentumPhase, bool hasPreciseScrollingDeltas)
    : WebEvent(WTFMove(event))
    , m_position(position)
    , m_globalPosition(globalPosition)
    , m_delta(delta)
    , m_wheelTicks(wheelTicks)
    , m_granularity(granularity)
    , m_phase(phase)
    , m_momentumPhase(momentumPhase)
    , m_hasPreciseScrollingDeltas(hasPreciseScrollingDeltas)
{
    ASSERT(isWheelEventType(type()));
}
#endif

bool WebWheelEvent::isWheelEventType(Type type)
{
    return type == Wheel;
}

} // namespace WebKit
