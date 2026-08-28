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
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/TextStream.h>

namespace WebKit {

using namespace WebCore;

WTF_MAKE_TZONE_ALLOCATED_IMPL(WebWheelEvent);

Ref<WebWheelEvent> WebWheelEvent::create(WebEventData&& eventData, WebWheelEventData&& wheelData)
{
    return adoptRef(*new WebWheelEvent(WTF::move(eventData), WTF::move(wheelData)));
}

Ref<WebWheelEvent> WebWheelEvent::create(WebWheelEventInit&& init)
{
    return create(WTF::move(init.event), WTF::move(init.wheel));
}

Ref<WebWheelEvent> WebWheelEvent::copy() const
{
    return create(WebEventData { eventData() }, WebWheelEventData { m_data });
}

WebWheelEvent::WebWheelEvent(WebEventData&& eventData, WebWheelEventData&& wheelData)
    : WebEvent(WTF::move(eventData))
    , m_data(WTF::move(wheelData))
{
    ASSERT(isWheelEventType(type()));
}

bool WebWheelEvent::isWheelEventType(WebEventType type)
{
    return type == WebEventType::Wheel;
}

WTF::TextStream& operator<<(WTF::TextStream& ts, WebWheelEvent::Granularity granularity)
{
    using enum WebWheelEvent::Granularity;
    switch (granularity) {
    case ScrollByPageWheelEvent: ts << "scrollByPageWheelEvent"; break;
    case ScrollByPixelWheelEvent: ts << "scrollByPixelWheelEvent"; break;
    }
    return ts;
}

WTF::TextStream& operator<<(WTF::TextStream& ts, WebWheelEvent::Phase phase)
{
    using enum WebWheelEvent::Phase;
    switch (phase) {
    case None: ts << "none"; break;
    case Began: ts << "began"; break;
    case Stationary: ts << "stationary"; break;
    case Changed: ts << "changed"; break;
    case Ended: ts << "ended"; break;
    case Cancelled: ts << "cancelled"; break;
    case MayBegin: ts << "mayBegin"; break;
    case WillBegin: ts << "willBegin"; break;
    }
    return ts;
}

WTF::TextStream& operator<<(WTF::TextStream& ts, WebWheelEvent::MomentumEndType endType)
{
    using enum WebWheelEvent::MomentumEndType;
    switch (endType) {
    case Unknown: ts << "unknown"; break;
    case Interrupted: ts << "interrupted"; break;
    case Natural: ts << "natural"; break;
    }
    return ts;
}

WTF::TextStream& operator<<(WTF::TextStream& ts, const WebWheelEvent& event)
{
    TextStream::GroupScope group(ts);
    ts.dumpProperty("position"_s, event.position());
    ts.dumpProperty("globalPosition"_s, event.globalPosition());
    ts.dumpProperty("delta"_s, event.delta());
    ts.dumpProperty("wheelTicks"_s, event.wheelTicks());
    ts.dumpProperty("granularity"_s, event.granularity());
    ts.dumpProperty("directionInvertedFromDevice"_s, event.directionInvertedFromDevice());
    ts.dumpProperty("phase"_s, event.phase());
    ts.dumpProperty("momentumPhase"_s, event.momentumPhase());
    ts.dumpProperty("momentumEndType"_s, event.momentumEndType());
#if PLATFORM(COCOA) || PLATFORM(GTK) || USE(LIBWPE) || ENABLE(WPE_PLATFORM)
    ts.dumpProperty("hasPreciseScrollingDeltas"_s, event.hasPreciseScrollingDeltas());
#endif
#if PLATFORM(COCOA)
    ts.dumpProperty("ioHIDEventTimestamp"_s, event.ioHIDEventTimestamp().secondsSinceEpoch().value());
    ts.dumpProperty("rawPlatformDelta"_s, event.rawPlatformDelta());
    ts.dumpProperty("scrollCount"_s, event.scrollCount());
    ts.dumpProperty("unacceleratedScrollingDelta"_s, event.unacceleratedScrollingDelta());
    ts.dumpProperty("inputSource"_s, event.inputSource() == WebEventInputSource::Automation ? "automation"_s : "userDriven"_s);
#endif
    return ts;
}

} // namespace WebKit
