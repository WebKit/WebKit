/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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
#import "WKGeolocationProviderIOS.h"

#if PLATFORM(IOS)

#import "GeolocationPermissionRequestProxy.h"
#import "WebContext.h"
#import "WebGeolocationManagerProxy.h"
#import "WebSecurityOrigin.h"
#import <WebGeolocationPosition.h>
#import <WebCore/GeolocationPosition.h>
#import <WebKit2/WKGeolocationPermissionRequest.h>
#import <wtf/Assertions.h>
#import <wtf/PassRefPtr.h>
#import <wtf/RefPtr.h>
#import <wtf/RetainPtr.h>
#import <wtf/HashSet.h>

// FIXME: Remove use of WebKit1 from WebKit2
#import <WebKit/WebGeolocationCoreLocationProvider.h>
#import <WebKit/WebAllowDenyPolicyListener.h>

using namespace WebKit;

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"

@interface WKGeolocationProviderIOS ()
-(void)_startUpdating;
-(void)_stopUpdating;
-(void)_setEnableHighAccuracy:(BOOL)enable;
@end

@interface WKGeolocationProviderIOS (WebGeolocationCoreLocationUpdateListener) <WebGeolocationCoreLocationUpdateListener>
@end

@interface WKWebAllowDenyPolicyListener : NSObject<WebAllowDenyPolicyListener>
- (id)initWithProvider:(WKGeolocationProviderIOS*)provider permissionRequestProxy:(PassRefPtr<GeolocationPermissionRequestProxy>)permissionRequestProxy;
- (void)denyOnlyThisRequest NO_RETURN_DUE_TO_ASSERT;
@end

namespace WebKit {
void decidePolicyForGeolocationRequestFromOrigin(WebCore::SecurityOrigin*, const String& urlString, id<WebAllowDenyPolicyListener>, UIWindow*);
};

static void startUpdatingCallback(WKGeolocationManagerRef geolocationManager, const void* clientInfo)
{
    WKGeolocationProviderIOS *geolocationProvider = reinterpret_cast<WKGeolocationProviderIOS*>(const_cast<void*>(clientInfo));
    ASSERT([geolocationProvider isKindOfClass:[WKGeolocationProviderIOS class]]);
    [geolocationProvider _startUpdating];
}

static void stopUpdatingCallback(WKGeolocationManagerRef geolocationManager, const void* clientInfo)
{
    WKGeolocationProviderIOS *geolocationProvider = reinterpret_cast<WKGeolocationProviderIOS*>(const_cast<void*>(clientInfo));
    ASSERT([geolocationProvider isKindOfClass:[WKGeolocationProviderIOS class]]);
    [geolocationProvider _stopUpdating];
}

static void setEnableHighAccuracy(WKGeolocationManagerRef geolocationManager, bool enable, const void* clientInfo)
{
    WKGeolocationProviderIOS *geolocationProvider = reinterpret_cast<WKGeolocationProviderIOS*>(const_cast<void*>(clientInfo));
    ASSERT([geolocationProvider isKindOfClass:[WKGeolocationProviderIOS class]]);
    [geolocationProvider _setEnableHighAccuracy:enable];
}

struct GeolocationRequestData {
    RefPtr<WebCore::SecurityOrigin> origin;
    RefPtr<WebFrameProxy> frame;
    RefPtr<GeolocationPermissionRequestProxy> permissionRequest;
    RetainPtr<UIWindow> window;
};

@implementation WKGeolocationProviderIOS {
    RefPtr<WebGeolocationManagerProxy> _geolocationManager;
    RetainPtr<WebGeolocationCoreLocationProvider> _coreLocationProvider;
    BOOL _isWebCoreGeolocationActive;
    RefPtr<WebGeolocationPosition> _lastActivePosition;
    Vector<GeolocationRequestData> _requestsWaitingForCoreLocationStart;
    HashSet<void*> _requestsInWarmUp;
}

-(void)_stopUpdatingIfPossible
{
    if (_isWebCoreGeolocationActive)
        return;

    if (_requestsWaitingForCoreLocationStart.isEmpty() && _requestsInWarmUp.isEmpty()) {
        [_coreLocationProvider stop];
        _lastActivePosition.clear();
    }
}

#pragma mark - WKGeolocationProvider callbacks implementation.
-(void)_startUpdating
{
    _isWebCoreGeolocationActive = YES;
    [_coreLocationProvider start];

    // If we have the last position, it is from the initialization or warm up. It is the last known
    // good position so we can return it directly.
    if (_lastActivePosition)
        _geolocationManager->providerDidChangePosition(_lastActivePosition.get());
}

-(void)_stopUpdating
{
    _isWebCoreGeolocationActive = NO;
    [self _stopUpdatingIfPossible];
}

-(void)_setEnableHighAccuracy:(BOOL)enableHighAccuracy
{
    [_coreLocationProvider setEnableHighAccuracy:enableHighAccuracy];
}

#pragma mark - Public API implementation.

-(id)init
{
    ASSERT_NOT_REACHED();
    [self release];
    return nil;
}

-(id)initWithContext:(WebContext*)context
{
    self = [super init];
    if (!self)
        return nil;
    _geolocationManager = context->supplement<WebGeolocationManagerProxy>();
    WKGeolocationProvider providerCallback = {
        kWKGeolocationProviderCurrentVersion,
        self,
        startUpdatingCallback,
        stopUpdatingCallback,
        setEnableHighAccuracy
    };
    _geolocationManager->initializeProvider(reinterpret_cast<WKGeolocationProviderBase*>(&providerCallback));
    _coreLocationProvider = adoptNS([[WebGeolocationCoreLocationProvider alloc] initWithListener:self]);
    return self;
}

-(void)decidePolicyForGeolocationRequestFromOrigin:(WKSecurityOriginRef)origin frame:(WKFrameRef)frame request:(WKGeolocationPermissionRequestRef)permissionRequest window:(UIWindow*)window
{
    GeolocationRequestData geolocationRequestData;
    geolocationRequestData.origin = toImpl(origin)->securityOrigin();
    geolocationRequestData.frame = toImpl(frame);
    geolocationRequestData.permissionRequest = toImpl(permissionRequest);
    geolocationRequestData.window = window;
    _requestsWaitingForCoreLocationStart.append(geolocationRequestData);
    [_coreLocationProvider start];
}
@end

#pragma mark - WebGeolocationCoreLocationUpdateListener implementation.

@implementation WKGeolocationProviderIOS (WebGeolocationCoreLocationUpdateListener)
- (void)geolocationDelegateStarted
{
    Vector<GeolocationRequestData> requests;
    requests.swap(_requestsWaitingForCoreLocationStart);
    HashSet<void*> latestRequestsForWarmup;
    for (size_t i = 0; i < requests.size(); ++i) {
        GeolocationPermissionRequestProxy* permissionRequest = requests[i].permissionRequest.get();
        latestRequestsForWarmup.add(permissionRequest);
        _requestsInWarmUp.add(permissionRequest);
    }

    // Start the warmup period in which we keep CoreLocation running while waiting for the user response.
    const int64_t warmUpTimeoutInterval = 20 * NSEC_PER_SEC;
    dispatch_time_t when = dispatch_time(DISPATCH_TIME_NOW, warmUpTimeoutInterval);
    dispatch_after(when, dispatch_get_main_queue(), ^{
        HashSet<void*>::const_iterator end = latestRequestsForWarmup.end();
        for (HashSet<void*>::const_iterator it = latestRequestsForWarmup.begin(); it != end; ++it)
            _requestsInWarmUp.remove(*it);
        [self _stopUpdatingIfPossible];
    });

    for (size_t i = 0; i < requests.size(); ++i) {
        RetainPtr<WKWebAllowDenyPolicyListener> policyListener = adoptNS([[WKWebAllowDenyPolicyListener alloc] initWithProvider:self permissionRequestProxy:requests[i].permissionRequest.get()]);
        decidePolicyForGeolocationRequestFromOrigin(requests[i].origin.get(), requests[i].frame->url(), policyListener.get(), requests[i].window.get());
    }
}

- (void)geolocationDelegateUnableToStart
{
    for (size_t i = 0; i < _requestsWaitingForCoreLocationStart.size(); ++i)
        _requestsWaitingForCoreLocationStart[i].permissionRequest->deny();
    _requestsWaitingForCoreLocationStart.clear();
    [self _stopUpdatingIfPossible];
}

- (void)positionChanged:(WebCore::GeolocationPosition*)position
{
    _lastActivePosition = WebGeolocationPosition::create(position->timestamp(), position->latitude(), position->longitude(), position->accuracy(), position->canProvideAltitude(), position->altitude(), position->canProvideAltitudeAccuracy(), position->altitudeAccuracy(), position->canProvideHeading(), position->heading(), position->canProvideSpeed(), position->speed());
    _geolocationManager->providerDidChangePosition(_lastActivePosition.get());
}

- (void)errorOccurred:(NSString *)errorMessage
{
    _geolocationManager->providerDidFailToDeterminePosition(errorMessage);
}

- (void)resetGeolocation
{
    _geolocationManager->resetPermissions();
}

#pragma mark - Methods for udating the state WKWebAllowDenyPolicyListener
- (void)permissionDenied:(GeolocationPermissionRequestProxy*)permissionRequestProxy
{
    _requestsInWarmUp.remove(permissionRequestProxy);
    [self _stopUpdatingIfPossible];
}

@end

# pragma mark - Implementation of WKWebAllowDenyPolicyListener
@implementation WKWebAllowDenyPolicyListener {
    RetainPtr<WKGeolocationProviderIOS> _provider;
    RefPtr<GeolocationPermissionRequestProxy> _permissionRequestProxy;
}

- (id)initWithProvider:(WKGeolocationProviderIOS*)provider permissionRequestProxy:(PassRefPtr<GeolocationPermissionRequestProxy>)permissionRequestProxy
{
    self = [super init];
    if (!self)
        return nil;

    _provider = provider;
    _permissionRequestProxy = permissionRequestProxy;
    return self;
}

- (void)allow
{
    _permissionRequestProxy->allow();
}

- (void)deny
{
    [_provider permissionDenied:_permissionRequestProxy.get()];
    _permissionRequestProxy->deny();
}

- (void)denyOnlyThisRequest
{
    // The method denyOnlyThisRequest is iAd specific for WebKit1.
    ASSERT_NOT_REACHED();
}

- (BOOL)shouldClearCache
{
    return NO;
}
@end

#pragma clang diagnostic pop

#endif // PLATFORM(IOS)
