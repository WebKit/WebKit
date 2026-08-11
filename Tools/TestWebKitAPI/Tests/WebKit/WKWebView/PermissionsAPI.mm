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
#import "TestURLSchemeHandler.h"
#import <WebKit/WKContext.h>
#import <WebKit/WKFrameInfo.h>
#import <WebKit/WKGeolocationManager.h>
#import <WebKit/WKGeolocationPosition.h>
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/WKSecurityOrigin.h>
#import <WebKit/WKSecurityOriginRef.h>
#import <WebKit/WKString.h>
#import <WebKit/WKWebViewConfigurationPrivate.h>
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/_WKFeature.h>
#import <wtf/text/StringBuilder.h>

static bool permissionsDidReceiveMessage;
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
    permissionsDidReceiveMessage = true;
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

#if ENABLE(IPC_TESTING_API)

static Vector<String> gObservedGeolocationOriginHosts;
static bool gDidReceiveGeolocationRequestForForgeryTest;

@interface ForgedOriginGeolocationDelegate : NSObject<WKUIDelegate>
@end

@implementation ForgedOriginGeolocationDelegate
- (void)_webView:(WKWebView *)webView queryPermission:(NSString *)name forOrigin:(WKSecurityOrigin *)origin completionHandler:(void (^)(WKPermissionDecision))completionHandler
{
    completionHandler(WKPermissionDecisionPrompt);
}

- (void)_webView:(WKWebView *)webView requestGeolocationPermissionForFrame:(WKFrameInfo *)frame decisionHandler:(void (^)(BOOL))decisionHandler
{
    gObservedGeolocationOriginHosts.append(String(frame.securityOrigin.host));
    gDidReceiveGeolocationRequestForForgeryTest = true;
    decisionHandler(NO);
}
@end

#endif // ENABLE(IPC_TESTING_API)


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
    permissionsDidReceiveMessage = false;
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

    TestWebKitAPI::Util::run(&permissionsDidReceiveMessage);
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
    permissionsDidReceiveMessage = false;
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

    TestWebKitAPI::Util::run(&permissionsDidReceiveMessage);
    EXPECT_STREQ(((NSString *)[scriptMessage body]).UTF8String, "granted");

    permissionsDidReceiveMessage = false;
    queryPermissionDelegateResult = WKPermissionDecisionPrompt;

    auto originString = adoptWK(WKStringCreateWithUTF8CString("https://example.com/"));
    auto origin = adoptWK(WKSecurityOriginCreateFromString(originString.get()));
    [WKWebView _permissionChanged:@"geolocation" forOrigin:(__bridge WKSecurityOrigin *)origin.get()];

    TestWebKitAPI::Util::run(&permissionsDidReceiveMessage);
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

    permissionsDidReceiveMessage = false;
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

    TestWebKitAPI::Util::run(&permissionsDidReceiveMessage);
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

#if ENABLE(IPC_TESTING_API)

// A compromised WebContent process may forge FrameInfoData::securityOrigin in the
// RequestGeolocationPermissionForFrame IPC. The origin presented to the UIDelegate must be
// re-derived UI-side from WebFrameProxy::securityOrigin() rather than trusted from the message.
TEST(PermissionsAPI, GeolocationFrameInfoOriginIgnoresWebProcessForgery)
{
    gObservedGeolocationOriginHosts = { };
    gDidReceiveGeolocationRequestForForgeryTest = false;

    RetainPtr pool = adoptNS([[WKProcessPool alloc] init]);
    WKGeolocationProviderV1 providerCallback;
    zeroBytes(providerCallback);
    providerCallback.base.version = 1;
    WKGeolocationManagerSetProvider(WKContextGetGeolocationManager((WKContextRef)pool.get()), &providerCallback.base);

    RetainPtr configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    configuration.get().processPool = pool.get();
    for (_WKFeature *feature in [WKPreferences _features]) {
        if ([feature.key isEqualToString:@"IPCTestingAPIEnabled"]) {
            [[configuration preferences] _setEnabled:YES forFeature:feature];
            break;
        }
    }

    // Captures the raw bytes of the legitimate RequestGeolocationPermissionForFrame IPC,
    // byte-replaces every occurrence of the page's real host with a same-length forged host,
    // and replays the message.
    static constexpr auto byteReplayHTML =
    "<script>"
    "if (window.IPC) {"
    "    const msgName = IPC.messages.WebPageProxy_RequestGeolocationPermissionForFrame.name;"
    "    const enc = new TextEncoder();"
    "    const realBytes = enc.encode('localhost');"
    "    const evilBytes = enc.encode('evil.host');"
    "    let replayed = false;"
    "    IPC.addOutgoingMessageListener('UI', function (desc) {"
    "        if (!desc || desc.name !== msgName || replayed || !desc.buffer) return;"
    "        replayed = true;"
    "        const mutated = new Uint8Array(new Uint8Array(desc.buffer));"
    "        outer: for (let i = 0; i + realBytes.length <= mutated.length; i++) {"
    "            for (let j = 0; j < realBytes.length; j++) if (mutated[i+j] !== realBytes[j]) continue outer;"
    "            for (let j = 0; j < evilBytes.length; j++) mutated[i+j] = evilBytes[j];"
    "            i += realBytes.length - 1;"
    "        }"
    "        IPC.sendMessage('UI', desc.destinationID, msgName, mutated.slice(16));"
    "    });"
    "}"
    "navigator.geolocation.getCurrentPosition(function(){}, function(){});"
    "</script>"_s;

    RetainPtr schemeHandler = adoptNS([[TestURLSchemeHandler alloc] init]);
    [schemeHandler setStartURLSchemeTaskHandler:^(WKWebView *, id<WKURLSchemeTask> task) {
        RetainPtr response = adoptNS([[NSURLResponse alloc] initWithURL:task.request.URL MIMEType:@"text/html" expectedContentLength:0 textEncodingName:nil]);
        [task didReceiveResponse:response.get()];
        [task didReceiveData:[NSData dataWithBytes:byteReplayHTML.characters() length:byteReplayHTML.length()]];
        [task didFinish];
    }];
    [configuration setURLSchemeHandler:schemeHandler.get() forURLScheme:@"wcptest"];

    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500) configuration:configuration.get()]);
    RetainPtr delegate = adoptNS([[ForgedOriginGeolocationDelegate alloc] init]);
    [webView setUIDelegate:delegate.get()];

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"wcptest://localhost/"]]];

    TestWebKitAPI::Util::run(&gDidReceiveGeolocationRequestForForgeryTest);
    // Give the replayed message a chance to dispatch to the delegate.
    TestWebKitAPI::Util::runFor(0.25_s);

    EXPECT_GT(gObservedGeolocationOriginHosts.size(), 0u);
    for (auto& host : gObservedGeolocationOriginHosts) {
        EXPECT_WK_STREQ(host, "localhost"_s);
        EXPECT_FALSE(host.contains("evil"_s));
    }
}

#endif // ENABLE(IPC_TESTING_API)

} // namespace TestWebKitAPI
