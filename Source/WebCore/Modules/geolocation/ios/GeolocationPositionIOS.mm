/*
 * Copyright (C) 2017 Apple Inc. All Rights Reserved.
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

#import "config.h"
#import "GeolocationPosition.h"

#if PLATFORM(IOS_FAMILY)

#import <CoreLocation/CLLocation.h>

namespace WebCore {

GeolocationPosition::GeolocationPosition(CLLocation* location)
    : timestamp(location.timestamp.timeIntervalSince1970)
    , latitude(location.coordinate.latitude)
    , longitude(location.coordinate.longitude)
    , accuracy(location.horizontalAccuracy)
{
    if (location.verticalAccuracy >= 0.0) {
        altitude = location.altitude;
        altitudeAccuracy = location.verticalAccuracy;
    }
    if (location.speed >= 0.0)
        speed = location.speed;
    if (location.course >= 0.0)
        heading = location.course;
#if !PLATFORM(IOSMAC)
    if (location.floor)
        floorLevel = location.floor.level;
#endif
}

}

#endif // PLATFORM(IOS_FAMILY)
