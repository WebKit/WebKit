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
#include "RenderStyleConstants.h"
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

    void unite(const Region&, const RenderStyle&, bool overrideUserModifyIsEditable = false);
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

    void unite(const Region&, const RenderStyle&, bool overrideUserModifyIsEditable = false);
    void translate(const IntSize&);

    bool contains(const IntPoint& point) const { return m_region.contains(point); }
    bool contains(const IntRect& rect) const { return m_region.contains(rect); }
    bool intersects(const IntRect& rect) const { return m_region.intersects(rect); }

    const Region& region() const { return m_region; }

#if ENABLE(TOUCH_ACTION_REGIONS)
    bool hasTouchActions() const { return !m_touchActionRegions.isEmpty(); }
    WEBCORE_EXPORT OptionSet<TouchAction> touchActionsForPoint(const IntPoint&) const;
    const Region* regionForTouchAction(TouchAction) const;
#endif

#if ENABLE(WHEEL_EVENT_REGIONS)
    OptionSet<EventListenerRegionType> eventListenerRegionTypesForPoint(const IntPoint&) const;
    const Region& eventListenerRegionForType(EventListenerRegionType) const;
#endif

#if ENABLE(EDITABLE_REGION)
    void ensureEditableRegion();
    bool hasEditableRegion() const { return m_editableRegion.hasValue(); }
    WEBCORE_EXPORT bool containsEditableElementsInRect(const IntRect&) const;
    Vector<IntRect, 1> rectsForEditableElements() const { return m_editableRegion ? m_editableRegion->rects() : Vector<IntRect, 1> { }; }
#endif

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static Optional<EventRegion> decode(Decoder&);
    // FIXME: Remove legacy decode.
    template<class Decoder> static WARN_UNUSED_RETURN bool decode(Decoder&, EventRegion&);

    void dump(TextStream&) const;

private:
#if ENABLE(TOUCH_ACTION_REGIONS)
    void uniteTouchActions(const Region&, OptionSet<TouchAction>);
#endif
    void uniteEventListeners(const Region&, OptionSet<EventListenerRegionType>);

    Region m_region;
#if ENABLE(TOUCH_ACTION_REGIONS)
    Vector<Region> m_touchActionRegions;
#endif
#if ENABLE(WHEEL_EVENT_REGIONS)
    Region m_wheelEventListenerRegion;
    Region m_nonPassiveWheelEventListenerRegion;
#endif
#if ENABLE(EDITABLE_REGION)
    Optional<Region> m_editableRegion;
#endif
};

WEBCORE_EXPORT TextStream& operator<<(TextStream&, const EventRegion&);

template<class Encoder>
void EventRegion::encode(Encoder& encoder) const
{
    encoder << m_region;
#if ENABLE(TOUCH_ACTION_REGIONS)
    encoder << m_touchActionRegions;
#endif
#if ENABLE(EDITABLE_REGION)
    encoder << m_editableRegion;
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

#if ENABLE(TOUCH_ACTION_REGIONS)
    Optional<Vector<Region>> touchActionRegions;
    decoder >> touchActionRegions;
    if (!touchActionRegions)
        return WTF::nullopt;

    eventRegion.m_touchActionRegions = WTFMove(*touchActionRegions);
#endif

#if ENABLE(EDITABLE_REGION)
    Optional<Optional<Region>> editableRegion;
    decoder >> editableRegion;
    if (!editableRegion)
        return WTF::nullopt;
    eventRegion.m_editableRegion = WTFMove(*editableRegion);
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

#if ENABLE(EDITABLE_REGION)

inline void EventRegion::ensureEditableRegion()
{
    if (!m_editableRegion)
        m_editableRegion.emplace();
}

#endif

}
