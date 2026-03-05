/*
 * Copyright (C) 2010-2020 Apple Inc. All rights reserved.
 * Copyright (C) 2012 Nokia Corporation and/or its subsidiary(-ies)
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

#pragma once

#include "WebEvent.h"
#include <WebCore/FloatSize.h>
#include <WebCore/IntPoint.h>

namespace WTF {
class TextStream;
}

namespace WebKit {

class WebWheelEvent : public WebEvent {
public:
    enum class Granularity : uint8_t {
        ScrollByPageWheelEvent,
        ScrollByPixelWheelEvent
    };

    enum class Phase : uint8_t {
        None,
        Began,
        Stationary,
        Changed,
        Ended,
        Cancelled,
        MayBegin,
        WillBegin,
    };

    enum class MomentumEndType : uint8_t {
        Unknown,
        Interrupted,
        Natural,
    };

    WebWheelEvent(WebEvent&&, const WebCore::IntPoint& position, const WebCore::IntPoint& globalPosition, const WebCore::FloatSize& delta, const WebCore::FloatSize& wheelTicks, Granularity);
#if PLATFORM(COCOA)
    WebWheelEvent(WebEvent&&, const WebCore::IntPoint& position, const WebCore::IntPoint& globalPosition, const WebCore::FloatSize& delta, const WebCore::FloatSize& wheelTicks, Granularity, bool directionInvertedFromDevice, Phase, Phase momentumPhase, bool hasPreciseScrollingDeltas, uint32_t scrollCount, const WebCore::FloatSize& unacceleratedScrollingDelta, MonotonicTime ioHIDEventTimestamp, std::optional<WebCore::FloatSize> rawPlatformDelta, MomentumEndType);
#elif PLATFORM(GTK) || USE(LIBWPE) || ENABLE(WPE_PLATFORM)
    WebWheelEvent(WebEvent&&, const WebCore::IntPoint& position, const WebCore::IntPoint& globalPosition, const WebCore::FloatSize& delta, const WebCore::FloatSize& wheelTicks, Granularity, Phase, Phase momentumPhase, bool hasPreciseScrollingDeltas);
#endif

    const WebCore::IntPoint position() const { return m_position; }
    void setPosition(WebCore::IntPoint position) { m_position = position; }
    const WebCore::IntPoint globalPosition() const { return m_globalPosition; }
    const WebCore::FloatSize delta() const { return m_delta; }
    const WebCore::FloatSize wheelTicks() const { return m_wheelTicks; }
    Granularity granularity() const { return m_granularity; }
    bool directionInvertedFromDevice() const { return m_directionInvertedFromDevice; }
    Phase phase() const { return m_phase; }
    Phase momentumPhase() const { return m_momentumPhase; }
    MomentumEndType momentumEndType() const { return m_momentumEndType; }
#if PLATFORM(COCOA) || PLATFORM(GTK) || USE(LIBWPE) || ENABLE(WPE_PLATFORM)
    bool hasPreciseScrollingDeltas() const { return m_hasPreciseScrollingDeltas; }
#endif
#if PLATFORM(COCOA)
    MonotonicTime ioHIDEventTimestamp() const { return m_ioHIDEventTimestamp; }
    std::optional<WebCore::FloatSize> rawPlatformDelta() const { return m_rawPlatformDelta; }
    void setRawPlatformDelta(std::optional<WebCore::FloatSize>&& delta) { m_rawPlatformDelta = WTF::move(delta); }
    uint32_t scrollCount() const { return m_scrollCount; }
    const WebCore::FloatSize& unacceleratedScrollingDelta() const { return m_unacceleratedScrollingDelta; }
#endif

    bool isMomentumEvent() const { return momentumPhase() != Phase::None && momentumPhase() != Phase::WillBegin; }

    static bool NODELETE isWheelEventType(WebEventType);

private:
    WebCore::IntPoint m_position;
    WebCore::IntPoint m_globalPosition;
    WebCore::FloatSize m_delta;
    WebCore::FloatSize m_wheelTicks;
    Granularity m_granularity { Granularity::ScrollByPageWheelEvent };
    Phase m_phase { Phase::None };
    Phase m_momentumPhase { Phase::None };

    MomentumEndType m_momentumEndType { MomentumEndType::Unknown };
    bool m_directionInvertedFromDevice { false };
#if PLATFORM(COCOA) || PLATFORM(GTK) || USE(LIBWPE) || ENABLE(WPE_PLATFORM)
    bool m_hasPreciseScrollingDeltas { false };
#endif
#if PLATFORM(COCOA)
    MonotonicTime m_ioHIDEventTimestamp;
    std::optional<WebCore::FloatSize> m_rawPlatformDelta;
    uint32_t m_scrollCount { 0 };
    WebCore::FloatSize m_unacceleratedScrollingDelta;
#endif
};

WTF::TextStream& operator<<(WTF::TextStream&, WebWheelEvent::Granularity);
WTF::TextStream& operator<<(WTF::TextStream&, WebWheelEvent::Phase);
WTF::TextStream& operator<<(WTF::TextStream&, WebWheelEvent::MomentumEndType);
WTF::TextStream& operator<<(WTF::TextStream&, const WebWheelEvent&);

} // namespace WebKit

SPECIALIZE_TYPE_TRAITS_BEGIN(WebKit::WebWheelEvent)
static bool isType(const WebKit::WebEvent& event) { return WebKit::WebWheelEvent::isWheelEventType(event.type()); }
SPECIALIZE_TYPE_TRAITS_END()
