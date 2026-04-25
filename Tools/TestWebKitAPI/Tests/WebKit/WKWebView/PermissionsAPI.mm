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

#import "Helpers/PlatformUtilities.h"
#import "Helpers/cocoa/TestWKWebView.h"
#import <WebKit/WKContext.h>
#import <WebKit/WKGeolocationManager.h>
#import <WebKit/WKGeolocationPosition.h>
#import <WebKit/WKSecurityOriginRef.h>
#import <WebKit/WKString.h>
#import <WebKit/WKWebViewConfigurationPrivate.h>
#import <WebKit/WKWebViewPrivate.h>
#import <wtf/text/StringBuilder.h>

static bool didReceiveMessage;
static bool didReceiveQueryPermission;
static WKPermissionDecision queryPermissionDelegateResult;
static RetainPtr<WKScriptMessage> scriptMessage;
static bool didReceiveGeolocationRequest;
static bool requestGeolocationPermissionDelegateResult;

enum class RequestGeolocationSincePageLoad : bool { No, Yes };
enum class GeolocationPermissionState {
    GrantedPermanently,
    DeniedPermanently,
    GrantedFromPrompt,
    DeniedFromPrompt
};

@interface PermissionsAPIMessageHandler : NSObject <WKScriptMessageHandler>
@end

@implementation PermissionsAPIMessageHandler
- (void)userContentController:(WKUserContentController *)userContentController didReceiveScriptMessage:(WKScriptMessage *)message
{
    scriptMessage = message;
    didReceiveMessage = true;
}
@end


@interface PermissionsAPIUIDelegate : NSObject<WKUIDelegate>
- (void)_webView:(WKWebView *)webView queryPermission:(NSString*) name forOrigin:(WKSecurityOrigin *)origin completionHandler:(void (^)(WKPermissionDecision state))completionHandler;
@end

@implementation PermissionsAPIUIDelegate
- (void)_webView:(WKWebView *)webView queryPermission:(NSString*) name forOrigin:(WKSecurityOrigin *)origin completionHandler:(void (^)(WKPermissionDecision state))completionHandler {
    didReceiveQueryPermission = true;
    completionHandler(queryPermissionDelegateResult);
}
@end


@interface PermissionsAPIAndGeolocationRequestUIDelegate : PermissionsAPIUIDelegate
- (void)_webView:(WKWebView *)webView requestGeolocationPermissionForFrame:(WKFrameInfo *)frame decisionHandler:(void (^)(BOOL allowed))decisionHandler;
@end

@implementation PermissionsAPIAndGeolocationRequestUIDelegate
- (void)_webView:(WKWebView *)webView requestGeolocationPermissionForFrame:(WKFrameInfo *)frame decisionHandler:(void (^)(BOOL allowed))decisionHandler {
    didReceiveGeolocationRequest = true;
    decisionHandler(requestGeolocationPermissionDelegateResult);
}
@end


namespace TestWebKitAPI {

static void urlEncodeIfNeeded(uint8_t byte, StringBuilder& buffer)
{
    if (byte < '0' || (byte > '9' && byte < 'A') || (byte > 'Z' && byte < 'a') || byte > 'z') {
        buffer.append('%');
        buffer.append(upperNibbleToASCIIHexDigit(byte));
        buffer.append(lowerNibbleToASCIIHexDigit(byte));
        return;
    }
    buffer.append(byte);
}

TEST(PermissionsAPI, DataURL)
{
    RetainPtr configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    configuration.get()._allowTopNavigationToDataURLs = YES;
    RetainPtr messageHandler = adoptNS([[PermissionsAPIMessageHandler alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:messageHandler.get() name:@"msg"];

    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500) configuration:configuration.get()]);
    RetainPtr delegate = adoptNS([[PermissionsAPIUIDelegate alloc] init]);
    [webView setUIDelegate:delegate.get()];

    queryPermissionDelegateResult = WKPermissionDecisionDeny;
    didReceiveMessage = false;
    didReceiveQueryPermission = false;

    char script[] = "<script>\
        navigator.permissions.query({ name : 'geolocation' }).then((result) => { \
            window.webkit.messageHandlers.msg.postMessage(result.state);\
        }, () => { \
            window.webkit.messageHandlers.msg.postMessage('FAIL');\
        });\
    </script>";

    StringBuilder buffer;
    buffer.append("data:text/html,"_s);
    for (size_t cptr = 0; cptr < sizeof(script) - 1; ++cptr)
        urlEncodeIfNeeded(script[cptr], buffer);

    RetainPtr request = adoptNS([[NSURLRequest alloc] initWithURL:adoptNS([[NSURL alloc] initWithString:buffer.createNSString().get()]).get()]);
    [webView loadRequest:request.get()];

    TestWebKitAPI::Util::run(&didReceiveMessage);
    EXPECT_STREQ(((NSString *)[scriptMessage body]).UTF8String, "prompt");

    EXPECT_FALSE(didReceiveQueryPermission);
}

TEST(PermissionsAPI, OnChange)
{
    RetainPtr configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    RetainPtr messageHandler = adoptNS([[PermissionsAPIMessageHandler alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:messageHandler.get() name:@"msg"];

    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500) configuration:configuration.get()]);
    RetainPtr delegate = adoptNS([[PermissionsAPIUIDelegate alloc] init]);
    [webView setUIDelegate:delegate.get()];

    queryPermissionDelegateResult = WKPermissionDecisionGrant;
    didReceiveMessage = false;
    didReceiveQueryPermission = false;

    NSString *script = @"<script>"
        "navigator.permissions.query({ name : 'geolocation' }).then((permissionStatus) => {"
        "    window.webkit.messageHandlers.msg.postMessage(permissionStatus.state);"
        "    permissionStatus.onchange = () => {"
        "        window.webkit.messageHandlers.msg.postMessage(permissionStatus.state);"
        "    };"
        "}, () => {"
        "    window.webkit.messageHandlers.msg.postMessage('FAIL');"
        "});"
        "</script>";

    [webView synchronouslyLoadHTMLString:script baseURL:[NSURL URLWithString:@"https://example.com/"]];

    TestWebKitAPI::Util::run(&didReceiveMessage);
    EXPECT_STREQ(((NSString *)[scriptMessage body]).UTF8String, "granted");

    didReceiveMessage = false;
    queryPermissionDelegateResult = WKPermissionDecisionPrompt;

    auto originString = adoptWK(WKStringCreateWithUTF8CString("https://example.com/"));
    auto origin = adoptWK(WKSecurityOriginCreateFromString(originString.get()));
    [WKWebView _permissionChanged:@"geolocation" forOrigin:(__bridge WKSecurityOrigin *)origin.get()];

    TestWebKitAPI::Util::run(&didReceiveMessage);
    EXPECT_STREQ(((NSString *)[scriptMessage body]).UTF8String, "prompt");
}

static void testPermissionsAPIForGeolocation(GeolocationPermissionState geolocationPermissionState, RequestGeolocationSincePageLoad requestGeolocationSincePageLoad, String expectedResult)
{
    RetainPtr pool = adoptNS([[WKProcessPool alloc] init]);

    WKGeolocationProviderV1 providerCallback;
    zeroBytes(providerCallback);
    providerCallback.base.version = 1;
    providerCallback.startUpdating = [] (WKGeolocationManagerRef manager, const void*) {
        WKGeolocationManagerProviderDidChangePosition(manager, adoptWK(WKGeolocationPositionCreate(0, 50.644358, 3.345453, 2.53)).get());
    };
    WKGeolocationManagerSetProvider(WKContextGetGeolocationManager((WKContextRef)pool.get()), &providerCallback.base);

    RetainPtr configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    configuration.get().processPool = pool.get();

    RetainPtr messageHandler = adoptNS([[PermissionsAPIMessageHandler alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:messageHandler.get() name:@"msg"];

    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500) configuration:configuration.get()]);
    RetainPtr delegate = adoptNS([[PermissionsAPIAndGeolocationRequestUIDelegate alloc] init]);
    [webView setUIDelegate:delegate.get()];

    switch (geolocationPermissionState) {
    case GeolocationPermissionState::GrantedPermanently:
        queryPermissionDelegateResult = WKPermissionDecisionGrant;
        break;
    case GeolocationPermissionState::DeniedPermanently:
        queryPermissionDelegateResult = WKPermissionDecisionDeny;
        break;
    case GeolocationPermissionState::GrantedFromPrompt:
        queryPermissionDelegateResult = WKPermissionDecisionPrompt;
        requestGeolocationPermissionDelegateResult = true;
        break;
    case GeolocationPermissionState::DeniedFromPrompt:
        queryPermissionDelegateResult = WKPermissionDecisionPrompt;
        requestGeolocationPermissionDelegateResult = false;
        break;
    }

    didReceiveMessage = false;
    didReceiveQueryPermission = false;

    NSString *scriptWithGeolocationRequestedSincePageLoad = @"<script>"
        "let success = () => {"
        "    navigator.permissions.query({ name : 'geolocation' }).then((permissionStatus) => {"
        "        window.webkit.messageHandlers.msg.postMessage(permissionStatus.state);"
        "    }, () => {"
        "        window.webkit.messageHandlers.msg.postMessage('FAIL');"
        "    });"
        "};"
        "let failure = () => {"
        "    navigator.permissions.query({ name : 'geolocation' }).then((permissionStatus) => {"
        "        window.webkit.messageHandlers.msg.postMessage(permissionStatus.state);"
        "    }, () => {"
        "        window.webkit.messageHandlers.msg.postMessage('FAIL');"
        "    });"
        "};"
        "navigator.geolocation.getCurrentPosition(success, failure);"
        "</script>";

    NSString *scriptWithoutGeolocationRequestedSincePageLoad = @"<script>"
        "navigator.permissions.query({ name : 'geolocation' }).then((permissionStatus) => {"
        "    window.webkit.messageHandlers.msg.postMessage(permissionStatus.state);"
        "}, () => {"
        "    window.webkit.messageHandlers.msg.postMessage('FAIL');"
        "});"
        "</script>";

    if (requestGeolocationSincePageLoad == RequestGeolocationSincePageLoad::Yes)
        [webView synchronouslyLoadHTMLString:scriptWithGeolocationRequestedSincePageLoad baseURL:[NSURL URLWithString:@"https://example.com/"]];
    else
        [webView synchronouslyLoadHTMLString:scriptWithoutGeolocationRequestedSincePageLoad baseURL:[NSURL URLWithString:@"https://example.com/"]];

    TestWebKitAPI::Util::run(&didReceiveMessage);
    EXPECT_STREQ(((NSString *)[scriptMessage body]).UTF8String, expectedResult.utf8().data());
}

TEST(PermissionsAPI, GeolocationPermissionGrantedFromPromptAndGeolocationRequestedSinceLoad)
{
    testPermissionsAPIForGeolocation(GeolocationPermissionState::GrantedFromPrompt, RequestGeolocationSincePageLoad::Yes, "granted"_s);
}

TEST(PermissionsAPI, GeolocationPermissionGrantedFromPromptAndGeolocationNotRequestedSinceLoad)
{
    testPermissionsAPIForGeolocation(GeolocationPermissionState::GrantedFromPrompt, RequestGeolocationSincePageLoad::No, "prompt"_s);
}

TEST(PermissionsAPI, GeolocationPermissionDeniedFromPromptAndGeolocationRequestedSinceLoad)
{
    testPermissionsAPIForGeolocation(GeolocationPermissionState::DeniedFromPrompt, RequestGeolocationSincePageLoad::Yes, "denied"_s);
}

TEST(PermissionsAPI, GeolocationPermissionDeniedFromPromptAndGeolocationNotRequestedSinceLoad)
{
    testPermissionsAPIForGeolocation(GeolocationPermissionState::DeniedFromPrompt, RequestGeolocationSincePageLoad::No, "prompt"_s);
}

TEST(PermissionsAPI, GeolocationPermissionGrantedPermanentlyAndGeolocationRequestedSinceLoad)
{
    testPermissionsAPIForGeolocation(GeolocationPermissionState::GrantedPermanently, RequestGeolocationSincePageLoad::Yes, "granted"_s);
}

TEST(PermissionsAPI, GeolocationPermissionGrantedPermanentlyAndGeolocationNotRequestedSinceLoad)
{
    testPermissionsAPIForGeolocation(GeolocationPermissionState::GrantedPermanently, RequestGeolocationSincePageLoad::No, "granted"_s);
}

TEST(PermissionsAPI, GeolocationPermissionDeniedPermanentlyAndGeolocationRequestedSinceLoad)
{
    testPermissionsAPIForGeolocation(GeolocationPermissionState::DeniedPermanently, RequestGeolocationSincePageLoad::Yes, "denied"_s);
}

TEST(PermissionsAPI, GeolocationPermissionDeniedPermanentlyAndGeolocationNotRequestedSinceLoad)
{
    testPermissionsAPIForGeolocation(GeolocationPermissionState::DeniedPermanently, RequestGeolocationSincePageLoad::No, "prompt"_s);
}

} // namespace TestWebKitAPI
