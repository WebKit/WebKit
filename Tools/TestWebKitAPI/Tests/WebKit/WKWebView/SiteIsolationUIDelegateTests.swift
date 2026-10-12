// Copyright (C) 2026 Apple Inc. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
// 1. Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
// PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
// BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
// THE POSSIBILITY OF SUCH DAMAGE.

private import TestWebKitAPILibrary
import Testing
import WebKit
private import WebKit_Private.WKUIDelegatePrivate

import struct CoreGraphics.CGRect
import class Foundation.NSObject
import struct Foundation.URL
import struct Swift.String

// A page has a single UI delegate, and a frame's web content process needs to know about it to show dialogs for the
// frame. With site isolation, that includes processes of cross-site iframes that were created before the delegate was
// set. These tests set the delegate after such an iframe loads.

@MainActor
private final class BeforeUnloadDelegate: NSObject, @preconcurrency WKUIDelegatePrivate {
    // swift-format-ignore: NoLeadingUnderscores
    func _webView(
        _ webView: WKWebView,
        runBeforeUnloadConfirmPanelWithMessage message: String,
        initiatedByFrame frame: WKFrameInfo
    ) async -> Bool {
        true
    }
}

@MainActor
struct SiteIsolationUIDelegateTests {
    @Test
    func beforeUnloadDelegateSetAfterCrossOriginIframeLoads() async throws {
        var server = try HTTPServer(protocol: .httpsProxy) {
            Route("/mainframe") {
                "<iframe src='https://webkit.org/iframe'></iframe>"
            }

            Route("/iframe") {
                "<body>subframe text<script>addEventListener('beforeunload', () => parent.postMessage('beforeunload', '*'));</script></body>"
            }

            Route("/iframe2") {
                "<body>second subframe text</body>"
            }
        }

        try await server.run { serverConfiguration in
            let (webView, navigationDelegate) = siteIsolatedWebViewAndDelegate(
                configuration: .init(httpsProxyFor: serverConfiguration),
                frame: CGRect(x: 0, y: 0, width: 800, height: 600)
            )
            webView.load(URLRequest(url: try #require(URL(string: "https://example.com/mainframe"))))
            try await navigationDelegate.waitForDidFinishNavigation()

            // A web view only holds its UI delegate weakly, so keep it alive until the end of the test.
            let uiDelegate = BeforeUnloadDelegate()
            defer { withExtendedLifetime(uiDelegate) {} }
            webView.uiDelegate = uiDelegate

            try await webView.callJavaScript {
                "window.messages = []; addEventListener('message', event => messages.push(event.data))"
            }

            let childFrame = try #require(await webView.firstChildFrame)
            try await webView.callJavaScript(in: childFrame) { "setTimeout(() => { location.href = '/iframe2' }, 0)" }

            try await waitForCondition("the main frame to receive the message from the iframe's beforeunload listener") {
                try await webView.callJavaScript(returning: Int.self) { "return messages.length" } == 1
            }
            let message = try await webView.callJavaScript(returning: String.self) { "return messages[0]" }
            #expect(message == "beforeunload")
        }
    }
}
