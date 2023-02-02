/*
 * Copyright (C) 2012-2014 Apple Inc. All rights reserved.
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

#if PLATFORM(IOS_FAMILY)

#import "APIFrameInfo.h"
#import "APISecurityOrigin.h"
#import "CompletionHandlerCallChecker.h"
#import "WKFrameInfoInternal.h"
#import "WKGeolocationManager.h"
#import "WKProcessPoolInternal.h"
#import "WKUIDelegatePrivate.h"
#import "WKWebGeolocationPolicyDecider.h"
#import "WKWebViewInternal.h"
#import "WebFrameProxy.h"
#import "WebGeolocationManagerProxy.h"
#import "WebProcessPool.h"
#import "_WKGeolocationCoreLocationProvider.h"
#import "_WKGeolocationPositionInternal.h"
#import <WebCore/GeolocationPosition.h>
#import <WebGeolocationPosition.h>
#import <wtf/Assertions.h>
#import <wtf/BlockPtr.h>
#import <wtf/HashSet.h>
#import <wtf/RefPtr.h>
#import <wtf/RetainPtr.h>
#import <wtf/URL.h>

ALLOW_DEPRECATED_DECLARATIONS_BEGIN

@interface WKGeolocationProviderIOS (_WKGeolocationCoreLocationListener) <_WKGeolocationCoreLocationListener>
@end

@interface WKWebAllowDenyPolicyListener : NSObject<WKWebAllowDenyPolicyListener>
- (id)initWithCompletionHandler:(Function<void(bool)>&&)completionHandler;
@end

struct GeolocationRequestData {
    URL url;
    WebKit::FrameInfoData frameInfo;
    Function<void(bool)> completionHandler;
    RetainPtr<WKWebView> view;
};

@implementation WKGeolocationProviderIOS {
    RefPtr<WebKit::WebGeolocationManagerProxy> _geolocationManager;
    RetainPtr<id <_WKGeolocationCoreLocationProvider>> _coreLocationProvider;
    BOOL _isWebCoreGeolocationActive;
    RefPtr<WebKit::WebGeolocationPosition> _lastActivePosition;
    Deque<GeolocationRequestData> _requestsWaitingForCoreLocationAuthorization;
}

#pragma mark - WKGeolocationProvider callbacks implementation.

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

- (void)_startUpdating
{
    _isWebCoreGeolocationActive = YES;
    [_coreLocationProvider start];

    // If we have the last position, it is from the initialization or warm up. It is the last known
    // good position so we can return it directly.
    if (_lastActivePosition)
        _geolocationManager->providerDidChangePosition(_lastActivePosition.get());
}

- (void)_stopUpdating
{
    _isWebCoreGeolocationActive = NO;
    [_coreLocationProvider stop];
    _lastActivePosition = nullptr;
}

- (void)_setEnableHighAccuracy:(BOOL)enableHighAccuracy
{
    [_coreLocationProvider setEnableHighAccuracy:enableHighAccuracy];
}

#pragma mark - Public API implementation.

- (id)init
{
    ASSERT_NOT_REACHED();
    [self release];
    return nil;
}

- (id)initWithProcessPool:(WebKit::WebProcessPool&)processPool
{
    self = [super init];
    if (!self)
        return nil;

    // On iOS, WebKit normally provides the location. However, if the client sets a coreLocationProvider, then we use that one instead.
    // This is useful for WebKitTestRunner to provide a dummy geolocation provider. It is also used by certain apps to deny all
    // geolocation authorization as a way to disable support for geolocation.
    if (wrapper(processPool)._coreLocationProvider) {
        _geolocationManager = processPool.supplement<WebKit::WebGeolocationManagerProxy>();
        WKGeolocationProviderV1 providerCallback = {
            { 1, self },
            startUpdatingCallback,
            stopUpdatingCallback,
            setEnableHighAccuracy
        };
        WKGeolocationManagerSetProvider(toAPI(_geolocationManager.get()), &providerCallback.base);
        _coreLocationProvider = wrapper(processPool)._coreLocationProvider;
        [_coreLocationProvider setListener:self];
    }
    return self;
}

- (void)decidePolicyForGeolocationRequestFromOrigin:(WebKit::FrameInfoData&&)frameInfo completionHandler:(Function<void(bool)>&&)completionHandler view:(WKWebView *)contentView
{
    WebCore::RegistrableDomain registrableDomain(frameInfo.securityOrigin);
    GeolocationRequestData geolocationRequestData { [contentView URL], WTFMove(frameInfo), WTFMove(completionHandler), contentView };
    _requestsWaitingForCoreLocationAuthorization.append(WTFMove(geolocationRequestData));
    if (_coreLocationProvider) {
        // Step 1: ask the user if the app can use Geolocation.
        [_coreLocationProvider requestGeolocationAuthorization];
    } else {
        // Step 1: ask CoreLocation if the app can use Geolocation.
        WebCore::CoreLocationGeolocationProvider::requestAuthorization(registrableDomain, [self, strongSelf = retainPtr(self)](bool authorized) {
            if (authorized)
                [self geolocationAuthorizationGranted];
            else
                [self geolocationAuthorizationDenied];
        });
    }
}
@end

#pragma mark - WebGeolocationCoreLocationUpdateListener implementation.

@implementation WKGeolocationProviderIOS (WebGeolocationCoreLocationUpdateListener)

- (void)geolocationAuthorizationGranted
{
    // Step 2: ask the user if this particular page can use geolocation.
    if (_requestsWaitingForCoreLocationAuthorization.isEmpty())
        return;

    auto request = _requestsWaitingForCoreLocationAuthorization.takeFirst();
    Function<void(bool)> decisionHandler = [completionHandler = WTFMove(request.completionHandler), protectedSelf = retainPtr(self)](bool result) {
        completionHandler(result);
        [protectedSelf geolocationAuthorizationGranted];
    };

    id<WKUIDelegatePrivate> uiDelegate = static_cast<id <WKUIDelegatePrivate>>([request.view UIDelegate]);
    if ([uiDelegate respondsToSelector:@selector(_webView:requestGeolocationAuthorizationForURL:frame:decisionHandler:)]) {
        RetainPtr<WKFrameInfo> frameInfo = wrapper(API::FrameInfo::create(WTFMove(request.frameInfo), request.view->_page.get()));
        auto checker = WebKit::CompletionHandlerCallChecker::create(uiDelegate, @selector(_webView:requestGeolocationAuthorizationForURL:frame:decisionHandler:));
        [uiDelegate _webView:request.view.get() requestGeolocationAuthorizationForURL:request.url frame:frameInfo.get() decisionHandler:makeBlockPtr([decisionHandler = WTFMove(decisionHandler), checker = WTFMove(checker)](BOOL authorized) {
            if (checker->completionHandlerHasBeenCalled())
                return;
            checker->didCallCompletionHandler();
            decisionHandler(!!authorized);
        }).get()];
        return;
    }

    auto policyListener = adoptNS([[WKWebAllowDenyPolicyListener alloc] initWithCompletionHandler:WTFMove(decisionHandler)]);
    [[WKWebGeolocationPolicyDecider sharedPolicyDecider] decidePolicyForGeolocationRequestFromOrigin:WebCore::SecurityOriginData::fromURL(request.url) requestingURL:request.url view:request.view.get() listener:policyListener.get()];
}

- (void)geolocationAuthorizationDenied
{
    auto requests = WTFMove(_requestsWaitingForCoreLocationAuthorization);
    for (const auto& requestData : requests)
        requestData.completionHandler(false);
}

- (void)positionChanged:(_WKGeolocationPosition *)position
{
    _lastActivePosition = position->_geolocationPosition.get();
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

@end

# pragma mark - Implementation of WKWebAllowDenyPolicyListener
@implementation WKWebAllowDenyPolicyListener {
    Function<void(bool)> _completionHandler;
}

- (id)initWithCompletionHandler:(Function<void(bool)>&&)completionHandler
{
    self = [super init];
    if (!self)
        return nil;

    _completionHandler = WTFMove(completionHandler);
    return self;
}

- (void)allow
{
    _completionHandler(true);
}

- (void)deny
{
    _completionHandler(false);
}
@end

ALLOW_DEPRECATED_DECLARATIONS_END

#endif // PLATFORM(IOS_FAMILY)
