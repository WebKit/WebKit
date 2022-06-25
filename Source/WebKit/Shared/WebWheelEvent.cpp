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

WebWheelEvent::WebWheelEvent(Type type, const IntPoint& position, const IntPoint& globalPosition, const FloatSize& delta, const FloatSize& wheelTicks, Granularity granularity, OptionSet<Modifier> modifiers, WallTime timestamp)
    : WebEvent(type, modifiers, timestamp)
    , m_position(position)
    , m_globalPosition(globalPosition)
    , m_delta(delta)
    , m_wheelTicks(wheelTicks)
    , m_granularity(granularity)
{
    ASSERT(isWheelEventType(type));
}

#if PLATFORM(COCOA)
WebWheelEvent::WebWheelEvent(Type type, const IntPoint& position, const IntPoint& globalPosition, const FloatSize& delta, const FloatSize& wheelTicks, Granularity granularity, bool directionInvertedFromDevice, Phase phase, Phase momentumPhase, bool hasPreciseScrollingDeltas, uint32_t scrollCount, const WebCore::FloatSize& unacceleratedScrollingDelta, OptionSet<Modifier> modifiers, WallTime timestamp, WallTime ioHIDEventTimestamp, std::optional<WebCore::FloatSize> rawPlatformDelta, MomentumEndType momentumEndType)
    : WebEvent(type, modifiers, timestamp)
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
    ASSERT(isWheelEventType(type));
}
#elif PLATFORM(GTK) || USE(LIBWPE)
WebWheelEvent::WebWheelEvent(Type type, const IntPoint& position, const IntPoint& globalPosition, const FloatSize& delta, const FloatSize& wheelTicks, Phase phase, Phase momentumPhase, Granularity granularity, bool hasPreciseScrollingDeltas, OptionSet<Modifier> modifiers, WallTime timestamp)
    : WebEvent(type, modifiers, timestamp)
    , m_position(position)
    , m_globalPosition(globalPosition)
    , m_delta(delta)
    , m_wheelTicks(wheelTicks)
    , m_granularity(granularity)
    , m_phase(phase)
    , m_momentumPhase(momentumPhase)
    , m_hasPreciseScrollingDeltas(hasPreciseScrollingDeltas)
{
    ASSERT(isWheelEventType(type));
}
#endif

void WebWheelEvent::encode(IPC::Encoder& encoder) const
{
    WebEvent::encode(encoder);

    encoder << m_position;
    encoder << m_globalPosition;
    encoder << m_delta;
    encoder << m_wheelTicks;
    encoder << m_granularity;
    encoder << m_momentumEndType;
    encoder << m_directionInvertedFromDevice;
#if PLATFORM(COCOA) || PLATFORM(GTK) || USE(LIBWPE)
    encoder << m_phase;
    encoder << m_momentumPhase;
    encoder << m_hasPreciseScrollingDeltas;
#endif
#if PLATFORM(COCOA)
    encoder << m_ioHIDEventTimestamp;
    encoder << m_rawPlatformDelta;
    encoder << m_scrollCount;
    encoder << m_unacceleratedScrollingDelta;
#endif
}

bool WebWheelEvent::decode(IPC::Decoder& decoder, WebWheelEvent& t)
{
    if (!WebEvent::decode(decoder, t))
        return false;
    if (!decoder.decode(t.m_position))
        return false;
    if (!decoder.decode(t.m_globalPosition))
        return false;
    if (!decoder.decode(t.m_delta))
        return false;
    if (!decoder.decode(t.m_wheelTicks))
        return false;
    if (!decoder.decode(t.m_granularity))
        return false;
    if (!decoder.decode(t.m_momentumEndType))
        return false;
    if (!decoder.decode(t.m_directionInvertedFromDevice))
        return false;
#if PLATFORM(COCOA) || PLATFORM(GTK) || USE(LIBWPE)
    if (!decoder.decode(t.m_phase))
        return false;
    if (!decoder.decode(t.m_momentumPhase))
        return false;
    if (!decoder.decode(t.m_hasPreciseScrollingDeltas))
        return false;
#endif
#if PLATFORM(COCOA)
    if (!decoder.decode(t.m_ioHIDEventTimestamp))
        return false;
    if (!decoder.decode(t.m_rawPlatformDelta))
        return false;
    if (!decoder.decode(t.m_scrollCount))
        return false;
    if (!decoder.decode(t.m_unacceleratedScrollingDelta))
        return false;
#endif
    return true;
}

bool WebWheelEvent::isWheelEventType(Type type)
{
    return type == Wheel;
}

} // namespace WebKit
