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

TrackingType EventTrackingRegions::trackingTypeForPoint(const String& eventName, const IntPoint& point)
{
    auto synchronousRegionIterator = eventSpecificSynchronousDispatchRegions.find(eventName);
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

void EventTrackingRegions::uniteSynchronousRegion(const String& eventName, const Region& region)
{
    if (region.isEmpty())
        return;

    auto addResult = eventSpecificSynchronousDispatchRegions.add(eventName, region);
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
#if ENABLE(POINTER_EVENTS)
        && a.touchActionData == b.touchActionData
#endif
        && a.eventSpecificSynchronousDispatchRegions == b.eventSpecificSynchronousDispatchRegions;
}

#if ENABLE(POINTER_EVENTS)
bool operator==(const TouchActionData& a, const TouchActionData& b)
{
    return a.touchActions == b.touchActions
        && a.scrollingNodeID == b.scrollingNodeID
        && a.region == b.region;
}
#endif

} // namespace WebCore
