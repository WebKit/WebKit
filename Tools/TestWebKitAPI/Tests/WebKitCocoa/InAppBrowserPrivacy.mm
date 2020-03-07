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
#import <wtf/text/WTFString.h>

static bool isDone;

#if USE(APPLE_INTERNAL_SDK)

static NSString * const userScriptSource = @"window.wkUserScriptInjected = true";

@interface TestInAppBrowserScriptMessageHandler : NSObject <WKScriptMessageHandler>
@end

@implementation TestInAppBrowserScriptMessageHandler

- (void)userContentController:(WKUserContentController *)userContentController didReceiveScriptMessage:(WKScriptMessage *)message
{
    isDone = true;
}

@end

TEST(InAppBrowserPrivacy, NonAppBoundDomainFailedUserScripts)
{
    auto messageHandler = adoptNS([[TestInAppBrowserScriptMessageHandler alloc] init]);
    auto userScript = adoptNS([[WKUserScript alloc] initWithSource:userScriptSource injectionTime:WKUserScriptInjectionTimeAtDocumentStart forMainFrameOnly:YES]);

    WKWebViewConfiguration *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];
    [configuration.userContentController addScriptMessageHandler:messageHandler.get() name:@"testInAppBrowserPrivacy"];
    [[configuration preferences] _setInAppBrowserPrivacyEnabled:NO];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectZero configuration:configuration]);

    NSURLRequest *request = [NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"in-app-browser-privacy-test-user-script" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]];
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

    isDone = false;

    [[configuration preferences] _setInAppBrowserPrivacyEnabled:YES];
    auto webView2 = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectZero configuration:configuration]);

    [webView2 loadRequest:request];
    TestWebKitAPI::Util::run(&isDone);

    [configuration.userContentController _addUserScriptImmediately:userScript.get()];
    [webView2 evaluateJavaScript:@"window.wkUserScriptInjected" completionHandler:^(id _Nullable result, NSError * _Nullable error) {
        EXPECT_FALSE(result);
        EXPECT_TRUE(!!error);
        isDone = true;
    }];

    isDone = false;
    TestWebKitAPI::Util::run(&isDone);
}

TEST(InAppBrowserPrivacy, NonAppBoundDomainFailedUserAgentScripts)
{
    WKWebViewConfiguration *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];
    [[configuration preferences] _setInAppBrowserPrivacyEnabled:NO];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectZero configuration:configuration]);

    NSURLRequest *request = [NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"in-app-browser-privacy-test-user-agent-script" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]];
    [webView loadRequest:request];
    [webView _test_waitForDidFinishNavigation];

    [webView evaluateJavaScript:@"window.wkUserScriptInjected" completionHandler:^(id _Nullable result, NSError * _Nullable error) {
        EXPECT_EQ(YES, [[webView objectByEvaluatingJavaScript:@"window.wkUserScriptInjected"] boolValue]);
        isDone = true;
    }];

    isDone = false;
    TestWebKitAPI::Util::run(&isDone);

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
    auto messageHandler = adoptNS([[TestInAppBrowserScriptMessageHandler alloc] init]);
    auto userScript = adoptNS([[WKUserScript alloc] initWithSource:userScriptSource injectionTime:WKUserScriptInjectionTimeAtDocumentStart forMainFrameOnly:YES]);

    WKWebViewConfiguration *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];
    [configuration.userContentController addScriptMessageHandler:messageHandler.get() name:@"testInAppBrowserPrivacy"];
    [[configuration preferences] _setInAppBrowserPrivacyEnabled:YES];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectZero configuration:configuration]);

    NSURLRequest *request = [NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"in-app-browser-privacy-test-user-script" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]];
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

#endif // USE(APPLE_INTERNAL_SDK)

TEST(InAppBrowserPrivacy, AppBoundDomains)
{
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

