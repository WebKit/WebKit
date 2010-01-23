/*
 * Copyright (C) 2009 Apple Inc. All Rights Reserved.
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

#if ENABLE(GEOLOCATION) && !ENABLE(CLIENT_BASED_GEOLOCATION)

#import "GeolocationServiceMac.h"

#import "Geoposition.h"
#import "PositionError.h"
#import "PositionOptions.h"
#import "SoftLinking.h"
#import <CoreLocation/CoreLocation.h>
#import <objc/objc-runtime.h>
#import <wtf/RefPtr.h>
#import <wtf/UnusedParam.h>

SOFT_LINK_FRAMEWORK(CoreLocation)

SOFT_LINK_CLASS(CoreLocation, CLLocationManager)
SOFT_LINK_CLASS(CoreLocation, CLLocation)

SOFT_LINK_CONSTANT(CoreLocation, kCLLocationAccuracyBest, double)
SOFT_LINK_CONSTANT(CoreLocation, kCLLocationAccuracyHundredMeters, double)

#define kCLLocationAccuracyBest getkCLLocationAccuracyBest()
#define kCLLocationAccuracyHundredMeters getkCLLocationAccuracyHundredMeters()

using namespace WebCore;

@interface WebCoreCoreLocationObserver : NSObject<CLLocationManagerDelegate>
{
    GeolocationServiceMac* m_callback;
}

- (id)initWithCallback:(GeolocationServiceMac*)callback;

- (void)locationManager:(CLLocationManager *)manager didUpdateToLocation:(CLLocation *)newLocation fromLocation:(CLLocation *)oldLocation;
- (void)locationManager:(CLLocationManager *)manager didFailWithError:(NSError *)error;

@end

namespace WebCore {

GeolocationService* GeolocationServiceMac::create(GeolocationServiceClient* client)
{
    return new GeolocationServiceMac(client);
}

GeolocationService::FactoryFunction* GeolocationService::s_factoryFunction = &GeolocationServiceMac::create;

GeolocationServiceMac::GeolocationServiceMac(GeolocationServiceClient* client)
    : GeolocationService(client)
    , m_objcObserver(AdoptNS, [[WebCoreCoreLocationObserver alloc] initWithCallback:this])
{
}

GeolocationServiceMac::~GeolocationServiceMac()
{
    [m_locationManager.get() stopUpdatingLocation];
    m_locationManager.get().delegate = nil;
}

bool GeolocationServiceMac::startUpdating(PositionOptions* options)
{
    #define CLLocationManager getCLLocationManagerClass()
    if (!m_locationManager.get()) {
        m_locationManager.adoptNS([[CLLocationManager alloc] init]);
        m_locationManager.get().delegate = m_objcObserver.get();
    }

    if (!m_locationManager.get().locationServicesEnabled)
        return false;

    if (options) {
        // CLLocationAccuracy values suggested by Ron Huang.
        CLLocationAccuracy accuracy = options->enableHighAccuracy() ? kCLLocationAccuracyBest : kCLLocationAccuracyHundredMeters;
        m_locationManager.get().desiredAccuracy = accuracy;
    }
    
    // This can safely be called multiple times.
    [m_locationManager.get() startUpdatingLocation];
    
    return true;
    #undef CLLocationManager
}

void GeolocationServiceMac::stopUpdating()
{
    [m_locationManager.get() stopUpdatingLocation];
}

void GeolocationServiceMac::suspend()
{
    [m_locationManager.get() stopUpdatingLocation];
}

void GeolocationServiceMac::resume()
{
    [m_locationManager.get() startUpdatingLocation];
}

void GeolocationServiceMac::positionChanged(PassRefPtr<Geoposition> position)
{
    m_lastPosition = position;
    GeolocationService::positionChanged();
}
    
void GeolocationServiceMac::errorOccurred(PassRefPtr<PositionError> error)
{
    m_lastError = error;
    GeolocationService::errorOccurred();
}

} // namespace WebCore

@implementation WebCoreCoreLocationObserver

- (id)initWithCallback:(GeolocationServiceMac *)callback
{
    self = [super init];
    if (self)
        m_callback = callback;
    return self;
}

- (void)locationManager:(CLLocationManager *)manager didUpdateToLocation:(CLLocation *)newLocation fromLocation:(CLLocation *)oldLocation
{
    ASSERT(m_callback);
    ASSERT(newLocation);
    UNUSED_PARAM(manager);
    UNUSED_PARAM(oldLocation);

    // Normalize
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
    
    WTF::RefPtr<WebCore::Coordinates> newCoordinates = WebCore::Coordinates::create(
                            newLocation.coordinate.latitude,
                            newLocation.coordinate.longitude,
                            canProvideAltitude,
                            altitude,
                            newLocation.horizontalAccuracy,
                            canProvideAltitudeAccuracy,
                            altitudeAccuracy,
                            canProvideHeading,
                            heading,
                            canProvideSpeed,
                            speed);
    WTF::RefPtr<WebCore::Geoposition> newPosition = WebCore::Geoposition::create(
                             newCoordinates.release(),
                             [newLocation.timestamp timeIntervalSince1970] * 1000.0); // seconds -> milliseconds
    
    m_callback->positionChanged(newPosition.release());
}

- (void)locationManager:(CLLocationManager *)manager didFailWithError:(NSError *)error
{
    ASSERT(m_callback);
    ASSERT(error);
 
    UNUSED_PARAM(manager);

    PositionError::ErrorCode code;
    switch ([error code]) {
        case kCLErrorDenied:
            code = PositionError::PERMISSION_DENIED;
            break;
        case kCLErrorLocationUnknown:
            code = PositionError::POSITION_UNAVAILABLE;
            break;
        default:
            code = PositionError::POSITION_UNAVAILABLE;
            break;
    }

    m_callback->errorOccurred(PositionError::create(code, [error localizedDescription]));
}

@end

#endif // ENABLE(GEOLOCATION)
