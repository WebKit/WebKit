/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#import "config.h"
#import "_WKGeolocationPositionInternal.h"

#if WK_API_ENABLED && TARGET_OS_IPHONE

#import <CoreLocation/CLLocation.h>

using namespace WebKit;

@implementation _WKGeolocationPosition

+ (instancetype)positionWithLocation:(CLLocation *)location
{
    if (!location)
        return nil;

    bool canProvideAltitude = true;
    bool canProvideAltitudeAccuracy = true;
    double altitude = location.altitude;
    double altitudeAccuracy = location.verticalAccuracy;
    if (altitudeAccuracy < 0.0) {
        canProvideAltitude = false;
        canProvideAltitudeAccuracy = false;
    }

    bool canProvideSpeed = true;
    double speed = location.speed;
    if (speed < 0.0)
        canProvideSpeed = false;

    bool canProvideHeading = true;
    double heading = location.course;
    if (heading < 0.0)
        canProvideHeading = false;

    CLLocationCoordinate2D coordinate = location.coordinate;
    double timestamp = location.timestamp.timeIntervalSince1970;

    return [wrapper(WebGeolocationPosition::create(timestamp, coordinate.latitude, coordinate.longitude, location.horizontalAccuracy, canProvideAltitude, altitude, canProvideAltitudeAccuracy, altitudeAccuracy, canProvideHeading, heading, canProvideSpeed, speed).leakRef()) autorelease];
}

- (void)dealloc
{
    _geolocationPosition->~WebGeolocationPosition();

    [super dealloc];
}

#pragma mark WKObject protocol implementation

- (API::Object&)_apiObject
{
    return *_geolocationPosition;
}

@end

#endif // WK_API_ENABLED && TARGET_OS_IPHONE
