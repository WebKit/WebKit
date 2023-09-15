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

#include "config.h"
#include "EventTrackingRegions.h"

#include "EventNames.h"

namespace WebCore {

ASCIILiteral EventTrackingRegions::eventName(EventType eventType)
{
    switch (eventType) {
    case EventType::Mousedown:
        return "mousedown"_s;
    case EventType::Mousemove:
        return "mousemove"_s;
    case EventType::Mouseup:
        return "mouseup"_s;
    case EventType::Mousewheel:
        return "mousewheel"_s;
    case EventType::Pointerdown:
        return "pointerdown"_s;
    case EventType::Pointerenter:
        return "pointerenter"_s;
    case EventType::Pointerleave:
        return "pointerleave"_s;
    case EventType::Pointermove:
        return "pointermove"_s;
    case EventType::Pointerout:
        return "pointerout"_s;
    case EventType::Pointerover:
        return "pointerover"_s;
    case EventType::Pointerup:
        return "pointerup"_s;
    case EventType::Touchend:
        return "touchend"_s;
    case EventType::Touchforcechange:
        return "touchforcechange"_s;
    case EventType::Touchmove:
        return "touchmove"_s;
    case EventType::Touchstart:
        return "touchstart"_s;
    case EventType::Wheel:
        return "wheel"_s;
    }
    return ASCIILiteral();
}

const AtomString& EventTrackingRegions::eventNameAtomString(const EventNames& eventNames, EventType eventType)
{
    switch (eventType) {
    case EventType::Mousedown:
        return eventNames.mousedownEvent;
    case EventType::Mousemove:
        return eventNames.mousemoveEvent;
    case EventType::Mouseup:
        return eventNames.mouseupEvent;
    case EventType::Mousewheel:
        return eventNames.mousewheelEvent;
    case EventType::Pointerdown:
        return eventNames.pointerdownEvent;
    case EventType::Pointerenter:
        return eventNames.pointerenterEvent;
    case EventType::Pointerleave:
        return eventNames.pointerleaveEvent;
    case EventType::Pointermove:
        return eventNames.pointermoveEvent;
    case EventType::Pointerout:
        return eventNames.pointeroutEvent;
    case EventType::Pointerover:
        return eventNames.pointeroverEvent;
    case EventType::Pointerup:
        return eventNames.pointerupEvent;
    case EventType::Touchend:
        return eventNames.touchendEvent;
    case EventType::Touchforcechange:
        return eventNames.touchforcechangeEvent;
    case EventType::Touchmove:
        return eventNames.touchmoveEvent;
    case EventType::Touchstart:
        return eventNames.touchstartEvent;
    case EventType::Wheel:
        return eventNames.wheelEvent;
    }
    return nullAtom();
}

TrackingType EventTrackingRegions::trackingTypeForPoint(EventType eventType, const IntPoint& point)
{
    auto synchronousRegionIterator = eventSpecificSynchronousDispatchRegions.find(eventType);
    if (synchronousRegionIterator != eventSpecificSynchronousDispatchRegions.end()) {
        if (synchronousRegionIterator->value.contains(point))
            return TrackingType::Synchronous;
    }

    if (asynchronousDispatchRegion.contains(point))
        return TrackingType::Asynchronous;
    return TrackingType::NotTracking;
}

bool EventTrackingRegions::isEmpty() const
{
    return asynchronousDispatchRegion.isEmpty() && eventSpecificSynchronousDispatchRegions.isEmpty();
}

void EventTrackingRegions::translate(IntSize offset)
{
    asynchronousDispatchRegion.translate(offset);
    for (auto& slot : eventSpecificSynchronousDispatchRegions)
        slot.value.translate(offset);
}

void EventTrackingRegions::uniteSynchronousRegion(EventType eventType, const Region& region)
{
    if (region.isEmpty())
        return;

    auto addResult = eventSpecificSynchronousDispatchRegions.add(eventType, region);
    if (!addResult.isNewEntry)
        addResult.iterator->value.unite(region);
}

void EventTrackingRegions::unite(const EventTrackingRegions& eventTrackingRegions)
{
    asynchronousDispatchRegion.unite(eventTrackingRegions.asynchronousDispatchRegion);
    for (auto& slot : eventTrackingRegions.eventSpecificSynchronousDispatchRegions)
        uniteSynchronousRegion(slot.key, slot.value);
}

} // namespace WebCore
