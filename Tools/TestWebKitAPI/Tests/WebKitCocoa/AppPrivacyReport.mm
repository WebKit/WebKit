/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
#import "TestNavigationDelegate.h"
#import "TestUIDelegate.h"
#import "TestWKWebView.h"
#import "WKWebViewConfigurationExtras.h"
#import <Foundation/NSURLRequest.h>
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/WKWebViewPrivateForTesting.h>
#import <WebKit/WKWebsiteDataStorePrivate.h>
#import <WebKit/_WKSessionState.h>
#import <WebCore/ResourceRequest.h>
#import <pal/spi/cf/CFNetworkSPI.h>
#import <wtf/RunLoop.h>
#import <wtf/text/WTFString.h>

#if ENABLE(APP_PRIVACY_REPORT)
TEST(AppPrivacyReport, DefaultRequestIsAppInitiated)
{
    TestWebKitAPI::HTTPServer server(TestWebKitAPI::HTTPServer::respondWithChallengeThenOK);

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectZero configuration:configuration.get()]);
    NSString *url = [NSString stringWithFormat:@"http://127.0.0.1:%d/", server.port()];

    __block bool isDone = false;
    // Don't set the attribution API on NSURLRequest to make sure the default is app initiated.
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:url]]];
    [webView _test_waitForDidFinishNavigation];

    [webView _lastNavigationWasAppInitiated:^(BOOL isAppInitiated) {
        EXPECT_TRUE(isAppInitiated);
        isDone = true;
    }];

    TestWebKitAPI::Util::run(&isDone);

    isDone = false;
    [webView _appPrivacyReportTestingData:^(struct WKAppPrivacyReportTestingData data) {
        EXPECT_TRUE(data.hasLoadedAppInitiatedRequestTesting);
        EXPECT_FALSE(data.hasLoadedNonAppInitiatedRequestTesting);
        isDone = true;
    }];

    TestWebKitAPI::Util::run(&isDone);
}

TEST(AppPrivacyReport, AppInitiatedRequest)
{
    TestWebKitAPI::HTTPServer server(TestWebKitAPI::HTTPServer::respondWithChallengeThenOK);
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectZero configuration:configuration.get()]);
    NSString *url = [NSString stringWithFormat:@"http://127.0.0.1:%d/", server.port()];

    __block bool isDone = false;
    NSMutableURLRequest *appInitiatedRequest = [NSMutableURLRequest requestWithURL:[NSURL URLWithString:url]];
    appInitiatedRequest.attribution = NSURLRequestAttributionDeveloper;

    [webView loadRequest:appInitiatedRequest];
    [webView _test_waitForDidFinishNavigation];

    [webView _lastNavigationWasAppInitiated:^(BOOL isAppInitiated) {
        EXPECT_TRUE(isAppInitiated);
        isDone = true;
    }];

    TestWebKitAPI::Util::run(&isDone);

    isDone = false;
    [webView _appPrivacyReportTestingData:^(struct WKAppPrivacyReportTestingData data) {
        EXPECT_TRUE(data.hasLoadedAppInitiatedRequestTesting);
        EXPECT_FALSE(data.hasLoadedNonAppInitiatedRequestTesting);
        isDone = true;
    }];

    TestWebKitAPI::Util::run(&isDone);
}

TEST(AppPrivacyReport, NonAppInitiatedRequest)
{
    TestWebKitAPI::HTTPServer server(TestWebKitAPI::HTTPServer::respondWithChallengeThenOK);

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectZero configuration:configuration.get()]);
    NSString *url = [NSString stringWithFormat:@"http://127.0.0.1:%d/", server.port()];

    __block bool isDone = false;
    NSMutableURLRequest *nonAppInitiatedRequest = [NSMutableURLRequest requestWithURL:[NSURL URLWithString:url]];
    nonAppInitiatedRequest.attribution = NSURLRequestAttributionUser;

    [webView loadRequest:nonAppInitiatedRequest];
    [webView _test_waitForDidFinishNavigation];

    [webView _lastNavigationWasAppInitiated:^(BOOL isAppInitiated) {
        EXPECT_FALSE(isAppInitiated);
        isDone = true;
    }];

    TestWebKitAPI::Util::run(&isDone);

    isDone = false;
    [webView _appPrivacyReportTestingData:^(struct WKAppPrivacyReportTestingData data) {
        EXPECT_FALSE(data.hasLoadedAppInitiatedRequestTesting);
        EXPECT_TRUE(data.hasLoadedNonAppInitiatedRequestTesting);
        isDone = true;
    }];

    TestWebKitAPI::Util::run(&isDone);
}

TEST(AppPrivacyReport, AppInitiatedRequestWithNavigation)
{
    TestWebKitAPI::HTTPServer server(TestWebKitAPI::HTTPServer::respondWithChallengeThenOK);

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectZero configuration:configuration.get()]);
    NSString *appInitiatedURL = [NSString stringWithFormat:@"http://127.0.0.1:%d/", server.port()];
    NSString *nonAppInitiatedURL = [NSString stringWithFormat:@"http://localhost:%d/", server.port()];

    NSMutableURLRequest *appInitiatedRequest = [NSMutableURLRequest requestWithURL:[NSURL URLWithString:appInitiatedURL]];
    appInitiatedRequest.attribution = NSURLRequestAttributionDeveloper;

    [webView loadRequest:appInitiatedRequest];
    [webView _test_waitForDidFinishNavigation];

    __block bool isDone = false;
    [webView _lastNavigationWasAppInitiated:^(BOOL isAppInitiated) {
        EXPECT_TRUE(isAppInitiated);
        isDone = true;
    }];

    TestWebKitAPI::Util::run(&isDone);

    isDone = false;
    [webView _appPrivacyReportTestingData:^(struct WKAppPrivacyReportTestingData data) {
        EXPECT_TRUE(data.hasLoadedAppInitiatedRequestTesting);
        EXPECT_FALSE(data.hasLoadedNonAppInitiatedRequestTesting);
        isDone = true;
    }];

    TestWebKitAPI::Util::run(&isDone);

    isDone = false;
    NSMutableURLRequest *nonAppInitiatedRequest = [NSMutableURLRequest requestWithURL:[NSURL URLWithString:nonAppInitiatedURL]];
    nonAppInitiatedRequest.attribution = NSURLRequestAttributionUser;

    isDone = false;
    [webView _clearAppPrivacyReportTestingData:^{
        isDone = true;
    }];

    TestWebKitAPI::Util::run(&isDone);

    // Load a non app initated request and confirm the loads are marked correctly.
    [webView loadRequest:nonAppInitiatedRequest];
    [webView _test_waitForDidFinishNavigation];

    [webView _lastNavigationWasAppInitiated:^(BOOL isAppInitiated) {
        EXPECT_FALSE(isAppInitiated);
        isDone = true;
    }];

    TestWebKitAPI::Util::run(&isDone);

    isDone = false;
    [webView _appPrivacyReportTestingData:^(struct WKAppPrivacyReportTestingData data) {
        EXPECT_FALSE(data.hasLoadedAppInitiatedRequestTesting);
        EXPECT_TRUE(data.hasLoadedNonAppInitiatedRequestTesting);
        isDone = true;
    }];

    TestWebKitAPI::Util::run(&isDone);
}

TEST(AppPrivacyReport, AppInitiatedRequestWithSubFrame)
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration _setOverrideContentSecurityPolicy:@"script-src 'nonce-b'"];

    __block bool isDone = false;
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    NSMutableURLRequest *appInitiatedRequest = [NSMutableURLRequest requestWithURL:[NSBundle.test_resourcesBundle URLForResource:@"page-with-csp" withExtension:@"html"]];
    appInitiatedRequest.attribution = NSURLRequestAttributionDeveloper;

    [webView loadRequest:appInitiatedRequest];

    [webView waitForMessages:@[@"MainFrame: B", @"Subframe: B"]];

    [webView _appPrivacyReportTestingData: ^(struct WKAppPrivacyReportTestingData data) {
        EXPECT_TRUE(data.hasLoadedAppInitiatedRequestTesting);
        EXPECT_FALSE(data.hasLoadedNonAppInitiatedRequestTesting);
        isDone = true;
    }];

    TestWebKitAPI::Util::run(&isDone);
}

TEST(AppPrivacyReport, NonAppInitiatedRequestWithSubFrame)
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration _setOverrideContentSecurityPolicy:@"script-src 'nonce-b'"];

    __block bool isDone = false;
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    NSMutableURLRequest *nonAppInitiatedRequest = [NSMutableURLRequest requestWithURL:[NSBundle.test_resourcesBundle URLForResource:@"page-with-csp" withExtension:@"html"]];
    nonAppInitiatedRequest.attribution = NSURLRequestAttributionUser;

    [webView loadRequest:nonAppInitiatedRequest];

    [webView waitForMessages:@[@"MainFrame: B", @"Subframe: B"]];

    [webView _appPrivacyReportTestingData: ^(struct WKAppPrivacyReportTestingData data) {
        EXPECT_FALSE(data.hasLoadedAppInitiatedRequestTesting);
        EXPECT_TRUE(data.hasLoadedNonAppInitiatedRequestTesting);
        isDone = true;
    }];

    TestWebKitAPI::Util::run(&isDone);
}

static constexpr auto mainSWBytes = R"SWRESOURCE(
<script>
try {
    navigator.serviceWorker.register('/sw.js').then(function(reg) {
        if (reg.active) {
            alert('worker already active');
            return;
        }
        worker = reg.installing;
        worker.addEventListener('statechange', function() {
            if (worker.state == 'activated')
                alert('successfully registered');
        });
    }).catch(function(error) {
        alert('Registration failed with: ' + error);
    });
} catch(e) {
    alert('Exception: ' + e);
}
</script>
)SWRESOURCE"_s;

enum class ResponseType { Synthetic, Fetched };
enum class IsAppInitiated : bool { No, Yes };
static void runTest(ResponseType responseType, IsAppInitiated isAppInitiated)
{
    static bool isDone = false;
    [[WKWebsiteDataStore defaultDataStore] removeDataOfTypes:[WKWebsiteDataStore allWebsiteDataTypes] modifiedSince:[NSDate distantPast] completionHandler:^() {
        isDone = true;
    }];
    TestWebKitAPI::Util::run(&isDone);

    ASCIILiteral js;
    const char* expectedAlert = nullptr;

    switch (responseType) {
    case ResponseType::Synthetic:
        js = "self.addEventListener('fetch', (event) => { event.respondWith(new Response(new Blob(['<script>alert(\"synthetic response\")</script>'], {type: 'text/html'}))); })"_s;
        expectedAlert = "synthetic response";
        break;
    case ResponseType::Fetched:
        js = "self.addEventListener('fetch', (event) => { event.respondWith(fetch('/fetched.html')) });"_s;
        expectedAlert = "fetched from server";
        break;
    }

    TestWebKitAPI::HTTPServer server({
        { "/"_s, { mainSWBytes } },
        { "/sw.js"_s, { {{ "Content-Type"_s, "application/javascript"_s }}, js } },
        { "/fetched.html"_s, { "<script>alert('fetched from server')</script>"_s } },
    }, TestWebKitAPI::HTTPServer::Protocol::Https);

    auto webView = adoptNS([WKWebView new]);

    auto delegate = adoptNS([TestNavigationDelegate new]);
    [delegate setDidReceiveAuthenticationChallenge:^(WKWebView *, NSURLAuthenticationChallenge *challenge, void (^callback)(NSURLSessionAuthChallengeDisposition, NSURLCredential *)) {
        EXPECT_WK_STREQ(challenge.protectionSpace.authenticationMethod, NSURLAuthenticationMethodServerTrust);
        callback(NSURLSessionAuthChallengeUseCredential, [NSURLCredential credentialForTrust:challenge.protectionSpace.serverTrust]);
    }];
    webView.get().navigationDelegate = delegate.get();

    NSMutableURLRequest *request = [NSMutableURLRequest requestWithURL:[NSURL URLWithString:[NSString stringWithFormat:@"https://127.0.0.1:%d/", server.port()]]];
    auto attributionValue = isAppInitiated == IsAppInitiated::Yes ? NSURLRequestAttributionDeveloper : NSURLRequestAttributionUser;
    request.attribution = attributionValue;

    [webView loadRequest:request];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "successfully registered");

    if (responseType != ResponseType::Fetched)
        server.cancel();

    bool expectingAppInitiatedRequests = isAppInitiated == IsAppInitiated::Yes;
    isDone = false;
    [webView _appPrivacyReportTestingData: ^(struct WKAppPrivacyReportTestingData data) {
        EXPECT_EQ(data.hasLoadedAppInitiatedRequestTesting, expectingAppInitiatedRequests);
        EXPECT_EQ(data.hasLoadedNonAppInitiatedRequestTesting, !expectingAppInitiatedRequests);
        isDone = true;
    }];

    isDone = false;
    [webView _clearAppPrivacyReportTestingData:^{
        isDone = true;
    }];

    TestWebKitAPI::Util::run(&isDone);

    [webView reload];
    EXPECT_WK_STREQ([webView _test_waitForAlert], expectedAlert);

    isDone = false;
    [webView _appPrivacyReportTestingData: ^(struct WKAppPrivacyReportTestingData data) {
        if (responseType == ResponseType::Synthetic) {
            EXPECT_FALSE(data.hasLoadedAppInitiatedRequestTesting);
            EXPECT_FALSE(data.hasLoadedNonAppInitiatedRequestTesting);
        } else {
            EXPECT_EQ(data.hasLoadedAppInitiatedRequestTesting, expectingAppInitiatedRequests);
            EXPECT_EQ(data.hasLoadedNonAppInitiatedRequestTesting, !expectingAppInitiatedRequests);
        }
        isDone = true;
    }];

    TestWebKitAPI::Util::run(&isDone);
}

TEST(AppPrivacyReport, NonAppInitiatedRequestWithServiceWorker)
{
    runTest(ResponseType::Synthetic, IsAppInitiated::No);
    runTest(ResponseType::Fetched, IsAppInitiated::No);
}

TEST(AppPrivacyReport, AppInitiatedRequestWithServiceWorker)
{
    runTest(ResponseType::Synthetic, IsAppInitiated::Yes);
    runTest(ResponseType::Fetched, IsAppInitiated::Yes);
}

TEST(AppPrivacyReport, MultipleWebViewsWithSharedServiceWorker)
{
    static bool isDone = false;

    [[WKWebsiteDataStore defaultDataStore] removeDataOfTypes:[WKWebsiteDataStore allWebsiteDataTypes] modifiedSince:[NSDate distantPast] completionHandler:^() {
        isDone = true;
    }];
    TestWebKitAPI::Util::run(&isDone);

    constexpr auto js = "self.addEventListener('fetch', (event) => { event.respondWith(fetch('/fetched.html')) })"_s;
    const char* expectedAlert = "fetched from server";

    TestWebKitAPI::HTTPServer server({
        { "/"_s, { mainSWBytes } },
        { "/sw.js"_s, { {{ "Content-Type"_s, "application/javascript"_s }}, js } },
        { "/fetched.html"_s, { "<script>alert('fetched from server')</script>"_s } },
    }, TestWebKitAPI::HTTPServer::Protocol::Https);

    auto webView1 = adoptNS([WKWebView new]);
    auto webView2 = adoptNS([WKWebView new]);

    auto delegate = adoptNS([TestNavigationDelegate new]);
    [delegate setDidReceiveAuthenticationChallenge:^(WKWebView *, NSURLAuthenticationChallenge *challenge, void (^callback)(NSURLSessionAuthChallengeDisposition, NSURLCredential *)) {
        EXPECT_WK_STREQ(challenge.protectionSpace.authenticationMethod, NSURLAuthenticationMethodServerTrust);
        callback(NSURLSessionAuthChallengeUseCredential, [NSURLCredential credentialForTrust:challenge.protectionSpace.serverTrust]);
    }];
    webView1.get().navigationDelegate = delegate.get();
    webView2.get().navigationDelegate = delegate.get();

    NSMutableURLRequest *appInitiatedRequest = [NSMutableURLRequest requestWithURL:[NSURL URLWithString:[NSString stringWithFormat:@"https://127.0.0.1:%d/", server.port()]]];
    appInitiatedRequest.attribution = NSURLRequestAttributionDeveloper;
    [webView1 loadRequest:appInitiatedRequest];
    EXPECT_WK_STREQ([webView1 _test_waitForAlert], "successfully registered");

    [webView1 reload];
    EXPECT_WK_STREQ([webView1 _test_waitForAlert], expectedAlert);

    isDone = false;
    [webView1 _appPrivacyReportTestingData: ^(struct WKAppPrivacyReportTestingData data) {
        EXPECT_TRUE(data.hasLoadedAppInitiatedRequestTesting);
        EXPECT_FALSE(data.hasLoadedNonAppInitiatedRequestTesting);
        isDone = true;
    }];

    TestWebKitAPI::Util::run(&isDone);

    isDone = false;
    [webView1 _clearAppPrivacyReportTestingData:^{
        isDone = true;
    }];

    TestWebKitAPI::Util::run(&isDone);

    isDone = false;
    NSMutableURLRequest *nonAppInitiatedRequest = [NSMutableURLRequest requestWithURL:[NSURL URLWithString:[NSString stringWithFormat:@"https://127.0.0.1:%d/", server.port()]]];
    nonAppInitiatedRequest.attribution = NSURLRequestAttributionUser;
    [webView2 loadRequest:nonAppInitiatedRequest];
    EXPECT_WK_STREQ([webView2 _test_waitForAlert], "fetched from server");

    [webView2 reload];
    EXPECT_WK_STREQ([webView2 _test_waitForAlert], expectedAlert);

    [webView2 _appPrivacyReportTestingData: ^(struct WKAppPrivacyReportTestingData data) {
        EXPECT_FALSE(data.hasLoadedAppInitiatedRequestTesting);
        EXPECT_TRUE(data.hasLoadedNonAppInitiatedRequestTesting);
        isDone = true;
    }];
    TestWebKitAPI::Util::run(&isDone);
}

static void softUpdateTest(IsAppInitiated isAppInitiated)
{
    static bool isDone = false;
    [[WKWebsiteDataStore defaultDataStore] removeDataOfTypes:[WKWebsiteDataStore allWebsiteDataTypes] modifiedSince:[NSDate distantPast] completionHandler:^() {
        isDone = true;
    }];
    TestWebKitAPI::Util::run(&isDone);

    auto delegate = adoptNS([TestNavigationDelegate new]);
    [delegate setDidReceiveAuthenticationChallenge:^(WKWebView *, NSURLAuthenticationChallenge *challenge, void (^callback)(NSURLSessionAuthChallengeDisposition, NSURLCredential *)) {
        EXPECT_WK_STREQ(challenge.protectionSpace.authenticationMethod, NSURLAuthenticationMethodServerTrust);
        callback(NSURLSessionAuthChallengeUseCredential, [NSURLCredential credentialForTrust:challenge.protectionSpace.serverTrust]);
    }];

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto webView1 = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    auto webView2 = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    webView1.get().navigationDelegate = delegate.get();
    webView2.get().navigationDelegate = delegate.get();

    uint16_t serverPort;
    static constexpr auto js = "self.addEventListener('fetch', (event) => { event.respondWith(new Response(new Blob(['<script>alert(\"synthetic response\")</script>'], {type: 'text/html'}))); })"_s;

    {
        TestWebKitAPI::HTTPServer server1({
            { "/"_s, { mainSWBytes } },
            { "/sw.js"_s, { {{ "Content-Type"_s, "application/javascript"_s }}, js } },
        }, TestWebKitAPI::HTTPServer::Protocol::Https, nullptr, testIdentity());
        serverPort = server1.port();

        NSURLRequest *request = server1.request();
        if (isAppInitiated == IsAppInitiated::No) {
            NSMutableURLRequest *nonAppInitiatedRequest = [NSMutableURLRequest requestWithURL:[NSURL URLWithString:[NSString stringWithFormat:@"https://127.0.0.1:%d/", server1.port()]]];
            nonAppInitiatedRequest.attribution = NSURLRequestAttributionDeveloper;
            request = nonAppInitiatedRequest;
        }

        [webView1 loadRequest:request];
        EXPECT_WK_STREQ([webView1 _test_waitForAlert], "successfully registered");

        server1.cancel();
    }

    {
        TestWebKitAPI::HTTPServer server2({
            { "/"_s, { mainSWBytes } },
            { "/sw.js"_s, { {{ "Content-Type"_s, "application/javascript"_s }}, js } }
        }, TestWebKitAPI::HTTPServer::Protocol::Https, nullptr, testIdentity2(), serverPort);

        NSMutableURLRequest *request2 = [NSMutableURLRequest requestWithURL:[NSURL URLWithString:[NSString stringWithFormat:@"https://127.0.0.1:%d/", server2.port()]]];
        auto attributionValue = isAppInitiated == IsAppInitiated::Yes ? NSURLRequestAttributionDeveloper : NSURLRequestAttributionUser;
        request2.attribution = attributionValue;

        isDone = false;
        [webView1 _clearAppPrivacyReportTestingData:^{
            isDone = true;
        }];

        TestWebKitAPI::Util::run(&isDone);

        [webView2 loadRequest:request2];
        EXPECT_WK_STREQ([webView2 _test_waitForAlert], "synthetic response");
    }

    isDone = false;
    bool expectingAppInitiatedRequests = isAppInitiated == IsAppInitiated::Yes;
    while (!isDone) {
        [webView2 _appPrivacyReportTestingData: ^(struct WKAppPrivacyReportTestingData data) {
            if (!data.didPerformSoftUpdate)
                return;

            EXPECT_EQ(data.hasLoadedAppInitiatedRequestTesting, expectingAppInitiatedRequests);
            EXPECT_EQ(data.hasLoadedNonAppInitiatedRequestTesting, !expectingAppInitiatedRequests);
            isDone = true;
        }];
        TestWebKitAPI::Util::spinRunLoop(1);
    }
}

TEST(AppPrivacyReport, AppInitiatedRequestWithServiceWorkerSoftUpdate)
{
    softUpdateTest(IsAppInitiated::Yes);
}

TEST(AppPrivacyReport, NonAppInitiatedRequestWithServiceWorkerSoftUpdate)
{
    softUpdateTest(IsAppInitiated::No);
}

static void runWebProcessPlugInTest(IsAppInitiated isAppInitiated)
{
    TestWebKitAPI::HTTPServer server(TestWebKitAPI::HTTPServer::respondWithChallengeThenOK);

    WKWebViewConfiguration *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"AppPrivacyReportPlugIn"];
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500) configuration:configuration]);

    NSString *url = [NSString stringWithFormat:@"http://127.0.0.1:%d/", server.port()];

    __block bool isDone = false;
    NSMutableURLRequest *request = [NSMutableURLRequest requestWithURL:[NSURL URLWithString:url]];
    auto attributionValue = isAppInitiated == IsAppInitiated::Yes ? NSURLRequestAttributionDeveloper : NSURLRequestAttributionUser;
    request.attribution = attributionValue;

    [webView loadRequest:request];
    [webView _test_waitForDidFinishNavigation];

    isDone = false;
    bool expectingAppInitiatedRequests = isAppInitiated == IsAppInitiated::Yes;
    [webView _appPrivacyReportTestingData:^(struct WKAppPrivacyReportTestingData data) {
        EXPECT_EQ(data.hasLoadedAppInitiatedRequestTesting, expectingAppInitiatedRequests);
        EXPECT_EQ(data.hasLoadedNonAppInitiatedRequestTesting, !expectingAppInitiatedRequests);
        isDone = true;
    }];

    TestWebKitAPI::Util::run(&isDone);
}

TEST(AppPrivacyReport, WebProcessPluginTestAppInitiated)
{
    runWebProcessPlugInTest(IsAppInitiated::Yes);
}

TEST(AppPrivacyReport, WebProcessPluginTestNonAppInitiated)
{
    runWebProcessPlugInTest(IsAppInitiated::No);
}

#if WK_HAVE_C_SPI

static constexpr auto mainSWBytesDefaultValue = R"SWRESOURCE(
<script>

function log(msg)
{
    window.webkit.messageHandlers.sw.postMessage(msg);
}

navigator.serviceWorker.addEventListener("message", function(event) {
    log(event.data);
});

try {
    navigator.serviceWorker.register('/sw.js').then(function(reg) {
        if (reg.active) {
            worker = reg.active;
            worker.postMessage("SECOND");
            return;
        }
        worker = reg.installing;
        worker.addEventListener('statechange', function() {
            if (worker.state == 'activated')
                worker.postMessage("FIRST");
        });
    }).catch(function(error) {
        log('Registration failed with: ' + error);
    });
} catch(e) {
    log('Exception: ' + e);
}
</script>
)SWRESOURCE"_s;

static constexpr auto scriptBytesDefaultValue = R"SWRESOURCE(
self.addEventListener('message', async (event) => {
    if (!self.internals) {
        event.source.postMessage('No internals');
        return;
    }

    queryAppPrivacyReportValue(event, false);
});

async function queryAppPrivacyReportValue(event, haveSentInitialMessage)
{
    var result = await internals.lastNavigationWasAppInitiated();
    if (result) {
        if (event.data == "FIRST") {
            event.source.postMessage('app initiated');
            return;
        }

        if (!haveSentInitialMessage)
            event.source.postMessage('starts app initiated');

        queryAppPrivacyReportValue(event, true);
        return;
    }

    event.source.postMessage('non app initiated');
}

)SWRESOURCE"_s;


static String expectedMessage;
static bool receivedMessage = false;

@interface SWAppInitiatedRequestMessageHandler : NSObject <WKScriptMessageHandler>
@end

@implementation SWAppInitiatedRequestMessageHandler
- (void)userContentController:(WKUserContentController *)userContentController didReceiveScriptMessage:(WKScriptMessage *)message
{
    EXPECT_WK_STREQ(message.body, expectedMessage);
    receivedMessage = true;
}
@end

TEST(AppPrivacyReport, RegisterServiceWorkerClientUpdatesAppInitiatedValue)
{
    static bool isDone = false;

    [[WKWebsiteDataStore defaultDataStore] removeDataOfTypes:[WKWebsiteDataStore allWebsiteDataTypes] modifiedSince:[NSDate distantPast] completionHandler:^() {
        isDone = true;
    }];
    TestWebKitAPI::Util::run(&isDone);

    WKWebViewConfiguration *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];
    WKRetainPtr<WKContextRef> context = adoptWK(TestWebKitAPI::Util::createContextForInjectedBundleTest("InternalsInjectedBundleTest"));
    configuration.processPool = (WKProcessPool *)context.get();

    RetainPtr<SWAppInitiatedRequestMessageHandler> messageHandler = adoptNS([[SWAppInitiatedRequestMessageHandler alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:messageHandler.get() name:@"sw"];

    auto webView1 = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration]);
    auto webView2 = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration]);

    auto delegate = adoptNS([TestNavigationDelegate new]);
    [delegate setDidReceiveAuthenticationChallenge:^(WKWebView *, NSURLAuthenticationChallenge *challenge, void (^callback)(NSURLSessionAuthChallengeDisposition, NSURLCredential *)) {
        EXPECT_WK_STREQ(challenge.protectionSpace.authenticationMethod, NSURLAuthenticationMethodServerTrust);
        callback(NSURLSessionAuthChallengeUseCredential, [NSURLCredential credentialForTrust:challenge.protectionSpace.serverTrust]);
    }];

    webView1.get().navigationDelegate = delegate.get();
    webView2.get().navigationDelegate = delegate.get();

    TestWebKitAPI::HTTPServer server({
        { "/main.html"_s, { mainSWBytesDefaultValue } },
        { "/sw.js"_s, { { { "Content-Type"_s, "application/javascript"_s } }, scriptBytesDefaultValue } },
    });

    // Load WebView with an app initiated request. We expect the ServiceWorkerThreadProxy to be app initiated.
    expectedMessage = "app initiated";
    [webView1 loadRequest:server.request("/main.html")];
    TestWebKitAPI::Util::run(&receivedMessage);

    // Load WebView with a non app initiated request. We expect the ServiceWorkerThreadProxy to be app initiated
    // at first, but then become non app initiated once the second webView is closed and its client is unregistered.
    expectedMessage = "starts app initiated";
    receivedMessage = false;
    NSMutableURLRequest *nonAppInitiatedRequest = [NSMutableURLRequest requestWithURL:[NSURL URLWithString:[NSString stringWithFormat:@"http://127.0.0.1:%d/main.html", server.port()]]];
    nonAppInitiatedRequest.attribution = NSURLRequestAttributionUser;

    [webView2 loadRequest:nonAppInitiatedRequest];
    TestWebKitAPI::Util::run(&receivedMessage);

    // Close the app initiated view. We expect that the existing worker will become non app initiated
    // now that all app initiated clients have been removed.
    expectedMessage = "non app initiated";
    receivedMessage = false;
    [webView1 _close];
    webView1 = nullptr;

    TestWebKitAPI::Util::run(&receivedMessage);
}

#endif // WK_HAVE_C_SPI

static void loadSimulatedRequestTest(IsAppInitiated isAppInitiated)
{
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);

    auto delegate = adoptNS([[TestNavigationDelegate alloc] init]);
    [webView setNavigationDelegate:delegate.get()];

    NSMutableURLRequest *loadRequest = [NSMutableURLRequest requestWithURL:[NSURL URLWithString:@"about:blank"]];
    loadRequest.attribution = isAppInitiated == IsAppInitiated::Yes ? NSURLRequestAttributionDeveloper : NSURLRequestAttributionUser;

    NSString *HTML = @"<html><head></head><body><iframe src='http://127.0.0.1/'></iframe></body></html>";

    [webView loadSimulatedRequest:loadRequest responseHTMLString:HTML];
    [delegate waitForDidFinishNavigation];

    static bool isDone = false;
    bool expectingAppInitiatedRequests = isAppInitiated == IsAppInitiated::Yes;
    [webView _appPrivacyReportTestingData:^(struct WKAppPrivacyReportTestingData data) {
        EXPECT_EQ(data.hasLoadedAppInitiatedRequestTesting, expectingAppInitiatedRequests);
        EXPECT_EQ(data.hasLoadedNonAppInitiatedRequestTesting, !expectingAppInitiatedRequests);
        isDone = true;
    }];
    TestWebKitAPI::Util::run(&isDone);
}

// FIXME: Re-enable these two tests once webkit.org/b/232166 is resolved.
TEST(AppPrivacyReport, LoadSimulatedRequestIsAppInitiated)
{
    loadSimulatedRequestTest(IsAppInitiated::Yes);
}

TEST(AppPrivacyReport, LoadSimulatedRequestIsNonAppInitiated)
{
    loadSimulatedRequestTest(IsAppInitiated::No);
}

static void restoreFromSessionStateTest(IsAppInitiated isAppInitiated)
{
    auto webView1 = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);

    NSMutableURLRequest *request = [NSMutableURLRequest requestWithURL:[NSURL URLWithString:@"https://www.apple.com/"]];
    request.attribution = isAppInitiated == IsAppInitiated::Yes ? NSURLRequestAttributionDeveloper : NSURLRequestAttributionUser;

    [webView1 loadRequest:request];
    [webView1 _test_waitForDidFinishNavigation];

    RetainPtr<_WKSessionState> sessionState = [webView1 _sessionState];
    [webView1 _close];

    static bool isDone = false;
    [[WKWebsiteDataStore defaultDataStore] removeDataOfTypes:[WKWebsiteDataStore allWebsiteDataTypes] modifiedSince:[NSDate distantPast] completionHandler:^() {
        isDone = true;
    }];
    TestWebKitAPI::Util::run(&isDone);

    auto webView2 = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
    [webView2 _restoreSessionState:sessionState.get() andNavigate:YES];
    [webView2 _test_waitForDidFinishNavigation];

    EXPECT_WK_STREQ(@"https://www.apple.com/", [[webView2 URL] absoluteString]);

    isDone = false;
    bool expectingAppInitiatedRequests = isAppInitiated == IsAppInitiated::Yes;
    [webView2 _appPrivacyReportTestingData:^(struct WKAppPrivacyReportTestingData data) {
        EXPECT_EQ(data.hasLoadedAppInitiatedRequestTesting, expectingAppInitiatedRequests);
        EXPECT_EQ(data.hasLoadedNonAppInitiatedRequestTesting, !expectingAppInitiatedRequests);
        isDone = true;
    }];
    TestWebKitAPI::Util::run(&isDone);
}

// FIXME: Re-enable this test once webkit.org/b/254289 has been resolved.
TEST(AppPrivacyReport, DISABLED_RestoreFromSessionStateIsAppInitiated)
{
    restoreFromSessionStateTest(IsAppInitiated::Yes);
}

// FIXME: Re-enable this test once webkit.org/b/254289 has been resolved.
TEST(AppPrivacyReport, DISABLED_RestoreFromSessionStateIsNonAppInitiated)
{
    restoreFromSessionStateTest(IsAppInitiated::No);
}

static void restoreFromInteractionStateTest(IsAppInitiated isAppInitiated)
{
    auto webView1 = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);

    NSMutableURLRequest *request = [NSMutableURLRequest requestWithURL:[NSURL URLWithString:@"https://www.apple.com/"]];
    request.attribution = isAppInitiated == IsAppInitiated::Yes ? NSURLRequestAttributionDeveloper : NSURLRequestAttributionUser;

    [webView1 loadRequest:request];
    [webView1 _test_waitForDidFinishNavigation];

    id interactionState = [webView1 interactionState];
    [webView1 _close];

    static bool isDone = false;
    [[WKWebsiteDataStore defaultDataStore] removeDataOfTypes:[WKWebsiteDataStore allWebsiteDataTypes] modifiedSince:[NSDate distantPast] completionHandler:^() {
        isDone = true;
    }];
    TestWebKitAPI::Util::run(&isDone);

    auto webView2 = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
    [webView2 setInteractionState:interactionState];
    [webView2 _test_waitForDidFinishNavigation];

    EXPECT_WK_STREQ(@"https://www.apple.com/", [[webView2 URL] absoluteString]);

    isDone = false;
    bool expectingAppInitiatedRequests = isAppInitiated == IsAppInitiated::Yes;
    [webView2 _appPrivacyReportTestingData:^(struct WKAppPrivacyReportTestingData data) {
        EXPECT_EQ(data.hasLoadedAppInitiatedRequestTesting, expectingAppInitiatedRequests);
        EXPECT_EQ(data.hasLoadedNonAppInitiatedRequestTesting, !expectingAppInitiatedRequests);
        isDone = true;
    }];
    TestWebKitAPI::Util::run(&isDone);
}

// FIXME: Re-enable this test once webkit.org/b/254289 has been resolved.
TEST(AppPrivacyReport, DISABLED_RestoreFromInteractionStateIsAppInitiated)
{
    restoreFromInteractionStateTest(IsAppInitiated::Yes);
}

// FIXME: Re-enable this test once webkit.org/b/254289 has been resolved.
TEST(AppPrivacyReport, DISABLED_RestoreFromInteractionStateIsNonAppInitiated)
{
    restoreFromInteractionStateTest(IsAppInitiated::No);
}

static void loadFileTest(IsAppInitiated isAppInitiated)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);

    NSURL *file = [NSBundle.test_resourcesBundle URLForResource:@"file-with-iframe" withExtension:@"html"];
    NSMutableURLRequest *request = [NSMutableURLRequest requestWithURL:file];
    request.attribution = isAppInitiated == IsAppInitiated::Yes ? NSURLRequestAttributionDeveloper : NSURLRequestAttributionUser;

    [webView loadFileRequest:request allowingReadAccessToURL:file.URLByDeletingLastPathComponent];
    [webView _test_waitForDidFinishNavigation];

    static bool isDone = false;
    bool expectingAppInitiatedRequests = isAppInitiated == IsAppInitiated::Yes;
    [webView _appPrivacyReportTestingData:^(struct WKAppPrivacyReportTestingData data) {
        EXPECT_EQ(data.hasLoadedAppInitiatedRequestTesting, expectingAppInitiatedRequests);
        EXPECT_EQ(data.hasLoadedNonAppInitiatedRequestTesting, !expectingAppInitiatedRequests);
        isDone = true;
    }];
    TestWebKitAPI::Util::run(&isDone);
}

TEST(AppPrivacyReport, LoadFileRequestIsAppInitiated)
{
    loadFileTest(IsAppInitiated::Yes);
}

TEST(AppPrivacyReport, LoadFileRequestIsNonAppInitiated)
{
    loadFileTest(IsAppInitiated::No);
}

TEST(AppPrivacyReport, NSURLRequestConstructorAttribution)
{
    NSString *url = @"https://webkit.org";

    NSMutableURLRequest *appInitiatedRequest = [NSMutableURLRequest requestWithURL:[NSURL URLWithString:url]];
    EXPECT_EQ(appInitiatedRequest.attribution, NSURLRequestAttributionDeveloper);

    WebCore::ResourceRequest testRequest(appInitiatedRequest);
    EXPECT_TRUE(testRequest.isAppInitiated());

    WebCore::ResourceRequest testRequestCopy(testRequest);
    EXPECT_TRUE(testRequestCopy.isAppInitiated());

    auto nsRequestFromCopy = testRequestCopy.nsURLRequest(WebCore::HTTPBodyUpdatePolicy::DoNotUpdateHTTPBody);
    EXPECT_EQ(nsRequestFromCopy.attribution, NSURLRequestAttributionDeveloper);
    
    appInitiatedRequest.attribution = NSURLRequestAttributionUser;
    WebCore::ResourceRequest testRequest2(appInitiatedRequest);
    EXPECT_FALSE(testRequest2.isAppInitiated());

    WebCore::ResourceRequest testRequestCopy2(testRequest2);
    EXPECT_FALSE(testRequestCopy2.isAppInitiated());

    auto nsRequestFromCopy2 = testRequestCopy2.nsURLRequest(WebCore::HTTPBodyUpdatePolicy::DoNotUpdateHTTPBody);
    EXPECT_EQ(nsRequestFromCopy2.attribution, NSURLRequestAttributionUser);

    appInitiatedRequest.attribution = NSURLRequestAttributionDeveloper;
    WebCore::ResourceRequest testRequest3(appInitiatedRequest);
    EXPECT_TRUE(testRequest3.isAppInitiated());

    auto nsRequestFromModifiedRequest = testRequest3.nsURLRequest(WebCore::HTTPBodyUpdatePolicy::DoNotUpdateHTTPBody);
    EXPECT_EQ(nsRequestFromModifiedRequest.attribution, NSURLRequestAttributionDeveloper);

    testRequest3.setIsAppInitiated(false);
    auto nsRequestFromModifiedRequest2 = testRequest3.nsURLRequest(WebCore::HTTPBodyUpdatePolicy::DoNotUpdateHTTPBody);
    EXPECT_EQ(nsRequestFromModifiedRequest2.attribution, NSURLRequestAttributionUser);

    testRequest3.setIsAppInitiated(true);
    auto nsRequestFromModifiedRequest3 = testRequest3.nsURLRequest(WebCore::HTTPBodyUpdatePolicy::DoNotUpdateHTTPBody);
    EXPECT_EQ(nsRequestFromModifiedRequest3.attribution, NSURLRequestAttributionDeveloper);

    // Changing the nsURLRequest copy generated by the accessor doesn't change the ResourceRequest
    appInitiatedRequest.attribution = NSURLRequestAttributionUser;
    EXPECT_TRUE(testRequest.isAppInitiated());

    WebCore::ResourceRequest testRequest4(appInitiatedRequest);
    EXPECT_FALSE(testRequest4.isAppInitiated());
}

#endif // APP_PRIVACY_REPORT
