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
private import WebKit_Private.WKWebViewPrivate

import struct Foundation.URL
import struct Swift.String

@MainActor
struct SiteIsolationContentsOfAllFramesTests {
    @Test
    func contentsOfAllFramesIncludesCrossSiteAndSameSiteIframes() async throws {
        var server = try HTTPServer(protocol: .httpsProxy) {
            Route("/mainframe") {
                """
                <body>main frame text\
                <iframe src='https://webkit.org/iframe'></iframe>\
                <iframe src='https://example.com/same-site-iframe'></iframe></body>
                """
            }

            Route("/iframe") {
                "<body>cross-site text</body>"
            }

            Route("/same-site-iframe") {
                "<body>same-site text</body>"
            }
        }

        try await server.run { serverConfiguration in
            let (webView, navigationDelegate) = siteIsolatedWebViewAndDelegate(
                configuration: WKWebViewConfiguration(httpsProxyFor: serverConfiguration),
                frame: CGRect(x: 0, y: 0, width: 800, height: 600)
            )
            webView.load(URLRequest(url: try #require(URL(string: "https://example.com/mainframe"))))
            try await navigationDelegate.waitForDidFinishNavigation()

            let contents = await webView._getContentsOfAllFramesAsString()
            #expect(contents == "main frame text\n\ncross-site text\n\nsame-site text")
        }
    }

    @Test
    func clearContentWorldReachesCrossSiteIframe() async throws {
        var server = try HTTPServer(protocol: .httpsProxy) {
            Route("/mainframe") {
                "<iframe src='https://webkit.org/iframe'></iframe>"
            }

            Route("/iframe") {
                "<p>iframe</p>"
            }
        }

        try await server.run { serverConfiguration in
            let configuration = WKWebViewConfiguration(httpsProxyFor: serverConfiguration)
            let (webView, navigationDelegate) = siteIsolatedWebViewAndDelegate(
                configuration: configuration,
                frame: CGRect(x: 0, y: 0, width: 800, height: 600)
            )

            webView.load(URLRequest(url: try #require(URL(string: "https://example.com/mainframe"))))
            try await navigationDelegate.waitForDidFinishNavigation()
            let childFrame = try #require(await webView.firstChildFrame)

            let worldConfiguration = WKContentWorld.Configuration()
            worldConfiguration.nodeSnapshotCreationEnabled = true
            let world = WKContentWorld(configuration: worldConfiguration)

            func expando(in frame: WKFrameInfo?) async throws -> String {
                try await webView.callJavaScript(returning: String.self, in: frame, contentWorld: world) {
                    "return String(document.contentWorldExpando)"
                }
            }

            for frame in [nil, childFrame] {
                try await webView.callJavaScript(in: frame, contentWorld: world) {
                    "document.contentWorldExpando = 'set'"
                }
                #expect(try await expando(in: frame) == "set")
            }

            await webView._clearContentWorld(world)

            for frame in [nil, childFrame] {
                #expect(try await expando(in: frame) == "undefined")
            }
        }
    }
}
