/*
 * Copyright (C) 2016 Apple Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "Region.h"
#include <wtf/Forward.h>
#include <wtf/HashMap.h>
#include <wtf/text/StringHash.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

enum class TrackingType : uint8_t {
    NotTracking = 0,
    Asynchronous = 1,
    Synchronous = 2
};

struct EventTrackingRegions {
    enum class Event : uint8_t {
        Mousedown,
        Mousemove,
        Mouseup,
        Mousewheel,
        Pointerdown,
        Pointerenter,
        Pointerleave,
        Pointermove,
        Pointerout,
        Pointerover,
        Pointerup,
        Touchend,
        Touchforcechange,
        Touchmove,
        Touchstart,
        Wheel,
    };

    WEBCORE_EXPORT static ASCIILiteral eventName(Event);

    // Region for which events can be dispatched without blocking scrolling.
    Region asynchronousDispatchRegion;

    // Regions for which events must be sent before performing the default behavior.
    // The key is the Event Name with an active handler.
    using EventSpecificSynchronousDispatchRegions = HashMap<Event, Region, WTF::IntHash<Event>, WTF::StrongEnumHashTraits<Event>>;
    EventSpecificSynchronousDispatchRegions eventSpecificSynchronousDispatchRegions;

    bool isEmpty() const;

    void translate(IntSize);
    void uniteSynchronousRegion(Event, const Region&);
    void unite(const EventTrackingRegions&);

    TrackingType trackingTypeForPoint(Event, const IntPoint&);
};

bool operator==(const EventTrackingRegions&, const EventTrackingRegions&);
inline bool operator!=(const EventTrackingRegions& a, const EventTrackingRegions& b) { return !(a == b); }

} // namespace WebCore

namespace WTF {

template<> struct EnumTraits<WebCore::EventTrackingRegions::Event> {
    using values = EnumValues<
        WebCore::EventTrackingRegions::Event,
        WebCore::EventTrackingRegions::Event::Mousedown,
        WebCore::EventTrackingRegions::Event::Mousemove,
        WebCore::EventTrackingRegions::Event::Mouseup,
        WebCore::EventTrackingRegions::Event::Mousewheel,
        WebCore::EventTrackingRegions::Event::Pointerdown,
        WebCore::EventTrackingRegions::Event::Pointerenter,
        WebCore::EventTrackingRegions::Event::Pointerleave,
        WebCore::EventTrackingRegions::Event::Pointermove,
        WebCore::EventTrackingRegions::Event::Pointerout,
        WebCore::EventTrackingRegions::Event::Pointerover,
        WebCore::EventTrackingRegions::Event::Pointerup,
        WebCore::EventTrackingRegions::Event::Touchend,
        WebCore::EventTrackingRegions::Event::Touchforcechange,
        WebCore::EventTrackingRegions::Event::Touchmove,
        WebCore::EventTrackingRegions::Event::Touchstart,
        WebCore::EventTrackingRegions::Event::Wheel
    >;
};

} // namespace WTF
