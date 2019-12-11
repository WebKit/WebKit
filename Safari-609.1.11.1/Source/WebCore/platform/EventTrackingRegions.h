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
    // Region for which events can be dispatched without blocking scrolling.
    Region asynchronousDispatchRegion;

    // Regions for which events must be sent before performing the default behavior.
    // The key is the Event Name with an active handler.
    HashMap<String, Region> eventSpecificSynchronousDispatchRegions;

    bool isEmpty() const;

    void translate(IntSize);
    void uniteSynchronousRegion(const String& eventName, const Region&);
    void unite(const EventTrackingRegions&);

    TrackingType trackingTypeForPoint(const String& eventName, const IntPoint&);
};

bool operator==(const EventTrackingRegions&, const EventTrackingRegions&);
inline bool operator!=(const EventTrackingRegions& a, const EventTrackingRegions& b) { return !(a == b); }

} // namespace WebCore
