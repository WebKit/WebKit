/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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

#if HAVE(CORE_LOCATION)
#import "CoreLocationGeolocationProvider.h"

#import "GeolocationPositionData.h"
#import "RegistrableDomain.h"
#import <CoreLocation/CLLocationManagerDelegate.h>
#import <CoreLocation/CoreLocation.h>
#import <wtf/SoftLinking.h>

#if USE(APPLE_INTERNAL_SDK)
#import <CoreLocation/CLLocationManager_Private.h>
#endif

SOFT_LINK_FRAMEWORK(CoreLocation)

SOFT_LINK_CLASS(CoreLocation, CLLocationManager)
SOFT_LINK_CLASS(CoreLocation, CLLocation)

SOFT_LINK_CONSTANT(CoreLocation, kCLLocationAccuracyBest, double)
SOFT_LINK_CONSTANT(CoreLocation, kCLLocationAccuracyHundredMeters, double)

#define kCLLocationAccuracyBest getkCLLocationAccuracyBest()
#define kCLLocationAccuracyHundredMeters getkCLLocationAccuracyHundredMeters()

static bool isAuthorizationGranted(CLAuthorizationStatus status)
{
    return status == kCLAuthorizationStatusAuthorizedAlways
#if HAVE(CORE_LOCATION_AUTHORIZED_WHEN_IN_USE)
        || status == kCLAuthorizationStatusAuthorizedWhenInUse
#endif
        ;
}

@interface WebCLLocationManager : NSObject<CLLocationManagerDelegate>
@end

@implementation WebCLLocationManager {
    RetainPtr<CLLocationManager> _locationManager;
    WebCore::CoreLocationGeolocationProvider::Client* _client;
    String _websiteIdentifier;
    BOOL _isWaitingForAuthorization;
}

- (instancetype)initWithWebsiteIdentifier:(const String&)websiteIdentifier client:(WebCore::CoreLocationGeolocationProvider::Client&)client
{
    self = [super init];
    if (!self)
        return nil;

    // FIXME: Call initWithWebsiteIdentifier and pass the websiteIdentifier when HAVE(CORE_LOCATION_WEBSITE_IDENTIFIERS) and !websiteIdentifier.isEmpty() once <rdar://88834301> is fixed.
    UNUSED_PARAM(websiteIdentifier);
    _locationManager = adoptNS([allocCLLocationManagerInstance() init]);
    _client = &client;
    _websiteIdentifier = websiteIdentifier;
    [_locationManager setDelegate:self];
    return self;
}

- (void)dealloc
{
    [_locationManager setDelegate:nil];
    [super dealloc];
}

- (void)start
{
    if (![getCLLocationManagerClass() locationServicesEnabled] || !isAuthorizationGranted([_locationManager authorizationStatus])) {
        [_locationManager stopUpdatingLocation];
        _client->resetGeolocation(_websiteIdentifier);
        return;
    }

    [_locationManager startUpdatingLocation];
}

- (void)stop
{
    [_locationManager stopUpdatingLocation];
}

- (void)setEnableHighAccuracy:(BOOL)highAccuracyEnabled
{
    [_locationManager setDesiredAccuracy:highAccuracyEnabled ? kCLLocationAccuracyBest : kCLLocationAccuracyHundredMeters];
}

- (void)requestGeolocationAuthorization
{
    if (![getCLLocationManagerClass() locationServicesEnabled]) {
        _client->geolocationAuthorizationDenied(_websiteIdentifier);
        return;
    }

    switch ([_locationManager authorizationStatus]) {
    case kCLAuthorizationStatusNotDetermined: {
        if (!_isWaitingForAuthorization) {
            _isWaitingForAuthorization = YES;
            [_locationManager requestWhenInUseAuthorization];
        }
        break;
    }
    case kCLAuthorizationStatusAuthorizedAlways:
#if HAVE(CORE_LOCATION_AUTHORIZED_WHEN_IN_USE)
    case kCLAuthorizationStatusAuthorizedWhenInUse:
#endif
        _client->geolocationAuthorizationGranted(_websiteIdentifier);
        break;
    case kCLAuthorizationStatusRestricted:
    case kCLAuthorizationStatusDenied:
        _client->geolocationAuthorizationDenied(_websiteIdentifier);
        break;
    }
}

- (void)locationManagerDidChangeAuthorization:(CLLocationManager *)manager
{
    auto status = [_locationManager authorizationStatus];
    if (_isWaitingForAuthorization) {
        switch (status) {
        case kCLAuthorizationStatusNotDetermined:
            // This can happen after resume if the user has still not answered the dialog. We just have to wait for the permission.
            break;
        case kCLAuthorizationStatusDenied:
        case kCLAuthorizationStatusRestricted:
            _isWaitingForAuthorization = NO;
            _client->geolocationAuthorizationDenied(_websiteIdentifier);
            break;
        case kCLAuthorizationStatusAuthorizedAlways:
#if HAVE(CORE_LOCATION_AUTHORIZED_WHEN_IN_USE)
        case kCLAuthorizationStatusAuthorizedWhenInUse:
#endif
            _isWaitingForAuthorization = NO;
            _client->geolocationAuthorizationGranted(_websiteIdentifier);
            break;
        }
    } else {
        if (status == kCLAuthorizationStatusDenied || status == kCLAuthorizationStatusRestricted) {
            [_locationManager stopUpdatingLocation];
            _client->resetGeolocation(_websiteIdentifier);
        }
    }
}

- (void)locationManager:(CLLocationManager *)manager didUpdateLocations:(NSArray *)locations
{
    UNUSED_PARAM(manager);
    for (CLLocation *location in locations)
        _client->positionChanged(_websiteIdentifier, WebCore::GeolocationPositionData { location });
}

- (void)locationManager:(CLLocationManager *)manager didFailWithError:(NSError *)error
{
    ASSERT(error);
    UNUSED_PARAM(manager);

    if ([error code] == kCLErrorDenied) {
        // Ignore the error here and let locationManager:didChangeAuthorizationStatus: handle the permission.
        return;
    }

    NSString *errorMessage = [error localizedDescription];
    _client->errorOccurred(_websiteIdentifier, errorMessage);
}

@end

namespace WebCore {

CoreLocationGeolocationProvider::CoreLocationGeolocationProvider(const RegistrableDomain& registrableDomain, Client& client)
    : m_locationManager(adoptNS([[WebCLLocationManager alloc] initWithWebsiteIdentifier:registrableDomain.string() client:client]))
{
    // Request permission for the app to use geolocation (if it doesn't already).
    [m_locationManager requestGeolocationAuthorization];
}

CoreLocationGeolocationProvider::~CoreLocationGeolocationProvider()
{
    [m_locationManager stop];
}

void CoreLocationGeolocationProvider::start()
{
    [m_locationManager start];
}

void CoreLocationGeolocationProvider::stop()
{
    [m_locationManager stop];
}

void CoreLocationGeolocationProvider::setEnableHighAccuracy(bool highAccuracyEnabled)
{
    [m_locationManager setEnableHighAccuracy:highAccuracyEnabled];
}

class AuthorizationChecker final : public RefCounted<AuthorizationChecker>, public CoreLocationGeolocationProvider::Client {
public:
    static Ref<AuthorizationChecker> create()
    {
        return adoptRef(*new AuthorizationChecker);
    }

    void check(const RegistrableDomain& registrableDomain, CompletionHandler<void(bool)>&& completionHandler)
    {
        m_completionHandler = WTFMove(completionHandler);
        m_provider = makeUnique<CoreLocationGeolocationProvider>(registrableDomain, *this);
    }

private:
    AuthorizationChecker() = default;

    void geolocationAuthorizationGranted(const String&) final
    {
        if (m_completionHandler)
            m_completionHandler(true);
    }

    void geolocationAuthorizationDenied(const String&) final
    {
        if (m_completionHandler)
            m_completionHandler(false);
    }

    void positionChanged(const String&, GeolocationPositionData&&) final { }
    void errorOccurred(const String&, const String&) final
    {
        if (m_completionHandler)
            m_completionHandler(false);
    }
    void resetGeolocation(const String&) final { }

    std::unique_ptr<CoreLocationGeolocationProvider> m_provider;
    CompletionHandler<void(bool)> m_completionHandler;
};

void CoreLocationGeolocationProvider::requestAuthorization(const RegistrableDomain& registrableDomain, CompletionHandler<void(bool)>&& completionHandler)
{
    auto authorizationChecker = AuthorizationChecker::create();
    authorizationChecker->check(registrableDomain, [authorizationChecker, completionHandler = WTFMove(completionHandler)](bool authorized) mutable {
        completionHandler(authorized);
    });
}

} // namespace WebCore

#endif // HAVE(CORE_LOCATION)
