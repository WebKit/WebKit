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

#if ENABLE(WEB_RTC)

#import "HTTPServer.h"
#import "PlatformUtilities.h"
#import "TestNavigationDelegate.h"
#import "TestWKWebView.h"

@interface WebRTCMessageHandler : NSObject <WKScriptMessageHandler>
- (void)setMessageHandler:(Function<void(WKScriptMessage*)>&&)messageHandler;
@end

@implementation WebRTCMessageHandler  {
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

TEST(WebKit2, RTCDataChannelPostMessage)
{
    __block bool removedAnyExistingData = false;
    [[WKWebsiteDataStore defaultDataStore] removeDataOfTypes:[WKWebsiteDataStore allWebsiteDataTypes] modifiedSince:[NSDate distantPast] completionHandler:^() {
        removedAnyExistingData = true;
    }];
    TestWebKitAPI::Util::run(&removedAnyExistingData);

    static constexpr auto main =
    "<script>"
    "let registration;"
    "async function register() {"
    "    registration = await navigator.serviceWorker.register('/sw.js');"
    "    if (registration.active) {"
    "        window.webkit.messageHandlers.webrtc.postMessage('READY');"
    "        return;"
    "    }"
    "    worker = registration.installing;"
    "    worker.addEventListener('statechange', function() {"
    "        if (worker.state == 'activated')"
    "            window.webkit.messageHandlers.webrtc.postMessage('READY');"
    "    });"
    "}"
    "register();"
    ""
    "let channel1, channel2;"
    "let pc1, pc2;"
    "async function doTransferDataChannelTest() {"
        "pc1 = new RTCPeerConnection();"
        "pc2 = new RTCPeerConnection();"
        ""
        "pc1.onicecandidate = (event) => pc2.addIceCandidate(event.candidate);"
        "pc2.onicecandidate = (event) => pc1.addIceCandidate(event.candidate);"
        ""
        "channel1 = pc1.createDataChannel('test');"
        "registration.active.postMessage({ channel: channel1 }, [channel1]);"
        "let promise = new Promise(resolve => pc2.ondatachannel = (event) => resolve(event.channel));"
        ""
        "const offer = await pc1.createOffer();"
        "await pc1.setLocalDescription(offer);"
        "await pc2.setRemoteDescription(offer);"
        "const answer = await pc2.createAnswer();"
        "await pc2.setLocalDescription(answer);"
        "await pc1.setRemoteDescription(answer);"
        ""
        "channel2 = await promise;"
        "if (channel2.readyState === 'closed') {"
        "   window.webkit.messageHandlers.webrtc.postMessage('CLOSED');"
        "   return;"
        "}"
        "if (channel2.readyState !== 'open')"
        "    await new Promise(resolve => channel2.onopen = resolve);"
        ""
        "promise = new Promise(resolve => navigator.serviceWorker.onmessage = (event) => resolve(event.data));"
        "channel2.send('TRANSFERED');"
        "window.webkit.messageHandlers.webrtc.postMessage(await promise);"
    "}"
    "function transferDataChannel() {"
    "   doTransferDataChannelTest();"
    "}"
    "</script>"_s;

    static constexpr auto js = "self.onmessage = (event) => { "
    "    const source = event.source;"
    "    const channel = event.data.channel;"
    "    if (channel.readyState === 'closed')"
    "        source.postMessage('closed');"
    "    channel.onclose = (e) => source.postMessage('close event');"
    "    channel.onmessage = (e) => source.postMessage(e.data);"
    "};"_s;

    HTTPServer server({
        { "/"_s, { main } },
        { "/sw.js"_s, { {{ "Content-Type"_s, "application/javascript"_s }}, js } },
        { "/"_s, { main } },
    }, HTTPServer::Protocol::Https);
    auto* request = server.request();

    auto navigationDelegate = adoptNS([TestNavigationDelegate new]);
    [navigationDelegate setDidReceiveAuthenticationChallenge:^(WKWebView *, NSURLAuthenticationChallenge *challenge, void (^callback)(NSURLSessionAuthChallengeDisposition, NSURLCredential *)) {
        callback(NSURLSessionAuthChallengeUseCredential, [NSURLCredential credentialForTrust:challenge.protectionSpace.serverTrust]);
    }];

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto messageHandler = adoptNS([[WebRTCMessageHandler alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:messageHandler.get() name:@"webrtc"];

    auto webView1 = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500) configuration:configuration.get()]);
    webView1.get().navigationDelegate = navigationDelegate.get();

    [messageHandler setMessageHandler:[](WKScriptMessage *message) {
        EXPECT_WK_STREQ(@"READY", [message body]);
        isReady = true;
    }];
    isReady = false;
    [webView1 loadRequest:request];
    TestWebKitAPI::Util::run(&isReady);

    auto webView2 = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500) configuration:configuration.get()]);
    webView2.get().navigationDelegate = navigationDelegate.get();

    [messageHandler setMessageHandler:[](WKScriptMessage *message) {
        EXPECT_WK_STREQ(@"READY", [message body]);
        isReady = true;
    }];
    isReady = false;
    [webView2 loadRequest:request];
    TestWebKitAPI::Util::run(&isReady);

    [messageHandler setMessageHandler:[](WKScriptMessage *message) {
        EXPECT_WK_STREQ(@"TRANSFERED", [message body]);
        isReady = true;
    }];

    // Transfer is probably in-process.
    isReady = false;
    [webView1 stringByEvaluatingJavaScript:@"transferDataChannel()"];
    TestWebKitAPI::Util::run(&isReady);

    // Transfer is probably out-of-process.
    isReady = false;
    [webView2 stringByEvaluatingJavaScript:@"transferDataChannel()"];
    TestWebKitAPI::Util::run(&isReady);
}

} // namespace TestWebKitAPI

#endif // ENABLE(WEB_RTC)
