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
#import "TestNavigationDelegate.h"
#import "TestWKWebView.h"

@interface LoadingMessageHandler : NSObject <WKScriptMessageHandler>
- (void)setMessageHandler:(Function<void(WKScriptMessage*)>&&)messageHandler;
@end

@implementation LoadingMessageHandler  {
Function<void(WKScriptMessage*)> _messageHandler;
}
- (void)setMessageHandler:(Function<void(WKScriptMessage*)>&&)messageHandler {
    _messageHandler = WTFMove(messageHandler);
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

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto messageHandler = adoptNS([[LoadingMessageHandler alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:messageHandler.get() name:@"loading"];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500) configuration:configuration.get()]);

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

} // namespace TestWebKitAPI
