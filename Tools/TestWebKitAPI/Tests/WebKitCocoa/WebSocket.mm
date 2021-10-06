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
#import "TestUIDelegate.h"
#import <WebKit/WKWebViewConfigurationPrivate.h>
#import <WebKit/WKWebsiteDataStorePrivate.h>
#import <wtf/RetainPtr.h>
#import <wtf/WeakObjCPtr.h>

namespace TestWebKitAPI {

TEST(WebSocket, LongMessageNoDeflate)
{
    // https://datatracker.ietf.org/doc/html/rfc6455#section-5.2
    constexpr size_t headerSizeWithLargePayloadLength = 10;
    constexpr size_t maskingKeySize = 4;
    constexpr uint8_t payloadLengthIndicatingLargeExtendedPayloadLength = 127;
    constexpr uint8_t fin = 0x80;
    constexpr uint8_t textFrame = 0x1;

    constexpr uint64_t twoMegabytes = 1024 * 1024 * 2;
    constexpr size_t expectedReceiveSize = twoMegabytes + headerSizeWithLargePayloadLength + maskingKeySize;

    HTTPServer server([=](Connection connection) {
        connection.webSocketHandshake([=] {
            connection.receiveBytes([=] (Vector<uint8_t>&& received) {
                constexpr size_t headerSize = headerSizeWithLargePayloadLength + maskingKeySize;
                EXPECT_EQ(expectedReceiveSize, received.size());
                std::array<uint8_t, headerSizeWithLargePayloadLength> expectedHeader { 0x81, 0xff, 0x0, 0x0, 0x0, 0x0, 0x0, 0x20, 0x0, 0x0, };
                for (size_t i = 0 ; i < headerSizeWithLargePayloadLength; i++)
                    EXPECT_EQ(received[i], expectedHeader[i]);
                std::array<uint8_t, maskingKeySize> mask { received[10], received[11], received[12], received[13] };
                for (size_t i = headerSize; i < expectedReceiveSize; i++)
                    EXPECT_EQ(received[i] ^ mask[(i - headerSize) % maskingKeySize], 'x');

                Vector<uint8_t> bytesToSend;
                bytesToSend.reserveInitialCapacity(twoMegabytes + headerSizeWithLargePayloadLength);
                bytesToSend.uncheckedAppend(fin | textFrame);
                bytesToSend.uncheckedAppend(payloadLengthIndicatingLargeExtendedPayloadLength);
                for (size_t i = 0; i < 8; i++)
                    bytesToSend.uncheckedAppend((twoMegabytes >> (8 * (7 - i))) & 0xFF);
                for (size_t i = 0; i < twoMegabytes; i++)
                    bytesToSend.uncheckedAppend('x');

                connection.send(WTFMove(bytesToSend));
            }, expectedReceiveSize);
        });
    });

    NSString *html = [NSString stringWithFormat:@""
        "<script>"
        "    let twoMegabytes = 2*1024*1024;"
        "    var ws = new WebSocket('ws://127.0.0.1:%d/');"
        "    ws.onopen = function() { this.send('x'.repeat(twoMegabytes)); };"
        "    ws.onmessage = function(msg) { alert(msg.data.length == twoMegabytes ? 'PASS' : 'FAIL - wrong receive length'); };"
        "    ws.onerror = function(error) { alert('FAIL - error ' + error.message); }"
        "</script>", server.port()];
    auto webView = adoptNS([WKWebView new]);
    [webView loadHTMLString:html baseURL:nil];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "PASS");
}

TEST(WebSocket, PageWithAttributedBundleIdentifierDestroyed)
{
    HTTPServer server([](Connection connection) {
        connection.webSocketHandshake();
    });

    NSString *html = [NSString stringWithFormat:@""
    "<script>"
    "    var ws = new WebSocket('ws://127.0.0.1:%d/');"
    "    ws.onopen = function() { alert('opened successfully'); };"
    "    ws.onerror = function(error) { alert('FAIL - error ' + error.message); }"
    "</script>", server.port()];

    pid_t originalNetworkProcessPID { 0 };
    auto configuration = adoptNS([WKWebViewConfiguration new]);
    __block size_t webSocketsConnected { 0 };
    auto delegate = adoptNS([TestUIDelegate new]);
    delegate.get().runJavaScriptAlertPanelWithMessage = ^(WKWebView *, NSString *message, WKFrameInfo *, void (^completionHandler)(void)) {
        EXPECT_WK_STREQ(message, "opened successfully");
        webSocketsConnected++;
        completionHandler();
    };
    constexpr size_t viewCount = 20;

    @autoreleasepool {
        std::array<RetainPtr<WKWebView>, viewCount> views;
        for (size_t i = 0; i < viewCount; i++) {
            configuration.get()._attributedBundleIdentifier = [NSString stringWithFormat:@"test.bundle.identifier.%zu", i];
            views[i] = adoptNS([[WKWebView alloc] initWithFrame:CGRectMake(0, 0, 100, 100) configuration:configuration.get()]);
            views[i].get().UIDelegate = delegate.get();
            [views[i] loadHTMLString:html baseURL:nil];
        }
        while (webSocketsConnected < viewCount)
            Util::spinRunLoop();
        originalNetworkProcessPID = configuration.get().websiteDataStore._networkProcessIdentifier;
    }

    Util::spinRunLoop(viewCount * 3);
    EXPECT_EQ(originalNetworkProcessPID, configuration.get().websiteDataStore._networkProcessIdentifier);
}

#if HAVE(NSURLSESSION_WEBSOCKET)
TEST(WebSocket, CloseCode)
{
    bool receivedWebSocketClose { false };
    Vector<uint8_t> closeData;
    HTTPServer webSocketServer([&](Connection connection) {
        connection.webSocketHandshake([&, connection] {
            connection.receiveBytes([&, connection](Vector<uint8_t>&& v) {
                // https://datatracker.ietf.org/doc/html/rfc6455#section-5.2
                constexpr uint8_t connectionCloseOpcode { 0x08 };
                constexpr uint8_t opcodeMask { 0x08 };
                constexpr uint8_t maskMask { 0x80 };
                constexpr uint8_t payloadLengthMask { 0x7F };
                constexpr uint8_t headerSize { 6 };
                ASSERT_GT(v.size(), 1u);
                EXPECT_EQ(v[0] & opcodeMask, connectionCloseOpcode);
                EXPECT_TRUE(v[1] & maskMask);
                size_t length = v[1] & payloadLengthMask;
                ASSERT_EQ(length, v.size() - headerSize);
                std::array<uint8_t, 4> mask { v[2], v[3], v[4], v[5] };
                for (size_t i = headerSize; i < v.size(); i++)
                    closeData.append(v[i] ^ mask[(i - headerSize) % 4]);
                receivedWebSocketClose = true;
                connection.receiveBytes([](Vector<uint8_t>&& v) {
                    EXPECT_EQ(v.size(), 0u);
                });
            });
        });
    });

    auto htmlWithOnOpen = [&] (const char* onopen) {
        return [NSString stringWithFormat:@""
            "<script>"
            "    var ws = new WebSocket('ws://127.0.0.1:%d/');"
            "    ws.onopen = function() { %s };"
                "</script>", webSocketServer.port(), onopen];
    };

    HTTPServer httpServer({
        { "/navigateAway", { htmlWithOnOpen("window.location = '/navigationTarget'") }},
        { "/navigationTarget", { "hi" } },
        { "/closeCustomCode", { htmlWithOnOpen("ws.close(3000)") } },
        { "/closeNoArguments", { htmlWithOnOpen("ws.close()") } },
        { "/closeBothParameters", { htmlWithOnOpen("ws.close(3001, 'custom reason')") } },
    });

    auto appendString = [] (Vector<uint8_t>& vector, const char* string) {
        auto length = strlen(string);
        for (size_t i = 0; i < length; i++)
            vector.append(string[i]);
    };

    auto webView = adoptNS([WKWebView new]);
    [webView loadRequest:httpServer.request("/navigateAway")];
    Util::run(&receivedWebSocketClose);
    Vector<uint8_t> expected { 0x3, 0xe9 }; // NSURLSessionWebSocketCloseCodeGoingAway
    appendString(expected, "WebSocket is closed due to suspension.");
    EXPECT_EQ(closeData, expected);

    receivedWebSocketClose = false;
    closeData = { };
    expected = { 0xb, 0xb8 }; // 3000
    [webView loadRequest:httpServer.request("/closeCustomCode")];
    Util::run(&receivedWebSocketClose);
    EXPECT_EQ(closeData, expected);

    receivedWebSocketClose = false;
    closeData = { };
    expected = { };
    [webView loadRequest:httpServer.request("/closeNoArguments")];
    Util::run(&receivedWebSocketClose);
    EXPECT_EQ(closeData, expected);

    receivedWebSocketClose = false;
    closeData = { };
    expected = { 0xb, 0xb9 }; // 3001
    appendString(expected, "custom reason");
    [webView loadRequest:httpServer.request("/closeBothParameters")];
    Util::run(&receivedWebSocketClose);
    EXPECT_EQ(closeData, expected);
}
#endif // HAVE(NSURLSESSION_WEBSOCKET)

TEST(WebSocket, BlockedWithSubresources)
{
    HTTPServer server([](Connection connection) {
        connection.webSocketHandshake();
    });

    NSString *html = [NSString stringWithFormat:@""
    "<script>"
    "    var ws = new WebSocket('ws://127.0.0.1:%d/');"
    "    ws.onopen = function() { alert('opened successfully'); };"
    "    ws.onerror = function(error) { alert('FAIL - error ' + error.message); }"
    "</script>", server.port()];

    {
        auto configuration = adoptNS([WKWebViewConfiguration new]);
        configuration.get()._loadsSubresources = NO;
        auto webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectZero configuration:configuration.get()]);
        [webView loadHTMLString:html baseURL:nil];
        EXPECT_WK_STREQ([webView _test_waitForAlert], "FAIL - error undefined");
    }
    auto webView = adoptNS([WKWebView new]);
    [webView loadHTMLString:html baseURL:nil];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "opened successfully");
}

}
