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
#import <wtf/TZoneMallocInlines.h>

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

@interface WebCLLocationManager : NSObject<CLLocationManagerDelegate>
@end

@implementation WebCLLocationManager {
    RetainPtr<CLLocationManager> _locationManager;
    WebCore::CoreLocationGeolocationProvider::Client* _client;
    String _websiteIdentifier;
    BOOL _isWaitingForAuthorization;
    WebCore::CoreLocationGeolocationProvider::Mode _mode;
}

- (instancetype)initWithWebsiteIdentifier:(const String&)websiteIdentifier client:(WebCore::CoreLocationGeolocationProvider::Client&)client mode:(WebCore::CoreLocationGeolocationProvider::Mode)mode
{
    self = [super init];
    if (!self)
        return nil;

    _isWaitingForAuthorization = YES;
    _mode = mode;

#if USE(APPLE_INTERNAL_SDK) && HAVE(CORE_LOCATION_WEBSITE_IDENTIFIERS)
    if (!websiteIdentifier.isEmpty())
        _locationManager = adoptNS([allocCLLocationManagerInstance() initWithWebsiteIdentifier:websiteIdentifier.createNSString().get()]);
#else
    UNUSED_PARAM(websiteIdentifier);
#endif
    if (!_locationManager)
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

- (void)stop
{
    [_locationManager stopUpdatingLocation];
}

- (void)setEnableHighAccuracy:(BOOL)highAccuracyEnabled
{
    [_locationManager setDesiredAccuracy:highAccuracyEnabled ? kCLLocationAccuracyBest : kCLLocationAccuracyHundredMeters];
}

- (void)locationManagerDidChangeAuthorization:(CLLocationManager *)manager
{
    auto status = [_locationManager authorizationStatus];
    if (_isWaitingForAuthorization) {
        switch (status) {
        case kCLAuthorizationStatusNotDetermined:
            [_locationManager requestWhenInUseAuthorization];
            break;
        case kCLAuthorizationStatusDenied:
        case kCLAuthorizationStatusRestricted:
            _isWaitingForAuthorization = NO;
            _client->geolocationAuthorizationDenied(_websiteIdentifier);
            break;
        case kCLAuthorizationStatusAuthorizedAlways:
        case kCLAuthorizationStatusAuthorizedWhenInUse:
            _isWaitingForAuthorization = NO;
            _client->geolocationAuthorizationGranted(_websiteIdentifier);
            if (_mode != WebCore::CoreLocationGeolocationProvider::Mode::AuthorizationOnly)
                [_locationManager startUpdatingLocation];
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

WTF_MAKE_TZONE_ALLOCATED_IMPL(CoreLocationGeolocationProvider);

CoreLocationGeolocationProvider::CoreLocationGeolocationProvider(const RegistrableDomain& registrableDomain, Client& client, Mode mode)
    : m_locationManager(adoptNS([[WebCLLocationManager alloc] initWithWebsiteIdentifier:registrableDomain.string() client:client mode:mode]))
{
}

CoreLocationGeolocationProvider::~CoreLocationGeolocationProvider()
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
        m_provider = makeUnique<CoreLocationGeolocationProvider>(registrableDomain, *this, CoreLocationGeolocationProvider::Mode::AuthorizationOnly);
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
