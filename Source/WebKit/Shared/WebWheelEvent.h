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

namespace WebKit {

class WebWheelEvent : public WebEvent {
public:
    enum Granularity {
        ScrollByPageWheelEvent,
        ScrollByPixelWheelEvent
    };

    enum Phase {
        PhaseNone        = 0,
        PhaseBegan       = 1 << 0,
        PhaseStationary  = 1 << 1,
        PhaseChanged     = 1 << 2,
        PhaseEnded       = 1 << 3,
        PhaseCancelled   = 1 << 4,
        PhaseMayBegin    = 1 << 5,
    };

    WebWheelEvent() = default;

    WebWheelEvent(Type, const WebCore::IntPoint& position, const WebCore::IntPoint& globalPosition, const WebCore::FloatSize& delta, const WebCore::FloatSize& wheelTicks, Granularity, OptionSet<Modifier>, WallTime timestamp);
#if PLATFORM(COCOA)
    WebWheelEvent(Type, const WebCore::IntPoint& position, const WebCore::IntPoint& globalPosition, const WebCore::FloatSize& delta, const WebCore::FloatSize& wheelTicks, Granularity, bool directionInvertedFromDevice, Phase, Phase momentumPhase, bool hasPreciseScrollingDeltas, uint32_t scrollCount, const WebCore::FloatSize& unacceleratedScrollingDelta, OptionSet<Modifier>, WallTime timestamp);
#elif PLATFORM(GTK) || USE(LIBWPE)
    WebWheelEvent(Type, const WebCore::IntPoint& position, const WebCore::IntPoint& globalPosition, const WebCore::FloatSize& delta, const WebCore::FloatSize& wheelTicks, Phase, Phase momentumPhase, Granularity, bool hasPreciseScrollingDeltas, OptionSet<Modifier>, WallTime timestamp);
#endif

    const WebCore::IntPoint position() const { return m_position; }
    const WebCore::IntPoint globalPosition() const { return m_globalPosition; }
    const WebCore::FloatSize delta() const { return m_delta; }
    const WebCore::FloatSize wheelTicks() const { return m_wheelTicks; }
    Granularity granularity() const { return static_cast<Granularity>(m_granularity); }
    bool directionInvertedFromDevice() const { return m_directionInvertedFromDevice; }
    Phase phase() const { return static_cast<Phase>(m_phase); }
    Phase momentumPhase() const { return static_cast<Phase>(m_momentumPhase); }
#if PLATFORM(COCOA) || PLATFORM(GTK) || USE(LIBWPE)
    bool hasPreciseScrollingDeltas() const { return m_hasPreciseScrollingDeltas; }
#endif
#if PLATFORM(COCOA)
    uint32_t scrollCount() const { return m_scrollCount; }
    const WebCore::FloatSize& unacceleratedScrollingDelta() const { return m_unacceleratedScrollingDelta; }
#endif

    void encode(IPC::Encoder&) const;
    static WARN_UNUSED_RETURN bool decode(IPC::Decoder&, WebWheelEvent&);

private:
    static bool isWheelEventType(Type);

    WebCore::IntPoint m_position;
    WebCore::IntPoint m_globalPosition;
    WebCore::FloatSize m_delta;
    WebCore::FloatSize m_wheelTicks;
    uint32_t m_granularity { ScrollByPageWheelEvent };
    uint32_t m_phase { Phase::PhaseNone };
    uint32_t m_momentumPhase { Phase::PhaseNone };
    bool m_directionInvertedFromDevice { false };
#if PLATFORM(COCOA) || PLATFORM(GTK) || USE(LIBWPE)
    bool m_hasPreciseScrollingDeltas { false };
#endif
#if PLATFORM(COCOA)
    uint32_t m_scrollCount { 0 };
    WebCore::FloatSize m_unacceleratedScrollingDelta;
#endif
};

} // namespace WebKit
