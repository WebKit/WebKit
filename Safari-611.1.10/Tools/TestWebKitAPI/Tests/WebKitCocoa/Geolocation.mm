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

#if PLATFORM(IOS_FAMILY)

#import "PlatformUtilities.h"
#import "TestWKWebView.h"
#import <CoreLocation/CLLocation.h>
#import <WebKit/WKProcessPoolPrivate.h>
#import <WebKit/WKUIDelegatePrivate.h>
#import <WebKit/_WKGeolocationCoreLocationProvider.h>
#import <WebKit/_WKGeolocationPosition.h>
#import <wtf/MainThread.h>
#import <wtf/RetainPtr.h>

static bool hasReceivedAlert;

@interface TestCoreLocationProvider : NSObject<_WKGeolocationCoreLocationProvider>
@property (nonatomic) BOOL shouldAuthorizeGeolocation;
@property (nonatomic) BOOL authorizationWasRequested;
@property (nonatomic) BOOL simulateError;
@end

@implementation TestCoreLocationProvider {
    __unsafe_unretained id <_WKGeolocationCoreLocationListener> _listener;
}

// _WKGeolocationCoreLocationProvider implementation

- (void)setListener:(id <_WKGeolocationCoreLocationListener>)listener
{
    ASSERT(!_listener);
    _listener = listener;
}

- (void)requestGeolocationAuthorization
{
    _authorizationWasRequested = YES;
    WTF::callOnMainThread([shouldAuthorizeGeolocation = _shouldAuthorizeGeolocation, listener = _listener] {
        if (shouldAuthorizeGeolocation)
            [listener geolocationAuthorizationGranted];
        else
            [listener geolocationAuthorizationDenied];
    });
}

- (void)start
{
    ASSERT(_shouldAuthorizeGeolocation);
    WTF::callOnMainThread([listener = _listener, simulateError = _simulateError] {
        if (!simulateError) {
            auto location = adoptNS([[CLLocation alloc] initWithLatitude:37.3348 longitude:-122.009]);
            [listener positionChanged:[_WKGeolocationPosition positionWithLocation:location.get()]];
        } else
            [listener errorOccurred:@"Error Message"];
    });
}

- (void)stop
{
}

- (void)setEnableHighAccuracy:(BOOL)flag
{
}

@end

@interface GeolocationTestUIDelegate : NSObject <WKUIDelegatePrivate>

@property (nonatomic) BOOL allowGeolocation;
@property (nonatomic) BOOL callDecisionHandlerTwice;
@property (nonatomic) BOOL authorizationWasRequested;
@property (nonatomic, readonly) NSString *alertMessage;

@end

@implementation GeolocationTestUIDelegate

static void expectException(void (^completionHandler)())
{
    bool exceptionThrown = false;
    @try {
        completionHandler();
    } @catch (NSException *exception) {
        EXPECT_WK_STREQ(NSInternalInconsistencyException, exception.name);
        exceptionThrown = true;
    }
    EXPECT_TRUE(exceptionThrown);
}

- (void)_webView:(WKWebView *)webView requestGeolocationAuthorizationForURL:(NSURL *)url frame:(WKFrameInfo *)frame decisionHandler:(void (^)(BOOL))decisionHandler
{
    _authorizationWasRequested = YES;
    decisionHandler(_allowGeolocation);

    if (_callDecisionHandlerTwice) {
        expectException(^ {
            decisionHandler(_allowGeolocation);
        });
    }
}

- (void)webView:(WKWebView *)webView runJavaScriptAlertPanelWithMessage:(NSString *)message initiatedByFrame:(WKFrameInfo *)frame completionHandler:(void (^)())completionHandler
{
    completionHandler();
    _alertMessage = message;
    hasReceivedAlert = true;
}

@end


using namespace std;

namespace TestWebKitAPI {

// These tests need to use TestWKWebView because it sets up a visible window for the web
// view. Without this, the web process would wait until the page is visible before sending
// the requests to the UI process.

TEST(WebKit, GeolocationDeniedByLocationProvider)
{
    auto uiDelegate = adoptNS([[GeolocationTestUIDelegate alloc] init]);
    auto coreLocationProvider = adoptNS([[TestCoreLocationProvider alloc] init]);
    auto processPool = adoptNS([[WKProcessPool alloc] init]);
    processPool.get()._coreLocationProvider = coreLocationProvider.get();
    auto config = adoptNS([[WKWebViewConfiguration alloc] init]);
    config.get().processPool = processPool.get();
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:config.get()]);
    webView.get().UIDelegate = uiDelegate.get();

    coreLocationProvider.get().shouldAuthorizeGeolocation = NO;
    uiDelegate.get().allowGeolocation = NO;
    
    hasReceivedAlert = false;
    [webView loadTestPageNamed:@"GeolocationGetCurrentPositionResult"];

    TestWebKitAPI::Util::run(&hasReceivedAlert);

    EXPECT_WK_STREQ(uiDelegate.get().alertMessage, "ERROR:1");
    EXPECT_TRUE(coreLocationProvider.get().authorizationWasRequested);
    EXPECT_FALSE(uiDelegate.get().authorizationWasRequested);
}

TEST(WebKit, GeolocationDeniedByAPI)
{
    auto uiDelegate = adoptNS([[GeolocationTestUIDelegate alloc] init]);
    auto coreLocationProvider = adoptNS([[TestCoreLocationProvider alloc] init]);
    auto processPool = adoptNS([[WKProcessPool alloc] init]);
    processPool.get()._coreLocationProvider = coreLocationProvider.get();
    auto config = adoptNS([[WKWebViewConfiguration alloc] init]);
    config.get().processPool = processPool.get();
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:config.get()]);
    webView.get().UIDelegate = uiDelegate.get();

    coreLocationProvider.get().shouldAuthorizeGeolocation = YES;
    uiDelegate.get().allowGeolocation = NO;

    hasReceivedAlert = false;
    [webView loadTestPageNamed:@"GeolocationGetCurrentPositionResult"];

    TestWebKitAPI::Util::run(&hasReceivedAlert);

    EXPECT_WK_STREQ(uiDelegate.get().alertMessage, "ERROR:1");
    EXPECT_TRUE(coreLocationProvider.get().authorizationWasRequested);
    EXPECT_TRUE(uiDelegate.get().authorizationWasRequested);
}

TEST(WebKit, GeolocationAllowedByAPI)
{
    auto uiDelegate = adoptNS([[GeolocationTestUIDelegate alloc] init]);
    auto coreLocationProvider = adoptNS([[TestCoreLocationProvider alloc] init]);
    auto processPool = adoptNS([[WKProcessPool alloc] init]);
    processPool.get()._coreLocationProvider = coreLocationProvider.get();
    auto config = adoptNS([[WKWebViewConfiguration alloc] init]);
    config.get().processPool = processPool.get();
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:config.get()]);
    webView.get().UIDelegate = uiDelegate.get();

    coreLocationProvider.get().shouldAuthorizeGeolocation = YES;
    uiDelegate.get().allowGeolocation = YES;

    hasReceivedAlert = false;
    [webView loadTestPageNamed:@"GeolocationGetCurrentPositionResult"];

    TestWebKitAPI::Util::run(&hasReceivedAlert);

    EXPECT_WK_STREQ(uiDelegate.get().alertMessage, "SUCCESS");
    EXPECT_TRUE(coreLocationProvider.get().authorizationWasRequested);
    EXPECT_TRUE(uiDelegate.get().authorizationWasRequested);
}

TEST(WebKit, GeolocationError)
{
    auto uiDelegate = adoptNS([[GeolocationTestUIDelegate alloc] init]);
    auto coreLocationProvider = adoptNS([[TestCoreLocationProvider alloc] init]);
    auto processPool = adoptNS([[WKProcessPool alloc] init]);
    processPool.get()._coreLocationProvider = coreLocationProvider.get();
    auto config = adoptNS([[WKWebViewConfiguration alloc] init]);
    config.get().processPool = processPool.get();
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:config.get()]);
    webView.get().UIDelegate = uiDelegate.get();

    coreLocationProvider.get().shouldAuthorizeGeolocation = YES;
    coreLocationProvider.get().simulateError = YES;
    uiDelegate.get().allowGeolocation = YES;

    hasReceivedAlert = false;
    [webView loadTestPageNamed:@"GeolocationGetCurrentPositionResult"];

    TestWebKitAPI::Util::run(&hasReceivedAlert);

    EXPECT_WK_STREQ(uiDelegate.get().alertMessage, "ERROR:2");
    EXPECT_TRUE(coreLocationProvider.get().authorizationWasRequested);
    EXPECT_TRUE(uiDelegate.get().authorizationWasRequested);
}

TEST(WebKit, DuplicateGeolocationAuthorizationCallbackCalls)
{
    auto uiDelegate = adoptNS([[GeolocationTestUIDelegate alloc] init]);
    auto coreLocationProvider = adoptNS([[TestCoreLocationProvider alloc] init]);
    auto processPool = adoptNS([[WKProcessPool alloc] init]);
    processPool.get()._coreLocationProvider = coreLocationProvider.get();
    auto config = adoptNS([[WKWebViewConfiguration alloc] init]);
    config.get().processPool = processPool.get();
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:config.get()]);
    webView.get().UIDelegate = uiDelegate.get();

    coreLocationProvider.get().shouldAuthorizeGeolocation = YES;
    uiDelegate.get().allowGeolocation = YES;
    uiDelegate.get().callDecisionHandlerTwice = YES;

    hasReceivedAlert = false;
    [webView loadTestPageNamed:@"GeolocationGetCurrentPositionResult"];

    TestWebKitAPI::Util::run(&hasReceivedAlert);

    EXPECT_WK_STREQ(uiDelegate.get().alertMessage, "SUCCESS");
    EXPECT_TRUE(coreLocationProvider.get().authorizationWasRequested);
    EXPECT_TRUE(uiDelegate.get().authorizationWasRequested);
}

} // namespace TestWebKitAPI

#endif // PLATFORM(IOS_FAMILY)
