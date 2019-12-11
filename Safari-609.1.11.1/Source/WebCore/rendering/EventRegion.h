/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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

#include "AffineTransform.h"
#include "Region.h"
#include "TouchAction.h"
#include <wtf/OptionSet.h>
#include <wtf/Vector.h>

namespace WebCore {

class EventRegion;
class RenderStyle;

class EventRegionContext {
public:
    explicit EventRegionContext(EventRegion&);

    void pushTransform(const AffineTransform&);
    void popTransform();

    void pushClip(const IntRect&);
    void popClip();

    void unite(const Region&, const RenderStyle&);
    bool contains(const IntRect&) const;

private:
    EventRegion& m_eventRegion;
    Vector<AffineTransform> m_transformStack;
    Vector<IntRect> m_clipStack;
};

class EventRegion {
public:
    WEBCORE_EXPORT EventRegion();

    EventRegionContext makeContext() { return EventRegionContext(*this); }

    bool isEmpty() const { return m_region.isEmpty(); }

    WEBCORE_EXPORT bool operator==(const EventRegion&) const;

    WEBCORE_EXPORT void unite(const Region&, const RenderStyle&);
    WEBCORE_EXPORT void translate(const IntSize&);

    bool contains(const IntPoint& point) const { return m_region.contains(point); }
    bool contains(const IntRect& rect) const { return m_region.contains(rect); }

    const Region& region() const { return m_region; }

#if ENABLE(POINTER_EVENTS)
    bool hasTouchActions() const { return !m_touchActionRegions.isEmpty(); }
    WEBCORE_EXPORT OptionSet<TouchAction> touchActionsForPoint(const IntPoint&) const;

    const Region* regionForTouchAction(TouchAction) const;
#endif

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static Optional<EventRegion> decode(Decoder&);
    // FIXME: Remove legacy decode.
    template<class Decoder> static bool decode(Decoder&, EventRegion&);

private:
#if ENABLE(POINTER_EVENTS)
    void uniteTouchActions(const Region&, OptionSet<TouchAction>);
#endif
    friend TextStream& operator<<(TextStream&, const EventRegion&);

    Region m_region;
#if ENABLE(POINTER_EVENTS)
    Vector<Region> m_touchActionRegions;
#endif
};

TextStream& operator<<(TextStream&, const EventRegion&);

template<class Encoder>
void EventRegion::encode(Encoder& encoder) const
{
    encoder << m_region;
#if ENABLE(POINTER_EVENTS)
    encoder << m_touchActionRegions;
#endif
}

template<class Decoder>
Optional<EventRegion> EventRegion::decode(Decoder& decoder)
{
    Optional<Region> region;
    decoder >> region;
    if (!region)
        return WTF::nullopt;

    EventRegion eventRegion;
    eventRegion.m_region = WTFMove(*region);

#if ENABLE(POINTER_EVENTS)
    Optional<Vector<Region>> touchActionRegions;
    decoder >> touchActionRegions;
    if (!touchActionRegions)
        return WTF::nullopt;

    eventRegion.m_touchActionRegions = WTFMove(*touchActionRegions);
#endif

    return eventRegion;
}

template<class Decoder>
bool EventRegion::decode(Decoder& decoder, EventRegion& eventRegion)
{
    Optional<EventRegion> decodedEventRegion;
    decoder >> decodedEventRegion;
    if (!decodedEventRegion)
        return false;
    eventRegion = WTFMove(*decodedEventRegion);
    return true;
}

}
