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

#import "Helpers/cocoa/HTTPServer.h"
#import "Helpers/PlatformUtilities.h"
#import "Helpers/Test.h"
#import "Helpers/cocoa/MiniTURNServer.h"
#import "Helpers/cocoa/TestNavigationDelegate.h"
#import "Helpers/cocoa/TestWKWebView.h"
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/_WKFeature.h>
#import <wtf/Function.h>

@interface WebRTCMessageHandler : NSObject <WKScriptMessageHandler>
- (void)setMessageHandler:(Function<void(WKScriptMessage*)>&&)messageHandler;
@end

@implementation WebRTCMessageHandler  {
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
    ""
    "async function closePCTest(pc) {"
    "    const promise = new Promise(resolve => navigator.serviceWorker.onmessage = (event) => resolve(event.data));"
    "    pc.close();"
    "    window.webkit.messageHandlers.webrtc.postMessage(await promise);"
    "}"
    ""
    "function closePC1() {"
    "    closePCTest(pc1);"
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

    RetainPtr navigationDelegate = adoptNS([TestNavigationDelegate new]);
    [navigationDelegate setDidReceiveAuthenticationChallenge:^(WKWebView *, NSURLAuthenticationChallenge *challenge, void (^callback)(NSURLSessionAuthChallengeDisposition, NSURLCredential *)) {
        callback(NSURLSessionAuthChallengeUseCredential, [NSURLCredential credentialForTrust:challenge.protectionSpace.serverTrust]);
    }];

    RetainPtr configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    RetainPtr messageHandler = adoptNS([[WebRTCMessageHandler alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:messageHandler.get() name:@"webrtc"];

    RetainPtr webView1 = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500) configuration:configuration.get()]);
    webView1.get().navigationDelegate = navigationDelegate.get();

    [messageHandler setMessageHandler:[](WKScriptMessage *message) {
        EXPECT_WK_STREQ(@"READY", [message body]);
        isReady = true;
    }];
    isReady = false;
    [webView1 loadRequest:request];
    TestWebKitAPI::Util::run(&isReady);

    RetainPtr webView2 = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500) configuration:configuration.get()]);
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

    [messageHandler setMessageHandler:[](WKScriptMessage *message) {
        EXPECT_WK_STREQ(@"close event", [message body]);
        isReady = true;
    }];

    isReady = false;
    [webView1 stringByEvaluatingJavaScript:@"closePC1()"];
    TestWebKitAPI::Util::run(&isReady);

    isReady = false;
    [webView2 stringByEvaluatingJavaScript:@"closePC1()"];
    TestWebKitAPI::Util::run(&isReady);
}

TEST(WebKit2, WebRTCTurnAllocationWithDirectDNSDisabled)
{
    MiniTURNServer turnServer;

    static constexpr auto pageTemplate =
    "<html><body><script>"
    "function runTest(port, transportType) {"
    "    gatherRelayCandidate(port, transportType);"
    "}"
    "async function gatherRelayCandidate(port, transportType) {"
    "    try {"
    "        const pc = new RTCPeerConnection({"
    "            iceServers: [{"
    "                urls: ["
    "                    `turn:localhost:${port}?transport=${transportType}`"
    "                ],"
    "                username: 'testUser',"
    "                credential: 'testPass'"
    "            }],"
    "            iceTransportPolicy: 'relay'"
    "        });"
    "        pc.onicecandidate = (event) => {"
    "            if (!event.candidate)"
    "                return;"
    "            const c = event.candidate.candidate;"
    "            if (!c.includes(' typ relay '))"
    "                return;"
    "            pc.close();"
    "            window.webkit.messageHandlers.webrtc.postMessage('GOT_RELAY');"
    "        };"
    "        pc.createDataChannel('probe');"
    "        const offer = await pc.createOffer();"
    "        await pc.setLocalDescription(offer);"
    "    } catch (e) {"
    "        window.webkit.messageHandlers.webrtc.postMessage('GOT_ERROR: ' + e.message);"
    "        pc.close();"
    "    }"
    "}"
    "</script></body></html>"_s;

    HTTPServer server({ { "/"_s, { pageTemplate } } }, HTTPServer::Protocol::Http);

    RetainPtr navigationDelegate = adoptNS([TestNavigationDelegate new]);
    RetainPtr configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    for (_WKFeature *feature in [WKPreferences _features]) {
        if ([feature.key isEqualToString:@"WebRTCDNSResolutionBySocketEnabled"])
            [[configuration preferences] _setEnabled:YES forFeature:feature];
    }

    RetainPtr messageHandler = adoptNS([[WebRTCMessageHandler alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:messageHandler.get() name:@"webrtc"];

    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500) configuration:configuration.get()]);
    webView.get().navigationDelegate = navigationDelegate.get();

    bool gotRelayCandidate = false;
    RetainPtr<NSString> errorMessage;
    [messageHandler setMessageHandler:[&gotRelayCandidate, &errorMessage](WKScriptMessage *message) {
        NSString *body = [message body];
        if ([body isEqualToString:@"GOT_RELAY"])
            gotRelayCandidate = true;
        else
            errorMessage = body;
    }];

    [webView loadRequest:server.request()];
    [webView _test_waitForDidFinishNavigation];

    [webView stringByEvaluatingJavaScript:
        [NSString stringWithFormat:@"runTest(%u, 'tcp')", turnServer.tcpPort()]];
    TestWebKitAPI::Util::run(&gotRelayCandidate);
    EXPECT_NULL(errorMessage.get());
    EXPECT_TRUE(turnServer.takeRequests().containsIf([](auto& request) {
        return MiniTURNServer::isStunMethodAllocate(request.method) && request.transport == MiniTURNServer::Transport::Tcp;
    }));

    errorMessage = { };
    gotRelayCandidate = false;
    [webView stringByEvaluatingJavaScript:
        [NSString stringWithFormat:@"runTest(%u, 'udp')", turnServer.udpPort()]];
    TestWebKitAPI::Util::run(&gotRelayCandidate);
    EXPECT_NULL(errorMessage.get());
    EXPECT_TRUE(turnServer.takeRequests().containsIf([](auto& request) {
        return MiniTURNServer::isStunMethodAllocate(request.method) && request.transport == MiniTURNServer::Transport::Udp;
    }));
}

} // namespace TestWebKitAPI

#endif // ENABLE(WEB_RTC)
