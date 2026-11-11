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

#if HAVE_UIVIEW_RESERVED_REGION

import CoreGraphics
private import TestWebKitAPILibrary
import Testing
import UIKit
import WebKit
private import WebKit_Private.WKWebViewPrivate
private import WebKit_Private.WKWebViewPrivateForTestingIOS

import struct Foundation.URL
import struct Swift.String

@MainActor
struct SiteIsolationViewportSegmentsTests {
    @Test(.timeLimit(.minutes(1)), arguments: [false, true])
    func crossSiteIframeDoesNotSeeViewportSegments(segmentsSetBeforeLoad: Bool) async throws {
        var server = try HTTPServer(protocol: .httpsProxy) {
            Route("/mainframe") {
                "<iframe src='https://webkit.org/iframe'></iframe>"
            }

            Route("/iframe") {
                "iframe"
            }
        }

        try await server.run { serverConfiguration in
            let (webView, navigationDelegate) = siteIsolatedWebViewAndDelegate(
                configuration: Self.configuration(for: serverConfiguration),
                frame: CGRect(x: 0, y: 0, width: 800, height: 600)
            )

            if segmentsSetBeforeLoad {
                webView.splitViewportIntoTwoColumns()
            }
            webView.load(URLRequest(url: try #require(URL(string: "https://example.com/mainframe"))))
            try await navigationDelegate.waitForDidFinishNavigation()
            if !segmentsSetBeforeLoad {
                webView.splitViewportIntoTwoColumns()
            }

            try await Self.expectOnlyMainFrameSeesViewportSegments(in: webView)
        }
    }

    // With the opened window in example.com's process, the opener's page there turns into a remote page, which then
    // hosts the iframe. With the opened window in webkit.org's process, the opener's main frame moves into the opener's
    // remote page there.
    @Test(.timeLimit(.minutes(1)), arguments: ["example.com", "webkit.org"])
    func crossSiteIframeDoesNotSeeViewportSegmentsAfterOpenerNavigates(openedWindowHost: String) async throws {
        var server = try HTTPServer(protocol: .httpsProxy) {
            Route("/opener") {
                "opener"
            }

            Route("/opened") {
                "opened"
            }

            Route("/mainframe") {
                "<iframe src='https://example.com/iframe'></iframe>"
            }

            Route("/iframe") {
                "iframe"
            }
        }

        try await server.run { serverConfiguration in
            let (webView, navigationDelegate) = siteIsolatedWebViewAndDelegate(
                configuration: Self.configuration(for: serverConfiguration),
                frame: CGRect(x: 0, y: 0, width: 800, height: 600)
            )
            let openedWebViews = OpenedWebViewRecorder()
            webView.uiDelegate = openedWebViews
            webView.splitViewportIntoTwoColumns()

            webView.load(URLRequest(url: try #require(URL(string: "https://example.com/opener"))))
            try await navigationDelegate.waitForDidFinishNavigation()
            try await webView.callJavaScript(arguments: ["url": "https://\(openedWindowHost)/opened"]) { "window.open(url)" }
            let opened = try await openedWebViews.nextOpenedWebView()
            try await opened.navigationDelegate.waitForDidFinishNavigation()

            try await webView.callJavaScript { "setTimeout(() => { location.href = 'https://webkit.org/mainframe' }, 0)" }
            try await navigationDelegate.waitForDidFinishNavigation()
            let mainFrameMovedIntoOpenedWindowProcess = webView._webProcessIdentifier == opened.webView._webProcessIdentifier
            #expect(mainFrameMovedIntoOpenedWindowProcess == (openedWindowHost == "webkit.org"))

            try await Self.expectOnlyMainFrameSeesViewportSegments(in: webView)
        }
    }

    private static func configuration(for serverConfiguration: HTTPServer.Configuration) -> WKWebViewConfiguration {
        let configuration = WKWebViewConfiguration(httpsProxyFor: serverConfiguration)
        configuration.setFeature("ViewportSegmentsAPIEnabled", enabled: true)
        return configuration
    }

    private static func expectOnlyMainFrameSeesViewportSegments(in webView: WKWebView) async throws {
        try await waitForCondition("the main frame to match two horizontal viewport segments") {
            try await webView.matchesTwoHorizontalViewportSegments(in: nil)
        }
        #expect(try await webView.viewportSegmentCount(in: nil) == 2)

        let childFrame = try #require(await webView.firstChildFrame)
        #expect(try await !webView.matchesTwoHorizontalViewportSegments(in: childFrame))
        #expect(try await webView.viewportSegmentCount(in: childFrame) == 1)
    }
}

extension WKWebView {
    fileprivate func splitViewportIntoTwoColumns() {
        _setViewportFoldRects(forTesting: [NSValue(cgRect: CGRect(x: 400, y: 0, width: 0, height: 600))])
    }

    fileprivate func matchesTwoHorizontalViewportSegments(in frame: WKFrameInfo?) async throws -> Bool {
        try await callJavaScript(returning: Bool.self, in: frame) {
            "return matchMedia('(horizontal-viewport-segments: 2)').matches"
        }
    }

    fileprivate func viewportSegmentCount(in frame: WKFrameInfo?) async throws -> Int {
        try await callJavaScript(returning: Int.self, in: frame) {
            "return viewport.segments.length"
        }
    }
}

#endif // HAVE_UIVIEW_RESERVED_REGION
