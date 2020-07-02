/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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

#import "PlatformUtilities.h"
#import "ServiceWorkerTCPServer.h"
#import "TestNavigationDelegate.h"
#import "TestURLSchemeHandler.h"
#import "TestWKWebView.h"
#import "WKWebViewConfigurationExtras.h"
#import <WebCore/RegistrableDomain.h>
#import <WebCore/RuntimeApplicationChecks.h>
#import <WebKit/WKHTTPCookieStorePrivate.h>
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/WKURLSchemeTaskPrivate.h>
#import <WebKit/WKUserContentControllerPrivate.h>
#import <WebKit/WKWebsiteDataStorePrivate.h>
#import <WebKit/_WKUserContentWorld.h>
#import <WebKit/_WKUserStyleSheet.h>
#import <wtf/RunLoop.h>
#import <wtf/text/WTFString.h>

#if PLATFORM(IOS_FAMILY)

#if USE(APPLE_INTERNAL_SDK)
#import <WebKitAdditions/InAppBrowserPrivacyTestAdditions.h>

static bool isDone;

@interface AppBoundDomainDelegate : NSObject <WKNavigationDelegate>
- (void)waitForDidFinishNavigation;
- (NSError *)waitForDidFailProvisionalNavigationError;
@end

@implementation AppBoundDomainDelegate {
    bool _navigationFinished;
    RetainPtr<NSError> _provisionalNavigationFailedError;
}

- (void)webView:(WKWebView *)webView didFinishNavigation:(WKNavigation *)navigation
{
    _navigationFinished = true;
}

- (void)webView:(WKWebView *)webView didFailProvisionalNavigation:(null_unspecified WKNavigation *)navigation withError:(NSError *)error
{
    _provisionalNavigationFailedError = error;
}

- (void)waitForDidFinishNavigation
{
    TestWebKitAPI::Util::run(&_navigationFinished);
}

- (NSError *)waitForDidFailProvisionalNavigationError
{
    while (!_provisionalNavigationFailedError)
        TestWebKitAPI::Util::spinRunLoop();
    return _provisionalNavigationFailedError.autorelease();
}

@end

static NSString * const userScriptSource = @"window.wkUserScriptInjected = true";
static bool mainFrameReceivedScriptSource = false;
static bool subFrameReceivedScriptSource = false;

@interface InAppBrowserSchemeHandler : NSObject <WKURLSchemeHandler>
@end

@implementation InAppBrowserSchemeHandler

- (void)webView:(WKWebView *)webView startURLSchemeTask:(id <WKURLSchemeTask>)task
{
    NSString *response = nil;
    if ([task.request.URL.path isEqualToString:@"/in-app-browser-privacy-test-user-script"])
        response = @"<body id = 'body'><script>window.wkUserScriptInjected = false;</script>";
    else if ([task.request.URL.path isEqualToString:@"/in-app-browser-privacy-test-user-agent-script"])
        response = @"<script> window.wkUserScriptInjected = true; </script>";
    else if ([task.request.URL.path isEqualToString:@"/in-app-browser-privacy-test-user-style-sheets"])
        response = @"<body style='background-color: red;'></body>";
    else if ([task.request.URL.path isEqualToString:@"/in-app-browser-privacy-test-user-style-sheets-iframe"])
        response = @"<body style='background-color: red;'><iframe src='in-app-browser:///in-app-browser-privacy-test-user-style-sheets'></iframe></body>";
    else if ([task.request.URL.path isEqualToString:@"/in-app-browser-privacy-test-message-handler"])
        response = @"<body style='background-color: green;'></body><script>if (window.webkit.messageHandlers)\nwindow.webkit.messageHandlers.testHandler.postMessage('Failed'); \nelse \n document.body.style.background = 'red';</script>";
    else if ([task.request.URL.path isEqualToString:@"/app-bound-domain-load"])
        response = @"<body></body>";
    else if ([task.request.URL.path isEqualToString:@"/in-app-browser-privacy-test-user-script-iframe"])
        response = @"<body id = 'body'></body><iframe src='in-app-browser://apple.com/in-app-browser-privacy-test-user-script' id='nestedFrame'></iframe></body>";
    else if ([task.request.URL.path isEqualToString:@"/in-app-browser-privacy-test-user-script-nested-iframe"])
        response = @"<body id = 'body'></body><iframe src='in-app-browser://nonAppBoundDomain/in-app-browser-privacy-test-user-script-iframe' id='iframe'></iframe></body>";
    else if (((id<WKURLSchemeTaskPrivate>)task)._frame.isMainFrame && [task.request.URL.path isEqualToString:@"/should-load-for-main-frame-only"])
        mainFrameReceivedScriptSource = true;
    else if ([task.request.URL.path isEqualToString:@"/should-load-for-main-frame-only"])
        subFrameReceivedScriptSource = true;

    [task didReceiveResponse:[[[NSURLResponse alloc] initWithURL:task.request.URL MIMEType:@"text/html" expectedContentLength:response.length textEncodingName:nil] autorelease]];
    [task didReceiveData:[response dataUsingEncoding:NSUTF8StringEncoding]];
    [task didFinish];
}

- (void)webView:(WKWebView *)webView stopURLSchemeTask:(id <WKURLSchemeTask>)task
{
}

@end

static void cleanUpInAppBrowserPrivacyTestSettings()
{
    WebCore::clearApplicationBundleIdentifierTestingOverride();
    IN_APP_BROWSER_PRIVACY_ADDITIONS_2
}

static void initializeInAppBrowserPrivacyTestSettings()
{
    WTF::initializeMainThread();
    WebCore::clearApplicationBundleIdentifierTestingOverride();
    IN_APP_BROWSER_PRIVACY_ADDITIONS
}

TEST(InAppBrowserPrivacy, NonAppBoundDomainFailedUserScriptAtStart)
{
    isDone = false;
    initializeInAppBrowserPrivacyTestSettings();
    auto userScript = adoptNS([[WKUserScript alloc] initWithSource:userScriptSource injectionTime:WKUserScriptInjectionTimeAtDocumentStart forMainFrameOnly:YES]);

    WKWebViewConfiguration *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];
    auto schemeHandler = adoptNS([[InAppBrowserSchemeHandler alloc] init]);
    [configuration setURLSchemeHandler:schemeHandler.get() forURLScheme:@"in-app-browser"];
    [configuration.userContentController addUserScript:userScript.get()];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectZero configuration:configuration]);
    NSURLRequest *request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"in-app-browser:///in-app-browser-privacy-test-user-script"]];
    [webView loadRequest:request];
    [webView _test_waitForDidFinishNavigation];

    // Check that request to read this variable is rejected.
    [webView evaluateJavaScript:@"window.wkUserScriptInjected" completionHandler:^(id _Nullable result, NSError * _Nullable error) {
        EXPECT_FALSE(result);
        EXPECT_TRUE(!!error);
        EXPECT_EQ(error.code, WKErrorJavaScriptAppBoundDomain);
        isDone = true;
    }];

    isDone = false;
    TestWebKitAPI::Util::run(&isDone);

    // Disable script injection blocking to check that original attempt to set this variable was rejected.
    [[[webView configuration] preferences] _setNeedsInAppBrowserPrivacyQuirks:YES];
    [webView evaluateJavaScript:@"window.wkUserScriptInjected" completionHandler:^(id _Nullable result, NSError * _Nullable error) {
        EXPECT_EQ(NO, [result boolValue]);
        EXPECT_FALSE(!!error);
        cleanUpInAppBrowserPrivacyTestSettings();
        isDone = true;
    }];

    isDone = false;
    TestWebKitAPI::Util::run(&isDone);
}

TEST(InAppBrowserPrivacy, NonAppBoundDomainFailedUserScriptAtEnd)
{
    isDone = false;
    initializeInAppBrowserPrivacyTestSettings();
    auto userScript = adoptNS([[WKUserScript alloc] initWithSource:userScriptSource injectionTime:WKUserScriptInjectionTimeAtDocumentEnd forMainFrameOnly:YES]);
    
    WKWebViewConfiguration *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];
    auto schemeHandler = adoptNS([[InAppBrowserSchemeHandler alloc] init]);
    [configuration setURLSchemeHandler:schemeHandler.get() forURLScheme:@"in-app-browser"];
    [configuration.userContentController addUserScript:userScript.get()];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectZero configuration:configuration]);
    NSURLRequest *request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"in-app-browser:///in-app-browser-privacy-test-user-script"]];
    [webView loadRequest:request];
    [webView _test_waitForDidFinishNavigation];

    // Check that request to read this variable is rejected.
    [webView evaluateJavaScript:@"window.wkUserScriptInjected" completionHandler:^(id _Nullable result, NSError * _Nullable error) {
        EXPECT_FALSE(result);
        EXPECT_TRUE(!!error);
        EXPECT_EQ(error.code, WKErrorJavaScriptAppBoundDomain);
        isDone = true;
    }];

    isDone = false;
    TestWebKitAPI::Util::run(&isDone);

    // Disable script injection blocking to check that original attempt to set this variable was rejected.
    [[[webView configuration] preferences] _setNeedsInAppBrowserPrivacyQuirks:YES];
    [webView evaluateJavaScript:@"window.wkUserScriptInjected" completionHandler:^(id _Nullable result, NSError * _Nullable error) {
        EXPECT_EQ(NO, [result boolValue]);
        EXPECT_FALSE(!!error);
        cleanUpInAppBrowserPrivacyTestSettings();
        isDone = true;
    }];

    isDone = false;
    TestWebKitAPI::Util::run(&isDone);
}

TEST(InAppBrowserPrivacy, NonAppBoundDomainFailedUserAgentScripts)
{
    initializeInAppBrowserPrivacyTestSettings();

    WKWebViewConfiguration *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];
    // Disable blocking of restricted APIs for test setup.
    [[configuration preferences] _setNeedsInAppBrowserPrivacyQuirks:YES];

    auto schemeHandler = adoptNS([[InAppBrowserSchemeHandler alloc] init]);
    [configuration setURLSchemeHandler:schemeHandler.get() forURLScheme:@"in-app-browser"];
    
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectZero configuration:configuration]);
    NSURLRequest *request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"in-app-browser:///in-app-browser-privacy-test-user-agent-script"]];
    [webView loadRequest:request];
    [webView _test_waitForDidFinishNavigation];

    [webView evaluateJavaScript:@"window.wkUserScriptInjected" completionHandler:^(id _Nullable result, NSError * _Nullable error) {
        EXPECT_EQ(YES, [result boolValue]);
        isDone = true;
    }];

    isDone = false;
    TestWebKitAPI::Util::run(&isDone);
    
    // Enable blocking of restricted APIs.
    [[configuration preferences] _setNeedsInAppBrowserPrivacyQuirks:NO];

    auto webView2 = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectZero configuration:configuration]);
    isDone = false;
    [webView2 loadRequest:request];
    [webView2 _test_waitForDidFinishNavigation];

    [webView2 evaluateJavaScript:@"window.wkUserScriptInjected" completionHandler:^(id _Nullable result, NSError * _Nullable error) {
        EXPECT_FALSE(result);
        EXPECT_TRUE(!!error);
        EXPECT_EQ(error.code, WKErrorJavaScriptAppBoundDomain);
        cleanUpInAppBrowserPrivacyTestSettings();
        isDone = true;
    }];

    isDone = false;
    TestWebKitAPI::Util::run(&isDone);
}

TEST(InAppBrowserPrivacy, AppBoundDomains)
{
    initializeInAppBrowserPrivacyTestSettings();
    isDone = false;
    [[WKWebsiteDataStore defaultDataStore] _appBoundDomains:^(NSArray<NSString *> *domains) {
        NSArray *domainsToCompare = @[@"127.0.0.1", @"apple.com", @"bar.com", @"example.com", @"foo.com", @"localhost", @"testdomain1",  @"webkit.org"];

        NSArray *sortedDomains = [domains sortedArrayUsingSelector:@selector(caseInsensitiveCompare:)];

        int length = [sortedDomains count];
        EXPECT_EQ(length, 8);
        for (int i = 0; i < length; i++)
            EXPECT_WK_STREQ([sortedDomains objectAtIndex:i], [domainsToCompare objectAtIndex:i]);

        cleanUpInAppBrowserPrivacyTestSettings();
        isDone = true;
    }];
    TestWebKitAPI::Util::run(&isDone);
}

TEST(InAppBrowserPrivacy, AppBoundSchemes)
{
    initializeInAppBrowserPrivacyTestSettings();
    isDone = false;
    [[WKWebsiteDataStore defaultDataStore] _appBoundSchemes:^(NSArray<NSString *> *schemes) {
        NSArray *schemesToCompare = @[@"app-bound-custom-scheme", @"test"];

        NSArray *sortedSchemes = [schemes sortedArrayUsingSelector:@selector(caseInsensitiveCompare:)];

        int length = [sortedSchemes count];
        EXPECT_EQ(length, 2);
        for (int i = 0; i < length; i++)
            EXPECT_WK_STREQ([sortedSchemes objectAtIndex:i], [schemesToCompare objectAtIndex:i]);

        cleanUpInAppBrowserPrivacyTestSettings();
        isDone = true;
    }];
    TestWebKitAPI::Util::run(&isDone);
}

TEST(InAppBrowserPrivacy, LocalFilesAreAppBound)
{
    initializeInAppBrowserPrivacyTestSettings();
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectZero configuration:configuration.get()]);
    NSURLRequest *request = [NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"in-app-browser-privacy-local-file" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]];
    [webView loadRequest:request];
    [webView _test_waitForDidFinishNavigation];

    isDone = false;
    [webView _isForcedIntoAppBoundMode:^(BOOL isForcedIntoAppBoundMode) {
        EXPECT_TRUE(isForcedIntoAppBoundMode);
        cleanUpInAppBrowserPrivacyTestSettings();
        isDone = true;
    }];
    TestWebKitAPI::Util::run(&isDone);
}

TEST(InAppBrowserPrivacy, DataFilesAreAppBound)
{
    initializeInAppBrowserPrivacyTestSettings();
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectZero configuration:configuration.get()]);
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"data:text/html,start"]]];
    [webView _test_waitForDidFinishNavigation];

    isDone = false;
    [webView _isForcedIntoAppBoundMode:^(BOOL isForcedIntoAppBoundMode) {
        EXPECT_TRUE(isForcedIntoAppBoundMode);
        cleanUpInAppBrowserPrivacyTestSettings();
        isDone = true;
    }];
    TestWebKitAPI::Util::run(&isDone);
}

TEST(InAppBrowserPrivacy, AboutFilesAreAppBound)
{
    initializeInAppBrowserPrivacyTestSettings();
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectZero configuration:configuration.get()]);
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"about:blank"]]];
    [webView _test_waitForDidFinishNavigation];

    isDone = false;
    [webView _isForcedIntoAppBoundMode:^(BOOL isForcedIntoAppBoundMode) {
        EXPECT_TRUE(isForcedIntoAppBoundMode);
        cleanUpInAppBrowserPrivacyTestSettings();
        isDone = true;
    }];
    TestWebKitAPI::Util::run(&isDone);
}

static NSString *styleSheetSource = @"body { background-color: green !important; }";
static NSString *backgroundColorScript = @"window.getComputedStyle(document.body, null).getPropertyValue('background-color')";
static const char* redInRGB = "rgb(255, 0, 0)";
static const char* blackInRGB = "rgba(0, 0, 0, 0)";

static void expectScriptEvaluatesToColor(WKWebView *webView, NSString *script, const char* color)
{
    static bool didCheckBackgroundColor;

    [webView evaluateJavaScript:script completionHandler:^(id value, NSError * error) {
        EXPECT_TRUE([value isKindOfClass:[NSString class]]);
        EXPECT_WK_STREQ(color, value);
        didCheckBackgroundColor = true;
    }];

    TestWebKitAPI::Util::run(&didCheckBackgroundColor);
    didCheckBackgroundColor = false;
}

TEST(InAppBrowserPrivacy, NonAppBoundUserStyleSheetForSpecificWebViewFails)
{
    initializeInAppBrowserPrivacyTestSettings();

    RetainPtr<WKWebViewConfiguration> configuration = adoptNS([[WKWebViewConfiguration alloc] init]);

    auto schemeHandler = adoptNS([[InAppBrowserSchemeHandler alloc] init]);
    [configuration setURLSchemeHandler:schemeHandler.get() forURLScheme:@"in-app-browser"];
    RetainPtr<_WKUserContentWorld> world = [_WKUserContentWorld worldWithName:@"TestWorld"];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectZero configuration:configuration.get()]);
    RetainPtr<_WKUserStyleSheet> styleSheet = adoptNS([[_WKUserStyleSheet alloc] initWithSource:styleSheetSource forWKWebView:webView.get() forMainFrameOnly:YES level:_WKUserStyleUserLevel userContentWorld:world.get()]);
    [[configuration userContentController] _addUserStyleSheet:styleSheet.get()];

    auto delegate = adoptNS([AppBoundDomainDelegate new]);
    [webView setNavigationDelegate:delegate.get()];

    NSURLRequest *request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"in-app-browser:///in-app-browser-privacy-test-user-style-sheets"]];
    [webView loadRequest:request];
    NSError *error = [delegate waitForDidFailProvisionalNavigationError];
    EXPECT_EQ(error.code, WKErrorNavigationAppBoundDomain);
    cleanUpInAppBrowserPrivacyTestSettings();
}

TEST(InAppBrowserPrivacy, NonAppBoundUserStyleSheetForAllWebViewsFails)
{
    initializeInAppBrowserPrivacyTestSettings();

    RetainPtr<WKWebViewConfiguration> configuration = adoptNS([[WKWebViewConfiguration alloc] init]);

    auto schemeHandler = adoptNS([[InAppBrowserSchemeHandler alloc] init]);
    [configuration setURLSchemeHandler:schemeHandler.get() forURLScheme:@"in-app-browser"];
    
    RetainPtr<_WKUserStyleSheet> styleSheet = adoptNS([[_WKUserStyleSheet alloc] initWithSource:styleSheetSource forMainFrameOnly:YES]);
    [[configuration userContentController] _addUserStyleSheet:styleSheet.get()];
    
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectZero configuration:configuration.get()]);
    auto delegate = adoptNS([AppBoundDomainDelegate new]);
    [webView setNavigationDelegate:delegate.get()];

    NSURLRequest *request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"in-app-browser:///in-app-browser-privacy-test-user-style-sheets"]];
    [webView loadRequest:request];
    NSError *error = [delegate waitForDidFailProvisionalNavigationError];
    EXPECT_EQ(error.code, WKErrorNavigationAppBoundDomain);
    cleanUpInAppBrowserPrivacyTestSettings();
}

TEST(InAppBrowserPrivacy, NonAppBoundUserStyleSheetAffectingAllFramesFails)
{
    initializeInAppBrowserPrivacyTestSettings();

    RetainPtr<WKWebViewConfiguration> configuration = adoptNS([[WKWebViewConfiguration alloc] init]);

    auto schemeHandler = adoptNS([[InAppBrowserSchemeHandler alloc] init]);
    [configuration setURLSchemeHandler:schemeHandler.get() forURLScheme:@"in-app-browser"];
    RetainPtr<_WKUserStyleSheet> styleSheet = adoptNS([[_WKUserStyleSheet alloc] initWithSource:styleSheetSource forMainFrameOnly:NO]);
    [[configuration userContentController] _addUserStyleSheet:styleSheet.get()];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectZero configuration:configuration.get()]);
    auto delegate = adoptNS([AppBoundDomainDelegate new]);
    [webView setNavigationDelegate:delegate.get()];

    NSURLRequest *request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"in-app-browser:///in-app-browser-privacy-test-user-style-sheets-iframe"]];
    [webView loadRequest:request];
    NSError *error = [delegate waitForDidFailProvisionalNavigationError];
    EXPECT_EQ(error.code, WKErrorNavigationAppBoundDomain);
    cleanUpInAppBrowserPrivacyTestSettings();
}

TEST(InAppBrowserPrivacy, NonAppBoundDomainCannotAccessMessageHandlers)
{
    initializeInAppBrowserPrivacyTestSettings();
    isDone = false;

    RetainPtr<WKWebViewConfiguration> configuration = adoptNS([[WKWebViewConfiguration alloc] init]);

    auto schemeHandler = adoptNS([[InAppBrowserSchemeHandler alloc] init]);
    [configuration setURLSchemeHandler:schemeHandler.get() forURLScheme:@"in-app-browser"];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectZero configuration:configuration.get()]);

    [webView performAfterReceivingMessage:@"Failed" action:^() {
        FAIL();
    }];

    NSURLRequest *request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"in-app-browser:///in-app-browser-privacy-test-message-handler"]];
    [webView loadRequest:request];
    [webView _test_waitForDidFinishNavigation];
    
    // Disable script injection blocking to check that the request for message
    // handlers returned null.
    [[configuration preferences] _setNeedsInAppBrowserPrivacyQuirks:YES];

    // Set the background color to red if message handlers returned null so we can
    // check without needing a message handler.
    expectScriptEvaluatesToColor(webView.get(), backgroundColorScript, redInRGB);
    cleanUpInAppBrowserPrivacyTestSettings();
}

TEST(InAppBrowserPrivacy, AppBoundDomainCanAccessMessageHandlers)
{
    initializeInAppBrowserPrivacyTestSettings();
    isDone = false;

    RetainPtr<WKWebViewConfiguration> configuration = adoptNS([[WKWebViewConfiguration alloc] init]);

    auto schemeHandler = adoptNS([[InAppBrowserSchemeHandler alloc] init]);
    [configuration setURLSchemeHandler:schemeHandler.get() forURLScheme:@"in-app-browser"];
    [configuration setLimitsNavigationsToAppBoundDomains:YES];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectZero configuration:configuration.get()]);

    [webView performAfterReceivingMessage:@"Passed" action:^() {
        isDone = true;
    }];

    // Navigate to an app-bound domain and wait for the 'pass' message to test the handler is working fine.
    NSURLRequest *request = [NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"in-app-browser-privacy-local-file" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]];
    [webView loadRequest:request];
    [webView _test_waitForDidFinishNavigation];

    TestWebKitAPI::Util::run(&isDone);

    [webView performAfterReceivingMessage:@"Failed" action:^() {
        FAIL();
    }];

    cleanUpInAppBrowserPrivacyTestSettings();
}

static RetainPtr<WKHTTPCookieStore> globalCookieStore;
static bool gotFlag = false;

static void setUpCookieTest()
{
    globalCookieStore = [[WKWebsiteDataStore defaultDataStore] httpCookieStore];
    NSArray<NSHTTPCookie *> *cookies = nil;
    [globalCookieStore getAllCookies:[cookiesPtr = &cookies](NSArray<NSHTTPCookie *> *nsCookies) {
        *cookiesPtr = [nsCookies retain];
        gotFlag = true;
    }];

    TestWebKitAPI::Util::run(&gotFlag);

    for (id cookie in cookies) {
        gotFlag = false;
        [globalCookieStore deleteCookie:cookie completionHandler:[]() {
            gotFlag = true;
        }];
        TestWebKitAPI::Util::run(&gotFlag);
    }

    cookies = nil;
    gotFlag = false;
    [globalCookieStore getAllCookies:[cookiesPtr = &cookies](NSArray<NSHTTPCookie *> *nsCookies) {
        *cookiesPtr = [nsCookies retain];
        gotFlag = true;
    }];

    TestWebKitAPI::Util::run(&gotFlag);

    // Make sure the cookie store starts out empty.
    ASSERT_EQ(cookies.count, 0u);
    [cookies release];

    gotFlag = false;
}

TEST(InAppBrowserPrivacy, SetCookieForNonAppBoundDomainFails)
{
    initializeInAppBrowserPrivacyTestSettings();
    [[NSUserDefaults standardUserDefaults] setBool:YES forKey:@"WebKitDebugIsInAppBrowserPrivacyEnabled"];

    auto dataStore = [WKWebsiteDataStore defaultDataStore];
    auto webView = adoptNS([TestWKWebView new]);
    [webView loadHTMLString:@"Oh hello" baseURL:[NSURL URLWithString:@"http://webkit.org"]];
    [webView _test_waitForDidFinishNavigation];

    setUpCookieTest();
    globalCookieStore = [dataStore httpCookieStore];

    NSArray<NSHTTPCookie *> *cookies = nil;

    // Non app-bound cookie.
    RetainPtr<NSHTTPCookie> nonAppBoundCookie = [NSHTTPCookie cookieWithProperties:@{
        NSHTTPCookiePath: @"/",
        NSHTTPCookieName: @"nonAppBoundName",
        NSHTTPCookieValue: @"nonAppBoundValue",
        NSHTTPCookieDomain: @"nonAppBoundDomain.com",
    }];

    // App-bound cookie.
    RetainPtr<NSHTTPCookie> appBoundCookie = [NSHTTPCookie cookieWithProperties:@{
        NSHTTPCookiePath: @"/",
        NSHTTPCookieName: @"webKitName",
        NSHTTPCookieValue: @"webKitValue",
        NSHTTPCookieDomain: @"www.webkit.org",
    }];

    gotFlag = false;
    // This should not actually set a cookie.
    [globalCookieStore setCookie:nonAppBoundCookie.get() completionHandler:[]() {
        gotFlag = true;
    }];

    TestWebKitAPI::Util::run(&gotFlag);
    gotFlag = false;

    // This should successfully set a cookie.
    [globalCookieStore setCookie:appBoundCookie.get() completionHandler:[]() {
        gotFlag = true;
    }];

    TestWebKitAPI::Util::run(&gotFlag);

    cleanUpInAppBrowserPrivacyTestSettings();
    [[NSUserDefaults standardUserDefaults] setBool:NO forKey:@"WebKitDebugIsInAppBrowserPrivacyEnabled"];
    gotFlag = false;

    // Check the cookie store to make sure only one cookie was set.
    [globalCookieStore getAllCookies:[cookiesPtr = &cookies](NSArray<NSHTTPCookie *> *nsCookies) {
        *cookiesPtr = [nsCookies retain];
        gotFlag = true;
    }];

    TestWebKitAPI::Util::run(&gotFlag);

    ASSERT_EQ(cookies.count, 1u);

    [cookies release];
    gotFlag = false;
    [globalCookieStore deleteCookie:appBoundCookie.get() completionHandler:[]() {
        gotFlag = true;
    }];

    TestWebKitAPI::Util::run(&gotFlag);
}

TEST(InAppBrowserPrivacy, GetCookieForNonAppBoundDomainFails)
{
    // Since we can't set non-app-bound cookies with In-App Browser privacy protections on,
    // we can turn the protections off to set a cookie we will then try to get with protections enabled.
    [[NSUserDefaults standardUserDefaults] setBool:NO forKey:@"WebKitDebugIsInAppBrowserPrivacyEnabled"];

    setUpCookieTest();
    globalCookieStore = [[WKWebsiteDataStore defaultDataStore] httpCookieStore];
    NSArray<NSHTTPCookie *> *cookies = nil;

    // Non app-bound cookie.
    RetainPtr<NSHTTPCookie> nonAppBoundCookie = [NSHTTPCookie cookieWithProperties:@{
        NSHTTPCookiePath: @"/",
        NSHTTPCookieName: @"CookieName",
        NSHTTPCookieValue: @"CookieValue",
        NSHTTPCookieDomain: @"nonAppBoundDomain.com",
    }];

    // App-bound cookie.
    RetainPtr<NSHTTPCookie> appBoundCookie = [NSHTTPCookie cookieWithProperties:@{
        NSHTTPCookiePath: @"/",
        NSHTTPCookieName: @"OtherCookieName",
        NSHTTPCookieValue: @"OtherCookieValue",
        NSHTTPCookieDomain: @"www.webkit.org",
    }];

    auto webView = adoptNS([TestWKWebView new]);
    [webView synchronouslyLoadHTMLString:@"start network process"];

    // This should successfully set a cookie because protections are off.
    [globalCookieStore setCookie:nonAppBoundCookie.get() completionHandler:[]() {
        gotFlag = true;
    }];

    TestWebKitAPI::Util::run(&gotFlag);
    gotFlag = false;

    [globalCookieStore setCookie:appBoundCookie.get() completionHandler:[]() {
        gotFlag = true;
    }];

    TestWebKitAPI::Util::run(&gotFlag);

    gotFlag = false;
    [globalCookieStore getAllCookies:[cookiesPtr = &cookies](NSArray<NSHTTPCookie *> *nsCookies) {
        *cookiesPtr = [nsCookies retain];
        gotFlag = true;
    }];

    TestWebKitAPI::Util::run(&gotFlag);

    // Confirm both cookies are in the store.
    ASSERT_EQ(cookies.count, 2u);

    // Now enable protections and ensure we can only retrieve the app-bound cookies.
    [[NSUserDefaults standardUserDefaults] setBool:YES forKey:@"WebKitDebugIsInAppBrowserPrivacyEnabled"];
    initializeInAppBrowserPrivacyTestSettings();

    gotFlag = false;
    [globalCookieStore getAllCookies:[cookiesPtr = &cookies](NSArray<NSHTTPCookie *> *nsCookies) {
        *cookiesPtr = [nsCookies retain];
        gotFlag = true;
    }];

    TestWebKitAPI::Util::run(&gotFlag);

    ASSERT_EQ(cookies.count, 1u);

    [cookies release];

    gotFlag = false;
    [globalCookieStore deleteCookie:nonAppBoundCookie.get() completionHandler:[]() {
        gotFlag = true;
    }];

    TestWebKitAPI::Util::run(&gotFlag);

    gotFlag = false;
    [globalCookieStore deleteCookie:appBoundCookie.get() completionHandler:[]() {
        // Reset flag.
        [[NSUserDefaults standardUserDefaults] setBool:NO forKey:@"WebKitDebugIsInAppBrowserPrivacyEnabled"];
        cleanUpInAppBrowserPrivacyTestSettings();
        gotFlag = true;
    }];

    TestWebKitAPI::Util::run(&gotFlag);
}

TEST(InAppBrowserPrivacy, GetCookieForURLFails)
{
    // Since we can't set non-app-bound cookies with In-App Browser privacy protections on,
    // we can turn the protections off to set a cookie we will then try to get with protections enabled.
    [[NSUserDefaults standardUserDefaults] setBool:NO forKey:@"WebKitDebugIsInAppBrowserPrivacyEnabled"];
    setUpCookieTest();

    globalCookieStore = [[WKWebsiteDataStore defaultDataStore] httpCookieStore];
    NSHTTPCookie *nonAppBoundCookie = [NSHTTPCookie cookieWithProperties:@{
        NSHTTPCookiePath: @"/",
        NSHTTPCookieName: @"nonAppBoundName",
        NSHTTPCookieValue: @"nonAppBoundValue",
        NSHTTPCookieDomain: @"nonAppBoundDomain.com",
    }];
    
    NSHTTPCookie *appBoundCookie = [NSHTTPCookie cookieWithProperties:@{
        NSHTTPCookiePath: @"/",
        NSHTTPCookieName: @"webKitName",
        NSHTTPCookieValue: @"webKitValue",
        NSHTTPCookieDomain: @"webkit.org",
    }];

    __block bool done = false;
    auto webView = adoptNS([TestWKWebView new]);
    [webView synchronouslyLoadHTMLString:@"start network process"];
    [globalCookieStore setCookie:nonAppBoundCookie completionHandler:^{
        [globalCookieStore setCookie:appBoundCookie completionHandler:^{

            // Now enable protections and ensure we can only retrieve the app-bound cookies.
            [[NSUserDefaults standardUserDefaults] setBool:YES forKey:@"WebKitDebugIsInAppBrowserPrivacyEnabled"];
            initializeInAppBrowserPrivacyTestSettings();

            [globalCookieStore _getCookiesForURL:[NSURL URLWithString:@"https://webkit.org/"] completionHandler:^(NSArray<NSHTTPCookie *> *cookies) {
                EXPECT_EQ(cookies.count, 1ull);
                EXPECT_WK_STREQ(cookies[0].name, "webKitName");
                [globalCookieStore _getCookiesForURL:[NSURL URLWithString:@"https://nonAppBoundDomain.com/"] completionHandler:^(NSArray<NSHTTPCookie *> *cookies) {
                    EXPECT_EQ(cookies.count, 0u);
                    [globalCookieStore deleteCookie:nonAppBoundCookie completionHandler:^{
                        [globalCookieStore deleteCookie:appBoundCookie completionHandler:^{
                            [[NSUserDefaults standardUserDefaults] setBool:NO forKey:@"WebKitDebugIsInAppBrowserPrivacyEnabled"];
                            cleanUpInAppBrowserPrivacyTestSettings();
                            done = true;
                        }];
                    }];
                }];
            }];
        }];
    }];
    TestWebKitAPI::Util::run(&done);
}

@interface SWInAppBrowserPrivacyMessageHandler : NSObject <WKScriptMessageHandler>
@end

@implementation SWInAppBrowserPrivacyMessageHandler
- (void)userContentController:(WKUserContentController *)userContentController didReceiveScriptMessage:(WKScriptMessage *)message
{
    EXPECT_WK_STREQ(@"Message from worker: ServiceWorker received: Hello from an app-bound domain", [message body]);
    isDone = true;
}
@end

static const char* mainBytes = R"SWRESOURCE(
<script>

function log(msg)
{
    window.webkit.messageHandlers.sw.postMessage(msg);
}

navigator.serviceWorker.addEventListener("message", function(event) {
    log("Message from worker: " + event.data);
});

try {

navigator.serviceWorker.register('/sw.js').then(function(reg) {
    worker = reg.installing ? reg.installing : reg.active;
    worker.postMessage("Hello from an app-bound domain");
}).catch(function(error) {
    log("Registration failed with: " + error);
});
} catch(e) {
    log("Exception: " + e);
}

</script>
)SWRESOURCE";

static const char* scriptBytes = R"SWRESOURCE(

self.addEventListener("message", (event) => {
    event.source.postMessage("ServiceWorker received: " + event.data);
});

)SWRESOURCE";

TEST(InAppBrowserPrivacy, AppBoundDomainAllowsServiceWorkers)
{
    initializeInAppBrowserPrivacyTestSettings();
    isDone = false;

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto messageHandler = adoptNS([[SWInAppBrowserPrivacyMessageHandler alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:messageHandler.get() name:@"sw"];
    [configuration preferences]._serviceWorkerEntitlementDisabledForTesting = YES;
    [configuration setLimitsNavigationsToAppBoundDomains:YES];

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    ServiceWorkerTCPServer server1({
        { "text/html", mainBytes },
        { "application/javascript", scriptBytes},
    });

    [WKWebsiteDataStore _allowWebsiteDataRecordsForAllOrigins];

    // Start with a clean slate data store
    [[WKWebsiteDataStore defaultDataStore] removeDataOfTypes:[WKWebsiteDataStore allWebsiteDataTypes] modifiedSince:[NSDate distantPast] completionHandler:^() {
        isDone = true;
    }];
    TestWebKitAPI::Util::run(&isDone);
    isDone = false;
    
    // Expect the service worker load to complete successfully.
    [webView loadRequest:server1.requestWithLocalhost()];
    TestWebKitAPI::Util::run(&isDone);
    isDone = false;

    [[WKWebsiteDataStore defaultDataStore] fetchDataRecordsOfTypes:[NSSet setWithObject:WKWebsiteDataTypeServiceWorkerRegistrations] completionHandler:^(NSArray<WKWebsiteDataRecord *> *websiteDataRecords) {
        EXPECT_EQ(1u, [websiteDataRecords count]);
        EXPECT_WK_STREQ(websiteDataRecords[0].displayName, "localhost");
        isDone = true;
    }];

    TestWebKitAPI::Util::run(&isDone);
    isDone = false;

    // Reset service worker entitlement.
    [webView _clearServiceWorkerEntitlementOverride:^(void) {
        cleanUpInAppBrowserPrivacyTestSettings();
        isDone = true;
    }];
    TestWebKitAPI::Util::run(&isDone);
    isDone = false;
}

TEST(InAppBrowserPrivacy, NonAppBoundDomainDoesNotAllowServiceWorkers)
{
    initializeInAppBrowserPrivacyTestSettings();
    isDone = false;

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto schemeHandler = adoptNS([[InAppBrowserSchemeHandler alloc] init]);
    [configuration setURLSchemeHandler:schemeHandler.get() forURLScheme:@"in-app-browser"];
    // Disable entitlement which allows service workers.
    [configuration preferences]._serviceWorkerEntitlementDisabledForTesting = YES;

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    // Load a non-app bound domain.
    NSURLRequest *request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"in-app-browser:///in-app-browser-privacy-test-user-style-sheets"]];
    [webView loadRequest:request];
    [webView _test_waitForDidFinishNavigation];
    
    // Expect that service workers are disabled for this webView.
    isDone = false;
    [webView _serviceWorkersEnabled:^(BOOL enabled) {
        EXPECT_FALSE(enabled);
        isDone = true;
    }];
    TestWebKitAPI::Util::run(&isDone);
    
    // Reset service worker entitlement.
    [webView _clearServiceWorkerEntitlementOverride:^(void) {
        cleanUpInAppBrowserPrivacyTestSettings();
        isDone = true;
    }];
    TestWebKitAPI::Util::run(&isDone);
    isDone = false;
}

TEST(InAppBrowserPrivacy, AppBoundFlagForNonAppBoundDomainFails)
{
    initializeInAppBrowserPrivacyTestSettings();
    isDone = false;

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto schemeHandler = adoptNS([[InAppBrowserSchemeHandler alloc] init]);
    [configuration setURLSchemeHandler:schemeHandler.get() forURLScheme:@"in-app-browser"];
    [configuration setLimitsNavigationsToAppBoundDomains:YES];

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    auto delegate = adoptNS([AppBoundDomainDelegate new]);
    [webView setNavigationDelegate:delegate.get()];

    // Load a non-app bound domain.
    NSURLRequest *request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"in-app-browser:///in-app-browser-privacy-test-user-style-sheets"]];
    [webView loadRequest:request];
    NSError *error = [delegate waitForDidFailProvisionalNavigationError];
    EXPECT_EQ(error.code, WKErrorNavigationAppBoundDomain);

    // Make sure the load didn't complete by checking the background color.
    // Red would indicate it finished loading.
    expectScriptEvaluatesToColor(webView.get(), backgroundColorScript, blackInRGB);
    cleanUpInAppBrowserPrivacyTestSettings();
}

TEST(InAppBrowserPrivacy, NavigateAwayFromAppBoundDomainWithAppBoundFlagFails)
{
    initializeInAppBrowserPrivacyTestSettings();
    isDone = false;

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto schemeHandler = adoptNS([[InAppBrowserSchemeHandler alloc] init]);
    [configuration setURLSchemeHandler:schemeHandler.get() forURLScheme:@"in-app-browser"];
    [configuration setLimitsNavigationsToAppBoundDomains:YES];

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    auto delegate = adoptNS([AppBoundDomainDelegate new]);
    [webView setNavigationDelegate:delegate.get()];
    
    // Navigate to an app-bound domain and expect a successful load.
    NSURLRequest *request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"in-app-browser://apple.com/app-bound-domain-load"]];
    [webView loadRequest:request];
    [delegate waitForDidFinishNavigation];
    
    // Now try to load a non-app bound domain and make sure it fails.
    request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"in-app-browser:///in-app-browser-privacy-test-user-style-sheets"]];
    [webView loadRequest:request];
    NSError *error = [delegate waitForDidFailProvisionalNavigationError];
    EXPECT_EQ(error.code, WKErrorNavigationAppBoundDomain);

    // Make sure the load didn't complete by checking the background color.
    // Red would indicate it finished loading.
    expectScriptEvaluatesToColor(webView.get(), backgroundColorScript, blackInRGB);
    cleanUpInAppBrowserPrivacyTestSettings();
}

TEST(InAppBrowserPrivacy, AppBoundDomainWithoutFlagTreatedAsNonAppBound)
{
    initializeInAppBrowserPrivacyTestSettings();
    isDone = false;

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto schemeHandler = adoptNS([[InAppBrowserSchemeHandler alloc] init]);
    [configuration setURLSchemeHandler:schemeHandler.get() forURLScheme:@"in-app-browser"];

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    auto delegate = adoptNS([AppBoundDomainDelegate new]);
    [webView setNavigationDelegate:delegate.get()];
    
    // Navigate to an app-bound domain and expect a successful load.
    NSURLRequest *request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"in-app-browser://apple.com/app-bound-domain-load"]];
    [webView loadRequest:request];
    [delegate waitForDidFinishNavigation];

    // But the navigation should not have been considered app-bound because the WebView configuration
    // specification was not set.
    isDone = false;
    [webView _isNavigatingToAppBoundDomain:^(BOOL isAppBound) {
        EXPECT_FALSE(isAppBound);
        cleanUpInAppBrowserPrivacyTestSettings();
        isDone = true;
    }];
    TestWebKitAPI::Util::run(&isDone);
}

TEST(InAppBrowserPrivacy, WebViewWithoutAppBoundFlagCanFreelyNavigate)
{
    initializeInAppBrowserPrivacyTestSettings();
    isDone = false;

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto schemeHandler = adoptNS([[InAppBrowserSchemeHandler alloc] init]);
    [configuration setURLSchemeHandler:schemeHandler.get() forURLScheme:@"in-app-browser"];

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    auto delegate = adoptNS([AppBoundDomainDelegate new]);
    [webView setNavigationDelegate:delegate.get()];
    
    // Navigate to an app-bound domain and expect a successful load.
    NSURLRequest *request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"in-app-browser://apple.com/app-bound-domain-load"]];
    [webView loadRequest:request];
    [delegate waitForDidFinishNavigation];

    isDone = false;
    [webView _isNavigatingToAppBoundDomain:^(BOOL isAppBound) {
        EXPECT_FALSE(isAppBound);
        isDone = true;
    }];
    TestWebKitAPI::Util::run(&isDone);

    // Navigate to an non app-bound domain and expect a successful load.
    request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"in-app-browser://in-app-browser-privacy-test-user-style-sheets"]];
    [webView loadRequest:request];
    [delegate waitForDidFinishNavigation];
    
    isDone = false;
    [webView _isNavigatingToAppBoundDomain:^(BOOL isAppBound) {
        EXPECT_FALSE(isAppBound);
        isDone = true;
    }];
    TestWebKitAPI::Util::run(&isDone);

    // Navigation should be successful, but this WebView should not get app-bound domain
    // privileges like script injection.
    isDone = false;
    [webView evaluateJavaScript:@"window.wkUserScriptInjected" completionHandler:^(id _Nullable result, NSError * _Nullable error) {
        EXPECT_TRUE(!!error);
        EXPECT_EQ(error.code, WKErrorJavaScriptAppBoundDomain);
        isDone = true;
    }];

    TestWebKitAPI::Util::run(&isDone);
}

TEST(InAppBrowserPrivacy, WebViewCannotUpdateAppBoundFlagOnceSet)
{
    initializeInAppBrowserPrivacyTestSettings();
    isDone = false;

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto schemeHandler = adoptNS([[InAppBrowserSchemeHandler alloc] init]);
    [configuration setURLSchemeHandler:schemeHandler.get() forURLScheme:@"in-app-browser"];
    [configuration setLimitsNavigationsToAppBoundDomains:YES];

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    auto delegate = adoptNS([AppBoundDomainDelegate new]);
    [webView setNavigationDelegate:delegate.get()];
    
    // Navigate to an app-bound domain and expect a successful load.
    NSURLRequest *request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"in-app-browser://apple.com/app-bound-domain-load"]];
    [webView loadRequest:request];
    [delegate waitForDidFinishNavigation];

    isDone = false;
    [webView _isNavigatingToAppBoundDomain:^(BOOL isAppBound) {
        EXPECT_TRUE(isAppBound);
        isDone = true;
    }];
    TestWebKitAPI::Util::run(&isDone);

    [[webView configuration] setLimitsNavigationsToAppBoundDomains:NO];
    // Now try to load a non-app bound domain and make sure it fails.
    request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"in-app-browser:///in-app-browser-privacy-test-user-style-sheets"]];
    [webView loadRequest:request];
    NSError *error = [delegate waitForDidFailProvisionalNavigationError];
    EXPECT_EQ(error.code, WKErrorNavigationAppBoundDomain);

    cleanUpInAppBrowserPrivacyTestSettings();
}

TEST(InAppBrowserPrivacy, InjectScriptThenNavigateToNonAppBoundDomainFails)
{
    isDone = false;
    initializeInAppBrowserPrivacyTestSettings();
    auto userScript = adoptNS([[WKUserScript alloc] initWithSource:userScriptSource injectionTime:WKUserScriptInjectionTimeAtDocumentStart forMainFrameOnly:YES]);

    WKWebViewConfiguration *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];
    auto schemeHandler = adoptNS([[InAppBrowserSchemeHandler alloc] init]);
    [configuration setURLSchemeHandler:schemeHandler.get() forURLScheme:@"in-app-browser"];
    [configuration.userContentController addUserScript:userScript.get()];
    [[configuration preferences] _setNeedsInAppBrowserPrivacyQuirks:NO];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectZero configuration:configuration]);
    auto delegate = adoptNS([AppBoundDomainDelegate new]);
    [webView setNavigationDelegate:delegate.get()];

    isDone = false;
    [webView evaluateJavaScript:@"window.wkUserScriptInjected" completionHandler:^(id _Nullable result, NSError * _Nullable error) {
        EXPECT_FALSE(!!error);
        isDone = true;
    }];

    TestWebKitAPI::Util::run(&isDone);
    
    // Load a non-app bound domain.
    NSURLRequest *request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"in-app-browser:///in-app-browser-privacy-test-user-agent-script"]];
    [webView loadRequest:request];
    NSError *error = [delegate waitForDidFailProvisionalNavigationError];
    EXPECT_EQ(error.code, WKErrorNavigationAppBoundDomain);
}

TEST(InAppBrowserPrivacy, InjectScriptInNonAppBoundSubframeAppBoundMainframeFails)
{
    isDone = false;
    initializeInAppBrowserPrivacyTestSettings();

    auto userScript = adoptNS([[WKUserScript alloc] initWithSource:@"var img = document.createElement('img'); img.src = 'in-app-browser:///should-load-for-main-frame-only'; document.getElementById('body').appendChild(img);" injectionTime:WKUserScriptInjectionTimeAtDocumentStart forMainFrameOnly:NO]);

    WKWebViewConfiguration *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];
    auto schemeHandler = adoptNS([[InAppBrowserSchemeHandler alloc] init]);
    [configuration setURLSchemeHandler:schemeHandler.get() forURLScheme:@"in-app-browser"];
    [[configuration preferences] _setNeedsInAppBrowserPrivacyQuirks:NO];
    [configuration setLimitsNavigationsToAppBoundDomains:YES];
    [[configuration userContentController] addUserScript:userScript.get()];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectZero configuration:configuration]);
    auto delegate = adoptNS([AppBoundDomainDelegate new]);
    [webView setNavigationDelegate:delegate.get()];

    // Load an app-bound domain with an iframe that is not app-bound.
    NSURLRequest *request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"in-app-browser://apple.com/in-app-browser-privacy-test-user-script-nested-iframe"]];
    [webView loadRequest:request];
    [delegate waitForDidFinishNavigation];

    EXPECT_TRUE(mainFrameReceivedScriptSource);
    EXPECT_FALSE(subFrameReceivedScriptSource);
    cleanUpInAppBrowserPrivacyTestSettings();
}

static NSMutableSet<WKFrameInfo *> *allFrames;

TEST(InAppBrowserPrivacy, JavaScriptInNonAppBoundFrameFails)
{
    allFrames = [[NSMutableSet<WKFrameInfo *> alloc] init];

    isDone = false;
    initializeInAppBrowserPrivacyTestSettings();

    auto userScript = adoptNS([[WKUserScript alloc] initWithSource:@"var img = document.createElement('img'); img.src = 'in-app-browser:///should-load-for-main-frame-only'; document.getElementById('body').appendChild(img);" injectionTime:WKUserScriptInjectionTimeAtDocumentStart forMainFrameOnly:NO]);

    WKWebViewConfiguration *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];
    auto schemeHandler = adoptNS([[InAppBrowserSchemeHandler alloc] init]);
    [configuration setURLSchemeHandler:schemeHandler.get() forURLScheme:@"in-app-browser"];
    [[configuration preferences] _setNeedsInAppBrowserPrivacyQuirks:NO];
    [configuration setLimitsNavigationsToAppBoundDomains:YES];
    [[configuration userContentController] addUserScript:userScript.get()];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectZero configuration:configuration]);
    auto navigationDelegate = adoptNS([[TestNavigationDelegate alloc] init]);

    __block bool didFinishNavigation = false;
    [navigationDelegate setDidFinishNavigation:^(WKWebView *, WKNavigation *) {
        didFinishNavigation = true;
    }];

    [navigationDelegate setDecidePolicyForNavigationAction:[&] (WKNavigationAction *action, void (^decisionHandler)(WKNavigationActionPolicy)) {
        if (action.targetFrame)
            [allFrames addObject:action.targetFrame];

        decisionHandler(WKNavigationActionPolicyAllow);
    }];

    [webView setNavigationDelegate:navigationDelegate.get()];
    
    // Load an app-bound domain with an iframe that is not app-bound.
    NSURLRequest *request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"in-app-browser://apple.com/in-app-browser-privacy-test-user-script-nested-iframe"]];
    [webView loadRequest:request];

    TestWebKitAPI::Util::run(&didFinishNavigation);

    EXPECT_EQ(allFrames.count, 3u);

    static size_t finishedFrames = 0;
    static bool isDone = false;

    for (WKFrameInfo *frame in allFrames) {
        bool isMainFrame = frame.isMainFrame;
        [webView callAsyncJavaScript:@"return location.href;" arguments:nil inFrame:frame inContentWorld:WKContentWorld.defaultClientWorld completionHandler:[isMainFrame] (id result, NSError *error) {
            if (isMainFrame) {
                EXPECT_TRUE([result isKindOfClass:[NSString class]]);
                EXPECT_FALSE(!!error);
                EXPECT_TRUE([result isEqualToString:@"in-app-browser://apple.com/in-app-browser-privacy-test-user-script-nested-iframe"]);
            } else {
                EXPECT_TRUE(!!error);
                EXPECT_EQ(error.code, WKErrorJavaScriptAppBoundDomain);
            }

            if (++finishedFrames == allFrames.count * 2)
                isDone = true;
        }];


        [webView evaluateJavaScript:@"location.href;" inFrame:frame inContentWorld:WKContentWorld.defaultClientWorld completionHandler:[isMainFrame] (id result, NSError *error) {
            if (isMainFrame) {
                EXPECT_TRUE([result isKindOfClass:[NSString class]]);
                EXPECT_FALSE(!!error);
                EXPECT_TRUE([result isEqualToString:@"in-app-browser://apple.com/in-app-browser-privacy-test-user-script-nested-iframe"]);
            } else {
                EXPECT_TRUE(!!error);
                EXPECT_EQ(error.code, WKErrorJavaScriptAppBoundDomain);
            }

            if (++finishedFrames == allFrames.count * 2)
                isDone = true;
        }];
    }
    
    TestWebKitAPI::Util::run(&isDone);
}

TEST(InAppBrowserPrivacy, WebViewCategory)
{
    initializeInAppBrowserPrivacyTestSettings();

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    EXPECT_EQ([configuration _webViewCategory], _WKWebViewCategoryAppBoundDomain);

    [configuration _setWebViewCategory:_WKWebViewCategoryInAppBrowser];
    EXPECT_EQ([configuration _webViewCategory], _WKWebViewCategoryInAppBrowser);

    [configuration _setWebViewCategory:_WKWebViewCategoryWebBrowser];
    EXPECT_EQ([configuration _webViewCategory], _WKWebViewCategoryWebBrowser);

    [configuration _setWebViewCategory:_WKWebViewCategoryAppBoundDomain];
    EXPECT_EQ([configuration _webViewCategory], _WKWebViewCategoryAppBoundDomain);

    cleanUpInAppBrowserPrivacyTestSettings();
}

TEST(InAppBrowserPrivacy, LoadFromHTMLStringsSucceedsIfAppBound)
{
    initializeInAppBrowserPrivacyTestSettings();
    isDone = false;

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto schemeHandler = adoptNS([[InAppBrowserSchemeHandler alloc] init]);
    [configuration setURLSchemeHandler:schemeHandler.get() forURLScheme:@"in-app-browser"];

    auto delegate = adoptNS([AppBoundDomainDelegate new]);

    NSString *HTML = @"<html><head></head><body><img src='in-app-browser:///in-app-browser-privacy-test-user-style-sheets/'></img></body></html>";
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    [webView setNavigationDelegate:delegate.get()];

    [webView loadHTMLString:HTML baseURL:[NSURL URLWithString:@"in-app-browser://apple.com/"]];
    [delegate waitForDidFinishNavigation];

    cleanUpInAppBrowserPrivacyTestSettings();
}

TEST(InAppBrowserPrivacy, LoadFromHTMLStringNoBaseURLIsAppBound)
{
    initializeInAppBrowserPrivacyTestSettings();
    isDone = false;

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto schemeHandler = adoptNS([[InAppBrowserSchemeHandler alloc] init]);
    [configuration setURLSchemeHandler:schemeHandler.get() forURLScheme:@"in-app-browser"];

    auto delegate = adoptNS([AppBoundDomainDelegate new]);

    NSString *HTML = @"<html><head></head><body><img src='in-app-browser:///in-app-browser-privacy-test-user-style-sheets/'></img></body></html>";
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    [webView setNavigationDelegate:delegate.get()];

    [webView loadHTMLString:HTML baseURL:nil];
    [delegate waitForDidFinishNavigation];

    isDone = false;
    [webView _isForcedIntoAppBoundMode:^(BOOL isForcedIntoAppBoundMode) {
        EXPECT_TRUE(isForcedIntoAppBoundMode);
        cleanUpInAppBrowserPrivacyTestSettings();
        isDone = true;
    }];
    TestWebKitAPI::Util::run(&isDone);
}

TEST(InAppBrowserPrivacy, LoadFromHTMLStringsFailsIfNotAppBound)
{
    initializeInAppBrowserPrivacyTestSettings();
    isDone = false;

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto schemeHandler = adoptNS([[InAppBrowserSchemeHandler alloc] init]);
    [configuration setURLSchemeHandler:schemeHandler.get() forURLScheme:@"in-app-browser"];

    auto delegate = adoptNS([AppBoundDomainDelegate new]);

    NSString *HTML = @"<html><head></head><body><img src='in-app-browser:///in-app-browser-privacy-test-user-style-sheets/'></img></body></html>";
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    [webView setNavigationDelegate:delegate.get()];

    [webView loadHTMLString:HTML baseURL:[NSURL URLWithString:@"in-app-browser:///in-app-browser-privacy-test-user-agent-script"]];
    NSError *error = [delegate waitForDidFailProvisionalNavigationError];
    EXPECT_EQ(error.code, WKErrorNavigationAppBoundDomain);

    isDone = false;
    [webView _isForcedIntoAppBoundMode:^(BOOL isForcedIntoAppBoundMode) {
        EXPECT_TRUE(isForcedIntoAppBoundMode);
        cleanUpInAppBrowserPrivacyTestSettings();
        isDone = true;
    }];
    TestWebKitAPI::Util::run(&isDone);
}

TEST(InAppBrowserPrivacy, AppBoundCustomScheme)
{
    initializeInAppBrowserPrivacyTestSettings();
    isDone = false;

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto schemeHandler = adoptNS([[TestURLSchemeHandler alloc] init]);
    [configuration setURLSchemeHandler:schemeHandler.get() forURLScheme:@"test"];
    [configuration setLimitsNavigationsToAppBoundDomains:YES];

    [schemeHandler setStartURLSchemeTaskHandler:^(WKWebView *, id<WKURLSchemeTask> task) {
        auto response = adoptNS([[NSURLResponse alloc] initWithURL:task.request.URL MIMEType:@"text/html" expectedContentLength:0 textEncodingName:nil]);
        [task didReceiveResponse:response.get()];
        [task didReceiveData:[NSData dataWithBytes:mainBytes length:strlen(mainBytes)]];
        [task didFinish];
    }];

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    
    NSURLRequest *request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"test://host/main.html"]];

    [webView loadRequest:request];
    [webView _test_waitForDidFinishNavigation];

    isDone = false;
    [webView _isNavigatingToAppBoundDomain:^(BOOL isAppBound) {
        EXPECT_TRUE(isAppBound);
        cleanUpInAppBrowserPrivacyTestSettings();
        isDone = true;
    }];
    TestWebKitAPI::Util::run(&isDone);
    
    // Make sure app-bound behavior works for this webview.
    isDone = false;
    [webView evaluateJavaScript:@"location.href;" completionHandler:^(id result, NSError *error) {
        EXPECT_TRUE([result isKindOfClass:[NSString class]]);
        EXPECT_FALSE(!!error);
        EXPECT_TRUE([result isEqualToString:@"test://host/main.html"]);

        isDone = true;
    }];
    
    TestWebKitAPI::Util::run(&isDone);
}

#endif // USE(APPLE_INTERNAL_SDK)

#endif // PLATFORM(IOS_FAMILY)
