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

namespace WebCore {

ASCIILiteral EventTrackingRegions::eventName(Event event)
{
    switch (event) {
    case Event::Mousedown:
        return "mousedown"_s;
    case Event::Mousemove:
        return "mousemove"_s;
    case Event::Mouseup:
        return "mouseup"_s;
    case Event::Mousewheel:
        return "mousewheel"_s;
    case Event::Pointerdown:
        return "pointerdown"_s;
    case Event::Pointerenter:
        return "pointerenter"_s;
    case Event::Pointerleave:
        return "pointerleave"_s;
    case Event::Pointermove:
        return "pointermove"_s;
    case Event::Pointerout:
        return "pointerout"_s;
    case Event::Pointerover:
        return "pointerover"_s;
    case Event::Pointerup:
        return "pointerup"_s;
    case Event::Touchend:
        return "touchend"_s;
    case Event::Touchforcechange:
        return "touchforcechange"_s;
    case Event::Touchmove:
        return "touchmove"_s;
    case Event::Touchstart:
        return "touchstart"_s;
    case Event::Wheel:
        return "wheel"_s;
    }
    return ASCIILiteral();
}

TrackingType EventTrackingRegions::trackingTypeForPoint(Event event, const IntPoint& point)
{
    auto synchronousRegionIterator = eventSpecificSynchronousDispatchRegions.find(event);
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

void EventTrackingRegions::uniteSynchronousRegion(Event event, const Region& region)
{
    if (region.isEmpty())
        return;

    auto addResult = eventSpecificSynchronousDispatchRegions.add(event, region);
    if (!addResult.isNewEntry)
        addResult.iterator->value.unite(region);
}

void EventTrackingRegions::unite(const EventTrackingRegions& eventTrackingRegions)
{
    asynchronousDispatchRegion.unite(eventTrackingRegions.asynchronousDispatchRegion);
    for (auto& slot : eventTrackingRegions.eventSpecificSynchronousDispatchRegions)
        uniteSynchronousRegion(slot.key, slot.value);
}

bool operator==(const EventTrackingRegions& a, const EventTrackingRegions& b)
{
    return a.asynchronousDispatchRegion == b.asynchronousDispatchRegion
        && a.eventSpecificSynchronousDispatchRegions == b.eventSpecificSynchronousDispatchRegions;
}

} // namespace WebCore
