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
#import "TestNavigationDelegate.h"
#import "TestWKWebView.h"
#import "WKWebViewConfigurationExtras.h"
#import <WebCore/RegistrableDomain.h>
#import <WebCore/RuntimeApplicationChecks.h>
#import <WebKit/WKHTTPCookieStorePrivate.h>
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/WKUserContentControllerPrivate.h>
#import <WebKit/WKWebsiteDataStorePrivate.h>
#import <WebKit/_WKUserContentWorld.h>
#import <WebKit/_WKUserStyleSheet.h>
#import <wtf/RunLoop.h>
#import <wtf/text/WTFString.h>

#if PLATFORM(IOS_FAMILY)

#if USE(APPLE_INTERNAL_SDK)
#include <WebKitAdditions/InAppBrowserPrivacyTestAdditions.h>

static bool isDone;

static NSString * const userScriptSource = @"window.wkUserScriptInjected = true";

@interface InAppBrowserSchemeHandler : NSObject <WKURLSchemeHandler>
@end

@implementation InAppBrowserSchemeHandler

- (void)webView:(WKWebView *)webView startURLSchemeTask:(id <WKURLSchemeTask>)task
{
    NSString *response = nil;
    if ([task.request.URL.path isEqualToString:@"/in-app-browser-privacy-test-user-script"])
        response = @"<script>window.wkUserScriptInjected = false;</script>";
    else if ([task.request.URL.path isEqualToString:@"/in-app-browser-privacy-test-user-agent-script"])
        response = @"<script> window.wkUserScriptInjected = true; </script>";
    else if ([task.request.URL.path isEqualToString:@"/in-app-browser-privacy-test-user-style-sheets"])
        response = @"<body style='background-color: red;'></body>";
    else if ([task.request.URL.path isEqualToString:@"/in-app-browser-privacy-test-user-style-sheets-iframe"])
        response = @"<body style='background-color: red;'><iframe src='in-app-browser:///in-app-browser-privacy-test-user-style-sheets'></iframe></body>";
    else if ([task.request.URL.path isEqualToString:@"/in-app-browser-privacy-test-message-handler"])
        response = @"<body style='background-color: green;'></body><script>if (window.webkit.messageHandlers)\nwindow.webkit.messageHandlers.testHandler.postMessage('Failed'); \nelse \n document.body.style.background = 'red';</script>";

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
    RunLoop::initializeMainRunLoop();
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
    [[configuration preferences] _setNeedsInAppBrowserPrivacyQuirks:NO];
    [[configuration preferences] _setInAppBrowserPrivacyEnabled:YES];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectZero configuration:configuration]);
    NSURLRequest *request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"in-app-browser:///in-app-browser-privacy-test-user-script"]];
    [webView loadRequest:request];
    [webView _test_waitForDidFinishNavigation];

    // Check that request to read this variable is rejected.
    [webView evaluateJavaScript:@"window.wkUserScriptInjected" completionHandler:^(id _Nullable result, NSError * _Nullable error) {
        EXPECT_FALSE(result);
        EXPECT_TRUE(!!error);
        isDone = true;
    }];

    isDone = false;
    TestWebKitAPI::Util::run(&isDone);

    // Turn back on In-App Browser Privacy quirks to check that original attempt to set this variable was rejected.
    isDone = false;
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
    [[configuration preferences] _setNeedsInAppBrowserPrivacyQuirks:NO];
    [[configuration preferences] _setInAppBrowserPrivacyEnabled:YES];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectZero configuration:configuration]);
    NSURLRequest *request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"in-app-browser:///in-app-browser-privacy-test-user-script"]];
    [webView loadRequest:request];
    [webView _test_waitForDidFinishNavigation];

    // Check that request to read this variable is rejected.
    [webView evaluateJavaScript:@"window.wkUserScriptInjected" completionHandler:^(id _Nullable result, NSError * _Nullable error) {
        EXPECT_FALSE(result);
        EXPECT_TRUE(!!error);
        isDone = true;
    }];

    isDone = false;
    TestWebKitAPI::Util::run(&isDone);

    // Turn back on In-App Browser Privacy quirks to check that original attempt to set this variable was rejected.
    isDone = false;
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
    WKWebViewConfiguration *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];
    [[configuration preferences] _setNeedsInAppBrowserPrivacyQuirks:YES];
    [[configuration preferences] _setInAppBrowserPrivacyEnabled:NO];

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

    initializeInAppBrowserPrivacyTestSettings();
    [[configuration preferences] _setNeedsInAppBrowserPrivacyQuirks:NO];
    [[configuration preferences] _setInAppBrowserPrivacyEnabled:YES];

    auto webView2 = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectZero configuration:configuration]);

    isDone = false;
    [webView2 loadRequest:request];
    [webView2 _test_waitForDidFinishNavigation];

    [webView2 evaluateJavaScript:@"window.wkUserScriptInjected" completionHandler:^(id _Nullable result, NSError * _Nullable error) {
        EXPECT_FALSE(result);
        EXPECT_TRUE(!!error);
        cleanUpInAppBrowserPrivacyTestSettings();
        isDone = true;
    }];

    isDone = false;
    TestWebKitAPI::Util::run(&isDone);
}

TEST(InAppBrowserPrivacy, SwapBackToAppBoundRejectsUserScript)
{
    initializeInAppBrowserPrivacyTestSettings();
    auto userScript = adoptNS([[WKUserScript alloc] initWithSource:userScriptSource injectionTime:WKUserScriptInjectionTimeAtDocumentStart forMainFrameOnly:YES]);

    WKWebViewConfiguration *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];
    [[configuration preferences] _setNeedsInAppBrowserPrivacyQuirks:NO];
    [[configuration preferences] _setInAppBrowserPrivacyEnabled:YES];

    auto schemeHandler = adoptNS([[InAppBrowserSchemeHandler alloc] init]);
    [configuration setURLSchemeHandler:schemeHandler.get() forURLScheme:@"in-app-browser"];
    
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectZero configuration:configuration]);
    NSURLRequest *request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"in-app-browser:///in-app-browser-privacy-test-user-script"]];
    [webView loadRequest:request];
    [webView _test_waitForDidFinishNavigation];

    [configuration.userContentController _addUserScriptImmediately:userScript.get()];
    [webView evaluateJavaScript:@"window.wkUserScriptInjected" completionHandler:^(id _Nullable result, NSError * _Nullable error) {
        EXPECT_FALSE(result);
        EXPECT_TRUE(!!error);
        isDone = true;
    }];

    isDone = false;
    TestWebKitAPI::Util::run(&isDone);

    request = [NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"in-app-browser-privacy-local-file" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]];
    [webView loadRequest:request];
    [webView _test_waitForDidFinishNavigation];
    
    isDone = false;
    [configuration.userContentController _addUserScriptImmediately:userScript.get()];
    [webView evaluateJavaScript:@"window.wkUserScriptInjected" completionHandler:^(id _Nullable result, NSError * _Nullable error) {
        EXPECT_FALSE(result);
        EXPECT_TRUE(!!error);
        cleanUpInAppBrowserPrivacyTestSettings();
        isDone = true;
    }];

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

TEST(InAppBrowserPrivacy, LocalFilesAreAppBound)
{
    initializeInAppBrowserPrivacyTestSettings();
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [[configuration preferences] _setInAppBrowserPrivacyEnabled:YES];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectZero configuration:configuration.get()]);
    NSURLRequest *request = [NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"in-app-browser-privacy-local-file" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]];
    [webView loadRequest:request];
    [webView _test_waitForDidFinishNavigation];

    isDone = false;
    [webView _isNavigatingToAppBoundDomain:^(BOOL isAppBound) {
        EXPECT_TRUE(isAppBound);
        cleanUpInAppBrowserPrivacyTestSettings();
        isDone = true;
    }];
    TestWebKitAPI::Util::run(&isDone);
}

TEST(InAppBrowserPrivacy, DataFilesAreAppBound)
{
    initializeInAppBrowserPrivacyTestSettings();
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [[configuration preferences] _setInAppBrowserPrivacyEnabled:YES];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectZero configuration:configuration.get()]);
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"data:text/html,start"]]];
    [webView _test_waitForDidFinishNavigation];

    isDone = false;
    [webView _isNavigatingToAppBoundDomain:^(BOOL isAppBound) {
        EXPECT_TRUE(isAppBound);
        cleanUpInAppBrowserPrivacyTestSettings();
        isDone = true;
    }];
    TestWebKitAPI::Util::run(&isDone);
}

TEST(InAppBrowserPrivacy, AboutFilesAreAppBound)
{
    initializeInAppBrowserPrivacyTestSettings();
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [[configuration preferences] _setInAppBrowserPrivacyEnabled:YES];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectZero configuration:configuration.get()]);
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"about:blank"]]];
    [webView _test_waitForDidFinishNavigation];

    isDone = false;
    [webView _isNavigatingToAppBoundDomain:^(BOOL isAppBound) {
        EXPECT_TRUE(isAppBound);
        cleanUpInAppBrowserPrivacyTestSettings();
        isDone = true;
    }];
    TestWebKitAPI::Util::run(&isDone);
}

static NSString *styleSheetSource = @"body { background-color: green !important; }";
static NSString *backgroundColorScript = @"window.getComputedStyle(document.body, null).getPropertyValue('background-color')";
static NSString *frameBackgroundColorScript = @"window.getComputedStyle(document.getElementsByTagName('iframe')[0].contentDocument.body, null).getPropertyValue('background-color')";
static const char* redInRGB = "rgb(255, 0, 0)";

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
    [[configuration preferences] _setInAppBrowserPrivacyEnabled:YES];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectZero configuration:configuration.get()]);
    NSURLRequest *request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"in-app-browser:///in-app-browser-privacy-test-user-style-sheets"]];
    [webView loadRequest:request];
    [webView _test_waitForDidFinishNavigation];

    RetainPtr<_WKUserContentWorld> world = [_WKUserContentWorld worldWithName:@"TestWorld"];
    RetainPtr<_WKUserStyleSheet> styleSheet = adoptNS([[_WKUserStyleSheet alloc] initWithSource:styleSheetSource forWKWebView:webView.get() forMainFrameOnly:YES userContentWorld:world.get()]);
    [[configuration userContentController] _addUserStyleSheet:styleSheet.get()];

    expectScriptEvaluatesToColor(webView.get(), backgroundColorScript, redInRGB);
    cleanUpInAppBrowserPrivacyTestSettings();
}

TEST(InAppBrowserPrivacy, NonAppBoundUserStyleSheetForAllWebViewsFails)
{
    initializeInAppBrowserPrivacyTestSettings();

    RetainPtr<WKWebViewConfiguration> configuration = adoptNS([[WKWebViewConfiguration alloc] init]);

    auto schemeHandler = adoptNS([[InAppBrowserSchemeHandler alloc] init]);
    [configuration setURLSchemeHandler:schemeHandler.get() forURLScheme:@"in-app-browser"];
    [[configuration preferences] _setInAppBrowserPrivacyEnabled:YES];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectZero configuration:configuration.get()]);
    NSURLRequest *request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"in-app-browser:///in-app-browser-privacy-test-user-style-sheets"]];
    [webView loadRequest:request];
    [webView _test_waitForDidFinishNavigation];

    RetainPtr<_WKUserStyleSheet> styleSheet = adoptNS([[_WKUserStyleSheet alloc] initWithSource:styleSheetSource forMainFrameOnly:YES]);
    [[configuration userContentController] _addUserStyleSheet:styleSheet.get()];

    expectScriptEvaluatesToColor(webView.get(), backgroundColorScript, redInRGB);
    cleanUpInAppBrowserPrivacyTestSettings();
}

TEST(InAppBrowserPrivacy, NonAppBoundUserStyleSheetAffectingAllFramesFails)
{
    initializeInAppBrowserPrivacyTestSettings();

    RetainPtr<WKWebViewConfiguration> configuration = adoptNS([[WKWebViewConfiguration alloc] init]);

    auto schemeHandler = adoptNS([[InAppBrowserSchemeHandler alloc] init]);
    [configuration setURLSchemeHandler:schemeHandler.get() forURLScheme:@"in-app-browser"];
    [[configuration preferences] _setInAppBrowserPrivacyEnabled:YES];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectZero configuration:configuration.get()]);
    NSURLRequest *request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"in-app-browser:///in-app-browser-privacy-test-user-style-sheets-iframe"]];
    [webView loadRequest:request];
    [webView _test_waitForDidFinishNavigation];

    RetainPtr<_WKUserStyleSheet> styleSheet = adoptNS([[_WKUserStyleSheet alloc] initWithSource:styleSheetSource forMainFrameOnly:NO]);
    [[configuration userContentController] _addUserStyleSheet:styleSheet.get()];

    // The main frame should be affected.
    expectScriptEvaluatesToColor(webView.get(), backgroundColorScript, redInRGB);

    // The subframe should also be affected.
    expectScriptEvaluatesToColor(webView.get(), frameBackgroundColorScript, redInRGB);
    cleanUpInAppBrowserPrivacyTestSettings();
}

TEST(InAppBrowserPrivacy, NonAppBoundDomainCannotAccessMessageHandlers)
{
    initializeInAppBrowserPrivacyTestSettings();
    isDone = false;

    RetainPtr<WKWebViewConfiguration> configuration = adoptNS([[WKWebViewConfiguration alloc] init]);

    auto schemeHandler = adoptNS([[InAppBrowserSchemeHandler alloc] init]);
    [configuration setURLSchemeHandler:schemeHandler.get() forURLScheme:@"in-app-browser"];
    [[configuration preferences] _setInAppBrowserPrivacyEnabled:YES];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectZero configuration:configuration.get()]);

    [webView performAfterReceivingMessage:@"Failed" action:^() {
        FAIL();
    }];

    NSURLRequest *request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"in-app-browser:///in-app-browser-privacy-test-message-handler"]];
    [webView loadRequest:request];
    [webView _test_waitForDidFinishNavigation];

    // Set the background color to red if message handlers returned null so we can
    // check without needing a message handler.
    expectScriptEvaluatesToColor(webView.get(), backgroundColorScript, redInRGB);
    cleanUpInAppBrowserPrivacyTestSettings();
}

TEST(InAppBrowserPrivacy, AppBoundToNonAppBoundDomainCannotAccessMessageHandlers)
{
    initializeInAppBrowserPrivacyTestSettings();
    isDone = false;

    RetainPtr<WKWebViewConfiguration> configuration = adoptNS([[WKWebViewConfiguration alloc] init]);

    auto schemeHandler = adoptNS([[InAppBrowserSchemeHandler alloc] init]);
    [configuration setURLSchemeHandler:schemeHandler.get() forURLScheme:@"in-app-browser"];
    [[configuration preferences] _setInAppBrowserPrivacyEnabled:YES];

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

    // Navigate away from an app-bound domain.
    NSURLRequest *request2 = [NSURLRequest requestWithURL:[NSURL URLWithString:@"in-app-browser:///in-app-browser-privacy-test-message-handler"]];
    [webView loadRequest:request2];
    [webView _test_waitForDidFinishNavigation];

    // Set the background color to red if message handlers returned null so we can
    // check without needing a message handler.
    expectScriptEvaluatesToColor(webView.get(), backgroundColorScript, redInRGB);
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

#endif // USE(APPLE_INTERNAL_SDK)

#endif // PLATFORM(IOS_FAMILY)
