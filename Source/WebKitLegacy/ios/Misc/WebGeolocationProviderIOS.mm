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

#if PLATFORM(IOS_FAMILY)

#import "WebGeolocationProviderIOS.h"

#import "WebDelegateImplementationCaching.h"
#import "WebGeolocationCoreLocationProvider.h"
#import <WebGeolocationPosition.h>
#import <WebUIDelegatePrivate.h>
#import <WebCore/GeolocationPosition.h>
#import <WebCore/WebCoreThread.h>
#import <WebCore/WebCoreThreadRun.h>
#import <wtf/HashSet.h>
#import <wtf/HashMap.h>
#import <wtf/RetainPtr.h>
#import <wtf/Vector.h>

using namespace WebCore;

@interface WebGeolocationPosition (Internal)
- (id)initWithGeolocationPosition:(GeolocationPosition&&)coreGeolocationPosition;
@end

// CoreLocation runs in the main thread. WebGeolocationProviderIOS lives on the WebThread.
// _WebCoreLocationUpdateThreadingProxy forward updates from CoreLocation to WebGeolocationProviderIOS.
@interface _WebCoreLocationUpdateThreadingProxy : NSObject<WebGeolocationCoreLocationUpdateListener>
- (id)initWithProvider:(WebGeolocationProviderIOS*)provider;
@end

typedef HashMap<RetainPtr<WebView>, RetainPtr<id<WebGeolocationProviderInitializationListener> > > GeolocationInitializationCallbackMap;

@implementation WebGeolocationProviderIOS {
@private
    RetainPtr<WebGeolocationCoreLocationProvider> _coreLocationProvider;
    RetainPtr<_WebCoreLocationUpdateThreadingProxy> _coreLocationUpdateListenerProxy;

    BOOL _enableHighAccuracy;
    BOOL _isSuspended;
    BOOL _shouldResetOnResume;

    // WebViews waiting for CoreLocation to be ready. If the Application does not yet have the permission to use Geolocation
    // we also have to wait for that to be granted.
    GeolocationInitializationCallbackMap _webViewsWaitingForCoreLocationAuthorization;

    // List of WebView needing the initial position after registerWebView:. This is needed because WebKit does not
    // handle sending the position synchronously in response to registerWebView:, so we queue sending lastPosition behind a timer.
    HashSet<WebView*> _pendingInitialPositionWebView;

    // List of WebViews registered to WebGeolocationProvider for Geolocation update.
    HashSet<WebView*> _registeredWebViews;

    // All the views that might need a reset if the permission change externally.
    HashSet<WebView*> _trackedWebViews;

    RetainPtr<NSTimer> _sendLastPositionAsynchronouslyTimer;
    RetainPtr<WebGeolocationPosition> _lastPosition;
}

static inline void abortSendLastPosition(WebGeolocationProviderIOS* provider)
{
    provider->_pendingInitialPositionWebView.clear();
    [provider->_sendLastPositionAsynchronouslyTimer.get() invalidate];
    provider->_sendLastPositionAsynchronouslyTimer.clear();
}

- (void)dealloc
{
    abortSendLastPosition(self);
    [super dealloc];
}

#pragma mark - Public API of WebGeolocationProviderIOS.
+ (WebGeolocationProviderIOS *)sharedGeolocationProvider
{
    static dispatch_once_t once;
    static WebGeolocationProviderIOS *sharedGeolocationProvider;
    dispatch_once(&once, ^{
        sharedGeolocationProvider = [[WebGeolocationProviderIOS alloc] init];
    });
    return sharedGeolocationProvider;
}

- (void)suspend
{
    ASSERT(WebThreadIsLockedOrDisabled());
    ASSERT(pthread_main_np());

    ASSERT(!_isSuspended);
    _isSuspended = YES;

    // A new position is acquired and sent to all registered views on resume.
    _lastPosition.clear();
    abortSendLastPosition(self);
    [_coreLocationProvider stop];
}

- (void)resume
{
    ASSERT(WebThreadIsLockedOrDisabled());
    ASSERT(pthread_main_np());

    ASSERT(_isSuspended);
    _isSuspended = NO;

    if (_shouldResetOnResume) {
        [self resetGeolocation];
        _shouldResetOnResume = NO;
        return;
    }

    if (_registeredWebViews.isEmpty() && _webViewsWaitingForCoreLocationAuthorization.isEmpty())
        return;

    if (!_coreLocationProvider) {
        ASSERT(!_coreLocationUpdateListenerProxy);
        _coreLocationUpdateListenerProxy = adoptNS([[_WebCoreLocationUpdateThreadingProxy alloc] initWithProvider:self]);
        _coreLocationProvider = adoptNS([[WebGeolocationCoreLocationProvider alloc] initWithListener:_coreLocationUpdateListenerProxy.get()]);
    }

    if (!_webViewsWaitingForCoreLocationAuthorization.isEmpty())
        [_coreLocationProvider requestGeolocationAuthorization];

    if (!_registeredWebViews.isEmpty()) {
        [_coreLocationProvider setEnableHighAccuracy:_enableHighAccuracy];
        [_coreLocationProvider start];
    }
}

#pragma mark - Internal utility methods

- (void)_handlePendingInitialPosition:(NSTimer*)timer
{
    ASSERT_UNUSED(timer, timer == _sendLastPositionAsynchronouslyTimer);
    ASSERT(WebThreadIsCurrent());

    if (_lastPosition) {
        for (auto& webView : copyToVector(_pendingInitialPositionWebView))
            [webView _geolocationDidChangePosition:_lastPosition.get()];
    }
    abortSendLastPosition(self);
}

#pragma mark - Implementation of WebGeolocationProvider

- (void)registerWebView:(WebView *)webView
{
    ASSERT(WebThreadIsLockedOrDisabled());

    if (_registeredWebViews.contains(webView))
        return;

    _registeredWebViews.add(webView);
    if (!CallUIDelegateReturningBoolean(YES, webView, @selector(webViewCanCheckGeolocationAuthorizationStatus:)))
        return;

    if (!_isSuspended) {
        dispatch_async(dispatch_get_main_queue(), ^{
            if (!_coreLocationProvider) {
                ASSERT(!_coreLocationUpdateListenerProxy);
                _coreLocationUpdateListenerProxy = adoptNS([[_WebCoreLocationUpdateThreadingProxy alloc] initWithProvider:self]);
                _coreLocationProvider = adoptNS([[WebGeolocationCoreLocationProvider alloc] initWithListener:_coreLocationUpdateListenerProxy.get()]);
            }
            [_coreLocationProvider start];
        });
    }

    // We send the lastPosition asynchronously because WebKit does not handle updating the position synchronously.
    // On WebKit2, we could skip that and send the position directly from the UIProcess.
    _pendingInitialPositionWebView.add(webView);
    if (!_sendLastPositionAsynchronouslyTimer) {
        _sendLastPositionAsynchronouslyTimer = [NSTimer timerWithTimeInterval:0 target:self selector:@selector(_handlePendingInitialPosition:) userInfo:nil repeats:NO];
        [WebThreadNSRunLoop() addTimer:_sendLastPositionAsynchronouslyTimer.get() forMode:NSDefaultRunLoopMode];
    }
}

- (void)unregisterWebView:(WebView *)webView
{
    ASSERT(WebThreadIsLockedOrDisabled());

    if (!_registeredWebViews.contains(webView))
        return;

    _registeredWebViews.remove(webView);
    _pendingInitialPositionWebView.remove(webView);

    if (_registeredWebViews.isEmpty()) {
        dispatch_async(dispatch_get_main_queue(), ^{
            [_coreLocationProvider stop];
        });
        _enableHighAccuracy = NO;
        _lastPosition.clear();
    }
}

- (WebGeolocationPosition *)lastPosition
{
    ASSERT(WebThreadIsLockedOrDisabled());
    return _lastPosition.get();
}

- (void)setEnableHighAccuracy:(BOOL)enableHighAccuracy
{
    ASSERT(WebThreadIsLockedOrDisabled());
    _enableHighAccuracy = _enableHighAccuracy || enableHighAccuracy;
    dispatch_async(dispatch_get_main_queue(), ^{
        [_coreLocationProvider setEnableHighAccuracy:_enableHighAccuracy];
    });
}

- (void)initializeGeolocationForWebView:(WebView *)webView listener:(id<WebGeolocationProviderInitializationListener>)listener
{
    ASSERT(WebThreadIsLockedOrDisabled());

    if (!CallUIDelegateReturningBoolean(YES, webView, @selector(webViewCanCheckGeolocationAuthorizationStatus:)))
        return;

    _webViewsWaitingForCoreLocationAuthorization.add(webView, listener);
    _trackedWebViews.add(webView);

    dispatch_async(dispatch_get_main_queue(), ^{
        if (!_coreLocationProvider) {
            ASSERT(!_coreLocationUpdateListenerProxy);
            _coreLocationUpdateListenerProxy = adoptNS([[_WebCoreLocationUpdateThreadingProxy alloc] initWithProvider:self]);
            _coreLocationProvider = adoptNS([[WebGeolocationCoreLocationProvider alloc] initWithListener:_coreLocationUpdateListenerProxy.get()]);
        }
        [_coreLocationProvider requestGeolocationAuthorization];
    });
}

- (void)geolocationAuthorizationGranted
{
    ASSERT(WebThreadIsCurrent());

    GeolocationInitializationCallbackMap requests;
    requests.swap(_webViewsWaitingForCoreLocationAuthorization);

    for (const auto& slot : requests)
        [slot.value initializationAllowedWebView:slot.key.get()];
}

- (void)geolocationAuthorizationDenied
{
    ASSERT(WebThreadIsCurrent());

    GeolocationInitializationCallbackMap requests;
    requests.swap(_webViewsWaitingForCoreLocationAuthorization);

    for (const auto& slot : requests)
        [slot.value initializationDeniedWebView:slot.key.get()];
}

- (void)stopTrackingWebView:(WebView*)webView
{
    ASSERT(WebThreadIsLockedOrDisabled());
    _trackedWebViews.remove(webView);
}

#pragma mark - Mirror to WebGeolocationCoreLocationUpdateListener called by the proxy.

- (void)positionChanged:(WebGeolocationPosition*)position
{
    ASSERT(WebThreadIsCurrent());

    abortSendLastPosition(self);

    _lastPosition = position;
    for (auto& webView : copyToVector(_registeredWebViews))
        [webView _geolocationDidChangePosition:_lastPosition.get()];
}

- (void)errorOccurred:(NSString *)errorMessage
{
    ASSERT(WebThreadIsCurrent());

    _lastPosition.clear();

    for (auto& webView : copyToVector(_registeredWebViews))
        [webView _geolocationDidFailWithMessage:errorMessage];
}

- (void)resetGeolocation
{
    ASSERT(WebThreadIsCurrent());

    if (_isSuspended) {
        _shouldResetOnResume = YES;
        return;
    }
    // 1) Stop all ongoing Geolocation initialization and tracking.
    _webViewsWaitingForCoreLocationAuthorization.clear();
    _registeredWebViews.clear();
    abortSendLastPosition(self);

    // 2) Reset the views, each frame will register back if needed.
    for (auto& webView : copyToVector(_trackedWebViews))
        [webView _resetAllGeolocationPermission];
}
@end

#pragma mark - _WebCoreLocationUpdateThreadingProxy implementation.
@implementation _WebCoreLocationUpdateThreadingProxy {
    WebGeolocationProviderIOS* _provider;
}

- (id)initWithProvider:(WebGeolocationProviderIOS*)provider
{
    self = [super init];
    if (self)
        _provider = provider;
    return self;
}

- (void)geolocationAuthorizationGranted
{
    WebThreadRun(^{
        [_provider geolocationAuthorizationGranted];
    });
}

- (void)geolocationAuthorizationDenied
{
    WebThreadRun(^{
        [_provider geolocationAuthorizationDenied];
    });
}

- (void)positionChanged:(WebCore::GeolocationPosition&&)position
{
    RetainPtr<WebGeolocationPosition> webPosition = adoptNS([[WebGeolocationPosition alloc] initWithGeolocationPosition:WTFMove(position)]);
    WebThreadRun(^{
        [_provider positionChanged:webPosition.get()];
    });
}

- (void)errorOccurred:(NSString *)errorMessage
{
    WebThreadRun(^{
        [_provider errorOccurred:errorMessage];
    });
}

- (void)resetGeolocation
{
    WebThreadRun(^{
        [_provider resetGeolocation];
    });
}
@end

#endif // PLATFORM(IOS_FAMILY)
