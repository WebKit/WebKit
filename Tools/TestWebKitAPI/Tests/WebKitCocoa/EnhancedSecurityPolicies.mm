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
#import "TestCocoa.h"
#import "TestNavigationDelegate.h"
#import "TestUIDelegate.h"
#import "TestWKWebView.h"
#import "WKWebViewConfigurationExtras.h"
#import <WebCore/SQLiteDatabase.h>
#import <WebCore/SQLiteStatement.h>
#import <WebKit/WKFoundation.h>
#import <WebKit/WKFrameInfoPrivate.h>
#import <WebKit/WKHTTPCookieStorePrivate.h>
#import <WebKit/WKHistoryDelegatePrivate.h>
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/WKProcessPoolPrivate.h>
#import <WebKit/WKWebViewConfigurationPrivate.h>
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/WKWebViewPrivateForTesting.h>
#import <WebKit/WKWebpagePreferencesPrivate.h>
#import <WebKit/WKWebsiteDataStorePrivate.h>
#import <WebKit/WebKit.h>
#import <WebKit/_WKFeature.h>
#import <WebKit/_WKFrameTreeNode.h>
#import <WebKit/_WKProcessPoolConfiguration.h>
#import <WebKit/_WKWebsiteDataStoreConfiguration.h>
#import <wtf/Assertions.h>
#import <wtf/Function.h>
#import <wtf/RetainPtr.h>
#import <wtf/text/MakeString.h>

using namespace TestWebKitAPI;

#if !PLATFORM(IOS)

@interface TestUIDelegate (EnhancedSecurityExtras)
- (NSArray *)waitForAlertWithEnhancedSecurity;
@end

@implementation TestUIDelegate (EnhancedSecurityExtras)

- (NSArray *)waitForAlertWithEnhancedSecurity
{
    EXPECT_FALSE(self.runJavaScriptAlertPanelWithMessage);

    __block bool finished = false;
    __block RetainPtr<NSString> alertMessage;
    __block RetainPtr<NSString> childFrameVariant;

    self.runJavaScriptAlertPanelWithMessage = ^(WKWebView *webView, NSString *message, WKFrameInfo *frameInfo, void (^completionHandler)(void)) {
        alertMessage = message;
        childFrameVariant = [webView _webContentProcessVariantForFrame:frameInfo._handle];
        finished = true;
        completionHandler();
    };

    TestWebKitAPI::Util::run(&finished);

    self.runJavaScriptAlertPanelWithMessage = nil;

    BOOL enhancedSecurityEnabled = [childFrameVariant isEqualToString:@"security"];

    NSArray *result = @[alertMessage.autorelease(), [NSNumber numberWithBool:enhancedSecurityEnabled]];
    return result;
}

@end

@interface WKWebView (EnhancedSecurityExtras)
- (NSArray *)_test_waitForAlertWithEnhancedSecurity;
@end

@implementation WKWebView (EnhancedSecurityExtras)

- (NSArray *)_test_waitForAlertWithEnhancedSecurity
{
    EXPECT_FALSE(self.UIDelegate);
    auto uiDelegate = adoptNS([TestUIDelegate new]);
    self.UIDelegate = uiDelegate.get();
    NSArray *result = [uiDelegate waitForAlertWithEnhancedSecurity];
    self.UIDelegate = nil;
    return result;
}

@end

static uint64_t observerCallbacks;
static RetainPtr<WKHTTPCookieStore> globalCookieStore;

@interface EnhancedSecurityCookieObserver : NSObject<WKHTTPCookieStoreObserver>
- (void)cookiesDidChangeInCookieStore:(WKHTTPCookieStore *)cookieStore;
@end

@implementation EnhancedSecurityCookieObserver

- (void)cookiesDidChangeInCookieStore:(WKHTTPCookieStore *)cookieStore
{
    ASSERT_EQ(cookieStore, globalCookieStore.get());
    ++observerCallbacks;
}

@end

@interface EnhancedSecurityHistoryDelegate : NSObject <WKHistoryDelegatePrivate> {
    @public
    RetainPtr<WKNavigationData> lastNavigationData;
    RetainPtr<NSString> lastUpdatedTitle;
    RetainPtr<NSURL> lastRedirectSource;
    RetainPtr<NSURL> lastRedirectDestination;
}
@end

@implementation EnhancedSecurityHistoryDelegate

- (void)_webView:(WKWebView *)webView didNavigateWithNavigationData:(WKNavigationData *)navigationData
{
    lastNavigationData = navigationData;
}

- (void)_webView:(WKWebView *)webView didUpdateHistoryTitle:(NSString *)title forURL:(NSURL *)URL
{
    lastUpdatedTitle = title;
}

- (void)_webView:(WKWebView *)webView didPerformServerRedirectFromURL:(NSURL *)sourceURL toURL:(NSURL *)destinationURL
{
    lastRedirectSource = sourceURL;
    lastRedirectDestination = destinationURL;
}

@end

static void testAlertWithEnhancedSecurity(RetainPtr<TestUIDelegate> uiDelegate, String message, BOOL enhancedSecurityEnabled)
{
    NSArray *result = [uiDelegate waitForAlertWithEnhancedSecurity];

    EXPECT_WK_STREQ(result[0], message);
    EXPECT_EQ([result[1] boolValue], enhancedSecurityEnabled);

    if ([result[1] boolValue] != enhancedSecurityEnabled)
        NSLog(@"Enhanced Security state mismatch for alert: %s (expected: %s, actual: %s)", message.utf8().data() , enhancedSecurityEnabled ? "Enabled" : "Disabled", [result[1] boolValue] ? "Enabled" : "Disabled");
}

static RetainPtr<TestWKWebView> enhancedSecurityTestConfiguration(
    const TestWebKitAPI::HTTPServer* plaintextServer,
    const TestWebKitAPI::HTTPServer* secureServer = nullptr,
    bool useSiteIsolation = false,
    bool useNonPersistentStore = true)
{
    auto configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];

    [configuration preferences].javaScriptCanOpenWindowsAutomatically = YES;

    auto preferences = [configuration preferences];
    for (_WKFeature *feature in [WKPreferences _features]) {
        if ((useSiteIsolation && [feature.key isEqualToString:@"SiteIsolationEnabled"])
            || [feature.key isEqualToString:@"EnhancedSecurityHeuristicsEnabled"]) {
            [preferences _setEnabled:YES forFeature:feature];
        }
    }

    auto storeConfiguration = useNonPersistentStore
        ? adoptNS([[_WKWebsiteDataStoreConfiguration alloc] initNonPersistentConfiguration])
        : adoptNS([_WKWebsiteDataStoreConfiguration new]);

    if (plaintextServer)
        [storeConfiguration setHTTPProxy:[NSURL URLWithString:[NSString stringWithFormat:@"http://127.0.0.1:%d/", plaintextServer->port()]]];

    if (secureServer)
        [storeConfiguration setHTTPSProxy:[NSURL URLWithString:[NSString stringWithFormat:@"https://127.0.0.1:%d/", secureServer->port()]]];

    [configuration setWebsiteDataStore:adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:storeConfiguration.get()]).get()];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 1, 1) configuration:configuration]);

    if (secureServer) {
        auto navigationDelegate = [TestNavigationDelegate new];
        [navigationDelegate allowAnyTLSCertificate];
        [webView setNavigationDelegate: navigationDelegate];
    }

    return webView;
}

enum class ExpectedEnhancedSecurity : bool { Disabled = false, Enabled = true };

static void runActionAndCheckEnhancedSecurityAlerts(
    RetainPtr<TestWKWebView> webView,
    Function<void()>&& performAction,
    std::initializer_list<std::pair<String, ExpectedEnhancedSecurity>> alerts)
{
    RELEASE_ASSERT(!webView.get().UIDelegate);

    __block auto uiDelegate = adoptNS([TestUIDelegate new]);
    [webView setUIDelegate:uiDelegate.get()];

    __block auto navigationDelegate = [webView navigationDelegate];
    __block RetainPtr<TestWKWebView> createdWebView;

    uiDelegate.get().createWebViewWithConfiguration = ^(WKWebViewConfiguration *configuration, WKNavigationAction *action, WKWindowFeatures *windowFeatures) {
        createdWebView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectZero configuration:configuration]);
        createdWebView.get().UIDelegate = uiDelegate.get();
        createdWebView.get().navigationDelegate = navigationDelegate;
        return createdWebView.get();
    };

    performAction();

    for (auto& pair : alerts)
        testAlertWithEnhancedSecurity(uiDelegate, pair.first, static_cast<bool>(pair.second));

    [webView setUIDelegate:nil];
}

static void loadRequestAndCheckEnhancedSecurityAlerts(
    RetainPtr<TestWKWebView> webView,
    NSString *url,
    std::initializer_list<std::pair<String, ExpectedEnhancedSecurity>> alerts)
{
    runActionAndCheckEnhancedSecurityAlerts(webView, [webView, url] {
        [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:url]]];
    }, alerts);
}

#define TEST_WITHOUT_SITE_ISOLATION(test_name) \
TEST(EnhancedSecurityPolicies, test_name) \
{ \
    run##test_name(false); \
} \

#define TEST_WITH_SITE_ISOLATION(test_name) \
TEST(EnhancedSecurityPolicies, test_name##WithSiteIsolation) \
{ \
    run##test_name(true); \
}

#define TEST_WITH_AND_WITHOUT_SITE_ISOLATION(test_name) \
TEST_WITHOUT_SITE_ISOLATION(test_name) \
\
TEST_WITH_SITE_ISOLATION(test_name)

// MARK: - Basic HTTP Detection Tests

static void runHttpLoad(bool useSiteIsolation)
{
    HTTPServer plaintextServer({
        { "http://insecure.example.internal/"_s, { "<script>alert('insecure-page')</script>"_s } },
    });

    auto webView = enhancedSecurityTestConfiguration(&plaintextServer, nullptr, useSiteIsolation);

    loadRequestAndCheckEnhancedSecurityAlerts(webView, @"http://insecure.example.internal/", {
        { "insecure-page"_s, ExpectedEnhancedSecurity::Enabled }
    });
}
TEST_WITH_AND_WITHOUT_SITE_ISOLATION(HttpLoad)

static void runHttpsLoad(bool useSiteIsolation)
{
    HTTPServer secureServer({
        { "/"_s, { "<script>alert('secure-page')</script>"_s } },
    }, HTTPServer::Protocol::HttpsProxy);

    auto webView = enhancedSecurityTestConfiguration(nullptr, &secureServer, useSiteIsolation);
    loadRequestAndCheckEnhancedSecurityAlerts(webView, @"https://secure.example.internal/", {
        { "secure-page"_s, ExpectedEnhancedSecurity::Disabled }
    });
}
TEST_WITH_AND_WITHOUT_SITE_ISOLATION(HttpsLoad)

static void runSameSiteHttpsUpgrade(bool useSiteIsolation)
{
    HTTPServer plaintextServer({
        { "http://example.co.uk/"_s, { 302, { { "Location"_s, "https://example.co.uk/"_s } }, emptyString() } }
    });

    HTTPServer secureServer({
        { "/"_s, { "<script>alert('secure-page')</script>"_s } },
    }, HTTPServer::Protocol::HttpsProxy);

    auto webView = enhancedSecurityTestConfiguration(&plaintextServer, &secureServer, useSiteIsolation);

    loadRequestAndCheckEnhancedSecurityAlerts(webView, @"http://example.co.uk/", {
        { "secure-page"_s, ExpectedEnhancedSecurity::Disabled }
    });
}
TEST_WITH_AND_WITHOUT_SITE_ISOLATION(SameSiteHttpsUpgrade)

// MARK: - Frame Tests

static void runHttpEmbeddingHttpIframe(bool useSiteIsolation)
{
    using namespace TestWebKitAPI;

    HTTPServer plaintextServer({
        { "http://insecure.example.internal/"_s, { "<iframe src='http://insecure.different.internal/'></iframe>"_s } },
        { "http://insecure.different.internal/"_s, { "<script>alert('redirected-page')</script>"_s } }
    });

    auto webView = enhancedSecurityTestConfiguration(&plaintextServer, nullptr, useSiteIsolation);
    loadRequestAndCheckEnhancedSecurityAlerts(webView, @"http://insecure.example.internal/", {
        { "redirected-page"_s, ExpectedEnhancedSecurity::Enabled }
    });
}
TEST_WITH_AND_WITHOUT_SITE_ISOLATION(HttpEmbeddingHttpIframe)

static void runHttpEmbedHttpsIframe(bool useSiteIsolation)
{
    HTTPServer plaintextServer({
        { "http://insecure.example.internal/"_s, { "<iframe src='https://secure.example.internal/'></iframe>"_s } },
    });

    HTTPServer secureServer({
        { "/"_s, { "<script>alert('embed-iframe')</script>"_s } },
    }, HTTPServer::Protocol::HttpsProxy);

    auto webView = enhancedSecurityTestConfiguration(&plaintextServer, &secureServer, useSiteIsolation);
    loadRequestAndCheckEnhancedSecurityAlerts(webView, @"http://insecure.example.internal/", {
        { "embed-iframe"_s, ExpectedEnhancedSecurity::Enabled }
    });
}
TEST_WITH_AND_WITHOUT_SITE_ISOLATION(HttpEmbedHttpsIframe)

// MARK: - Redirection Tests

static void runCrossSiteHttpRedirect(bool useSiteIsolation)
{
    HTTPServer plaintextServer({
        { "http://first.example.internal/"_s, { 302, { { "Location"_s, "http://second.different.internal/"_s } }, emptyString() } },
        { "http://second.different.internal/"_s, { "<script>alert('redirected-site')</script>"_s } },
    });

    auto webView = enhancedSecurityTestConfiguration(&plaintextServer, nullptr, useSiteIsolation);
    loadRequestAndCheckEnhancedSecurityAlerts(webView, @"http://first.example.internal/", {
        { "redirected-site"_s, ExpectedEnhancedSecurity::Enabled }
    });
}
TEST_WITH_AND_WITHOUT_SITE_ISOLATION(CrossSiteHttpRedirect)

static void runCrossSiteHttpToHttpsRedirect(bool useSiteIsolation)
{
    HTTPServer plaintextServer({
        { "http://insecure.example.internal/"_s, { 302, { { "Location"_s, "https://secure.different.internal/"_s } }, emptyString() } },
    });

    HTTPServer secureServer({
        { "/"_s, { "<script>alert('redirected-site')</script>"_s } },
    }, HTTPServer::Protocol::HttpsProxy);

    auto webView = enhancedSecurityTestConfiguration(&plaintextServer, &secureServer, useSiteIsolation);
    loadRequestAndCheckEnhancedSecurityAlerts(webView, @"http://insecure.example.internal/", {
        { "redirected-site"_s, ExpectedEnhancedSecurity::Enabled }
    });
}
TEST_WITH_AND_WITHOUT_SITE_ISOLATION(CrossSiteHttpToHttpsRedirect)

static void runHttpsOnlyExplicitlyBypassedWithHttpRedirect(bool useSiteIsolation)
{
    HTTPServer plaintextServer({
        { "http://site.example/secure"_s, { { }, "hi: not secure"_s } },
        { "http://site2.example/secure2"_s, { { }, "hi: not secure"_s } },
        { "http://site2.example/secure3"_s, { { }, "hi: not secure"_s } },
    }, HTTPServer::Protocol::Http);

    HTTPServer secureServer({
        { "/secure"_s, { 302, {{ "Location"_s, "http://site.example/secure"_s }}, "redirecting..."_s } },
        { "/secure2"_s, { 302, {{ "Location"_s, "http://site2.example/secure3"_s }}, "redirecting..."_s } },
        { "/secure3"_s, { 302, {{ "Location"_s, "http://site2.example/secure2"_s }}, "redirecting..."_s } },
    }, HTTPServer::Protocol::HttpsProxy);

    auto webView = enhancedSecurityTestConfiguration(&plaintextServer, &secureServer, useSiteIsolation);

    __block int errorCode { 0 };
    __block bool finishedSuccessfully { false };
    __block int loadCount { 0 };

    RetainPtr<TestNavigationDelegate> delegate = adoptNS([webView navigationDelegate]);
    delegate.get().decidePolicyForNavigationAction = ^(WKNavigationAction *action, void (^completionHandler)(WKNavigationActionPolicy)) {
        ++loadCount;
        completionHandler(WKNavigationActionPolicyAllow);
    };

    delegate.get().didFinishNavigation = ^(WKWebView *, WKNavigation *) {
        finishedSuccessfully = true;
    };

    delegate.get().didFailProvisionalNavigation = ^(WKWebView *, WKNavigation *, NSError *error) {
        EXPECT_NULL(error);
        if (error)
            errorCode = error.code;
    };

    [webView configuration].defaultWebpagePreferences._networkConnectionIntegrityPolicy = _WKWebsiteNetworkConnectionIntegrityPolicyHTTPSOnly | _WKWebsiteNetworkConnectionIntegrityPolicyHTTPSOnlyExplicitlyBypassedForDomain;

    errorCode = 0;
    finishedSuccessfully = false;
    loadCount = 0;

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://site.example/secure"]]];
    TestWebKitAPI::Util::run(&finishedSuccessfully);

    EXPECT_EQ(errorCode, 0);
    EXPECT_TRUE(finishedSuccessfully);
    EXPECT_EQ(loadCount, 2);
    EXPECT_WK_STREQ(@"http://site.example/secure", [webView URL].absoluteString);

    errorCode = 0;
    finishedSuccessfully = false;
    loadCount = 0;
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://site.example/secure2"]]];
    TestWebKitAPI::Util::run(&finishedSuccessfully);

    EXPECT_EQ(errorCode, 0);
    EXPECT_TRUE(finishedSuccessfully);
    EXPECT_EQ(loadCount, 3);
    EXPECT_WK_STREQ(@"http://site2.example/secure2", [webView URL].absoluteString);

    // Now use a different domain to the above to ensure a process swap happens,
    // this ensures testing of an edge case in the HTTPS upgrade / same-site HTTP logic.
    errorCode = 0;
    finishedSuccessfully = false;
    loadCount = 0;
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://different.example/secure2"]]];
    TestWebKitAPI::Util::run(&finishedSuccessfully);

    EXPECT_EQ(errorCode, 0);
    EXPECT_TRUE(finishedSuccessfully);
    EXPECT_EQ(loadCount, 3);
    EXPECT_WK_STREQ(@"http://site2.example/secure2", [webView URL].absoluteString);
}
TEST_WITHOUT_SITE_ISOLATION(HttpsOnlyExplicitlyBypassedWithHttpRedirect)

static void runHttpsToSameSiteHttpExplicitRedirect(bool useSiteIsolation)
{
    HTTPServer plaintextServer({
        { "http://download/redirectTarget"_s, { "<script>alert('insecure-site')</script>"_s } },
    });

    HTTPServer secureServer({
        { "/originalRequest"_s, { 302, { { "Location"_s, "http://download/redirectTarget"_s } }, emptyString() } },
    }, HTTPServer::Protocol::HttpsProxy);

    auto webView = enhancedSecurityTestConfiguration(&plaintextServer, &secureServer, useSiteIsolation);
    loadRequestAndCheckEnhancedSecurityAlerts(webView, @"https://download/originalRequest", {
        { "insecure-site"_s, ExpectedEnhancedSecurity::Enabled }
    });
}
TEST_WITH_AND_WITHOUT_SITE_ISOLATION(HttpsToSameSiteHttpExplicitRedirect)

// MARK: - Window Tests

static void runHttpOpeningHttpsWindow(bool useSiteIsolation)
{
    HTTPServer plaintextServer({
        { "http://insecure.example.internal/"_s, { "<script>window.onload = function() { window.open('https://secure.different.internal/'); }</script>"_s } },
    });

    HTTPServer secureServer({
        { "/"_s, { "<script>alert('opened-window')</script>"_s } },
    }, HTTPServer::Protocol::HttpsProxy);

    auto webView = enhancedSecurityTestConfiguration(&plaintextServer, &secureServer, useSiteIsolation);
    loadRequestAndCheckEnhancedSecurityAlerts(webView, @"http://insecure.example.internal/", {
        { "opened-window"_s, ExpectedEnhancedSecurity::Enabled }
    });
}
TEST_WITH_AND_WITHOUT_SITE_ISOLATION(HttpOpeningHttpsWindow)

static void runHttpOpeningHttpsTargetSelf(bool useSiteIsolation)
{
    HTTPServer plaintextServer({
        { "http://insecure.example.internal/"_s, { "<script>window.onload = function() { window.open('https://secure.different.internal/', '_self', 'noopener'); }</script>"_s } }
    });

    HTTPServer secureServer({
        { "/"_s, { "<script>alert('opened-window')</script>"_s } },
    }, HTTPServer::Protocol::HttpsProxy);

    auto webView = enhancedSecurityTestConfiguration(&plaintextServer, &secureServer, useSiteIsolation);
    loadRequestAndCheckEnhancedSecurityAlerts(webView, @"http://insecure.example.internal/", {
        { "opened-window"_s, ExpectedEnhancedSecurity::Enabled }
    });
}
TEST_WITH_AND_WITHOUT_SITE_ISOLATION(HttpOpeningHttpsTargetSelf)

static void runHttpsOpeningHttp(bool useSiteIsolation)
{
    HTTPServer plaintextServer({
        { "http://insecure.example.internal/"_s, { "<script>alert('opened-window'); alert(!!window.opener)</script>"_s } }
    });

    HTTPServer secureServer({
        { "/"_s, { "<script>window.onload = function() { window.open('http://insecure.example.internal/'); }</script>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto webView = enhancedSecurityTestConfiguration(&plaintextServer, &secureServer, useSiteIsolation);

    loadRequestAndCheckEnhancedSecurityAlerts(webView, @"https://secure.different.internal/", {
        { "opened-window"_s, ExpectedEnhancedSecurity::Disabled },
        { "true"_s, ExpectedEnhancedSecurity::Disabled }
    });
}
TEST_WITH_AND_WITHOUT_SITE_ISOLATION(HttpsOpeningHttp)

static void runOpenerMultipleNavigations(bool useSiteIsolation)
{
    HTTPServer plaintextServer({
        { "http://insecure.example.internal/"_s, { "<script>alert('opened-window'); alert(!!window.opener); window.location = 'http://insecure.final.internal/second-page'</script>"_s } },
        { "http://insecure.final.internal/second-page"_s, { "<script>alert('second-page'); alert(!!window.opener);</script>"_s } }
    });

    HTTPServer secureServer({
        { "/"_s, { "<script>window.onload = function() { window.open('http://insecure.example.internal/'); }</script>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto webView = enhancedSecurityTestConfiguration(&plaintextServer, &secureServer, useSiteIsolation);

    loadRequestAndCheckEnhancedSecurityAlerts(webView, @"https://secure.different.internal/", {
        { "opened-window"_s, ExpectedEnhancedSecurity::Disabled },
        { "true"_s, ExpectedEnhancedSecurity::Disabled },
        { "second-page"_s, ExpectedEnhancedSecurity::Disabled },
        { "true"_s, ExpectedEnhancedSecurity::Disabled }
    });
}
TEST_WITH_AND_WITHOUT_SITE_ISOLATION(OpenerMultipleNavigations)

static void runOpenerThenSelfNavigation(bool useSiteIsolation)
{
    HTTPServer plaintextServer({
        { "http://insecure.example.internal/"_s, { "<script>alert('initial-page'); window.open('https://secure.different.internal/'); window.location = 'https://secure.final.internal/self-navigate';</script>"_s } }
    });

    HTTPServer secureServer({
        { "/"_s, { "<script>setTimeout(function() { alert('opened-window'); alert(!!window.opener); window.opener.location = 'https://secure.final.internal/navigated'; }, 500);</script>"_s } },
        { "/self-navigate"_s, { "<script>alert('self-new-page');</script>"_s } },
        { "/navigated"_s, { "<script>alert('navigated');</script>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto webView = enhancedSecurityTestConfiguration(&plaintextServer, &secureServer, useSiteIsolation);

    // Ensure the navigation to secure.final.internal would be considered valid
    // to drop out of Enhanced Security by having a cookie present.
    RetainPtr<NSHTTPCookie> sessionCookie = [NSHTTPCookie cookieWithProperties:@{
        NSHTTPCookiePath: @"/",
        NSHTTPCookieName: @"CookieName",
        NSHTTPCookieValue: @"CookieValue",
        NSHTTPCookieDomain: @"secure.final.internal",
    }];
    [[webView configuration].websiteDataStore.httpCookieStore setCookie:sessionCookie.get() completionHandler:^{ }];

    loadRequestAndCheckEnhancedSecurityAlerts(webView, @"http://insecure.example.internal/", {
        { "initial-page"_s, ExpectedEnhancedSecurity::Enabled },
        { "self-new-page"_s, ExpectedEnhancedSecurity::Enabled },
        { "opened-window"_s, ExpectedEnhancedSecurity::Enabled },
        { "true"_s, ExpectedEnhancedSecurity::Enabled },
        { "navigated"_s, ExpectedEnhancedSecurity::Enabled },
    });
}

TEST_WITH_AND_WITHOUT_SITE_ISOLATION(OpenerThenSelfNavigation)

static void runHttpOpeningHttpsNoOpener(bool useSiteIsolation)
{
    HTTPServer plaintextServer({
        { "http://insecure.example.internal/"_s, { "<script>window.onload = function() { window.open('https://secure.different.internal/', '_blank', 'noopener'); }</script>"_s } }
    });

    HTTPServer secureServer({
        { "/"_s, { "<script>alert('opened-window')</script>"_s } },
    }, HTTPServer::Protocol::HttpsProxy);

    auto webView = enhancedSecurityTestConfiguration(&plaintextServer, &secureServer, useSiteIsolation);
    loadRequestAndCheckEnhancedSecurityAlerts(webView, @"http://insecure.example.internal/", {
        { "opened-window"_s, ExpectedEnhancedSecurity::Enabled }
    });
}
TEST_WITH_AND_WITHOUT_SITE_ISOLATION(HttpOpeningHttpsNoOpener)

static void runHttpLocationRedirectsHttps(bool useSiteIsolation)
{
    HTTPServer plaintextServer({
        { "http://insecure.example.internal/"_s, { "<script>window.onload = function() { window.location = 'https://secure.different.internal/'; }</script>"_s } }
    });

    HTTPServer secureServer({
        { "/"_s, { "<script>alert('location-redirected-site')</script>"_s } },
    }, HTTPServer::Protocol::HttpsProxy);

    auto webView = enhancedSecurityTestConfiguration(&plaintextServer, &secureServer, useSiteIsolation);
    loadRequestAndCheckEnhancedSecurityAlerts(webView, @"http://insecure.example.internal/", {
        { "location-redirected-site"_s, ExpectedEnhancedSecurity::Enabled }
    });
}
TEST_WITH_AND_WITHOUT_SITE_ISOLATION(HttpLocationRedirectsHttps)

// MARK: - User / Client Input Tests

static void runHttpThenUserNavigateToHttps(bool useSiteIsolation)
{
    HTTPServer plaintextServer({
        { "http://insecure.example.internal/"_s, { "<script>alert('insecure-first-site');</script>"_s } },
    });

    HTTPServer secureServer({
        { "/"_s, { "<script>alert('secure-second-site');</script>"_s } },
    }, HTTPServer::Protocol::HttpsProxy);

    auto webView = enhancedSecurityTestConfiguration(&plaintextServer, &secureServer, useSiteIsolation);

    loadRequestAndCheckEnhancedSecurityAlerts(webView, @"http://insecure.example.internal/", {
        { "insecure-first-site"_s, ExpectedEnhancedSecurity::Enabled }
    });

    loadRequestAndCheckEnhancedSecurityAlerts(webView, @"https://secure.example.internal/", {
        { "secure-second-site"_s, ExpectedEnhancedSecurity::Disabled }
    });
}
TEST_WITH_AND_WITHOUT_SITE_ISOLATION(HttpThenUserNavigateToHttps)

static void runHttpThenClickLinkToHttps(bool useSiteIsolation)
{
    HTTPServer plaintextServer({
        { "http://insecure.example.internal/"_s, { "<script>alert('insecure-first-site');</script><a id='testLink' href='https://secure.different.internal/'>Link</a>"_s } },
    });

    HTTPServer secureServer({
        { "/"_s, { "<script>alert('secure-second-site')</script>"_s } },
    }, HTTPServer::Protocol::HttpsProxy);

    auto webView = enhancedSecurityTestConfiguration(&plaintextServer, &secureServer, useSiteIsolation);

    loadRequestAndCheckEnhancedSecurityAlerts(webView, @"http://insecure.example.internal/", {
        { "insecure-first-site"_s, ExpectedEnhancedSecurity::Enabled }
    });

    runActionAndCheckEnhancedSecurityAlerts(webView, [webView]() {
        [webView clickOnElementID:@"testLink"];
    }, {
        { "secure-second-site"_s, ExpectedEnhancedSecurity::Enabled }
    });
}
TEST_WITH_AND_WITHOUT_SITE_ISOLATION(HttpThenClickLinkToHttps)

static void runHttpsToHttpsThenBack(bool useSiteIsolation)
{
    HTTPServer secureServer({
        { "/first"_s, { "<script>window.addEventListener('pageshow', function() { alert('first-page'); });</script>"_s } },
        { "/second"_s, { "<script>alert('second-page')</script>"_s } },
    }, HTTPServer::Protocol::HttpsProxy);

    auto webView = enhancedSecurityTestConfiguration(nullptr, &secureServer, useSiteIsolation);
    loadRequestAndCheckEnhancedSecurityAlerts(webView, @"https://secure.example.internal/first", {
        { "first-page"_s, ExpectedEnhancedSecurity::Disabled }
    });

    loadRequestAndCheckEnhancedSecurityAlerts(webView, @"https://secure.example.internal/second", {
        { "second-page"_s, ExpectedEnhancedSecurity::Disabled }
    });

    runActionAndCheckEnhancedSecurityAlerts(webView, [webView]() {
        auto* backForwardList = [webView backForwardList];

        EXPECT_TRUE(!!backForwardList.backItem);
        EXPECT_EQ(1U, backForwardList.backList.count);

        [webView goBack];
    }, {
        { "first-page"_s, ExpectedEnhancedSecurity::Disabled }
    });
}
TEST_WITH_AND_WITHOUT_SITE_ISOLATION(HttpsToHttpsThenBack)

static void runHttpNavigateToHttpsThenBack(bool useSiteIsolation)
{
    HTTPServer plaintextServer({
        { "http://insecure.example.internal/"_s, { "<script>window.addEventListener('pageshow', function() { alert('insecure-first-site'); });</script>"_s } },
    });

    HTTPServer secureServer({
        { "/"_s, { "<script>alert('secure-page');</script>"_s } },
    }, HTTPServer::Protocol::HttpsProxy);

    auto webView = enhancedSecurityTestConfiguration(&plaintextServer, &secureServer, useSiteIsolation);
    loadRequestAndCheckEnhancedSecurityAlerts(webView, @"http://insecure.example.internal/", {
        { "insecure-first-site"_s, ExpectedEnhancedSecurity::Enabled }
    });

    loadRequestAndCheckEnhancedSecurityAlerts(webView, @"https://secure.example.internal/", {
        { "secure-page"_s, ExpectedEnhancedSecurity::Disabled }
    });

    runActionAndCheckEnhancedSecurityAlerts(webView, [webView]() {
        auto* backForwardList = [webView backForwardList];

        EXPECT_TRUE(!!backForwardList.backItem);
        EXPECT_EQ(1U, backForwardList.backList.count);

        [webView goBack];
    }, {
        { "insecure-first-site"_s, ExpectedEnhancedSecurity::Enabled }
    });
}
TEST_WITH_AND_WITHOUT_SITE_ISOLATION(HttpNavigateToHttpsThenBack)

static void runMultiHopThenBack(bool useSiteIsolation)
{
    HTTPServer plaintextServer({
        { "http://insecure.example.internal/"_s, { "<script>alert('insecure-first-site');  window.location.href = 'https://secure.example.internal/tainted'; </script>"_s } },
    });

    HTTPServer secureServer({
        { "/tainted"_s, { "<script>window.addEventListener('pageshow', function() { alert('tainted-https-site'); });</script>"_s } },
        { "/secure"_s, { "<script>alert('secure-page');</script>"_s } },
    }, HTTPServer::Protocol::HttpsProxy);

    auto webView = enhancedSecurityTestConfiguration(&plaintextServer, &secureServer, useSiteIsolation);
    loadRequestAndCheckEnhancedSecurityAlerts(webView, @"http://insecure.example.internal/", {
        { "insecure-first-site"_s, ExpectedEnhancedSecurity::Enabled },
        { "tainted-https-site"_s, ExpectedEnhancedSecurity::Enabled }
    });

    loadRequestAndCheckEnhancedSecurityAlerts(webView, @"https://secure.different.internal/secure", {
        { "secure-page"_s, ExpectedEnhancedSecurity::Disabled }
    });

    runActionAndCheckEnhancedSecurityAlerts(webView, [webView]() {
        auto* backForwardList = [webView backForwardList];

        EXPECT_TRUE(!!backForwardList.backItem);
        EXPECT_EQ(1U, backForwardList.backList.count);

        [webView goBack];
    }, {
        { "tainted-https-site"_s, ExpectedEnhancedSecurity::Enabled }
    });
}
TEST_WITH_AND_WITHOUT_SITE_ISOLATION(MultiHopThenBack)

static void runMultiHopThenBackJavascript(bool useSiteIsolation)
{
    HTTPServer plaintextServer({
        { "http://insecure.example.internal/"_s, { "<script>alert('insecure-first-site');  window.location.href = 'https://secure.example.internal/tainted'; </script>"_s } },
    });

    HTTPServer secureServer({
        { "/tainted"_s, { "<script>window.addEventListener('pageshow', function() { alert('tainted-https-site'); });</script>"_s } },
        { "/secure"_s, { "<script>alert('secure-page');</script>"_s } },
    }, HTTPServer::Protocol::HttpsProxy);

    auto webView = enhancedSecurityTestConfiguration(&plaintextServer, &secureServer, useSiteIsolation);
    loadRequestAndCheckEnhancedSecurityAlerts(webView, @"http://insecure.example.internal/", {
        { "insecure-first-site"_s, ExpectedEnhancedSecurity::Enabled },
        { "tainted-https-site"_s, ExpectedEnhancedSecurity::Enabled }
    });

    loadRequestAndCheckEnhancedSecurityAlerts(webView, @"https://secure.different.internal/secure", {
        { "secure-page"_s, ExpectedEnhancedSecurity::Disabled }
    });

    runActionAndCheckEnhancedSecurityAlerts(webView, [webView]() {
        [webView evaluateJavaScript:@"history.back()" completionHandler:nil];
    }, {
        { "tainted-https-site"_s, ExpectedEnhancedSecurity::Enabled }
    });
}
TEST_WITH_AND_WITHOUT_SITE_ISOLATION(MultiHopThenBackJavascript)

static void runMultiHopThenBackToSecure(bool useSiteIsolation)
{
    HTTPServer plaintextServer({
        { "http://insecure.example.internal/"_s, { "<script>alert('insecure-site');</script>"_s } },
    });

    HTTPServer secureServer({
        { "/"_s, { "<script>window.addEventListener('pageshow', function() { alert('secure-page'); });</script>"_s } },
    }, HTTPServer::Protocol::HttpsProxy);

    auto webView = enhancedSecurityTestConfiguration(&plaintextServer, &secureServer, useSiteIsolation);
    loadRequestAndCheckEnhancedSecurityAlerts(webView, @"http://insecure.example.internal/", {
        { "insecure-site"_s, ExpectedEnhancedSecurity::Enabled }
    });

    loadRequestAndCheckEnhancedSecurityAlerts(webView, @"https://secure.example.internal/", {
        { "secure-page"_s, ExpectedEnhancedSecurity::Disabled }
    });

    loadRequestAndCheckEnhancedSecurityAlerts(webView, @"http://insecure.example.internal/", {
        { "insecure-site"_s, ExpectedEnhancedSecurity::Enabled }
    });

    runActionAndCheckEnhancedSecurityAlerts(webView, [webView]() {
        auto* backForwardList = [webView backForwardList];

        EXPECT_TRUE(!!backForwardList.backItem);
        EXPECT_EQ(2U, backForwardList.backList.count);

        [webView goBack];
    }, {
        { "secure-page"_s, ExpectedEnhancedSecurity::Disabled }
    });
}
TEST_WITH_AND_WITHOUT_SITE_ISOLATION(MultiHopThenBackToSecure)

static void runMultiHopThenBackToSecureJavascript(bool useSiteIsolation)
{
    HTTPServer plaintextServer({
        { "http://insecure.example.internal/"_s, { "<script>alert('insecure-site');</script>"_s } },
    });

    HTTPServer secureServer({
        { "/"_s, { "<script>window.addEventListener('pageshow', function() { alert('secure-page'); });</script>"_s } },
    }, HTTPServer::Protocol::HttpsProxy);

    auto webView = enhancedSecurityTestConfiguration(&plaintextServer, &secureServer, useSiteIsolation);
    loadRequestAndCheckEnhancedSecurityAlerts(webView, @"http://insecure.example.internal/", {
        { "insecure-site"_s, ExpectedEnhancedSecurity::Enabled }
    });

    loadRequestAndCheckEnhancedSecurityAlerts(webView, @"https://secure.example.internal/", {
        { "secure-page"_s, ExpectedEnhancedSecurity::Disabled }
    });

    loadRequestAndCheckEnhancedSecurityAlerts(webView, @"http://insecure.example.internal/", {
        { "insecure-site"_s, ExpectedEnhancedSecurity::Enabled }
    });

    runActionAndCheckEnhancedSecurityAlerts(webView, [webView]() {
        auto* backForwardList = [webView backForwardList];

        EXPECT_TRUE(!!backForwardList.backItem);
        EXPECT_EQ(2U, backForwardList.backList.count);

        [webView evaluateJavaScript:@"history.back()" completionHandler:nil];
    }, {
        { "secure-page"_s, ExpectedEnhancedSecurity::Disabled }
    });
}
TEST_WITH_AND_WITHOUT_SITE_ISOLATION(MultiHopThenBackToSecureJavascript)

static void runWebViewSameSiteMainFrameNavigation(bool useSiteIsolation)
{
    HTTPServer plaintextServer({
        { "http://insecure.example.internal/"_s, { "<script>alert('insecure-site');  window.location.href = 'https://secure.example.internal/tainted'; </script>"_s } },
    });

    HTTPServer secureServer({
        { "/"_s, { "<script>alert('secure-page');</script>"_s } },
        { "/tainted"_s, { "<script>alert('tainted-page');</script>"_s } },
    }, HTTPServer::Protocol::HttpsProxy);

    auto webView = enhancedSecurityTestConfiguration(&plaintextServer, &secureServer, useSiteIsolation);
    loadRequestAndCheckEnhancedSecurityAlerts(webView, @"http://insecure.example.internal/", {
        { "insecure-site"_s, ExpectedEnhancedSecurity::Enabled },
        { "tainted-page"_s, ExpectedEnhancedSecurity::Enabled }
    });

    loadRequestAndCheckEnhancedSecurityAlerts(webView, @"https://secure.example.internal/", {
        { "secure-page"_s, ExpectedEnhancedSecurity::Enabled }
    });

    loadRequestAndCheckEnhancedSecurityAlerts(webView, @"https://secure.different.internal/", {
        { "secure-page"_s, ExpectedEnhancedSecurity::Disabled }
    });
}
TEST_WITH_SITE_ISOLATION(WebViewSameSiteMainFrameNavigation)

// MARK: - Cookies / Local Storage / Meaningful Site Checks

static void runHttpThenNavigateToHttpsSiteWithCookies(bool useSiteIsolation)
{
    HTTPServer plaintextServer({
        { "http://insecure.example.internal/"_s, { "<script>alert('insecure-site');</script>"_s } },
    });

    HTTPServer secureServer({
        { "/"_s, { "<script>alert('secure-site-with-cookies');</script>"_s } },
        { "/set-cookie"_s, { "<script>document.cookie = 'CookieName=CookieValue; Path=/'; alert('cookie-set');</script>"_s } },
    }, HTTPServer::Protocol::HttpsProxy);

    auto webView = enhancedSecurityTestConfiguration(&plaintextServer, &secureServer, useSiteIsolation);

    loadRequestAndCheckEnhancedSecurityAlerts(webView, @"https://secure.example.internal/set-cookie", {
        { "cookie-set"_s, ExpectedEnhancedSecurity::Disabled }
    });

    loadRequestAndCheckEnhancedSecurityAlerts(webView, @"http://insecure.example.internal/", {
        { "insecure-site"_s, ExpectedEnhancedSecurity::Enabled }
    });

    runActionAndCheckEnhancedSecurityAlerts(webView, [webView]() {
        [webView evaluateJavaScript:@"window.location.href = 'https://secure.example.internal/'" completionHandler:nil];
    }, {
        { "secure-site-with-cookies"_s, ExpectedEnhancedSecurity::Disabled }
    });
}
TEST_WITH_AND_WITHOUT_SITE_ISOLATION(HttpThenNavigateToHttpsSiteWithCookies)

static void runHttpThenNavigateToHttpsSiteWithLocalStorage(bool useSiteIsolation)
{
    HTTPServer plaintextServer({
        { "http://insecure.example.internal/"_s, { "<script>alert('insecure-site');</script>"_s } },
    });

    HTTPServer secureServer({
        { "/"_s, { "<script>alert('secure-site-with-storage');</script>"_s } },
        { "/set-storage"_s, { "<script>localStorage.setItem('KeyName', 'KeyValue'); alert('storage-set');</script>"_s } },
    }, HTTPServer::Protocol::HttpsProxy);

    auto webView = enhancedSecurityTestConfiguration(&plaintextServer, &secureServer, useSiteIsolation);

    loadRequestAndCheckEnhancedSecurityAlerts(webView, @"https://secure.example.internal/set-storage", {
        { "storage-set"_s, ExpectedEnhancedSecurity::Disabled }
    });

    loadRequestAndCheckEnhancedSecurityAlerts(webView, @"http://insecure.example.internal/", {
        { "insecure-site"_s, ExpectedEnhancedSecurity::Enabled }
    });

    runActionAndCheckEnhancedSecurityAlerts(webView, [webView]() {
        [webView evaluateJavaScript:@"window.location.href = 'https://secure.example.internal/'" completionHandler:nil];
    }, {
        { "secure-site-with-storage"_s, ExpectedEnhancedSecurity::Disabled }
    });
}
TEST_WITH_AND_WITHOUT_SITE_ISOLATION(HttpThenNavigateToHttpsSiteWithLocalStorage)

static void runHttpThenHttpsThenNavigateToHttpsSiteWithCookies(bool useSiteIsolation)
{
    HTTPServer plaintextServer({
        { "http://insecure.example.internal/"_s, { "<script>alert('insecure-site');</script>"_s } },
    });

    HTTPServer secureServer({
        { "/"_s, { "<script>alert(document.cookie ? 'secure-site-with-cookies' : 'secure-site-without-cookies');</script>"_s } },
        { "/set-cookie"_s, { "<script>document.cookie = 'CookieName=CookieValue; Path=/'; alert('cookie-set');</script>"_s } },
    }, HTTPServer::Protocol::HttpsProxy);

    auto webView = enhancedSecurityTestConfiguration(&plaintextServer, &secureServer, useSiteIsolation);

    loadRequestAndCheckEnhancedSecurityAlerts(webView, @"https://secure.different.internal/set-cookie", {
        { "cookie-set"_s, ExpectedEnhancedSecurity::Disabled }
    });

    loadRequestAndCheckEnhancedSecurityAlerts(webView, @"http://insecure.example.internal/", {
        { "insecure-site"_s, ExpectedEnhancedSecurity::Enabled }
    });

    runActionAndCheckEnhancedSecurityAlerts(webView, [webView]() {
        [webView evaluateJavaScript:@"window.location.href = 'https://secure.example.internal/'" completionHandler:nil];
    }, {
        { "secure-site-without-cookies"_s, ExpectedEnhancedSecurity::Enabled }
    });

    runActionAndCheckEnhancedSecurityAlerts(webView, [webView]() {
        [webView evaluateJavaScript:@"window.location.href = 'https://secure.different.internal/'" completionHandler:nil];
    }, {
        { "secure-site-with-cookies"_s, ExpectedEnhancedSecurity::Disabled }
    });
}
TEST_WITH_AND_WITHOUT_SITE_ISOLATION(HttpThenHttpsThenNavigateToHttpsSiteWithCookies)

static void runHttpThenNavigateToHttpsSiteWithCookiesThenHttps(bool useSiteIsolation)
{
    HTTPServer plaintextServer({
        { "http://insecure.example.internal/"_s, { "<script>alert('insecure-site');</script>"_s } },
    });

    HTTPServer secureServer({
        { "/"_s, { "<script>alert(document.cookie ? 'secure-site-with-cookies' : 'secure-site-without-cookies');</script>"_s } },
        { "/set-cookie"_s, { "<script>document.cookie = 'CookieName=CookieValue; Path=/'; alert('cookie-set');</script>"_s } },
    }, HTTPServer::Protocol::HttpsProxy);

    auto webView = enhancedSecurityTestConfiguration(&plaintextServer, &secureServer, useSiteIsolation);

    loadRequestAndCheckEnhancedSecurityAlerts(webView, @"https://secure.different.internal/set-cookie", {
        { "cookie-set"_s, ExpectedEnhancedSecurity::Disabled }
    });

    loadRequestAndCheckEnhancedSecurityAlerts(webView, @"http://insecure.example.internal/", {
        { "insecure-site"_s, ExpectedEnhancedSecurity::Enabled }
    });

    runActionAndCheckEnhancedSecurityAlerts(webView, [webView]() {
        [webView evaluateJavaScript:@"window.location.href = 'https://secure.different.internal/'" completionHandler:nil];
    }, {
        { "secure-site-with-cookies"_s, ExpectedEnhancedSecurity::Disabled }
    });

    runActionAndCheckEnhancedSecurityAlerts(webView, [webView]() {
        [webView evaluateJavaScript:@"window.location.href = 'https://secure.example.internal/'" completionHandler:nil];
    }, {
        { "secure-site-without-cookies"_s, ExpectedEnhancedSecurity::Enabled }
    });
}
TEST_WITH_AND_WITHOUT_SITE_ISOLATION(HttpThenNavigateToHttpsSiteWithCookiesThenHttps)

static void runHttpThenNavigateToHttpsSetCookiesThenNavigateToHttpsAgain(bool useSiteIsolation)
{
    HTTPServer plaintextServer({
        { "http://insecure.example.internal/"_s, { "<script>alert('insecure-site');</script>"_s } },
    });

    HTTPServer secureServer({
        { "/"_s, { "<script>alert(document.cookie ? 'secure-site-with-cookies' : 'secure-site-without-cookies');</script>"_s } },
        { "/set-cookie"_s, { "<script>document.cookie = 'CookieName=CookieValue; Path=/'; alert('cookie-set');</script>"_s } },
    }, HTTPServer::Protocol::HttpsProxy);

    auto webView = enhancedSecurityTestConfiguration(&plaintextServer, &secureServer, useSiteIsolation);

    loadRequestAndCheckEnhancedSecurityAlerts(webView, @"http://insecure.example.internal/", {
        { "insecure-site"_s, ExpectedEnhancedSecurity::Enabled }
    });

    runActionAndCheckEnhancedSecurityAlerts(webView, [webView]() {
        [webView evaluateJavaScript:@"window.location.href = 'https://secure.example.internal/set-cookie'" completionHandler:nil];
    }, {
        { "cookie-set"_s, ExpectedEnhancedSecurity::Enabled }
    });

    runActionAndCheckEnhancedSecurityAlerts(webView, [webView]() {
        [webView evaluateJavaScript:@"window.location.href = 'https://secure.example.internal/'" completionHandler:nil];
    }, {
        { "secure-site-with-cookies"_s, ExpectedEnhancedSecurity::Enabled }
    });
}
TEST_WITH_AND_WITHOUT_SITE_ISOLATION(HttpThenNavigateToHttpsSetCookiesThenNavigateToHttpsAgain)

static void runHttpRedirectsHttpsWithExplicitNavigationToMeaningfulSite(bool useSiteIsolation)
{
    HTTPServer plaintextServer({
        { "http://insecure.example.internal/"_s, { "<script>window.onload = function() { window.location = 'https://secure.different.internal/'; }</script>"_s } }
    });

    HTTPServer secureServer({
        { "/"_s, { "<script>alert('location-redirected-site'); window.location = 'https://secure.other.internal/domain';</script>"_s } },
        { "/domain"_s, { "<script>alert(window.location);</script>"_s } },
        { "/explicit-set-cookies"_s, { "<script>document.cookie = 'CookieName=CookieValue; Path=/'; alert('explicit-navigation-set-cookies')</script>"_s } },
    }, HTTPServer::Protocol::HttpsProxy);

    auto webView = enhancedSecurityTestConfiguration(&plaintextServer, &secureServer, useSiteIsolation);
    loadRequestAndCheckEnhancedSecurityAlerts(webView, @"http://insecure.example.internal/", {
        { "location-redirected-site"_s, ExpectedEnhancedSecurity::Enabled },
        { "https://secure.other.internal/domain"_s, ExpectedEnhancedSecurity::Enabled }
    });

    loadRequestAndCheckEnhancedSecurityAlerts(webView, @"https://secure.different.internal/explicit-set-cookies", {
        { "explicit-navigation-set-cookies"_s, ExpectedEnhancedSecurity::Disabled }
    });

    loadRequestAndCheckEnhancedSecurityAlerts(webView, @"http://insecure.example.internal/", {
        { "location-redirected-site"_s, ExpectedEnhancedSecurity::Disabled }
    });
}
TEST_WITH_AND_WITHOUT_SITE_ISOLATION(HttpRedirectsHttpsWithExplicitNavigationToMeaningfulSite)

static void runWindowOpenThenNavigateToMeaningfulSite(bool useSiteIsolation)
{
    HTTPServer plaintextServer({
        { "http://insecure.example.internal/"_s, { "<script>alert('insecure-site');</script>"_s } },
    });

    HTTPServer secureServer({
        { "/"_s, { "<script>alert(document.cookie ? 'secure-site-with-cookies' : 'secure-site-without-cookies');</script>"_s } },
        { "/set-cookie"_s, { "<script>document.cookie = 'CookieName=CookieValue; Path=/'; alert('cookie-set');</script>"_s } },
    }, HTTPServer::Protocol::HttpsProxy);

    auto webView = enhancedSecurityTestConfiguration(&plaintextServer, &secureServer, useSiteIsolation);

    loadRequestAndCheckEnhancedSecurityAlerts(webView, @"http://insecure.example.internal/", {
        { "insecure-site"_s, ExpectedEnhancedSecurity::Enabled }
    });

    runActionAndCheckEnhancedSecurityAlerts(webView, [webView]() {
        [webView evaluateJavaScript:@"window.location.href = 'https://secure.example.internal/set-cookie'" completionHandler:nil];
    }, {
        { "cookie-set"_s, ExpectedEnhancedSecurity::Enabled }
    });

    runActionAndCheckEnhancedSecurityAlerts(webView, [webView]() {
        [webView evaluateJavaScript:@"window.open('https://secure.example.internal/', '_blank', 'noopener');" completionHandler:nil];
    }, {
        { "secure-site-with-cookies"_s, ExpectedEnhancedSecurity::Enabled }
    });
}
TEST_WITH_AND_WITHOUT_SITE_ISOLATION(WindowOpenThenNavigateToMeaningfulSite)

static void runReloadEnhancedSecurityRemains(bool useSiteIsolation)
{
    HTTPServer plaintextServer({
        { "http://insecure.example.internal/"_s, { "<script>alert('insecure-site');</script>"_s } },
    });

    HTTPServer secureServer({
        { "/"_s, { "<script>window.onload = function() { alert('secure-site'); }</script>"_s } },
    }, HTTPServer::Protocol::HttpsProxy);

    auto webView = enhancedSecurityTestConfiguration(&plaintextServer, &secureServer, useSiteIsolation);

    loadRequestAndCheckEnhancedSecurityAlerts(webView, @"http://insecure.example.internal/", {
        { "insecure-site"_s, ExpectedEnhancedSecurity::Enabled }
    });

    runActionAndCheckEnhancedSecurityAlerts(webView, [webView]() {
        [webView evaluateJavaScript:@"window.location.href = 'https://secure.example.internal/'" completionHandler:nil];
    }, {
        { "secure-site"_s, ExpectedEnhancedSecurity::Enabled }
    });

    runActionAndCheckEnhancedSecurityAlerts(webView, [webView]() {
        [webView reload];
    }, {
        { "secure-site"_s, ExpectedEnhancedSecurity::Enabled }
    });
}
TEST_WITH_AND_WITHOUT_SITE_ISOLATION(ReloadEnhancedSecurityRemains)

static void runJavascriptRefreshEnhancedSecurityRemains(bool useSiteIsolation)
{
    HTTPServer plaintextServer({
        { "http://insecure.example.internal/"_s, { "<script>alert('insecure-site');</script>"_s } },
    });

    HTTPServer secureServer({
        { "/"_s, { "<script>window.onload = function() { alert('secure-site'); }</script>"_s } },
    }, HTTPServer::Protocol::HttpsProxy);

    auto webView = enhancedSecurityTestConfiguration(&plaintextServer, &secureServer, useSiteIsolation);

    loadRequestAndCheckEnhancedSecurityAlerts(webView, @"http://insecure.example.internal/", {
        { "insecure-site"_s, ExpectedEnhancedSecurity::Enabled }
    });

    runActionAndCheckEnhancedSecurityAlerts(webView, [webView]() {
        [webView evaluateJavaScript:@"window.location.href = 'https://secure.example.internal/'" completionHandler:nil];
    }, {
        { "secure-site"_s, ExpectedEnhancedSecurity::Enabled }
    });

    runActionAndCheckEnhancedSecurityAlerts(webView, [webView]() {
        [webView evaluateJavaScript:@"window.location.reload();" completionHandler:nil];
    }, {
        { "secure-site"_s, ExpectedEnhancedSecurity::Enabled }
    });
}
TEST_WITH_AND_WITHOUT_SITE_ISOLATION(JavascriptRefreshEnhancedSecurityRemains)

static void runHttpThenNavigateToHttpsSiteWithCookiesViaAPI(bool useSiteIsolation)
{
    HTTPServer plaintextServer({
        { "http://insecure.example.internal/"_s, { "<script>alert('insecure-site');</script>"_s } },
    });

    HTTPServer secureServer({
        { "/"_s, { "<script>alert('secure-site-with-cookies');</script>"_s } },
    }, HTTPServer::Protocol::HttpsProxy);

    auto webView = enhancedSecurityTestConfiguration(&plaintextServer, &secureServer, useSiteIsolation);

    RetainPtr<NSHTTPCookie> sessionCookie = [NSHTTPCookie cookieWithProperties:@{
        NSHTTPCookiePath: @"/",
        NSHTTPCookieName: @"CookieName",
        NSHTTPCookieValue: @"CookieValue",
        NSHTTPCookieDomain: @"different.internal",
    }];
    [[webView configuration].websiteDataStore.httpCookieStore setCookie:sessionCookie.get() completionHandler:^{ }];

    loadRequestAndCheckEnhancedSecurityAlerts(webView, @"http://insecure.example.internal/", {
        { "insecure-site"_s, ExpectedEnhancedSecurity::Enabled }
    });

    runActionAndCheckEnhancedSecurityAlerts(webView, [webView]() {
        [webView evaluateJavaScript:@"window.location.href = 'https://different.internal/'" completionHandler:nil];
    }, {
        { "secure-site-with-cookies"_s, ExpectedEnhancedSecurity::Disabled }
    });
}
TEST_WITH_AND_WITHOUT_SITE_ISOLATION(HttpThenNavigateToHttpsSiteWithCookiesViaAPI)

// Intentionally flipped to match EnhancedSecurity::Disabled, EnhancedSecurity::EnabledInsecure
enum class SeenOutsideEnhancedSecurity : bool { Seen, NotSeen };

static NSURL *enhancedSecuritySitesPath()
{
    auto dataStoreConfiguration = adoptNS([[_WKWebsiteDataStoreConfiguration alloc] init]);
    NSURL *rootStorage = [dataStoreConfiguration.get().generalStorageDirectory URLByDeletingLastPathComponent];
    NSURL *enhancedSecurityDirectory = [rootStorage URLByAppendingPathComponent:@"EnhancedSecurity"];
    NSURL *enhancedSecurityFile = [enhancedSecurityDirectory URLByAppendingPathComponent:@"EnhancedSecuritySites.db"];
    return enhancedSecurityFile;
}

static RetainPtr<NSString> emptyEnhancedSecuritySitesPath()
{
    NSFileManager *defaultFileManager = NSFileManager.defaultManager;

    NSURL *enhancedSecurityFile = enhancedSecuritySitesPath();
    NSURL *enhancedSecurityDirectory = [enhancedSecurityFile URLByDeletingLastPathComponent];

    [defaultFileManager removeItemAtPath:enhancedSecurityDirectory.path error:nil];
    EXPECT_FALSE([defaultFileManager fileExistsAtPath:enhancedSecurityFile.path]);
    [defaultFileManager createDirectoryAtURL:enhancedSecurityDirectory withIntermediateDirectories:YES attributes:nil error:nil];

    return enhancedSecurityFile.path;
}

static void createEnhancedSecuritySitesTable(WebCore::SQLiteDatabase& database)
{
    constexpr auto createEnhancedSecuritySitesTable = "CREATE TABLE sites ("
        "site TEXT PRIMARY KEY, enhanced_security_state INT NOT NULL)"_s;

    EXPECT_TRUE(database.executeCommand(createEnhancedSecuritySitesTable));
}

static void addEnhancedSecuritySite(WebCore::SQLiteDatabase& database, String site, SeenOutsideEnhancedSecurity seenOutsideEnhancedSecurity)
{
    constexpr auto query = "INSERT INTO sites (site, enhanced_security_state) VALUES (?, ?)"_s;
    auto statement = database.prepareStatement(query);
    EXPECT_TRUE(!!statement);

    EXPECT_EQ(statement->bindText(1, site), SQLITE_OK);
    EXPECT_EQ(statement->bindInt(2, seenOutsideEnhancedSecurity == SeenOutsideEnhancedSecurity::Seen ? 0 : 1), SQLITE_OK);
    EXPECT_EQ(statement->step(), SQLITE_DONE);
    EXPECT_EQ(statement->reset(), SQLITE_OK);
}

static void setUpEnhancedSecuritySeenValues(std::initializer_list<std::pair<String, SeenOutsideEnhancedSecurity>> priorValues)
{
    auto database = makeUniqueRef<WebCore::SQLiteDatabase>();
    EXPECT_TRUE(database->open(emptyEnhancedSecuritySitesPath().get()));

    createEnhancedSecuritySitesTable(database.get());

    RetainPtr<NSMutableDictionary> itemsDictionary = adoptNS([[NSMutableDictionary alloc] initWithCapacity:priorValues.size()]);
    for (auto& pair : priorValues)
        addEnhancedSecuritySite(database.get(), pair.first, pair.second);

    database->close();
}

static void cleanUpEnhancedSecuritySites()
{
    NSFileManager *defaultFileManager = NSFileManager.defaultManager;
    NSString* enhancedSecurityFile = enhancedSecuritySitesPath().path;

    [defaultFileManager removeItemAtPath:enhancedSecurityFile error:nil];
    while ([defaultFileManager fileExistsAtPath:enhancedSecurityFile])
        usleep(10000);
}

static void runHttpThenNavigateToHttpsSiteWithCookiesViaAndExpectations(bool useSiteIsolation, SeenOutsideEnhancedSecurity seenOutsideEnhancedSecurity, ExpectedEnhancedSecurity finalExpectedEnhancedSecurity)
{
    setUpEnhancedSecuritySeenValues({
        { "different.internal"_s, seenOutsideEnhancedSecurity }
    });

    HTTPServer plaintextServer({
        { "http://insecure.example.internal/"_s, { "<script>alert('insecure-site');</script>"_s } },
    });

    HTTPServer secureServer({
        { "/"_s, { "<script>alert('secure-site-with-cookies');</script>"_s } },
    }, HTTPServer::Protocol::HttpsProxy);

    auto webView = enhancedSecurityTestConfiguration(&plaintextServer, &secureServer, useSiteIsolation, /* useNonPersistentStore */ false);

    RetainPtr<NSHTTPCookie> sessionCookie = [NSHTTPCookie cookieWithProperties:@{
        NSHTTPCookiePath: @"/",
        NSHTTPCookieName: @"CookieName",
        NSHTTPCookieValue: @"CookieValue",
        NSHTTPCookieDomain: @"different.internal",
    }];
    [[webView configuration].websiteDataStore.httpCookieStore setCookie:sessionCookie.get() completionHandler:^{ }];

    loadRequestAndCheckEnhancedSecurityAlerts(webView, @"http://insecure.example.internal/", {
        { "insecure-site"_s, ExpectedEnhancedSecurity::Enabled }
    });

    runActionAndCheckEnhancedSecurityAlerts(webView, [webView]() {
        [webView evaluateJavaScript:@"window.location.href = 'https://different.internal/'" completionHandler:nil];
    }, {
        { "secure-site-with-cookies"_s, finalExpectedEnhancedSecurity }
    });

    cleanUpEnhancedSecuritySites();
}

static void runHttpThenNavigateToHttpsSiteWithCookiesViaAPIAndNotSeenOutsideEnhancedSecurity(bool useSiteIsolation)
{
    runHttpThenNavigateToHttpsSiteWithCookiesViaAndExpectations(useSiteIsolation, SeenOutsideEnhancedSecurity::NotSeen, ExpectedEnhancedSecurity::Enabled);
}

TEST_WITH_AND_WITHOUT_SITE_ISOLATION(HttpThenNavigateToHttpsSiteWithCookiesViaAPIAndNotSeenOutsideEnhancedSecurity)

static void runHttpThenNavigateToHttpsSiteWithCookiesViaAPIAndSeenOutsideEnhancedSecurity(bool useSiteIsolation)
{
    runHttpThenNavigateToHttpsSiteWithCookiesViaAndExpectations(useSiteIsolation, SeenOutsideEnhancedSecurity::Seen, ExpectedEnhancedSecurity::Disabled);
}

TEST_WITH_AND_WITHOUT_SITE_ISOLATION(HttpThenNavigateToHttpsSiteWithCookiesViaAPIAndSeenOutsideEnhancedSecurity)

TEST(EnhancedSecurityPolicies, NonPersistentDataStoreCookieNotification)
{
    HTTPServer plaintextServer({
        { "http://insecure.example.internal/"_s, {{{ "Set-Cookie"_s, "testkey=testvalue"_s }}, "<script>alert('insecure-site')</script>"_s }}
    });

    auto webView = enhancedSecurityTestConfiguration(&plaintextServer, nullptr, /* useSiteIsolation */ false, /* useNonPersistentStore */ true);

    auto observer = adoptNS([EnhancedSecurityCookieObserver new]);
    globalCookieStore = webView.get().configuration.websiteDataStore.httpCookieStore;
    [globalCookieStore addObserver:observer.get()];

    observerCallbacks = 0;

    loadRequestAndCheckEnhancedSecurityAlerts(webView, @"http://insecure.example.internal/", {
        { "insecure-site"_s, ExpectedEnhancedSecurity::Enabled }
    });

    EXPECT_EQ(observerCallbacks, 1u);
}

TEST(EnhancedSecurityPolicies, HistoryEventsUseCorrectOriginalRequest)
{
    TestWebKitAPI::HTTPServer plaintextServer({
        { "http://example.internal/"_s, { "<head><title>Page Title</title></head><body><script>alert('first-page')</script></body>"_s } },
        { "http://example.internal/server_redirect_source"_s, { 301, { { "Location"_s, "server_redirect_destination"_s } } } },
        { "http://example.internal/server_redirect_destination"_s, { "<head><title>Target Page</title></head><body><script>alert('target-page')</script></body>"_s } }
    });

    auto webView = enhancedSecurityTestConfiguration(&plaintextServer, nullptr, /* useSiteIsolation */ false, /* useNonPersistentStore */ false);

    RetainPtr historyDelegate = adoptNS([[EnhancedSecurityHistoryDelegate alloc] init]);
    [webView _setHistoryDelegate:historyDelegate.get()];

    auto mainURL = "http://example.internal/"_s;
    RetainPtr url = adoptNS([[NSURL alloc] initWithString:mainURL.createNSString().get()]);
    RetainPtr request = adoptNS([[NSURLRequest alloc] initWithURL:url.get()]);

    runActionAndCheckEnhancedSecurityAlerts(webView, [webView, request] {
        [webView loadRequest:request.get()];
    }, { { "first-page"_s, ExpectedEnhancedSecurity::Enabled } });

    TestWebKitAPI::Util::waitFor([&] {
        return historyDelegate->lastNavigationData && historyDelegate->lastUpdatedTitle;
    });

    EXPECT_NS_EQUAL([historyDelegate->lastNavigationData originalRequest], request.get());
    EXPECT_NS_EQUAL([historyDelegate->lastNavigationData originalRequest].URL, url.get());
    EXPECT_NS_EQUAL(historyDelegate->lastUpdatedTitle.get(), @"Page Title");

    auto sourceURL = "http://example.internal/server_redirect_source"_s;
    auto destinationURL = "http://example.internal/server_redirect_destination"_s;
    url = adoptNS([[NSURL alloc] initWithString:sourceURL.createNSString().get()]);
    request = adoptNS([[NSURLRequest alloc] initWithURL:url.get()]);

    runActionAndCheckEnhancedSecurityAlerts(webView, [webView, request] {
        [webView loadRequest:request.get()];
    }, { { "target-page"_s, ExpectedEnhancedSecurity::Enabled } });

    TestWebKitAPI::Util::spinRunLoop();

    TestWebKitAPI::Util::waitFor([&] {
        return historyDelegate->lastRedirectSource && historyDelegate->lastRedirectDestination;
    });

    EXPECT_WK_STREQ([historyDelegate->lastRedirectSource absoluteString], sourceURL);
    EXPECT_WK_STREQ([historyDelegate->lastRedirectDestination absoluteString], destinationURL);
}

#endif
