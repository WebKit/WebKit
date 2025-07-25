/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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
#import "Test.h"
#import "TestNavigationDelegate.h"
#import "TestUIDelegate.h"
#import "TestWKWebView.h"
#import <WebKit/WKFoundation.h>
#import <WebKit/WKHTTPCookieStorePrivate.h>
#import <WebKit/WKProcessPoolPrivate.h>
#import <WebKit/WKWebsiteDataStorePrivate.h>
#import <WebKit/_WKAutomationSession.h>
#import <WebKit/_WKAutomationSessionDelegate.h>
#import <WebKit/_WKProcessPoolConfiguration.h>
#import <WebKit/_WKWebsiteDataStoreConfiguration.h>
#import <wtf/BlockPtr.h>
#import <wtf/ProcessPrivilege.h>
#import <wtf/RetainPtr.h>
#import <wtf/Seconds.h>
#import <wtf/StdLibExtras.h>
#import <wtf/text/MakeString.h>
#import <wtf/text/WTFString.h>

static bool cookieStoreObserverCallbackWasCalled;
static bool webDriverCookieOperationCompleted;

@interface WebDriverCookieObserver : NSObject<WKHTTPCookieStoreObserver>
@property (nonatomic) NSUInteger callbackCount;
@end

@implementation WebDriverCookieObserver

- (void)cookiesDidChangeInCookieStore:(WKHTTPCookieStore *)cookieStore
{
    self.callbackCount++;
    cookieStoreObserverCallbackWasCalled = true;
}

@end

@interface WebDriverAutomationDelegate : NSObject<_WKAutomationSessionDelegate>
@end

@implementation WebDriverAutomationDelegate

- (_WKAutomationSessionBrowsingContextOptions)_automationSessionWithIdentifier:(NSString *)sessionIdentifier requestedBrowsingContextOptions:(_WKAutomationSessionBrowsingContextOptions)options
{
    return _WKAutomationSessionBrowsingContextOptionsPreferNewTab;
}

- (WKWebView *)_automationSessionWithIdentifier:(NSString *)sessionIdentifier requestedNewBrowsingContextWithOptions:(_WKAutomationSessionBrowsingContextOptions)options
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    [webView loadHTMLString:@"<html><body>WebDriver Test Page</body></html>" baseURL:[NSURL URLWithString:@"http://localhost:8080/"]];
    return webView.autorelease();
}

@end

namespace TestWebKitAPI {

class WebDriverCookieTest : public testing::Test {
public:
    void SetUp() override
    {
        // Clean up any existing cookies
        auto dataStore = [WKWebsiteDataStore defaultDataStore];
        [dataStore removeDataOfTypes:[WKWebsiteDataStore allWebsiteDataTypes]
            modifiedSince:[NSDate distantPast]
            completionHandler:^{
                webDriverCookieOperationCompleted = true;
            }];

        Util::run(&webDriverCookieOperationCompleted);
        webDriverCookieOperationCompleted = false;

        // Set up automation session for WebDriver testing
        auto processPoolConfiguration = adoptNS([[_WKProcessPoolConfiguration alloc] init]);
        processPoolConfiguration.get().automationAllowed = YES;

        m_processPool = adoptNS([[WKProcessPool alloc] _initWithConfiguration:processPoolConfiguration.get()]);

        m_automationDelegate = adoptNS([[WebDriverAutomationDelegate alloc] init]);
        m_automationSession = adoptNS([[_WKAutomationSession alloc] init]);
        [m_automationSession setDelegate:m_automationDelegate.get()];
        [m_processPool _setAutomationSession:m_automationSession.get()];

        m_observer = adoptNS([[WebDriverCookieObserver alloc] init]);
        [m_observer setCallbackCount:0];

        cookieStoreObserverCallbackWasCalled = false;
    }

    void TearDown() override
    {
        [m_processPool _setAutomationSession:nil];
    }

protected:
    RetainPtr<WKProcessPool> m_processPool;
    RetainPtr<_WKAutomationSession> m_automationSession;
    RetainPtr<WebDriverAutomationDelegate> m_automationDelegate;
    RetainPtr<WebDriverCookieObserver> m_observer;
};

TEST_F(WebDriverCookieTest, BasicWebDriverCookieStorage)
{
    // Test 1: Basic WebDriver cookie storage through automation session

    auto dataStore = [WKWebsiteDataStore defaultDataStore];
    auto cookieStore = dataStore.httpCookieStore;

    [cookieStore addObserver:m_observer.get()];

    // Create a cookie that would be set by WebDriver
    auto cookie = [NSHTTPCookie cookieWithProperties:@{
        NSHTTPCookieName: @"webdriver_test_cookie",
        NSHTTPCookieValue: @"webdriver_test_value",
        NSHTTPCookieDomain: @"localhost",
        NSHTTPCookiePath: @"/",
    }];

    // Set cookie through WebDriver path (simulated via cookie store)
    [cookieStore setCookie:cookie completionHandler:^{
        webDriverCookieOperationCompleted = true;
    }];

    Util::run(&webDriverCookieOperationCompleted);
    webDriverCookieOperationCompleted = false;

    // Verify cookie was stored and observer was called
    EXPECT_TRUE(cookieStoreObserverCallbackWasCalled);
    EXPECT_GT([m_observer callbackCount], 0U);

    // Verify cookie is accessible
    [cookieStore getAllCookies:^(NSArray<NSHTTPCookie *> *cookies) {
        EXPECT_EQ(cookies.count, 1U);
        if (cookies.count > 0) {
            EXPECT_TRUE([cookies[0].name isEqualToString:@"webdriver_test_cookie"]);
            EXPECT_TRUE([cookies[0].value isEqualToString:@"webdriver_test_value"]);
        }
        webDriverCookieOperationCompleted = true;
    }];

    Util::run(&webDriverCookieOperationCompleted);
    webDriverCookieOperationCompleted = false;

    [cookieStore removeObserver:m_observer.get()];
}

TEST_F(WebDriverCookieTest, WebDriverCookieObserverSignaling)
{
    // Test 2: Verify that WebDriver cookie operations trigger observer callbacks
    // This tests the equivalent of libsoup 'changed' signal emission on Apple platforms

    auto dataStore = [WKWebsiteDataStore defaultDataStore];
    auto cookieStore = dataStore.httpCookieStore;

    [cookieStore addObserver:m_observer.get()];
    [m_observer setCallbackCount:0];
    cookieStoreObserverCallbackWasCalled = false;

    // Create multiple cookies to test observer notification for each
    NSArray *cookieProperties = @[
        @{
            NSHTTPCookieName: @"webdriver_observer_test_1",
            NSHTTPCookieValue: @"observer_value_1",
            NSHTTPCookieDomain: @"localhost",
            NSHTTPCookiePath: @"/",
        },
        @{
            NSHTTPCookieName: @"webdriver_observer_test_2",
            NSHTTPCookieValue: @"observer_value_2",
            NSHTTPCookieDomain: @"localhost",
            NSHTTPCookiePath: @"/",
        },
        @{
            NSHTTPCookieName: @"webdriver_observer_test_3",
            NSHTTPCookieValue: @"observer_value_3",
            NSHTTPCookieDomain: @"localhost",
            NSHTTPCookiePath: @"/",
        }
    ];

    __block NSUInteger expectedCallbacks = cookieProperties.count;

    // Set cookies through WebDriver-like path
    for (NSDictionary *properties : cookieProperties) {
        auto cookie = [NSHTTPCookie cookieWithProperties:properties];
        [cookieStore setCookie:cookie completionHandler:^{
            expectedCallbacks--;
            if (!expectedCallbacks)
                webDriverCookieOperationCompleted = true;
        }];
    }

    Util::run(&webDriverCookieOperationCompleted);
    webDriverCookieOperationCompleted = false;

    // Verify observer was called for each cookie
    EXPECT_TRUE(cookieStoreObserverCallbackWasCalled);
    EXPECT_EQ([m_observer callbackCount], cookieProperties.count);

    [cookieStore removeObserver:m_observer.get()];
}

TEST_F(WebDriverCookieTest, WebDriverCookieDOMConsistency)
{
    // Test 3: Verify WebDriver-set cookies are visible in DOM

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    configuration.get().processPool = m_processPool.get();
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    auto dataStore = configuration.get().websiteDataStore;
    auto cookieStore = dataStore.httpCookieStore;

    // Set a cookie through WebDriver path
    auto cookie = [NSHTTPCookie cookieWithProperties:@{
        NSHTTPCookieName: @"webdriver_dom_consistency_test",
        NSHTTPCookieValue: @"dom_consistency_value",
        NSHTTPCookieDomain: @"localhost",
        NSHTTPCookiePath: @"/",
    }];

    [cookieStore setCookie:cookie completionHandler:^{
        webDriverCookieOperationCompleted = true;
    }];

    Util::run(&webDriverCookieOperationCompleted);
    webDriverCookieOperationCompleted = false;

    // Load a page and check if cookie is accessible via document.cookie
    [webView loadHTMLString:@"<html><body><script>window.testResult = document.cookie.includes('webdriver_dom_consistency_test=dom_consistency_value');</script></body></html>"
        baseURL:[NSURL URLWithString:@"http://localhost:8080/"]];
    [webView _test_waitForDidFinishNavigation];

    // Check if cookie is visible in DOM
    id result = [webView objectByEvaluatingJavaScript:@"window.testResult"];
    EXPECT_TRUE([result isKindOfClass:[NSNumber class]]);
    EXPECT_TRUE([(NSNumber *)result boolValue]);

    // Also verify through document.cookie directly
    NSString *documentCookie = [webView stringByEvaluatingJavaScript:@"document.cookie"];
    EXPECT_TRUE([documentCookie containsString:@"webdriver_dom_consistency_test=dom_consistency_value"]);
}

TEST_F(WebDriverCookieTest, WebDriverCookieStressTest)
{
    // Test 4: Stress test - multiple rapid WebDriver cookie operations

    auto dataStore = [WKWebsiteDataStore defaultDataStore];
    auto cookieStore = dataStore.httpCookieStore;

    [cookieStore addObserver:m_observer.get()];
    [m_observer setCallbackCount:0];
    cookieStoreObserverCallbackWasCalled = false;

    const NSUInteger numCookies = 20;
    __block NSUInteger operationsCompleted = 0;

    // Rapidly add multiple cookies
    for (NSUInteger i = 0; i < numCookies; ++i) {
        auto cookie = [NSHTTPCookie cookieWithProperties:@{
            NSHTTPCookieName: [NSString stringWithFormat:@"webdriver_stress_%lu", (unsigned long)i],
            NSHTTPCookieValue: [NSString stringWithFormat:@"stress_value_%lu", (unsigned long)i],
            NSHTTPCookieDomain: @"localhost",
            NSHTTPCookiePath: @"/",
        }];

        [cookieStore setCookie:cookie completionHandler:^{
            operationsCompleted++;
            if (operationsCompleted == numCookies)
                webDriverCookieOperationCompleted = true;
        }];
    }

    Util::run(&webDriverCookieOperationCompleted);
    webDriverCookieOperationCompleted = false;

    // Verify all operations triggered observer callbacks
    EXPECT_TRUE(cookieStoreObserverCallbackWasCalled);
    EXPECT_EQ([m_observer callbackCount], numCookies);

    // Verify all cookies were stored
    [cookieStore getAllCookies:^(NSArray<NSHTTPCookie *> *cookies) {
        // Should have at least our stress test cookies (may have others from other tests)
        EXPECT_GE(cookies.count, numCookies);

        NSUInteger stressCookieCount = 0;
        for (NSHTTPCookie *cookie in cookies) {
            if ([cookie.name hasPrefix:@"webdriver_stress_"])
                stressCookieCount++;
        }
        EXPECT_EQ(stressCookieCount, numCookies);

        webDriverCookieOperationCompleted = true;
    }];

    Util::run(&webDriverCookieOperationCompleted);

    [cookieStore removeObserver:m_observer.get()];
}

} // namespace TestWebKitAPI
