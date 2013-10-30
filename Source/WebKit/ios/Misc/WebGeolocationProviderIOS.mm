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

#import "WebGeolocationProviderIOS.h"

#import "WebGeolocationCoreLocationProvider.h"
#import <WebGeolocationPosition.h>
#import <WebCore/GeolocationPosition.h>
#import <WebCore/WebCoreThread.h>
#import <WebCore/WebCoreThreadRun.h>
#import <wtf/HashSet.h>
#import <wtf/HashMap.h>
#import <wtf/RetainPtr.h>
#import <wtf/Vector.h>

#if PLATFORM(IOS)
#import "WebDelegateImplementationCaching.h"
#endif

using namespace WebCore;

@interface WebGeolocationPosition (Internal)
- (id)initWithGeolocationPosition:(PassRefPtr<GeolocationPosition>)coreGeolocationPosition;
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
    GeolocationInitializationCallbackMap _webViewsWaitingForCoreLocationStart;

    // WebViews that have required location services and are in "Warm Up" while waiting to be registered or denied for the SecurityOrigin.
    // The warm up expire after some time to avoid keeping CoreLocation alive while waiting for the user response for the Security Origin.
    HashSet<WebView*> _warmUpWebViews;

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
    [_coreLocationProvider.get() stop];
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

    if (_registeredWebViews.isEmpty() && _webViewsWaitingForCoreLocationStart.isEmpty() && _warmUpWebViews.isEmpty())
        return;

    if (!_coreLocationProvider) {
        ASSERT(!_coreLocationUpdateListenerProxy);
        _coreLocationUpdateListenerProxy = adoptNS([[_WebCoreLocationUpdateThreadingProxy alloc] initWithProvider:self]);
        _coreLocationProvider = adoptNS([[WebGeolocationCoreLocationProvider alloc] initWithListener:_coreLocationUpdateListenerProxy.get()]);
    }
    [_coreLocationProvider.get() setEnableHighAccuracy:_enableHighAccuracy];
    [_coreLocationProvider.get() start];
}

#pragma mark - Internal utility methods
- (void)_startCoreLocationDelegate
{
    ASSERT(!(_registeredWebViews.isEmpty() && _webViewsWaitingForCoreLocationStart.isEmpty()));

    if (_isSuspended)
        return;

    dispatch_async(dispatch_get_main_queue(), ^{
        if (!_coreLocationProvider.get()) {
            ASSERT(!_coreLocationUpdateListenerProxy);
            _coreLocationUpdateListenerProxy = adoptNS([[_WebCoreLocationUpdateThreadingProxy alloc] initWithProvider:self]);
            _coreLocationProvider = adoptNS([[WebGeolocationCoreLocationProvider alloc] initWithListener:_coreLocationUpdateListenerProxy.get()]);
        }
        [_coreLocationProvider.get() start];
    });
}

- (void)_stopCoreLocationDelegateIfNeeded
{
    if (_registeredWebViews.isEmpty() && _webViewsWaitingForCoreLocationStart.isEmpty() && _warmUpWebViews.isEmpty()) {
        dispatch_async(dispatch_get_main_queue(), ^{
            [_coreLocationProvider.get() stop];
        });
        _enableHighAccuracy = NO;
        _lastPosition.clear();
    }
}

- (void)_handlePendingInitialPosition:(NSTimer*)timer
{
    ASSERT_UNUSED(timer, timer == _sendLastPositionAsynchronouslyTimer);

    if (_lastPosition) {
        Vector<WebView*> webViewsCopy;
        copyToVector(_pendingInitialPositionWebView, webViewsCopy);
        for (size_t i = 0; i < webViewsCopy.size(); ++i)
            [webViewsCopy[i] _geolocationDidChangePosition:_lastPosition.get()];
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
    _warmUpWebViews.remove(webView);
#if PLATFORM(IOS)
    if (!CallUIDelegateReturningBoolean(YES, webView, @selector(webViewCanCheckGeolocationAuthorizationStatus:)))
        return;
#endif

    [self _startCoreLocationDelegate];

    // We send the lastPosition asynchronously because WebKit does not handle updating the position synchronously.
    // On WebKit2, we could skip that and send the position directly from the UIProcess.
    _pendingInitialPositionWebView.add(webView);
    if (!_sendLastPositionAsynchronouslyTimer)
        _sendLastPositionAsynchronouslyTimer = [NSTimer scheduledTimerWithTimeInterval:0
                                                                                target:self
                                                                              selector:@selector(_handlePendingInitialPosition:)
                                                                              userInfo:nil
                                                                               repeats:NO];
}

- (void)unregisterWebView:(WebView *)webView
{
    ASSERT(WebThreadIsLockedOrDisabled());

    if (!_registeredWebViews.contains(webView))
        return;

    _registeredWebViews.remove(webView);
    _pendingInitialPositionWebView.remove(webView);
    [self _stopCoreLocationDelegateIfNeeded];
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
        [_coreLocationProvider.get() setEnableHighAccuracy:_enableHighAccuracy];
    });
}

- (void)initializeGeolocationForWebView:(WebView *)webView listener:(id<WebGeolocationProviderInitializationListener>)listener
{
    ASSERT(WebThreadIsLockedOrDisabled());

#if PLATFORM(IOS)
    if (!CallUIDelegateReturningBoolean(YES, webView, @selector(webViewCanCheckGeolocationAuthorizationStatus:)))
        return;
#endif
    _webViewsWaitingForCoreLocationStart.add(webView, listener);
    _trackedWebViews.add(webView);

    [self _startCoreLocationDelegate];
}

- (void)cancelWarmUpForWebView:(WebView *)webView
{
    ASSERT(WebThreadIsLockedOrDisabled());

    _warmUpWebViews.remove(webView);
    [self _stopCoreLocationDelegateIfNeeded];
}

- (void)stopTrackingWebView:(WebView*)webView
{
    ASSERT(WebThreadIsLockedOrDisabled());
    _trackedWebViews.remove(webView);
}

#pragma mark - Mirror to WebGeolocationCoreLocationUpdateListener called by the proxy.
- (void)geolocationDelegateStarted
{
    ASSERT(WebThreadIsCurrent());

    // This could be called recursively. We must clean _webViewsWaitingForCoreLocationStart before invoking the callbacks.
    Vector<RetainPtr<WebView> > webViews;
    copyKeysToVector(_webViewsWaitingForCoreLocationStart, webViews);

    Vector<RetainPtr<id<WebGeolocationProviderInitializationListener> > > initializationListeners;
    copyValuesToVector(_webViewsWaitingForCoreLocationStart, initializationListeners);

    _webViewsWaitingForCoreLocationStart.clear();

    // Start warmup period for each view in intialization.
    const int64_t warmUpTimeoutInterval = 20 * NSEC_PER_SEC;
    dispatch_time_t when = dispatch_time(DISPATCH_TIME_NOW, warmUpTimeoutInterval);
    for (size_t i = 0; i < webViews.size(); ++i) {
        WebView *webView = webViews[i].get();

        _warmUpWebViews.add(webView);

        dispatch_after(when, dispatch_get_main_queue(), ^{
            _warmUpWebViews.remove(webView);
            [self _stopCoreLocationDelegateIfNeeded];
        });
    }

    // Inform the listeners the initialization succeeded.
    for (size_t i = 0; i < webViews.size(); ++i) {
        RetainPtr<WebView>& webView = webViews[i];
        RetainPtr<id<WebGeolocationProviderInitializationListener> >& listener = initializationListeners[i];
        [listener.get() initializationAllowedWebView:webView.get() provider:self];
    }
}

- (void)geolocationDelegateUnableToStart
{
    ASSERT(WebThreadIsCurrent());

    // This could be called recursively. We must clean _webViewsWaitingForCoreLocationStart before invoking the callbacks.
    Vector<RetainPtr<WebView> > webViews;
    copyKeysToVector(_webViewsWaitingForCoreLocationStart, webViews);

    Vector<RetainPtr<id<WebGeolocationProviderInitializationListener> > > initializationListeners;
    copyValuesToVector(_webViewsWaitingForCoreLocationStart, initializationListeners);

    _webViewsWaitingForCoreLocationStart.clear();

    // Inform the listeners of the failure.
    for (size_t i = 0; i < webViews.size(); ++i) {
        RetainPtr<WebView>& webView = webViews[i];
        RetainPtr<id<WebGeolocationProviderInitializationListener> >& listener = initializationListeners[i];
        [listener.get() initializationDeniedWebView:webView.get() provider:self];
    }
}

- (void)positionChanged:(WebGeolocationPosition*)position
{
    ASSERT(WebThreadIsCurrent());

    abortSendLastPosition(self);

    _lastPosition = position;
    Vector<WebView*> webViewsCopy;
    copyToVector(_registeredWebViews, webViewsCopy);
    for (size_t i = 0; i < webViewsCopy.size(); ++i)
        [webViewsCopy.at(i) _geolocationDidChangePosition:_lastPosition.get()];
}

- (void)errorOccurred:(NSString *)errorMessage
{
    ASSERT(WebThreadIsCurrent());

    _lastPosition.clear();

    Vector<WebView*> webViewsCopy;
    copyToVector(_registeredWebViews, webViewsCopy);
    for (size_t i = 0; i < webViewsCopy.size(); ++i)
        [webViewsCopy.at(i) _geolocationDidFailWithMessage:errorMessage];
}

- (void)resetGeolocation
{
    ASSERT(WebThreadIsCurrent());

    if (_isSuspended) {
        _shouldResetOnResume = YES;
        return;
    }
    // 1) Stop all ongoing Geolocation initialization and tracking.
    _webViewsWaitingForCoreLocationStart.clear();
    _warmUpWebViews.clear();
    _registeredWebViews.clear();
    abortSendLastPosition(self);
    [self _stopCoreLocationDelegateIfNeeded];

    // 2) Reset the views, each frame will register back if needed.
    Vector<WebView*> webViewsCopy;
    copyToVector(_trackedWebViews, webViewsCopy);
    for (size_t i = 0; i < webViewsCopy.size(); ++i)
        [webViewsCopy.at(i) _resetAllGeolocationPermission];
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

- (void)geolocationDelegateStarted
{
    WebThreadRun(^{ [_provider geolocationDelegateStarted]; });
}

- (void)geolocationDelegateUnableToStart
{
    WebThreadRun(^{ [_provider geolocationDelegateUnableToStart]; });
}

- (void)positionChanged:(WebCore::GeolocationPosition*)position
{
    RetainPtr<WebGeolocationPosition> webPosition = adoptNS([[WebGeolocationPosition alloc] initWithGeolocationPosition:position]);
    WebThreadRun(^{ [_provider positionChanged:webPosition.get()]; });
}

- (void)errorOccurred:(NSString *)errorMessage
{
    WebThreadRun(^{ [_provider errorOccurred:errorMessage]; });
}

- (void)resetGeolocation
{
    WebThreadRun(^{ [_provider resetGeolocation]; });
}
@end

#endif // PLATFORM(IOS)
