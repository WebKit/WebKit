/*
 * Copyright (C) 2017-2020 Apple Inc. All rights reserved.
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

#import "HTTPServer.h"
#import "PlatformUtilities.h"
#import "TestCocoa.h"
#import "TestNavigationDelegate.h"
#import "TestWKWebView.h"
#import <WebKit/WKProcessPoolPrivate.h>
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/WKWebViewPrivateForTesting.h>
#import <WebKit/WKWebpagePreferencesPrivate.h>
#import <WebKit/_WKProcessPoolConfiguration.h>
#import <wtf/Vector.h>

namespace TestWebKitAPI {

#if !PLATFORM(IOS)

static bool isEnhancedSecurityEnabled(WKWebView *webView)
{
    __block bool gotResponse = false;
    __block bool isEnhancedSecurityEnabledResult = false;
    [webView _isEnhancedSecurityEnabled:^(BOOL isEnhancedSecurityEnabled) {
        isEnhancedSecurityEnabledResult = isEnhancedSecurityEnabled;
        gotResponse = true;
    }];
    TestWebKitAPI::Util::run(&gotResponse);
    EXPECT_NE([webView _webProcessIdentifier], 0);
    return isEnhancedSecurityEnabledResult;
}

static bool isJITEnabled(WKWebView *webView)
{
    __block bool gotResponse = false;
    __block bool isJITEnabledResult = false;
    [webView _isJITEnabled:^(BOOL isJITEnabled) {
        isJITEnabledResult = isJITEnabled;
        gotResponse = true;
    }];
    TestWebKitAPI::Util::run(&gotResponse);
    EXPECT_NE([webView _webProcessIdentifier], 0);
    return isJITEnabledResult;
}

TEST(EnhancedSecurity, EnhancedSecurityEnablesTrue)
{
    auto webViewConfiguration = adoptNS([WKWebViewConfiguration new]);
    webViewConfiguration.get().defaultWebpagePreferences.securityRestrictionMode = WKSecurityRestrictionModeMaximizeCompatibility;
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration.get()]);
    NSURL *url = [NSBundle.test_resourcesBundle URLForResource:@"simple" withExtension:@"html"];
    [webView loadRequest:[NSURLRequest requestWithURL:url]];
    [webView _test_waitForDidFinishNavigation];
    EXPECT_EQ(true, isEnhancedSecurityEnabled(webView.get()));
// _webContentProcessVariantForFrame relies on private entitlements not available on public builds
#if USE(APPLE_INTERNAL_SDK)
    NSString *processVariant = [webView _webContentProcessVariantForFrame:nil];
    EXPECT_STREQ("security", processVariant.UTF8String);
#endif
}

TEST(EnhancedSecurity, EnhancedSecurityEnableFalse)
{
    auto webViewConfiguration = adoptNS([WKWebViewConfiguration new]);
    webViewConfiguration.get().defaultWebpagePreferences.securityRestrictionMode = WKSecurityRestrictionModeNone;
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration.get()]);
    NSURL *url = [NSBundle.test_resourcesBundle URLForResource:@"simple" withExtension:@"html"];
    [webView loadRequest:[NSURLRequest requestWithURL:url]];
    [webView _test_waitForDidFinishNavigation];
    EXPECT_EQ(false, isEnhancedSecurityEnabled(webView.get()));
// _webContentProcessVariantForFrame relies on private entitlements not available on public builds
#if USE(APPLE_INTERNAL_SDK)
    NSString *processVariant = [webView _webContentProcessVariantForFrame:nil];
    EXPECT_STREQ("standard", processVariant.UTF8String);
#endif
}

TEST(EnhancedSecurity, EnhancedSecurityDisablesJIT)
{
    auto webViewConfiguration = adoptNS([WKWebViewConfiguration new]);
    webViewConfiguration.get().defaultWebpagePreferences.securityRestrictionMode = WKSecurityRestrictionModeMaximizeCompatibility;
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration.get()]);
    NSURL *url = [NSBundle.test_resourcesBundle URLForResource:@"simple" withExtension:@"html"];
    [webView loadRequest:[NSURLRequest requestWithURL:url]];
    [webView _test_waitForDidFinishNavigation];
    EXPECT_EQ(false, isJITEnabled(webView.get()));
}

TEST(EnhancedSecurity, EnhancedSecurityNavigationStaysEnabledAfterNavigation)
{
    auto webViewConfiguration = adoptNS([WKWebViewConfiguration new]);
    webViewConfiguration.get().defaultWebpagePreferences.securityRestrictionMode = WKSecurityRestrictionModeMaximizeCompatibility;
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration.get()]);
    auto delegate = adoptNS([TestNavigationDelegate new]);
    [webView setNavigationDelegate:delegate.get()];

    __block bool finishedNavigation = false;
    delegate.get().didFinishNavigation = ^(WKWebView *, WKNavigation *) {
        finishedNavigation = true;
    };

    NSURL *url = [NSBundle.test_resourcesBundle URLForResource:@"simple" withExtension:@"html"];
    [webView loadRequest:[NSURLRequest requestWithURL:url]];
    TestWebKitAPI::Util::run(&finishedNavigation);
    EXPECT_EQ(true, isEnhancedSecurityEnabled(webView.get()));

    finishedNavigation = false;
    NSURL *url2 = [NSBundle.test_resourcesBundle URLForResource:@"simple2" withExtension:@"html"];
    [webView loadRequest:[NSURLRequest requestWithURL:url2]];
    TestWebKitAPI::Util::run(&finishedNavigation);

    EXPECT_EQ(true, isEnhancedSecurityEnabled(webView.get()));
}

TEST(EnhancedSecurity, PSONToEnhancedSecurity)
{
    auto webViewConfiguration = adoptNS([WKWebViewConfiguration new]);
    webViewConfiguration.get().defaultWebpagePreferences.securityRestrictionMode = WKSecurityRestrictionModeNone;
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration.get()]);
    auto delegate = adoptNS([TestNavigationDelegate new]);
    [webView setNavigationDelegate:delegate.get()];

    __block bool finishedNavigation = false;
    delegate.get().didFinishNavigation = ^(WKWebView *, WKNavigation *) {
        finishedNavigation = true;
    };

    NSURL *url = [NSBundle.test_resourcesBundle URLForResource:@"simple" withExtension:@"html"];
    [webView loadRequest:[NSURLRequest requestWithURL:url]];
    TestWebKitAPI::Util::run(&finishedNavigation);
    EXPECT_EQ(false, isEnhancedSecurityEnabled(webView.get()));
// _webContentProcessVariantForFrame relies on private entitlements not available on public builds
#if USE(APPLE_INTERNAL_SDK)
    EXPECT_STREQ("standard", [webView _webContentProcessVariantForFrame:nil].UTF8String);
#endif
    pid_t pid1 = [webView _webProcessIdentifier];
    EXPECT_NE(pid1, 0);

    finishedNavigation = false;

    delegate.get().decidePolicyForNavigationActionWithPreferences = ^(WKNavigationAction *action, WKWebpagePreferences *preferences, void (^completionHandler)(WKNavigationActionPolicy, WKWebpagePreferences *)) {
        EXPECT_EQ(preferences.securityRestrictionMode, WKSecurityRestrictionModeNone);
        preferences.securityRestrictionMode = WKSecurityRestrictionModeMaximizeCompatibility;
        completionHandler(WKNavigationActionPolicyAllow, preferences);
    };

    NSURL *url2 = [NSBundle.test_resourcesBundle URLForResource:@"simple2" withExtension:@"html"];
    [webView loadRequest:[NSURLRequest requestWithURL:url2]];
    TestWebKitAPI::Util::run(&finishedNavigation);

    EXPECT_EQ(true, isEnhancedSecurityEnabled(webView.get()));
// _webContentProcessVariantForFrame relies on private entitlements not available on public builds
#if USE(APPLE_INTERNAL_SDK)
    EXPECT_STREQ("security", [webView _webContentProcessVariantForFrame:nil].UTF8String);
#endif
    EXPECT_NE(pid1, [webView _webProcessIdentifier]);
}

TEST(EnhancedSecurity, PSONToEnhancedSecuritySamePage)
{
    auto webViewConfiguration = adoptNS([WKWebViewConfiguration new]);
    webViewConfiguration.get().defaultWebpagePreferences.securityRestrictionMode = WKSecurityRestrictionModeNone;
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration.get()]);
    auto delegate = adoptNS([TestNavigationDelegate new]);
    [webView setNavigationDelegate:delegate.get()];

    __block bool finishedNavigation = false;
    delegate.get().didFinishNavigation = ^(WKWebView *, WKNavigation *) {
        finishedNavigation = true;
    };

    NSURL *url = [NSBundle.test_resourcesBundle URLForResource:@"simple" withExtension:@"html"];
    [webView loadRequest:[NSURLRequest requestWithURL:url]];
    TestWebKitAPI::Util::run(&finishedNavigation);
    EXPECT_EQ(false, isEnhancedSecurityEnabled(webView.get()));
// _webContentProcessVariantForFrame relies on private entitlements not available on public builds
#if USE(APPLE_INTERNAL_SDK)
    EXPECT_STREQ("standard", [webView _webContentProcessVariantForFrame:nil].UTF8String);
#endif
    pid_t pid1 = [webView _webProcessIdentifier];
    EXPECT_NE(pid1, 0);

    finishedNavigation = false;

    delegate.get().decidePolicyForNavigationActionWithPreferences = ^(WKNavigationAction *action, WKWebpagePreferences *preferences, void (^completionHandler)(WKNavigationActionPolicy, WKWebpagePreferences *)) {
        EXPECT_EQ(preferences.securityRestrictionMode, WKSecurityRestrictionModeNone);
        preferences.securityRestrictionMode = WKSecurityRestrictionModeMaximizeCompatibility;
        completionHandler(WKNavigationActionPolicyAllow, preferences);
    };

    NSURL *url2 = [NSBundle.test_resourcesBundle URLForResource:@"simple" withExtension:@"html"];
    [webView loadRequest:[NSURLRequest requestWithURL:url2]];
    TestWebKitAPI::Util::run(&finishedNavigation);

    EXPECT_EQ(true, isEnhancedSecurityEnabled(webView.get()));
// _webContentProcessVariantForFrame relies on private entitlements not available on public builds
#if USE(APPLE_INTERNAL_SDK)
    EXPECT_STREQ("security", [webView _webContentProcessVariantForFrame:nil].UTF8String);
#endif
    EXPECT_NE(pid1, [webView _webProcessIdentifier]);
}

static RetainPtr<_WKProcessPoolConfiguration> psonProcessPoolConfiguration()
{
    auto processPoolConfiguration = adoptNS([[_WKProcessPoolConfiguration alloc] init]);
    processPoolConfiguration.get().processSwapsOnNavigation = YES;
    processPoolConfiguration.get().usesWebProcessCache = YES;
    processPoolConfiguration.get().prewarmsProcessesAutomatically = YES;
    processPoolConfiguration.get().processSwapsOnNavigationWithinSameNonHTTPFamilyProtocol = YES;
    return processPoolConfiguration;
}

TEST(EnhancedSecurity, PSONToEnhancedSecuritySharedProcessPool)
{
    auto processPoolConfiguration = psonProcessPoolConfiguration();
    auto processPool = adoptNS([[WKProcessPool alloc] _initWithConfiguration:processPoolConfiguration.get()]);

    auto webViewConfiguration = adoptNS([WKWebViewConfiguration new]);
    [webViewConfiguration setProcessPool:processPool.get()];
    webViewConfiguration.get().defaultWebpagePreferences.securityRestrictionMode = WKSecurityRestrictionModeNone;

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration.get()]);

    auto delegate = adoptNS([TestNavigationDelegate new]);
    [webView setNavigationDelegate:delegate.get()];

    __block bool finishedNavigation = false;
    delegate.get().didFinishNavigation = ^(WKWebView *, WKNavigation *) {
        finishedNavigation = true;
    };

    NSURL *url = [NSBundle.test_resourcesBundle URLForResource:@"simple" withExtension:@"html"];
    [webView loadRequest:[NSURLRequest requestWithURL:url]];
    TestWebKitAPI::Util::run(&finishedNavigation);
    EXPECT_EQ(false, isEnhancedSecurityEnabled(webView.get()));
// _webContentProcessVariantForFrame relies on private entitlements not available on public builds
#if USE(APPLE_INTERNAL_SDK)
    EXPECT_STREQ("standard", [webView _webContentProcessVariantForFrame:nil].UTF8String);
#endif
    pid_t pid1 = [webView _webProcessIdentifier];
    EXPECT_NE(pid1, 0);

    finishedNavigation = false;
    auto webView2 = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration.get()]);
    [webView2 setNavigationDelegate:delegate.get()];

    delegate.get().decidePolicyForNavigationActionWithPreferences = ^(WKNavigationAction *action, WKWebpagePreferences *preferences, void (^completionHandler)(WKNavigationActionPolicy, WKWebpagePreferences *)) {
        EXPECT_EQ(preferences.securityRestrictionMode, WKSecurityRestrictionModeNone);
        preferences.securityRestrictionMode = WKSecurityRestrictionModeMaximizeCompatibility;
        completionHandler(WKNavigationActionPolicyAllow, preferences);
    };

    NSURL *url2 = [NSBundle.test_resourcesBundle URLForResource:@"simple" withExtension:@"html"];
    [webView2 loadRequest:[NSURLRequest requestWithURL:url2]];
    TestWebKitAPI::Util::run(&finishedNavigation);

    EXPECT_EQ(true, isEnhancedSecurityEnabled(webView2.get()));
// _webContentProcessVariantForFrame relies on private entitlements not available on public builds
#if USE(APPLE_INTERNAL_SDK)
    EXPECT_STREQ("security", [webView2 _webContentProcessVariantForFrame:nil].UTF8String);
#endif
    EXPECT_NE(pid1, [webView2 _webProcessIdentifier]);
}

TEST(EnhancedSecurity, PSONToEnhancedSecuritySharedProcessPoolReverse)
{
    auto processPoolConfiguration = psonProcessPoolConfiguration();
    auto processPool = adoptNS([[WKProcessPool alloc] _initWithConfiguration:processPoolConfiguration.get()]);

    auto webViewConfiguration = adoptNS([WKWebViewConfiguration new]);
    [webViewConfiguration setProcessPool:processPool.get()];
    webViewConfiguration.get().defaultWebpagePreferences.securityRestrictionMode = WKSecurityRestrictionModeMaximizeCompatibility;

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration.get()]);

    auto delegate = adoptNS([TestNavigationDelegate new]);
    [webView setNavigationDelegate:delegate.get()];

    __block bool finishedNavigation = false;
    delegate.get().didFinishNavigation = ^(WKWebView *, WKNavigation *) {
        finishedNavigation = true;
    };

    NSURL *url = [NSBundle.test_resourcesBundle URLForResource:@"simple" withExtension:@"html"];
    [webView loadRequest:[NSURLRequest requestWithURL:url]];
    TestWebKitAPI::Util::run(&finishedNavigation);
    EXPECT_EQ(true, isEnhancedSecurityEnabled(webView.get()));
// _webContentProcessVariantForFrame relies on private entitlements not available on public builds
#if USE(APPLE_INTERNAL_SDK)
    EXPECT_STREQ("security", [webView _webContentProcessVariantForFrame:nil].UTF8String);
#endif
    pid_t pid1 = [webView _webProcessIdentifier];
    EXPECT_NE(pid1, 0);

    finishedNavigation = false;
    auto webView2 = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration.get()]);
    [webView2 setNavigationDelegate:delegate.get()];

    delegate.get().decidePolicyForNavigationActionWithPreferences = ^(WKNavigationAction *action, WKWebpagePreferences *preferences, void (^completionHandler)(WKNavigationActionPolicy, WKWebpagePreferences *)) {
        EXPECT_EQ(preferences.securityRestrictionMode, WKSecurityRestrictionModeMaximizeCompatibility);
        preferences.securityRestrictionMode = WKSecurityRestrictionModeNone;
        completionHandler(WKNavigationActionPolicyAllow, preferences);
    };

    NSURL *url2 = [NSBundle.test_resourcesBundle URLForResource:@"simple" withExtension:@"html"];
    [webView2 loadRequest:[NSURLRequest requestWithURL:url2]];
    TestWebKitAPI::Util::run(&finishedNavigation);

    EXPECT_EQ(false, isEnhancedSecurityEnabled(webView2.get()));
// _webContentProcessVariantForFrame relies on private entitlements not available on public builds
#if USE(APPLE_INTERNAL_SDK)
    EXPECT_STREQ("standard", [webView2 _webContentProcessVariantForFrame:nil].UTF8String);
#endif
    EXPECT_NE(pid1, [webView2 _webProcessIdentifier]);
}

#if USE(APPLE_INTERNAL_SDK)
TEST(EnhancedSecurity, ProcessVariantMatchesConfiguration)
{
    auto webViewConfiguration1 = adoptNS([WKWebViewConfiguration new]);
    webViewConfiguration1.get().defaultWebpagePreferences.securityRestrictionMode = WKSecurityRestrictionModeMaximizeCompatibility;
    auto webView1 = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration1.get()]);

    auto webViewConfiguration2 = adoptNS([WKWebViewConfiguration new]);
    webViewConfiguration2.get().defaultWebpagePreferences.securityRestrictionMode = WKSecurityRestrictionModeNone;
    auto webView2 = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration2.get()]);

    NSURL *url = [NSBundle.test_resourcesBundle URLForResource:@"simple" withExtension:@"html"];

    [webView1 loadRequest:[NSURLRequest requestWithURL:url]];
    [webView1 _test_waitForDidFinishNavigation];
    EXPECT_EQ(true, isEnhancedSecurityEnabled(webView1.get()));
    EXPECT_STREQ("security", [webView1 _webContentProcessVariantForFrame:nil].UTF8String);

    [webView2 loadRequest:[NSURLRequest requestWithURL:url]];
    [webView2 _test_waitForDidFinishNavigation];
    EXPECT_EQ(false, isEnhancedSecurityEnabled(webView2.get()));
    EXPECT_STREQ("standard", [webView2 _webContentProcessVariantForFrame:nil].UTF8String);
}
#endif

TEST(EnhancedSecurity, ProcessCanLaunch)
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    configuration.get().websiteDataStore = [WKWebsiteDataStore nonPersistentDataStore];
    configuration.get().processPool = adoptNS([[WKProcessPool alloc] init]).get();
    configuration.get().defaultWebpagePreferences.securityRestrictionMode = WKSecurityRestrictionModeMaximizeCompatibility;

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    [webView loadHTMLString:@"<html><body>test</body></html>" baseURL:nil];

    // Wait with explicit timeout instead of _test_waitForDidFinishNavigation
    __block bool navigationFinished = false;
    auto delegate = adoptNS([TestNavigationDelegate new]);
    delegate.get().didFinishNavigation = ^(WKWebView *, WKNavigation *) {
        navigationFinished = true;
    };
    [webView setNavigationDelegate:delegate.get()];

    // Wait up to 10 seconds for navigation to complete
    NSDate *timeout = [NSDate dateWithTimeIntervalSinceNow:10.0];
    while (!navigationFinished && [timeout timeIntervalSinceNow] > 0)
        [[NSRunLoop currentRunLoop] runMode:NSDefaultRunLoopMode beforeDate:[NSDate dateWithTimeIntervalSinceNow:0.1]];

    if (navigationFinished)
        NSLog(@"EnhancedSecurity ProcessCanLaunch: Navigation finished");
    else
        NSLog(@"EnhancedSecurity ProcessCanLaunch: Navigation TIMED OUT after 10 seconds");

    auto processID = [webView _webProcessIdentifier];
    NSLog(@"EnhancedSecurity ProcessCanLaunch: Process ID: %d", processID);
    EXPECT_NE(processID, 0);

}

TEST(EnhancedSecurity, CaptivePortalProcessCanLaunch)
{
    [WKProcessPool _setCaptivePortalModeEnabledGloballyForTesting:YES];

    // Create configuration AFTER setting global mode
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    configuration.get().websiteDataStore = [WKWebsiteDataStore nonPersistentDataStore];
    configuration.get().processPool = adoptNS([[WKProcessPool alloc] init]).get();

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    [webView loadHTMLString:@"<html><body>test</body></html>" baseURL:nil];

    // Wait with explicit timeout instead of _test_waitForDidFinishNavigation
    __block bool navigationFinished = false;
    auto delegate = adoptNS([TestNavigationDelegate new]);
    delegate.get().didFinishNavigation = ^(WKWebView *, WKNavigation *) {
        navigationFinished = true;
    };
    [webView setNavigationDelegate:delegate.get()];

    // Wait up to 10 seconds for navigation to complete
    NSDate *timeout = [NSDate dateWithTimeIntervalSinceNow:10.0];
    while (!navigationFinished && [timeout timeIntervalSinceNow] > 0)
        [[NSRunLoop currentRunLoop] runMode:NSDefaultRunLoopMode beforeDate:[NSDate dateWithTimeIntervalSinceNow:0.1]];

    if (navigationFinished)
        NSLog(@"CaptivePortal ProcessCanLaunch: Navigation finished");
    else
        NSLog(@"CaptivePortal ProcessCanLaunch: Navigation TIMED OUT after 10 seconds");

    auto processID = [webView _webProcessIdentifier];
    EXPECT_NE(processID, 0);

    [WKProcessPool _clearCaptivePortalModeEnabledGloballyForTesting];
}

TEST(EnhancedSecurity, EnhancedSecurityNavigationStaysEnabledAfterSubFrameNavigationRequestDisables)
{
    HTTPServer server({
        { "/example"_s, { "<iframe id='webkit_frame' src='https://example.com/webkit'></iframe>"_s } },
        { "/example_subframe"_s, { "<script>alert('done')</script>"_s } },
        { "/webkit"_s, { "<html></html>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto webViewConfiguration = server.httpsProxyConfiguration();
    [webViewConfiguration.processPool _setObject:@"WebProcessPlugInWithInternals" forBundleParameter:TestWebKitAPI::Util::TestPlugInClassNameParameter];
    webViewConfiguration.defaultWebpagePreferences._enhancedSecurityEnabled = YES;

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration]);
    auto delegate = adoptNS([TestNavigationDelegate new]);
    [delegate allowAnyTLSCertificate];

    [webView setNavigationDelegate:delegate.get()];

    __block bool finishedNavigation = false;
    delegate.get().didFinishNavigation = ^(WKWebView *, WKNavigation *) {
        finishedNavigation = true;
    };
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];

    TestWebKitAPI::Util::run(&finishedNavigation);

    EXPECT_EQ(true, isEnhancedSecurityEnabled(webView.get()));

    finishedNavigation = false;
    delegate.get().decidePolicyForNavigationActionWithPreferences = ^(WKNavigationAction *action, WKWebpagePreferences *preferences, void (^completionHandler)(WKNavigationActionPolicy, WKWebpagePreferences *)) {
        EXPECT_TRUE(preferences._enhancedSecurityEnabled);
        EXPECT_FALSE(action.sourceFrame.isMainFrame);
        preferences._enhancedSecurityEnabled = NO;
        completionHandler(WKNavigationActionPolicyAllow, preferences);
    };

    [webView evaluateJavaScript:@"location.href = 'https://example.com/example_subframe'" inFrame:[webView firstChildFrame] completionHandler:nil];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "done");

    EXPECT_EQ(true, isEnhancedSecurityEnabled(webView.get()));
// _webContentProcessVariantForFrame relies on private entitlements not available on public builds
#if USE(APPLE_INTERNAL_SDK)
    EXPECT_STREQ("security", [webView _webContentProcessVariantForFrame:nil].UTF8String);
    EXPECT_STREQ("security", [webView _webContentProcessVariantForFrame:[webView firstChildFrame]._handle].UTF8String);
#endif

}

TEST(EnhancedSecurity, EnhancedSecurityNavigationStaysEnabledAfterSubFrameNavigationRequestDisablesCrossOrigin)
{

    HTTPServer server({
        { "/example"_s, { "<iframe id='webkit_frame' src='https://example.com/webkit'></iframe>"_s } },
        { "/example_subframe"_s, { "<script>alert('done')</script>"_s } },
        { "/webkit"_s, { "<html></html>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto webViewConfiguration = server.httpsProxyConfiguration();
    [webViewConfiguration.processPool _setObject:@"WebProcessPlugInWithInternals" forBundleParameter:TestWebKitAPI::Util::TestPlugInClassNameParameter];
    webViewConfiguration.defaultWebpagePreferences._enhancedSecurityEnabled = YES;

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration]);
    auto delegate = adoptNS([TestNavigationDelegate new]);
    [delegate allowAnyTLSCertificate];

    [webView setNavigationDelegate:delegate.get()];

    __block bool finishedNavigation = false;
    delegate.get().didFinishNavigation = ^(WKWebView *, WKNavigation *) {
        finishedNavigation = true;
    };
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];

    TestWebKitAPI::Util::run(&finishedNavigation);

    EXPECT_EQ(true, isEnhancedSecurityEnabled(webView.get()));

    finishedNavigation = false;
    delegate.get().decidePolicyForNavigationActionWithPreferences = ^(WKNavigationAction *action, WKWebpagePreferences *preferences, void (^completionHandler)(WKNavigationActionPolicy, WKWebpagePreferences *)) {
        EXPECT_TRUE(preferences._enhancedSecurityEnabled);
        EXPECT_FALSE(action.sourceFrame.isMainFrame);
        preferences._enhancedSecurityEnabled = NO;
        completionHandler(WKNavigationActionPolicyAllow, preferences);
    };

    [webView evaluateJavaScript:@"location.href = 'https://webkit.org/example_subframe'" inFrame:[webView firstChildFrame] completionHandler:nil];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "done");

    EXPECT_EQ(true, isEnhancedSecurityEnabled(webView.get()));
// _webContentProcessVariantForFrame relies on private entitlements not available on public builds
#if USE(APPLE_INTERNAL_SDK)
    EXPECT_STREQ("security", [webView _webContentProcessVariantForFrame:nil].UTF8String);
    EXPECT_STREQ("security", [webView _webContentProcessVariantForFrame:[webView firstChildFrame]._handle].UTF8String);
#endif

}

TEST(EnhancedSecurity, EnhancedSecurityNavigationStaysDisabledAfterSubFrameNavigationRequestEnabled)
{

    HTTPServer server({
        { "/example"_s, { "<iframe id='webkit_frame' src='https://example.com/webkit'></iframe>"_s } },
        { "/example_subframe"_s, { "<script>alert('done')</script>"_s } },
        { "/webkit"_s, { "<html></html>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto webViewConfiguration = server.httpsProxyConfiguration();
    [webViewConfiguration.processPool _setObject:@"WebProcessPlugInWithInternals" forBundleParameter:TestWebKitAPI::Util::TestPlugInClassNameParameter];
    webViewConfiguration.defaultWebpagePreferences._enhancedSecurityEnabled = NO;

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration]);
    auto delegate = adoptNS([TestNavigationDelegate new]);
    [delegate allowAnyTLSCertificate];

    [webView setNavigationDelegate:delegate.get()];

    __block bool finishedNavigation = false;
    delegate.get().didFinishNavigation = ^(WKWebView *, WKNavigation *) {
        finishedNavigation = true;
    };
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];

    TestWebKitAPI::Util::run(&finishedNavigation);

    EXPECT_EQ(false, isEnhancedSecurityEnabled(webView.get()));

    finishedNavigation = false;
    delegate.get().decidePolicyForNavigationActionWithPreferences = ^(WKNavigationAction *action, WKWebpagePreferences *preferences, void (^completionHandler)(WKNavigationActionPolicy, WKWebpagePreferences *)) {
        EXPECT_FALSE(preferences._enhancedSecurityEnabled);
        EXPECT_FALSE(action.sourceFrame.isMainFrame);
        preferences._enhancedSecurityEnabled = YES;
        completionHandler(WKNavigationActionPolicyAllow, preferences);
    };

    [webView evaluateJavaScript:@"location.href = 'https://example.com/example_subframe'" inFrame:[webView firstChildFrame] completionHandler:nil];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "done");

    EXPECT_EQ(false, isEnhancedSecurityEnabled(webView.get()));
// _webContentProcessVariantForFrame relies on private entitlements not available on public builds
#if USE(APPLE_INTERNAL_SDK)
    EXPECT_STREQ("standard", [webView _webContentProcessVariantForFrame:nil].UTF8String);
    EXPECT_STREQ("standard", [webView _webContentProcessVariantForFrame:[webView firstChildFrame]._handle].UTF8String);
#endif

}

TEST(EnhancedSecurity, EnhancedSecurityNavigationStaysDisabledAfterSubFrameNavigationRequestEnabledCrossOrigin)
{

    HTTPServer server({
        { "/example"_s, { "<iframe id='webkit_frame' src='https://example.com/webkit'></iframe>"_s } },
        { "/example_subframe"_s, { "<script>alert('done')</script>"_s } },
        { "/webkit"_s, { "<html></html>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto webViewConfiguration = server.httpsProxyConfiguration();
    [webViewConfiguration.processPool _setObject:@"WebProcessPlugInWithInternals" forBundleParameter:TestWebKitAPI::Util::TestPlugInClassNameParameter];
    webViewConfiguration.defaultWebpagePreferences._enhancedSecurityEnabled = NO;

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration]);
    auto delegate = adoptNS([TestNavigationDelegate new]);
    [delegate allowAnyTLSCertificate];

    [webView setNavigationDelegate:delegate.get()];

    __block bool finishedNavigation = false;
    delegate.get().didFinishNavigation = ^(WKWebView *, WKNavigation *) {
        finishedNavigation = true;
    };
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];

    TestWebKitAPI::Util::run(&finishedNavigation);

    EXPECT_EQ(false, isEnhancedSecurityEnabled(webView.get()));

    finishedNavigation = false;
    delegate.get().decidePolicyForNavigationActionWithPreferences = ^(WKNavigationAction *action, WKWebpagePreferences *preferences, void (^completionHandler)(WKNavigationActionPolicy, WKWebpagePreferences *)) {
        EXPECT_FALSE(preferences._enhancedSecurityEnabled);
        EXPECT_FALSE(action.sourceFrame.isMainFrame);
        preferences._enhancedSecurityEnabled = YES;
        completionHandler(WKNavigationActionPolicyAllow, preferences);
    };

    [webView evaluateJavaScript:@"location.href = 'https://webkit.org/example_subframe'" inFrame:[webView firstChildFrame] completionHandler:nil];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "done");

    EXPECT_EQ(false, isEnhancedSecurityEnabled(webView.get()));
// _webContentProcessVariantForFrame relies on private entitlements not available on public builds
#if USE(APPLE_INTERNAL_SDK)
    EXPECT_STREQ("standard", [webView _webContentProcessVariantForFrame:nil].UTF8String);
    EXPECT_STREQ("standard", [webView _webContentProcessVariantForFrame:[webView firstChildFrame]._handle].UTF8String);
#endif
}

TEST(EnhancedSecurity, WindowOpenWithNoopenerFromEnhancedSecurityPage)
{
    HTTPServer server({
        { "/example"_s, { "<script>w = window.open('https://webkit.org/webkit', '_blank', 'noopener')</script>"_s } },
        { "/webkit"_s, { "hi"_s } },
    }, HTTPServer::Protocol::HttpsProxy);

    auto webViewConfiguration = server.httpsProxyConfiguration();
    webViewConfiguration.defaultWebpagePreferences._enhancedSecurityEnabled = YES;
    webViewConfiguration.preferences.javaScriptCanOpenWindowsAutomatically = YES;

    auto openerWebView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration]);
    auto openerDelegate = adoptNS([TestNavigationDelegate new]);
    [openerDelegate allowAnyTLSCertificate];
    [openerWebView setNavigationDelegate:openerDelegate.get()];

    __block RetainPtr<WKWebView> openedWebView;
    auto uiDelegate = adoptNS([TestUIDelegate new]);
    uiDelegate.get().createWebViewWithConfiguration = ^(WKWebViewConfiguration *configuration, WKNavigationAction *, WKWindowFeatures *) {
        openedWebView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration]);
        auto openedDelegate = adoptNS([TestNavigationDelegate new]);
        [openedDelegate allowAnyTLSCertificate];
        [openedWebView setNavigationDelegate:openedDelegate.get()];
        return openedWebView.get();
    };
    [openerWebView setUIDelegate:uiDelegate.get()];

    [openerWebView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    [openerDelegate waitForDidFinishNavigation];

    while (!openedWebView)
        TestWebKitAPI::Util::spinRunLoop();

    EXPECT_EQ(true, isEnhancedSecurityEnabled(openedWebView.get()));
// _webContentProcessVariantForFrame relies on private entitlements not available on public builds
#if USE(APPLE_INTERNAL_SDK)
    EXPECT_STREQ("security", [openedWebView _webContentProcessVariantForFrame:nil].UTF8String);
#endif

    bool hasOpener = [[openedWebView objectByEvaluatingJavaScript:@"!!window.opener"] boolValue];
    EXPECT_FALSE(hasOpener);
}

TEST(EnhancedSecurity, WindowOpenWithOpenerFromEnhancedSecurityPage)
{
    HTTPServer server({
        { "/example"_s, { "<script>w = window.open('https://webkit.org/webkit')</script>"_s } },
        { "/webkit"_s, { "hi"_s } },
    }, HTTPServer::Protocol::HttpsProxy);

    auto webViewConfiguration = server.httpsProxyConfiguration();
    webViewConfiguration.defaultWebpagePreferences._enhancedSecurityEnabled = YES;
    webViewConfiguration.preferences.javaScriptCanOpenWindowsAutomatically = YES;

    auto openerWebView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration]);
    auto openerDelegate = adoptNS([TestNavigationDelegate new]);
    [openerDelegate allowAnyTLSCertificate];
    [openerWebView setNavigationDelegate:openerDelegate.get()];

    __block RetainPtr<WKWebView> openedWebView;
    auto uiDelegate = adoptNS([TestUIDelegate new]);
    uiDelegate.get().createWebViewWithConfiguration = ^(WKWebViewConfiguration *configuration, WKNavigationAction *, WKWindowFeatures *) {
        openedWebView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration]);
        auto openedDelegate = adoptNS([TestNavigationDelegate new]);
        [openedDelegate allowAnyTLSCertificate];
        [openedWebView setNavigationDelegate:openedDelegate.get()];
        return openedWebView.get();
    };
    [openerWebView setUIDelegate:uiDelegate.get()];

    [openerWebView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    [openerDelegate waitForDidFinishNavigation];

    while (!openedWebView)
        TestWebKitAPI::Util::spinRunLoop();

    EXPECT_EQ(true, isEnhancedSecurityEnabled(openedWebView.get()));
// _webContentProcessVariantForFrame relies on private entitlements not available on public builds
#if USE(APPLE_INTERNAL_SDK)
    EXPECT_STREQ("security", [openedWebView _webContentProcessVariantForFrame:nil].UTF8String);
    EXPECT_EQ([openerWebView _webProcessIdentifier], [openedWebView _webProcessIdentifier]);
#endif
}

TEST(EnhancedSecurity, WindowOpenNoopenerFromEnhancedSecurityInheritsEnhancedSecurity)
{
    HTTPServer server({
        { "/target"_s, { "target page"_s } },
        { "/opener"_s, { "<script>function openwithnoopener() {w = window.open('https://google.com/', '_blank', 'noopener')}</script>"_s } },
    }, HTTPServer::Protocol::HttpsProxy);

    auto processPoolConfiguration = psonProcessPoolConfiguration();
    auto processPool = adoptNS([[WKProcessPool alloc] _initWithConfiguration:processPoolConfiguration.get()]);

    auto standardConfig = server.httpsProxyConfiguration();
    [standardConfig setProcessPool:processPool.get()];
    standardConfig.defaultWebpagePreferences._enhancedSecurityEnabled = NO;
    standardConfig.preferences.javaScriptCanOpenWindowsAutomatically = YES;

    auto standardWebView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:standardConfig]);
    auto standardDelegate = adoptNS([TestNavigationDelegate new]);
    [standardDelegate allowAnyTLSCertificate];
    [standardWebView setNavigationDelegate:standardDelegate.get()];

    [standardWebView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://webkit.org/target"]]];
    [standardDelegate waitForDidFinishNavigation];

    EXPECT_EQ(false, isEnhancedSecurityEnabled(standardWebView.get()));
// _webContentProcessVariantForFrame relies on private entitlements not available on public builds
#if USE(APPLE_INTERNAL_SDK)
    EXPECT_STREQ("standard", [standardWebView _webContentProcessVariantForFrame:nil].UTF8String);
#endif
    pid_t standardPid = [standardWebView _webProcessIdentifier];
    EXPECT_NE(standardPid, 0);

    auto enhancedConfig = server.httpsProxyConfiguration();
    [enhancedConfig setProcessPool:processPool.get()];
    enhancedConfig.defaultWebpagePreferences._enhancedSecurityEnabled = YES;
    enhancedConfig.preferences.javaScriptCanOpenWindowsAutomatically = YES;

    auto openerWebView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 1, 1) configuration:enhancedConfig]);
    auto openerDelegate = adoptNS([TestNavigationDelegate new]);
    [openerDelegate allowAnyTLSCertificate];
    [openerWebView setNavigationDelegate:openerDelegate.get()];

    __block RetainPtr<WKWebView> openedWebView;
    auto uiDelegate = adoptNS([TestUIDelegate new]);
    uiDelegate.get().createWebViewWithConfiguration = ^(WKWebViewConfiguration *configuration, WKNavigationAction *, WKWindowFeatures *) {
        openedWebView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 1, 1) configuration:configuration]);
        auto openedDelegate = adoptNS([TestNavigationDelegate new]);
        [openedDelegate allowAnyTLSCertificate];
        [openedWebView setNavigationDelegate:openedDelegate.get()];
        return openedWebView.get();
    };
    [openerWebView setUIDelegate:uiDelegate.get()];

    [openerWebView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/opener"]]];

    [openerDelegate waitForDidFinishNavigation];
    [openerWebView evaluateJavaScript:@"openwithnoopener();" completionHandler:nil];
    while (!openedWebView)
        TestWebKitAPI::Util::spinRunLoop();

    EXPECT_EQ(true, isEnhancedSecurityEnabled(openedWebView.get()));
// _webContentProcessVariantForFrame relies on private entitlements not available on public builds
#if USE(APPLE_INTERNAL_SDK)
    EXPECT_STREQ("security", [openedWebView _webContentProcessVariantForFrame:nil].UTF8String);
    EXPECT_NE(standardPid, [openedWebView _webProcessIdentifier]);
#endif

    bool hasOpener = [[openedWebView objectByEvaluatingJavaScript:@"!!window.opener"] boolValue];
    EXPECT_FALSE(hasOpener);
}

TEST(EnhancedSecurity, WindowOpenNoopenerFromStandardWithEnhancedSecurityViaDelegate)
{
    HTTPServer server({
        { "/opener"_s, { "<script>function openwithnoopener() {w = window.open('https://webkit.org/opened', '_blank', 'noopener')}</script>"_s } },
        { "/opened"_s, { "opened page"_s } },
    }, HTTPServer::Protocol::HttpsProxy);

    auto processPoolConfiguration = psonProcessPoolConfiguration();
    auto processPool = adoptNS([[WKProcessPool alloc] _initWithConfiguration:processPoolConfiguration.get()]);

    auto standardConfig = server.httpsProxyConfiguration();
    [standardConfig setProcessPool:processPool.get()];
    standardConfig.defaultWebpagePreferences._enhancedSecurityEnabled = NO;
    standardConfig.preferences.javaScriptCanOpenWindowsAutomatically = YES;

    auto openerWebView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:standardConfig]);
    auto openerDelegate = adoptNS([TestNavigationDelegate new]);
    [openerDelegate allowAnyTLSCertificate];
    [openerWebView setNavigationDelegate:openerDelegate.get()];

    __block RetainPtr<WKWebView> openedWebView;
    __block RetainPtr<TestNavigationDelegate> openedDelegate;
    auto uiDelegate = adoptNS([TestUIDelegate new]);
    uiDelegate.get().createWebViewWithConfiguration = ^(WKWebViewConfiguration *configuration, WKNavigationAction *, WKWindowFeatures *) {
        openedWebView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration]);
        openedDelegate = adoptNS([TestNavigationDelegate new]);
        [openedDelegate allowAnyTLSCertificate];

        openedDelegate.get().decidePolicyForNavigationActionWithPreferences = ^(WKNavigationAction *action, WKWebpagePreferences *preferences, void (^completionHandler)(WKNavigationActionPolicy, WKWebpagePreferences *)) {
            preferences._enhancedSecurityEnabled = YES;
            completionHandler(WKNavigationActionPolicyAllow, preferences);
        };

        [openedWebView setNavigationDelegate:openedDelegate.get()];
        return openedWebView.get();
    };
    [openerWebView setUIDelegate:uiDelegate.get()];

    [openerWebView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/opener"]]];
    [openerDelegate waitForDidFinishNavigation];

    EXPECT_EQ(false, isEnhancedSecurityEnabled(openerWebView.get()));

    [openerWebView evaluateJavaScript:@"openwithnoopener();" completionHandler:nil];
    while (!openedWebView)
        TestWebKitAPI::Util::spinRunLoop();

    [openedDelegate waitForDidFinishNavigation];

    EXPECT_EQ(false, isEnhancedSecurityEnabled(openerWebView.get()));
    EXPECT_EQ(true, isEnhancedSecurityEnabled(openedWebView.get()));

// _webContentProcessVariantForFrame relies on private entitlements not available on public builds
#if USE(APPLE_INTERNAL_SDK)
    pid_t openerPID = [openerWebView _webProcessIdentifier];
    pid_t openedPID = [openedWebView _webProcessIdentifier];
    bool isEnhanced = isEnhancedSecurityEnabled(openedWebView.get());

    if (isEnhanced) {
        EXPECT_STREQ("security", [openedWebView _webContentProcessVariantForFrame:nil].UTF8String);
        EXPECT_NE(openerPID, openedPID);
    } else
        EXPECT_STREQ("standard", [openedWebView _webContentProcessVariantForFrame:nil].UTF8String);
#endif

    bool hasOpener = [[openedWebView objectByEvaluatingJavaScript:@"!!window.opener"] boolValue];
    EXPECT_FALSE(hasOpener);
}

TEST(EnhancedSecurity, WindowOpenNoopenerFromEnhancedSecurityWithStandardViaDelegate)
{
    HTTPServer server({
        { "/opener"_s, { "<script>function openwithnoopener() {w = window.open('https://webkit.org/opened', '_blank', 'noopener')}</script>"_s } },
        { "/opened"_s, { "opened page"_s } },
    }, HTTPServer::Protocol::HttpsProxy);

    auto processPoolConfiguration = psonProcessPoolConfiguration();
    auto processPool = adoptNS([[WKProcessPool alloc] _initWithConfiguration:processPoolConfiguration.get()]);

    auto enhancedConfig = server.httpsProxyConfiguration();
    [enhancedConfig setProcessPool:processPool.get()];
    enhancedConfig.defaultWebpagePreferences._enhancedSecurityEnabled = YES;
    enhancedConfig.preferences.javaScriptCanOpenWindowsAutomatically = YES;

    auto openerWebView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:enhancedConfig]);
    auto openerDelegate = adoptNS([TestNavigationDelegate new]);
    [openerDelegate allowAnyTLSCertificate];
    [openerWebView setNavigationDelegate:openerDelegate.get()];

    __block RetainPtr<WKWebView> openedWebView;
    __block RetainPtr<TestNavigationDelegate> openedDelegate;
    auto uiDelegate = adoptNS([TestUIDelegate new]);
    uiDelegate.get().createWebViewWithConfiguration = ^(WKWebViewConfiguration *configuration, WKNavigationAction *, WKWindowFeatures *) {
        openedWebView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration]);
        openedDelegate = adoptNS([TestNavigationDelegate new]);
        [openedDelegate allowAnyTLSCertificate];

        openedDelegate.get().decidePolicyForNavigationActionWithPreferences = ^(WKNavigationAction *action, WKWebpagePreferences *preferences, void (^completionHandler)(WKNavigationActionPolicy, WKWebpagePreferences *)) {
            preferences._enhancedSecurityEnabled = NO;
            completionHandler(WKNavigationActionPolicyAllow, preferences);
        };

        [openedWebView setNavigationDelegate:openedDelegate.get()];
        return openedWebView.get();
    };
    [openerWebView setUIDelegate:uiDelegate.get()];

    [openerWebView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/opener"]]];
    [openerDelegate waitForDidFinishNavigation];

    EXPECT_EQ(true, isEnhancedSecurityEnabled(openerWebView.get()));

    [openerWebView evaluateJavaScript:@"openwithnoopener();" completionHandler:nil];
    while (!openedWebView)
        TestWebKitAPI::Util::spinRunLoop();

    [openedDelegate waitForDidFinishNavigation];

    EXPECT_EQ(true, isEnhancedSecurityEnabled(openerWebView.get()));
    EXPECT_EQ(false, isEnhancedSecurityEnabled(openedWebView.get()));

// _webContentProcessVariantForFrame relies on private entitlements not available on public builds
#if USE(APPLE_INTERNAL_SDK)
    pid_t openerPID = [openerWebView _webProcessIdentifier];
    EXPECT_STREQ("standard", [openedWebView _webContentProcessVariantForFrame:nil].UTF8String);
    EXPECT_NE(openerPID, [openedWebView _webProcessIdentifier]);
#endif

    bool hasOpener = [[openedWebView objectByEvaluatingJavaScript:@"!!window.opener"] boolValue];
    EXPECT_FALSE(hasOpener);
}

TEST(EnhancedSecurity, LockdownModeTakesPrecedenceOverEnhancedSecurity)
{
    auto webViewConfiguration = adoptNS([WKWebViewConfiguration new]);
    webViewConfiguration.get().defaultWebpagePreferences._enhancedSecurityEnabled = YES;
    webViewConfiguration.get().defaultWebpagePreferences.lockdownModeEnabled = YES;

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 1, 1) configuration:webViewConfiguration.get()]);
    NSURL *url = [NSBundle.test_resourcesBundle URLForResource:@"simple" withExtension:@"html"];
    [webView loadRequest:[NSURLRequest requestWithURL:url]];
    [webView _test_waitForDidFinishNavigation];

    EXPECT_EQ(false, isJITEnabled(webView.get()));
    EXPECT_EQ(false, isEnhancedSecurityEnabled(webView.get()));
// _webContentProcessVariantForFrame relies on private entitlements not available on public builds
#if USE(APPLE_INTERNAL_SDK)
    EXPECT_STREQ("lockdown", [webView _webContentProcessVariantForFrame:nil].UTF8String);
#endif
}

TEST(EnhancedSecurity, EnhancedSecurityRequestedWhenLockdownModeActive)
{
    auto webViewConfiguration = adoptNS([WKWebViewConfiguration new]);
    webViewConfiguration.get().defaultWebpagePreferences.lockdownModeEnabled = YES;

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration.get()]);
    auto delegate = adoptNS([TestNavigationDelegate new]);
    [webView setNavigationDelegate:delegate.get()];

    __block bool finishedNavigation = false;
    delegate.get().didFinishNavigation = ^(WKWebView *, WKNavigation *) {
        finishedNavigation = true;
    };

    NSURL *url = [NSBundle.test_resourcesBundle URLForResource:@"simple" withExtension:@"html"];
    [webView loadRequest:[NSURLRequest requestWithURL:url]];
    TestWebKitAPI::Util::run(&finishedNavigation);

    EXPECT_EQ(false, isJITEnabled(webView.get()));

    finishedNavigation = false;

    delegate.get().decidePolicyForNavigationActionWithPreferences = ^(WKNavigationAction *action, WKWebpagePreferences *preferences, void (^completionHandler)(WKNavigationActionPolicy, WKWebpagePreferences *)) {
        preferences._enhancedSecurityEnabled = YES;
        completionHandler(WKNavigationActionPolicyAllow, preferences);
    };

    NSURL *url2 = [NSBundle.test_resourcesBundle URLForResource:@"simple2" withExtension:@"html"];
    [webView loadRequest:[NSURLRequest requestWithURL:url2]];
    TestWebKitAPI::Util::run(&finishedNavigation);

    EXPECT_EQ(false, isJITEnabled(webView.get()));
    EXPECT_EQ(false, isEnhancedSecurityEnabled(webView.get()));
    EXPECT_EQ(true, webViewConfiguration.get().defaultWebpagePreferences.isLockdownModeEnabled);
// _webContentProcessVariantForFrame relies on private entitlements not available on public builds
#if USE(APPLE_INTERNAL_SDK)
    EXPECT_STREQ("lockdown", [webView _webContentProcessVariantForFrame:nil].UTF8String);
#endif
}

#if HAVE(ENHANCED_SECURITY_WEB_CONTENT_PROCESS)
TEST(EnhancedSecurity, SystemLockdownModeEnablesEnhancedSecurityWhenAPIOptsOut)
{
    [WKProcessPool _setCaptivePortalModeEnabledGloballyForTesting:YES];

    auto webViewConfiguration = adoptNS([WKWebViewConfiguration new]);
    webViewConfiguration.get().defaultWebpagePreferences.securityRestrictionMode = WKSecurityRestrictionModeNone;
    webViewConfiguration.get().defaultWebpagePreferences.lockdownModeEnabled = NO;

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration.get()]);
    NSURL *url = [NSBundle.test_resourcesBundle URLForResource:@"simple" withExtension:@"html"];
    [webView loadRequest:[NSURLRequest requestWithURL:url]];
    [webView _test_waitForDidFinishNavigation];

    EXPECT_EQ(false, isJITEnabled(webView.get()));

#if USE(APPLE_INTERNAL_SDK)
    NSString *processVariant = [webView _webContentProcessVariantForFrame:nil];
    EXPECT_STREQ("security", processVariant.UTF8String);
#endif

    [WKProcessPool _clearCaptivePortalModeEnabledGloballyForTesting];
}
#endif

#endif

} // namespace TestWebKitAPI
