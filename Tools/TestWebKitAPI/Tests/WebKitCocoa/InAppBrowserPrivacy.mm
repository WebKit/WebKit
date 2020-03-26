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

@interface TestInAppBrowserScriptMessageHandler : NSObject <WKScriptMessageHandler>
@end

@implementation TestInAppBrowserScriptMessageHandler

- (void)userContentController:(WKUserContentController *)userContentController didReceiveScriptMessage:(WKScriptMessage *)message
{
    isDone = true;
}

@end

@interface InAppBrowserSchemeHandler : NSObject <WKURLSchemeHandler>
@end

@implementation InAppBrowserSchemeHandler

- (void)webView:(WKWebView *)webView startURLSchemeTask:(id <WKURLSchemeTask>)task
{
    NSString *response = nil;
    if ([task.request.URL.path isEqualToString:@"/in-app-browser-privacy-test-user-script"])
        response = @"<script>window.webkit.messageHandlers.testInAppBrowserPrivacy.postMessage(\"done\");</script>";
    else if ([task.request.URL.path isEqualToString:@"/in-app-browser-privacy-test-user-agent-script"])
        response = @"<script> window.wkUserScriptInjected = true; </script>";
    else if ([task.request.URL.path isEqualToString:@"/in-app-browser-privacy-test-user-style-sheets"])
        response = @"<body style='background-color: red;'></body>";
    else if ([task.request.URL.path isEqualToString:@"/in-app-browser-privacy-test-user-style-sheets-iframe"])
        response = @"<body style='background-color: red;'><iframe src='in-app-browser:///in-app-browser-privacy-test-user-style-sheets'></iframe></body>";

    [task didReceiveResponse:[[[NSURLResponse alloc] initWithURL:task.request.URL MIMEType:@"text/html" expectedContentLength:response.length textEncodingName:nil] autorelease]];
    [task didReceiveData:[response dataUsingEncoding:NSUTF8StringEncoding]];
    [task didFinish];
}

- (void)webView:(WKWebView *)webView stopURLSchemeTask:(id <WKURLSchemeTask>)task
{
}

@end

static void initializeInAppBrowserPrivacyTestSettings()
{
    RunLoop::initializeMainRunLoop();
    WebCore::clearApplicationBundleIdentifierTestingOverride();
    IN_APP_BROWSER_PRIVACY_ADDITIONS
}

TEST(InAppBrowserPrivacy, NonAppBoundDomainFailedUserScriptAtStart)
{
    auto messageHandler = adoptNS([[TestInAppBrowserScriptMessageHandler alloc] init]);
    auto userScript = adoptNS([[WKUserScript alloc] initWithSource:userScriptSource injectionTime:WKUserScriptInjectionTimeAtDocumentStart forMainFrameOnly:YES]);

    // First set up the configuration without In-App Browser Privacy settings to ensure it works as expected.
    WKWebViewConfiguration *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];
    [[configuration preferences] _setNeedsInAppBrowserPrivacyQuirks:YES];
    [[configuration preferences] _setInAppBrowserPrivacyEnabled:NO];

    auto schemeHandler = adoptNS([[InAppBrowserSchemeHandler alloc] init]);
    [configuration setURLSchemeHandler:schemeHandler.get() forURLScheme:@"in-app-browser"];
    
    [configuration.userContentController addUserScript:userScript.get()];
    [configuration.userContentController addScriptMessageHandler:messageHandler.get() name:@"testInAppBrowserPrivacy"];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectZero configuration:configuration]);
    NSURLRequest *request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"in-app-browser:///in-app-browser-privacy-test-user-script"]];
    [webView loadRequest:request];
    [webView _test_waitForDidFinishNavigation];
    TestWebKitAPI::Util::run(&isDone);
    
    [webView evaluateJavaScript:@"window.wkUserScriptInjected" completionHandler:^(id _Nullable result, NSError * _Nullable error) {
        EXPECT_EQ(YES, [[webView objectByEvaluatingJavaScript:@"window.wkUserScriptInjected"] boolValue]);
        EXPECT_FALSE(!!error);
        isDone = true;
    }];

    isDone = false;
    TestWebKitAPI::Util::run(&isDone);

    // Now setup In-App Browser Privacy settings and expect a failed script evaluation result, and an error message.
    isDone = false;
    initializeInAppBrowserPrivacyTestSettings();
    [[configuration preferences] _setNeedsInAppBrowserPrivacyQuirks:NO];
    [[configuration preferences] _setInAppBrowserPrivacyEnabled:YES];

    auto webView2 = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectZero configuration:configuration]);
    [webView2 loadRequest:request];
    TestWebKitAPI::Util::run(&isDone);

    // Check that request to read this variable is rejected.
    [webView2 evaluateJavaScript:@"window.wkUserScriptInjected" completionHandler:^(id _Nullable result, NSError * _Nullable error) {
        EXPECT_FALSE(result);
        EXPECT_TRUE(!!error);
        isDone = true;
    }];

    isDone = false;
    TestWebKitAPI::Util::run(&isDone);

    // Turn back on In-App Browser Privacy quirks to check that original attempt to set this variable was rejected.
    isDone = false;
    [[[webView2 configuration] preferences] _setNeedsInAppBrowserPrivacyQuirks:YES];
    [webView2 evaluateJavaScript:@"window.wkUserScriptInjected" completionHandler:^(id _Nullable result, NSError * _Nullable error) {
        EXPECT_EQ(NO, [[webView2 objectByEvaluatingJavaScript:@"window.wkUserScriptInjected"] boolValue]);
        EXPECT_FALSE(!!error);
        isDone = true;
    }];

    isDone = false;
    TestWebKitAPI::Util::run(&isDone);
}

TEST(InAppBrowserPrivacy, NonAppBoundDomainFailedUserScriptAtEnd)
{
    auto messageHandler = adoptNS([[TestInAppBrowserScriptMessageHandler alloc] init]);
    auto userScript = adoptNS([[WKUserScript alloc] initWithSource:userScriptSource injectionTime:WKUserScriptInjectionTimeAtDocumentEnd forMainFrameOnly:YES]);

    // First set up the configuration without In-App Browser Privacy settings to ensure it works as expected.
    WKWebViewConfiguration *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];
    [[configuration preferences] _setNeedsInAppBrowserPrivacyQuirks:YES];
    [[configuration preferences] _setInAppBrowserPrivacyEnabled:NO];

    auto schemeHandler = adoptNS([[InAppBrowserSchemeHandler alloc] init]);
    [configuration setURLSchemeHandler:schemeHandler.get() forURLScheme:@"in-app-browser"];
    
    [configuration.userContentController addUserScript:userScript.get()];
    [configuration.userContentController addScriptMessageHandler:messageHandler.get() name:@"testInAppBrowserPrivacy"];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectZero configuration:configuration]);
    NSURLRequest *request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"in-app-browser:///in-app-browser-privacy-test-user-script"]];
    [webView loadRequest:request];
    [webView _test_waitForDidFinishNavigation];
    TestWebKitAPI::Util::run(&isDone);
    
    [webView evaluateJavaScript:@"window.wkUserScriptInjected" completionHandler:^(id _Nullable result, NSError * _Nullable error) {
        EXPECT_EQ(YES, [[webView objectByEvaluatingJavaScript:@"window.wkUserScriptInjected"] boolValue]);
        EXPECT_FALSE(!!error);
        isDone = true;
    }];

    isDone = false;
    TestWebKitAPI::Util::run(&isDone);

    // Now setup In-App Browser Privacy settings and expect a failed script evaluation result, and an error message.
    isDone = false;
    initializeInAppBrowserPrivacyTestSettings();
    [[configuration preferences] _setNeedsInAppBrowserPrivacyQuirks:NO];
    [[configuration preferences] _setInAppBrowserPrivacyEnabled:YES];

    auto webView2 = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectZero configuration:configuration]);
    [webView2 loadRequest:request];
    TestWebKitAPI::Util::run(&isDone);

    // Check that request to read this variable is rejected.
    [webView2 evaluateJavaScript:@"window.wkUserScriptInjected" completionHandler:^(id _Nullable result, NSError * _Nullable error) {
        EXPECT_FALSE(result);
        EXPECT_TRUE(!!error);
        isDone = true;
    }];

    isDone = false;
    TestWebKitAPI::Util::run(&isDone);

    // Turn back on In-App Browser Privacy quirks to check that original attempt to set this variable was rejected.
    isDone = false;
    [[[webView2 configuration] preferences] _setNeedsInAppBrowserPrivacyQuirks:YES];
    [webView2 evaluateJavaScript:@"window.wkUserScriptInjected" completionHandler:^(id _Nullable result, NSError * _Nullable error) {
        EXPECT_EQ(NO, [[webView2 objectByEvaluatingJavaScript:@"window.wkUserScriptInjected"] boolValue]);
        EXPECT_FALSE(!!error);
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
        EXPECT_EQ(YES, [[webView objectByEvaluatingJavaScript:@"window.wkUserScriptInjected"] boolValue]);
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
        isDone = true;
    }];

    isDone = false;
    TestWebKitAPI::Util::run(&isDone);
}

TEST(InAppBrowserPrivacy, SwapBackToAppBoundRejectsUserScript)
{
    initializeInAppBrowserPrivacyTestSettings();
    auto messageHandler = adoptNS([[TestInAppBrowserScriptMessageHandler alloc] init]);
    auto userScript = adoptNS([[WKUserScript alloc] initWithSource:userScriptSource injectionTime:WKUserScriptInjectionTimeAtDocumentStart forMainFrameOnly:YES]);

    WKWebViewConfiguration *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];
    [configuration.userContentController addScriptMessageHandler:messageHandler.get() name:@"testInAppBrowserPrivacy"];
    [[configuration preferences] _setNeedsInAppBrowserPrivacyQuirks:NO];
    [[configuration preferences] _setInAppBrowserPrivacyEnabled:YES];

    auto schemeHandler = adoptNS([[InAppBrowserSchemeHandler alloc] init]);
    [configuration setURLSchemeHandler:schemeHandler.get() forURLScheme:@"in-app-browser"];
    
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectZero configuration:configuration]);
    NSURLRequest *request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"in-app-browser:///in-app-browser-privacy-test-user-script"]];
    [webView loadRequest:request];
    [webView _test_waitForDidFinishNavigation];
    TestWebKitAPI::Util::run(&isDone);

    [configuration.userContentController _addUserScriptImmediately:userScript.get()];
    [webView evaluateJavaScript:@"window.wkUserScriptInjected" completionHandler:^(id _Nullable result, NSError * _Nullable error) {
        EXPECT_FALSE(result);
        EXPECT_TRUE(!!error);
        isDone = true;
    }];

    isDone = false;
    TestWebKitAPI::Util::run(&isDone);

    isDone = false;
    [webView _setIsNavigatingToAppBoundDomain:YES completionHandler: ^(void) {
        isDone = true;
    }];
    TestWebKitAPI::Util::run(&isDone);
    
    isDone = false;
    [configuration.userContentController _addUserScriptImmediately:userScript.get()];
    [webView evaluateJavaScript:@"window.wkUserScriptInjected" completionHandler:^(id _Nullable result, NSError * _Nullable error) {
        EXPECT_FALSE(result);
        EXPECT_TRUE(!!error);
        isDone = true;
    }];

    TestWebKitAPI::Util::run(&isDone);
}

TEST(InAppBrowserPrivacy, AppBoundDomains)
{
    initializeInAppBrowserPrivacyTestSettings();
    isDone = false;
    [[WKWebsiteDataStore defaultDataStore] _appBoundDomains:^(NSArray<NSString *> *domains) {
        NSArray *domainsToCompare = [NSArray arrayWithObjects:@"apple.com", @"bar.com", @"example.com", @"foo.com", @"localhost", @"webkit.org", nil];

        NSArray *sortedDomains = [domains sortedArrayUsingSelector:@selector(caseInsensitiveCompare:)];

        int length = [sortedDomains count];
        EXPECT_EQ(length, 6);
        for (int i = 0; i < length; i++)
            EXPECT_WK_STREQ([sortedDomains objectAtIndex:i], [domainsToCompare objectAtIndex:i]);

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
        isDone = true;
    }];
    TestWebKitAPI::Util::run(&isDone);
}

TEST(InAppBrowserPrivacy, IgnoreAppBoundDomainsAcceptsUserScripts)
{
    initializeInAppBrowserPrivacyTestSettings();
    auto messageHandler = adoptNS([[TestInAppBrowserScriptMessageHandler alloc] init]);
    auto userScript = adoptNS([[WKUserScript alloc] initWithSource:userScriptSource injectionTime:WKUserScriptInjectionTimeAtDocumentStart forMainFrameOnly:YES]);

    WKWebViewConfiguration *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];
    [configuration.userContentController addScriptMessageHandler:messageHandler.get() name:@"testInAppBrowserPrivacy"];
    [[configuration preferences] _setNeedsInAppBrowserPrivacyQuirks:NO];
    [[configuration preferences] _setInAppBrowserPrivacyEnabled:YES];
    [configuration _setIgnoresAppBoundDomains:YES];

    auto schemeHandler = adoptNS([[InAppBrowserSchemeHandler alloc] init]);
    [configuration setURLSchemeHandler:schemeHandler.get() forURLScheme:@"in-app-browser"];
    
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectZero configuration:configuration]);
    NSURLRequest *request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"in-app-browser:///in-app-browser-privacy-test-user-script"]];
    
    [webView loadRequest:request];
    [webView _test_waitForDidFinishNavigation];
    TestWebKitAPI::Util::run(&isDone);

    [configuration.userContentController _addUserScriptImmediately:userScript.get()];
    [webView evaluateJavaScript:@"window.wkUserScriptInjected" completionHandler:^(id _Nullable result, NSError * _Nullable error) {
        EXPECT_EQ(YES, [[webView objectByEvaluatingJavaScript:@"window.wkUserScriptInjected"] boolValue]);
        EXPECT_FALSE(!!error);
        isDone = true;
    }];

    isDone = false;
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
}

#endif // USE(APPLE_INTERNAL_SDK)

#endif // PLATFORM(IOS_FAMILY)
