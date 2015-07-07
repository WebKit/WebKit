/*
 * Copyright (C) 2008, 2009, 2010, 2012, 2014 Apple Inc. All Rights Reserved.
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

#if PLATFORM(IOS)

#import "WebGeolocationCoreLocationProvider.h"

#import <CoreLocation/CLLocation.h>
#import <CoreLocation/CLLocationManagerDelegate.h>
#import <CoreLocation/CoreLocation.h>
#import <CoreLocation/CoreLocationPriv.h>
#import <WebCore/GeolocationPosition.h>
#import <WebCore/SoftLinking.h>
#import <WebKitLogging.h>
#import <objc/objc-runtime.h>
#import <wtf/RefPtr.h>
#import <wtf/RetainPtr.h>

SOFT_LINK_FRAMEWORK(CoreLocation)

SOFT_LINK_CLASS(CoreLocation, CLLocationManager)
SOFT_LINK_CLASS(CoreLocation, CLLocation)

SOFT_LINK_CONSTANT(CoreLocation, kCLLocationAccuracyBest, double)
SOFT_LINK_CONSTANT(CoreLocation, kCLLocationAccuracyHundredMeters, double)

#define kCLLocationAccuracyBest getkCLLocationAccuracyBest()
#define kCLLocationAccuracyHundredMeters getkCLLocationAccuracyHundredMeters()

using namespace WebCore;

@interface WebGeolocationCoreLocationProvider () <CLLocationManagerDelegate>
@end

@implementation WebGeolocationCoreLocationProvider
{
    id<WebGeolocationCoreLocationUpdateListener> _positionListener;
    RetainPtr<CLLocationManager> _locationManager;
    BOOL _isWaitingForAuthorization;
    CLAuthorizationStatus _lastAuthorizationStatus;
}

- (void)createLocationManager
{
    ASSERT(!_locationManager);

#define CLLocationManager getCLLocationManagerClass()
    _locationManager = adoptNS([[CLLocationManager alloc] init]);
    _lastAuthorizationStatus = [CLLocationManager authorizationStatus];
#undef CLLocationManager

    [ _locationManager setDelegate:self];
}

- (id)initWithListener:(id<WebGeolocationCoreLocationUpdateListener>)listener
{
    ASSERT_MAIN_THREAD();
    self = [super init];
    if (self) {
        _positionListener = listener;
        [self createLocationManager];
    }
    return self;
}

- (void)dealloc
{
    ASSERT_MAIN_THREAD();
    [_locationManager setDelegate:nil];
    [super dealloc];
}

- (void)requestGeolocationAuthorization
{
    ASSERT_MAIN_THREAD();

    if (![getCLLocationManagerClass() locationServicesEnabled]) {
        [_positionListener geolocationAuthorizationDenied];
        return;
    }

    switch ([getCLLocationManagerClass() authorizationStatus]) {
    case kCLAuthorizationStatusNotDetermined: {
        if (!_isWaitingForAuthorization) {
            _isWaitingForAuthorization = YES;
            [_locationManager requestWhenInUseAuthorization];
        }
        break;
    }
    case kCLAuthorizationStatusAuthorizedAlways:
    case kCLAuthorizationStatusAuthorizedWhenInUse: {
        [_positionListener geolocationAuthorizationGranted];
        break;
    }
    case kCLAuthorizationStatusRestricted:
    case kCLAuthorizationStatusDenied:
        [_positionListener geolocationAuthorizationDenied];
        break;
    }
}

static bool isAuthorizationGranted(CLAuthorizationStatus authorizationStatus)
{
    return authorizationStatus == kCLAuthorizationStatusAuthorizedAlways || authorizationStatus == kCLAuthorizationStatusAuthorizedWhenInUse;
}

- (void)start
{
    ASSERT_MAIN_THREAD();

    if (![getCLLocationManagerClass() locationServicesEnabled]
        || !isAuthorizationGranted([getCLLocationManagerClass() authorizationStatus])) {
        [_locationManager stopUpdatingLocation];
        [_positionListener resetGeolocation];
        return;
    }

    [_locationManager startUpdatingLocation];
}

- (void)stop
{
    ASSERT_MAIN_THREAD();
    [_locationManager stopUpdatingLocation];
}

- (void)locationManager:(CLLocationManager *)manager didChangeAuthorizationStatus:(CLAuthorizationStatus)status
{
    if (_isWaitingForAuthorization) {
        switch (status) {
        case kCLAuthorizationStatusNotDetermined:
            // This can happen after resume if the user has still not answered the dialog. We just have to wait for the permission.
            break;
        case kCLAuthorizationStatusDenied:
        case kCLAuthorizationStatusRestricted:
            _isWaitingForAuthorization = NO;
            [_positionListener geolocationAuthorizationDenied];
            break;
        case kCLAuthorizationStatusAuthorizedAlways:
        case kCLAuthorizationStatusAuthorizedWhenInUse:
            _isWaitingForAuthorization = NO;
            [_positionListener geolocationAuthorizationGranted];
            break;
        }
    } else {
        if (!(isAuthorizationGranted(_lastAuthorizationStatus) && isAuthorizationGranted(status))) {
            [_locationManager stopUpdatingLocation];
            [_positionListener resetGeolocation];
        }
    }
    _lastAuthorizationStatus = status;
}

- (void)sendLocation:(CLLocation *)newLocation
{
    // Normalize.
    bool canProvideAltitude = true;
    bool canProvideAltitudeAccuracy = true;
    double altitude = newLocation.altitude;
    double altitudeAccuracy = newLocation.verticalAccuracy;
    if (altitudeAccuracy < 0.0) {
        canProvideAltitude = false;
        canProvideAltitudeAccuracy = false;
    }

    bool canProvideSpeed = true;
    double speed = newLocation.speed;
    if (speed < 0.0)
        canProvideSpeed = false;

    bool canProvideHeading = true;
    double heading = newLocation.course;
    if (heading < 0.0)
        canProvideHeading = false;

    double timestamp = [newLocation.timestamp timeIntervalSince1970];
    RefPtr<GeolocationPosition> geolocationPosition = GeolocationPosition::create(timestamp, newLocation.coordinate.latitude, newLocation.coordinate.longitude, newLocation.horizontalAccuracy, canProvideAltitude, altitude, canProvideAltitudeAccuracy, altitudeAccuracy, canProvideHeading, heading, canProvideSpeed, speed);
    [_positionListener positionChanged:geolocationPosition.get()];
}

- (void)locationManager:(CLLocationManager *)manager didUpdateLocations:(NSArray *)locations
{
    UNUSED_PARAM(manager);
    for (CLLocation *location in locations)
        [self sendLocation:location];
}

- (void)locationManager:(CLLocationManager *)manager didFailWithError:(NSError *)error
{
    ASSERT(_positionListener);
    ASSERT(error);
    UNUSED_PARAM(manager);

    if ([error code] == kCLErrorDenied) {
        // Ignore the error here and let locationManager:didChangeAuthorizationStatus: handle the permission.
        return;
    }

    NSString *errorMessage = [error localizedDescription];
    [_positionListener errorOccurred:errorMessage];
}

- (void)setEnableHighAccuracy:(BOOL)flag
{
    ASSERT_MAIN_THREAD();
    [_locationManager setDesiredAccuracy:flag ? kCLLocationAccuracyBest : kCLLocationAccuracyHundredMeters];
}

@end

#endif // PLATFORM(IOS)
