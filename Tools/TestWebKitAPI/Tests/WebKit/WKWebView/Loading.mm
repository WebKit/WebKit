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

#import "Helpers/cocoa/HTTPServer.h"
#import "Helpers/PlatformUtilities.h"
#import "Helpers/cocoa/TestNavigationDelegate.h"
#import "Helpers/cocoa/TestWKWebView.h"
#import <wtf/text/MakeString.h>

@interface LoadingMessageHandler : NSObject <WKScriptMessageHandler>
- (void)setMessageHandler:(Function<void(WKScriptMessage*)>&&)messageHandler;
@end

@implementation LoadingMessageHandler  {
Function<void(WKScriptMessage*)> _messageHandler;
}
- (void)setMessageHandler:(Function<void(WKScriptMessage*)>&&)messageHandler {
    _messageHandler = WTF::move(messageHandler);
}
- (void)userContentController:(WKUserContentController *)userContentController didReceiveScriptMessage:(WKScriptMessage *)message
{
    if (_messageHandler)
        _messageHandler(message);
}
@end

namespace TestWebKitAPI {

static bool isReady = false;

TEST(WebKit, LoadRequestWithSecPurposePrefetch)
{
    __block bool removedAnyExistingData = false;
    [[WKWebsiteDataStore defaultDataStore] removeDataOfTypes:[WKWebsiteDataStore allWebsiteDataTypes] modifiedSince:[NSDate distantPast] completionHandler:^() {
        removedAnyExistingData = true;
    }];
    TestWebKitAPI::Util::run(&removedAnyExistingData);

    static constexpr auto main =
    "<script>"
    "    window.webkit.messageHandlers.loading.postMessage('PASS');"
    "</script>"_s;

    HTTPServer server({
        { "/"_s, { main } },
    }, HTTPServer::Protocol::Http);

    RetainPtr configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    RetainPtr messageHandler = adoptNS([[LoadingMessageHandler alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:messageHandler.get() name:@"loading"];

    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500) configuration:configuration.get()]);

    [messageHandler setMessageHandler:[](WKScriptMessage *message) {
        EXPECT_WK_STREQ(@"PASS", [message body]);
        isReady = true;
    }];
    isReady = false;

    NSMutableURLRequest *request = [server.request() mutableCopy];
    [request addValue:@"prefetch" forHTTPHeaderField:@"Sec-Purpose"];

    [webView loadRequest:request];
    TestWebKitAPI::Util::run(&isReady);
}

TEST(WebKit, NavigateToInFlightPrefetchWithCrossOriginRedirect)
{
    static constexpr auto main =
    "<script type='speculationrules'>{\"prefetch\":[{\"source\":\"list\",\"urls\":[\"/redirect\"]}]}</script>"
    "<a id='link' href='/redirect'>link</a>"_s;

    // Same site but a different origin, so DocumentPrefetcher rejects the redirect without causing a process swap.
    HTTPServer destinationServer({
        { "/destination"_s, { "destination"_s } },
    }, HTTPServer::Protocol::Https);

    std::optional<Connection> prefetchConnection;
    auto redirectResponse = [&] {
        return makeString("HTTP/1.1 302 Found\r\nLocation: https://127.0.0.1:"_s, destinationServer.port(), "/destination\r\nContent-Length: 0\r\n\r\n"_s);
    };
    HTTPServer server(HTTPServer::UseCoroutines::Yes, [&](Connection connection) -> ConnectionTask {
        while (true) {
            auto request = co_await connection.awaitableReceiveHTTPRequest();
            auto path = HTTPServer::parsePath(request);
            if (path == "/redirect"_s) {
                if (contains(request.span(), "Sec-Purpose: prefetch"_span)) {
                    // Hold the prefetch response so the navigation has a chance to join the in-flight prefetch.
                    prefetchConnection = connection;
                    co_return;
                }
                // The navigation loads this itself if it did not join the prefetch before the prefetch was redirected.
                co_await connection.awaitableSend(redirectResponse());
                continue;
            }
            co_await connection.awaitableSend(HTTPResponse(main).serialize());
        }
    }, HTTPServer::Protocol::Https);

    // Whether the navigation joins the prefetch before the redirect arrives, and the order in which the
    // CachedResource's clients receive the redirect, both vary between runs, so try a few times.
    for (unsigned i = 0; i < 3; ++i) {
        prefetchConnection = std::nullopt;

        RetainPtr webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
        RetainPtr delegate = adoptNS([TestNavigationDelegate new]);
        [delegate allowAnyTLSCertificate];
        [webView setNavigationDelegate:delegate.get()];

        [webView loadRequest:server.request()];
        [delegate waitForDidFinishNavigation];
        while (!prefetchConnection)
            TestWebKitAPI::Util::spinRunLoop();

        [webView evaluateJavaScript:@"document.getElementById('link').click()" completionHandler:nil];
        [delegate waitForDidStartProvisionalNavigation];

        __block bool webProcessCrashed = false;
        __block bool navigationFinished = false;
        delegate.get().webContentProcessDidTerminate = ^(WKWebView *, _WKProcessTerminationReason) {
            webProcessCrashed = true;
            navigationFinished = true;
        };
        delegate.get().didFailProvisionalNavigation = ^(WKWebView *, WKNavigation *, NSError *) {
            navigationFinished = true;
        };
        delegate.get().didFinishNavigation = ^(WKWebView *, WKNavigation *) {
            navigationFinished = true;
        };

        // The server no longer reads from this connection, so make sure it isn't reused for later requests.
        prefetchConnection->send(makeString("HTTP/1.1 302 Found\r\nLocation: https://127.0.0.1:"_s, destinationServer.port(), "/destination\r\nContent-Length: 0\r\nConnection: close\r\n\r\n"_s));
        TestWebKitAPI::Util::run(&navigationFinished);
        EXPECT_FALSE(webProcessCrashed);
        if (webProcessCrashed)
            break;
        EXPECT_WK_STREQ(@"/destination", [webView URL].path);
    }
}

} // namespace TestWebKitAPI
