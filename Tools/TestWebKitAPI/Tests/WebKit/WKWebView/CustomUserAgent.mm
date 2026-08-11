/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

#import "Helpers/DeprecatedGlobalValues.h"
#import "Helpers/PlatformUtilities.h"
#import "Helpers/cocoa/HTTPServer.h"
#import "Helpers/cocoa/TestNavigationDelegate.h"
#import "Helpers/cocoa/TestWKWebView.h"
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/WKWebpagePreferencesPrivate.h>
#import <WebKit/WebKit.h>
#import <wtf/RetainPtr.h>
#import <wtf/text/MakeString.h>
#import <wtf/text/WTFString.h>

TEST(CustomUserAgent, UpdateCachedNavigatorUserAgent)
{
    RetainPtr webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"about:blank"]]];

    [webView _test_waitForDidFinishNavigation];

    // Query navigator.userAgent once so it gets cached.
    [webView evaluateJavaScript:@"navigator.userAgent;" completionHandler:^(id _Nullable response, NSError * _Nullable error) {
        ASSERT_TRUE(!error);
        NSString *userAgent = (NSString *)response;
        ASSERT_TRUE(!!userAgent);
        // Override user agent with a custom one.
        webView.get().customUserAgent = @"Custom UserAgent";
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;

    // Query navigator.userAgent again to make sure its cache was invalidated and it returns the updated value.
    [webView evaluateJavaScript:@"navigator.userAgent;" completionHandler:^(id _Nullable response, NSError * _Nullable error) {
        ASSERT_TRUE(!error);
        NSString *userAgent = (NSString *)response;
        ASSERT_TRUE(!!userAgent);
        EXPECT_WK_STREQ(@"Custom UserAgent", userAgent);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;
}

namespace TestWebKitAPI {

static String userAgentFromRequest(const Vector<char>& request)
{
    auto headers = String::fromUTF8(request.span()).split("\r\n"_s);
    auto index = headers.findIf([](auto& header) {
        return header.startsWith("User-Agent:"_s);
    });
    if (index == notFound)
        return { };
    return headers[index];
}

// Covers rdar://176265326. When the app updates webView.customUserAgent inside
// decidePolicyForNavigationAction for a 302 redirect target, the redirected
// request must carry the updated User-Agent even when no process swap occurs.
TEST(CustomUserAgent, PageLevelCustomUserAgentUpdatedInsidePolicyAppliesToRedirectTarget)
{
    String receivedUserAgentOnTarget;
    bool targetWasRequested = false;
    HTTPServer server(HTTPServer::UseCoroutines::Yes, [&](Connection connection) -> ConnectionTask {
        while (true) {
            auto request = co_await connection.awaitableReceiveHTTPRequest();
            auto path = HTTPServer::parsePath(request);
            if (path == "/redirect"_s) {
                co_await connection.awaitableSend(makeString(
                    "HTTP/1.1 302 Found\r\n"
                    "Location: /target\r\n"
                    "Content-Length: 0\r\n"
                    "\r\n"_s));
                continue;
            }
            if (path == "/target"_s) {
                receivedUserAgentOnTarget = userAgentFromRequest(request);
                targetWasRequested = true;
                co_await connection.awaitableSend(
                    "HTTP/1.1 200 OK\r\n"
                    "Content-Length: 0\r\n"
                    "\r\n"_s);
                continue;
            }
            EXPECT_FALSE(true);
        }
    });

    RetainPtr webView = adoptNS([TestWKWebView new]);
    [webView setCustomUserAgent:@"Initial"];

    RetainPtr navigationDelegate = adoptNS([TestNavigationDelegate new]);
    navigationDelegate.get().decidePolicyForNavigationAction = ^(WKNavigationAction *navigationAction, void (^decisionHandler)(WKNavigationActionPolicy)) {
        if ([navigationAction.request.URL.path isEqualToString:@"/target"])
            webView.get().customUserAgent = @"Updated";
        decisionHandler(WKNavigationActionPolicyAllow);
    };
    [webView setNavigationDelegate:navigationDelegate.get()];

    [webView loadRequest:server.request("/redirect"_s)];
    [navigationDelegate waitForDidFinishNavigation];

    EXPECT_TRUE(targetWasRequested);
    EXPECT_WK_STREQ("User-Agent: Updated", receivedUserAgentOnTarget.utf8().data());
}

// Same scenario as above, but using the WKWebpagePreferences SPI variant.
// The app does not set a page-level custom UA; the delegate supplies one only
// via preferences._customUserAgent inside the policy callback for /target.
TEST(CustomUserAgent, WebpagePreferencesCustomUserAgentAppliesToRedirectTarget)
{
    String receivedUserAgentOnTarget;
    bool targetWasRequested = false;
    HTTPServer server(HTTPServer::UseCoroutines::Yes, [&](Connection connection) -> ConnectionTask {
        while (true) {
            auto request = co_await connection.awaitableReceiveHTTPRequest();
            auto path = HTTPServer::parsePath(request);
            if (path == "/redirect"_s) {
                co_await connection.awaitableSend(makeString(
                    "HTTP/1.1 302 Found\r\n"
                    "Location: /target\r\n"
                    "Content-Length: 0\r\n"
                    "\r\n"_s));
                continue;
            }
            if (path == "/target"_s) {
                receivedUserAgentOnTarget = userAgentFromRequest(request);
                targetWasRequested = true;
                co_await connection.awaitableSend(
                    "HTTP/1.1 200 OK\r\n"
                    "Content-Length: 0\r\n"
                    "\r\n"_s);
                continue;
            }
            EXPECT_FALSE(true);
        }
    });

    RetainPtr webView = adoptNS([TestWKWebView new]);

    RetainPtr navigationDelegate = adoptNS([TestNavigationDelegate new]);
    navigationDelegate.get().decidePolicyForNavigationActionWithPreferences = ^(WKNavigationAction *navigationAction, WKWebpagePreferences *preferences, void (^decisionHandler)(WKNavigationActionPolicy, WKWebpagePreferences *)) {
        if ([navigationAction.request.URL.path isEqualToString:@"/target"])
            preferences._customUserAgent = @"Updated";
        decisionHandler(WKNavigationActionPolicyAllow, preferences);
    };
    [webView setNavigationDelegate:navigationDelegate.get()];

    [webView loadRequest:server.request("/redirect"_s)];
    [navigationDelegate waitForDidFinishNavigation];

    EXPECT_TRUE(targetWasRequested);
    EXPECT_WK_STREQ("User-Agent: Updated", receivedUserAgentOnTarget.utf8().data());
}

// Regression guard: when no custom UA is in effect, the redirect target must
// still receive the default browser User-Agent unmodified by the fix.
TEST(CustomUserAgent, NoCustomUserAgentRedirectUsesDefault)
{
    String receivedUserAgentOnTarget;
    bool targetWasRequested = false;
    HTTPServer server(HTTPServer::UseCoroutines::Yes, [&](Connection connection) -> ConnectionTask {
        while (true) {
            auto request = co_await connection.awaitableReceiveHTTPRequest();
            auto path = HTTPServer::parsePath(request);
            if (path == "/redirect"_s) {
                co_await connection.awaitableSend(makeString(
                    "HTTP/1.1 302 Found\r\n"
                    "Location: /target\r\n"
                    "Content-Length: 0\r\n"
                    "\r\n"_s));
                continue;
            }
            if (path == "/target"_s) {
                receivedUserAgentOnTarget = userAgentFromRequest(request);
                targetWasRequested = true;
                co_await connection.awaitableSend(
                    "HTTP/1.1 200 OK\r\n"
                    "Content-Length: 0\r\n"
                    "\r\n"_s);
                continue;
            }
            EXPECT_FALSE(true);
        }
    });

    RetainPtr webView = adoptNS([TestWKWebView new]);
    RetainPtr navigationDelegate = adoptNS([TestNavigationDelegate new]);
    [webView setNavigationDelegate:navigationDelegate.get()];

    [webView loadRequest:server.request("/redirect"_s)];
    [navigationDelegate waitForDidFinishNavigation];

    EXPECT_TRUE(targetWasRequested);
    EXPECT_TRUE(receivedUserAgentOnTarget.startsWith("User-Agent: Mozilla/"_s));
}

// Same scenario as PageLevel above, but using the WKWebpagePreferences
// _customUserAgentAsSiteSpecificQuirks SPI. That value is stored on the
// DocumentLoader as a separate field from the regular customUserAgent, so the
// fix must resolve it through FrameLoader::userAgent() rather than checking
// the plain customUserAgent directly.
TEST(CustomUserAgent, WebpagePreferencesCustomUserAgentAsSiteSpecificQuirksAppliesToRedirectTarget)
{
    String receivedUserAgentOnTarget;
    bool targetWasRequested = false;
    HTTPServer server(HTTPServer::UseCoroutines::Yes, [&](Connection connection) -> ConnectionTask {
        while (true) {
            auto request = co_await connection.awaitableReceiveHTTPRequest();
            auto path = HTTPServer::parsePath(request);
            if (path == "/redirect"_s) {
                co_await connection.awaitableSend(makeString(
                    "HTTP/1.1 302 Found\r\n"
                    "Location: /target\r\n"
                    "Content-Length: 0\r\n"
                    "\r\n"_s));
                continue;
            }
            if (path == "/target"_s) {
                receivedUserAgentOnTarget = userAgentFromRequest(request);
                targetWasRequested = true;
                co_await connection.awaitableSend(
                    "HTTP/1.1 200 OK\r\n"
                    "Content-Length: 0\r\n"
                    "\r\n"_s);
                continue;
            }
            EXPECT_FALSE(true);
        }
    });

    RetainPtr configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    configuration.get().preferences._needsSiteSpecificQuirks = YES;
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectZero configuration:configuration.get()]);

    RetainPtr navigationDelegate = adoptNS([TestNavigationDelegate new]);
    navigationDelegate.get().decidePolicyForNavigationActionWithPreferences = ^(WKNavigationAction *navigationAction, WKWebpagePreferences *preferences, void (^decisionHandler)(WKNavigationActionPolicy, WKWebpagePreferences *)) {
        if ([navigationAction.request.URL.path isEqualToString:@"/target"])
            preferences._customUserAgentAsSiteSpecificQuirks = @"QuirkUA";
        decisionHandler(WKNavigationActionPolicyAllow, preferences);
    };
    [webView setNavigationDelegate:navigationDelegate.get()];

    [webView loadRequest:server.request("/redirect"_s)];
    [navigationDelegate waitForDidFinishNavigation];

    EXPECT_TRUE(targetWasRequested);
    EXPECT_WK_STREQ("User-Agent: QuirkUA", receivedUserAgentOnTarget.utf8().data());
}

// SharedWorker is hosted in a separate WebProcess. The page-side custom UA must
// propagate through the network process and into that host process so the
// worker observes it via navigator.userAgent. Regression test for
// WebSharedWorkerContextManagerConnection's inverted user-agent fallback.
//
// Use WKWebpagePreferences._customUserAgent rather than WKWebView.customUserAgent
// because the page-level customUserAgent is also pushed to remote-worker host
// processes via WebProcessPool::updateRemoteWorkerUserAgent, which keeps the
// host's m_userAgent in sync with the page UA and would mask the bug. The
// per-navigation UA stops at the page's DocumentLoader, so the host's
// m_userAgent stays as the standard UA and the buggy assignment becomes
// observable.
TEST(CustomUserAgent, SharedWorkerNavigatorUserAgent)
{
    constexpr auto mainPage = R"PAGE(
<script>
const worker = new SharedWorker("sw.js");
worker.port.onmessage = e => window.webkit.messageHandlers.testHandler.postMessage(e.data);
worker.port.start();
</script>
)PAGE"_s;

    constexpr auto sharedWorkerScript = R"WORKER(
onconnect = e => {
    const port = e.ports[0];
    port.postMessage(navigator.userAgent);
    port.start();
};
)WORKER"_s;

    HTTPServer server({
        { "/"_s, { mainPage } },
        { "/sw.js"_s, { { { "Content-Type"_s, "application/javascript"_s } }, sharedWorkerScript } },
    });

    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);

    RetainPtr navigationDelegate = adoptNS([TestNavigationDelegate new]);
    navigationDelegate.get().decidePolicyForNavigationActionWithPreferences = ^(WKNavigationAction *navigationAction, WKWebpagePreferences *preferences, void (^decisionHandler)(WKNavigationActionPolicy, WKWebpagePreferences *)) {
        preferences._customUserAgent = @"SharedWorkerCustomUA/1.0";
        decisionHandler(WKNavigationActionPolicyAllow, preferences);
    };
    [webView setNavigationDelegate:navigationDelegate.get()];

    __block RetainPtr<NSString> reportedUserAgent;
    __block bool gotMessage = false;
    [webView performAfterReceivingAnyMessage:^(NSString *message) {
        reportedUserAgent = message;
        gotMessage = true;
    }];

    [webView loadRequest:server.request()];
    TestWebKitAPI::Util::run(&gotMessage);

    EXPECT_WK_STREQ("SharedWorkerCustomUA/1.0", reportedUserAgent.get());
}

} // namespace TestWebKitAPI
