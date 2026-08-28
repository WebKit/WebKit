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
#include "WebEventPhase.h"
#include <WebCore/FloatSize.h>
#include <WebCore/IntPoint.h>

namespace WTF {
class TextStream;
}

namespace WebKit {

// Moved out of WebWheelEvent so that WebWheelEventData can name them. WebWheelEvent keeps aliases,
// so existing WebWheelEvent::Granularity / ::MomentumEndType spellings continue to work.
enum class WebWheelEventGranularity : uint8_t {
    ScrollByPageWheelEvent,
    ScrollByPixelWheelEvent
};

enum class WebWheelEventMomentumEndType : uint8_t {
    Unknown,
    Interrupted,
    Natural,
};

// Field order matches WebEvent.serialization.in. Fields absent on a platform are ones no
// constructor there sets, so the corresponding accessors return constants.
struct WebWheelEventData {
    WebCore::IntPoint position;
    WebCore::IntPoint globalPosition;
    WebCore::FloatSize delta;
    WebCore::FloatSize wheelTicks;
    WebWheelEventGranularity granularity { WebWheelEventGranularity::ScrollByPageWheelEvent };
#if PLATFORM(COCOA)
    bool directionInvertedFromDevice { false };
#endif
#if PLATFORM(COCOA) || PLATFORM(GTK) || USE(LIBWPE) || ENABLE(WPE_PLATFORM)
    WebEventPhase phase { WebEventPhase::None };
    WebEventPhase momentumPhase { WebEventPhase::None };
    bool hasPreciseScrollingDeltas { false };
#endif
#if PLATFORM(COCOA)
    uint32_t scrollCount { 0 };
    WebCore::FloatSize unacceleratedScrollingDelta;
    MonotonicTime ioHIDEventTimestamp;
    std::optional<WebCore::FloatSize> rawPlatformDelta;
    WebWheelEventMomentumEndType momentumEndType { WebWheelEventMomentumEndType::Unknown };
    WebEventInputSource inputSource { WebEventInputSource::UserDriven };
    float momentumFastScrollMultiplier { 1 };
#endif
};

struct WebWheelEventInit {
    WebEventData event;
    WebWheelEventData wheel;
};

class WebWheelEvent : public WebEvent {
    WTF_MAKE_TZONE_ALLOCATED(WebWheelEvent);
public:
    using Granularity = WebWheelEventGranularity;
    using Phase = WebEventPhase;
    using MomentumEndType = WebWheelEventMomentumEndType;

    static Ref<WebWheelEvent> create(WebEventData&&, WebWheelEventData&&);
    static Ref<WebWheelEvent> create(WebWheelEventInit&&);

    // Callers that mutate an event they did not create must copy first, since events are now shared
    // rather than copied by value.
    Ref<WebWheelEvent> copy() const;

    const WebCore::IntPoint position() const { return m_data.position; }
    void setPosition(WebCore::IntPoint position) { m_data.position = position; }
    const WebCore::IntPoint globalPosition() const { return m_data.globalPosition; }
    const WebCore::FloatSize delta() const { return m_data.delta; }
    const WebCore::FloatSize wheelTicks() const { return m_data.wheelTicks; }
    Granularity granularity() const { return m_data.granularity; }
#if PLATFORM(COCOA)
    bool directionInvertedFromDevice() const { return m_data.directionInvertedFromDevice; }
    MomentumEndType momentumEndType() const { return m_data.momentumEndType; }
#else
    // No constructor on this platform takes these, so the defaults are the only possible values.
    bool directionInvertedFromDevice() const { return false; }
    MomentumEndType momentumEndType() const { return MomentumEndType::Unknown; }
#endif
#if PLATFORM(COCOA) || PLATFORM(GTK) || USE(LIBWPE) || ENABLE(WPE_PLATFORM)
    Phase phase() const { return m_data.phase; }
    Phase momentumPhase() const { return m_data.momentumPhase; }
    bool hasPreciseScrollingDeltas() const { return m_data.hasPreciseScrollingDeltas; }
#else
    Phase phase() const { return Phase::None; }
    Phase momentumPhase() const { return Phase::None; }
#endif
#if PLATFORM(COCOA)
    MonotonicTime ioHIDEventTimestamp() const { return m_data.ioHIDEventTimestamp; }
    std::optional<WebCore::FloatSize> rawPlatformDelta() const { return m_data.rawPlatformDelta; }
    void setRawPlatformDelta(std::optional<WebCore::FloatSize>&& delta) { m_data.rawPlatformDelta = WTF::move(delta); }
    uint32_t scrollCount() const { return m_data.scrollCount; }
    const WebCore::FloatSize& unacceleratedScrollingDelta() const LIFETIME_BOUND { return m_data.unacceleratedScrollingDelta; }
    WebEventInputSource inputSource() const { return m_data.inputSource; }
    float momentumFastScrollMultiplier() const { return m_data.momentumFastScrollMultiplier; }
    void setMomentumFastScrollMultiplier(float multiplier) { m_data.momentumFastScrollMultiplier = multiplier; }
#endif // PLATFORM(COCOA)

    bool isMomentumEvent() const { return momentumPhase() != Phase::None && momentumPhase() != Phase::WillBegin; }

    const WebWheelEventData& wheelData() const LIFETIME_BOUND { return m_data; }

    static bool NODELETE isWheelEventType(WebEventType);

protected:
    WebWheelEvent(WebEventData&&, WebWheelEventData&&);

private:
    WebWheelEventData m_data;
};

WTF::TextStream& operator<<(WTF::TextStream&, WebWheelEvent::Granularity);
WTF::TextStream& operator<<(WTF::TextStream&, WebWheelEvent::Phase);
WTF::TextStream& operator<<(WTF::TextStream&, WebWheelEvent::MomentumEndType);
WTF::TextStream& operator<<(WTF::TextStream&, const WebWheelEvent&);

} // namespace WebKit

SPECIALIZE_TYPE_TRAITS_BEGIN(WebKit::WebWheelEvent)
static bool isType(const WebKit::WebEvent& event) { return WebKit::WebWheelEvent::isWheelEventType(event.type()); }
SPECIALIZE_TYPE_TRAITS_END()
