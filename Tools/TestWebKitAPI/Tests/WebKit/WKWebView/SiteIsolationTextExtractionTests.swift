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

import CoreGraphics
private import TestWebKitAPILibrary
import Testing
import WebKit
private import WebKit_Private.WKWebViewPrivate
private import WebKit_Private._WKTextRun

import struct Foundation.URL
import struct Swift.String

@MainActor
struct SiteIsolationTextExtractionTests {
    @Test(.timeLimit(.minutes(1)))
    func requestAllTextIncludesTextOfCrossSiteIframeInIframeRect() async throws {
        var server = try HTTPServer(protocol: .httpsProxy) {
            Route("/mainframe") {
                """
                <meta name='viewport' content='width=device-width, initial-scale=1'>
                <body style='margin: 0'>
                <p>main frame text</p>
                <iframe id='iframe' src='https://webkit.org/iframe'
                    style='margin-left: 100px; width: 400px; height: 300px; border: none'></iframe>
                </body>
                """
            }

            Route("/iframe") {
                """
                <body style='margin: 0'>
                <div style='height: 100px'></div>
                <p style='margin: 0 40px'>iframe text</p>
                </body>
                """
            }
        }

        try await server.run { serverConfiguration in
            let (webView, navigationDelegate) = siteIsolatedWebViewAndDelegate(
                configuration: .init(httpsProxyFor: serverConfiguration),
                frame: CGRect(x: 0, y: 0, width: 800, height: 600)
            )
            webView.load(URLRequest(url: try #require(URL(string: "https://example.com/mainframe"))))
            try await navigationDelegate.waitForDidFinishNavigation()
            await webView.nextPresentationUpdate()

            let textRuns = try #require(await webView._requestAllText())
            #expect(textRuns.map(\.text) == ["main frame text", "iframe text"])

            let values = try await webView.callJavaScript(returning: [Double].self) {
                """
                const rect = document.getElementById('iframe').getBoundingClientRect();
                return [rect.x, rect.y, rect.width, rect.height];
                """
            }
            try #require(values.count == 4)
            let iframeRect = CGRect(x: values[0], y: values[1], width: values[2], height: values[3])

            let iframeTextRun = try #require(textRuns.first(where: { $0.text == "iframe text" }))
            #expect(!iframeTextRun.rectInWebView.isEmpty)
            #expect(iframeRect.contains(iframeTextRun.rectInWebView))
        }
    }
}
