/*
 * Copyright (C) 2008, 2009, 2010, 2012 Apple Inc. All Rights Reserved.
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

    [ _locationManager.get() setDelegate:self];
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
    _locationManager.get().delegate = nil;
    [super dealloc];
}

- (BOOL)handleExternalAuthorizationStatusChange:(CLAuthorizationStatus)newStatus
{
    CLAuthorizationStatus previousStatus = _lastAuthorizationStatus;
    _lastAuthorizationStatus = newStatus;
    if (!_isWaitingForAuthorization && previousStatus != newStatus) {
        [_locationManager.get() stopUpdatingLocation];
        [_positionListener resetGeolocation];
        return YES;
    }
    return NO;
}

- (void)start
{
    ASSERT_MAIN_THREAD();

#define CLLocationManager getCLLocationManagerClass()
    CLAuthorizationStatus authorizationStatus = [CLLocationManager authorizationStatus];
    if ([self handleExternalAuthorizationStatusChange:authorizationStatus])
        return;

    if (![CLLocationManager locationServicesEnabled]) {
        [_positionListener geolocationDelegateUnableToStart];
        return;
    }

    switch (authorizationStatus) {
    case kCLAuthorizationStatusNotDetermined:
        _isWaitingForAuthorization = YES;
        [_locationManager.get() startUpdatingLocation];
        return;
    case kCLAuthorizationStatusDenied:
    case kCLAuthorizationStatusRestricted:
        [_positionListener geolocationDelegateUnableToStart];
        return;
    case kCLAuthorizationStatusAuthorizedAlways:
    case kCLAuthorizationStatusAuthorizedWhenInUse:
        [_locationManager.get() startUpdatingLocation];
        [_positionListener geolocationDelegateStarted];
        return;
    }
#undef CLLocationManager
    ASSERT_NOT_REACHED();
    [_positionListener geolocationDelegateUnableToStart];
}

- (void)stop
{
    ASSERT_MAIN_THREAD();
    [_locationManager.get() stopUpdatingLocation];
}

- (void)locationManager:(CLLocationManager *)manager didChangeAuthorizationStatus:(CLAuthorizationStatus)status
{
    if ([self handleExternalAuthorizationStatusChange:status])
        return;

    if (_isWaitingForAuthorization) {
        switch (status) {
        case kCLAuthorizationStatusNotDetermined:
            // This can happen after resume if the user has still not answered the dialog. We just have to wait for the permission.
            break;
        case kCLAuthorizationStatusDenied:
        case kCLAuthorizationStatusRestricted:
            _isWaitingForAuthorization = NO;
            [_positionListener geolocationDelegateUnableToStart];
            break;
        case kCLAuthorizationStatusAuthorizedAlways:
        case kCLAuthorizationStatusAuthorizedWhenInUse:
            _isWaitingForAuthorization = NO;
            [_positionListener geolocationDelegateStarted];
            break;
        }
    }
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
    [_locationManager.get() setDesiredAccuracy:flag ? kCLLocationAccuracyBest : kCLLocationAccuracyHundredMeters];
}

@end

#endif // PLATFORM(IOS)
