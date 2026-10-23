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
private import WebKit_Private.WKPreferencesPrivate
private import WebKit_Private.WKWebViewPrivateForTestingIOS
private import WebKit_Private._WKFeature

import struct Foundation.URL
import struct Swift.String

@MainActor
struct SiteIsolationViewportSegmentsTests {
    @Test(.timeLimit(.minutes(1)), arguments: [false, true])
    func horizontalViewportSegmentsMediaQueryMatchesInCrossSiteIframe(segmentsSetBeforeLoad: Bool) async throws {
        var server = try HTTPServer(protocol: .httpsProxy) {
            Route("/mainframe") {
                "<iframe src='https://webkit.org/iframe'></iframe>"
            }

            Route("/iframe") {
                "<body>subframe text</body>"
            }
        }

        try await server.run { serverConfiguration in
            let configuration = WKWebViewConfiguration(httpsProxyFor: serverConfiguration)
            for feature in WKPreferences._features() where feature.key == "ViewportSegmentsAPIEnabled" {
                configuration.preferences._setEnabled(true, for: feature)
            }

            let (webView, navigationDelegate) = siteIsolatedWebViewAndDelegate(
                configuration: configuration,
                frame: CGRect(x: 0, y: 0, width: 800, height: 600)
            )

            func splitViewportIntoTwoColumns() {
                webView._setViewportFoldRects(forTesting: [NSValue(cgRect: CGRect(x: 400, y: 0, width: 0, height: 600))])
            }

            func matchesTwoHorizontalViewportSegments(in frame: WKFrameInfo?) async throws -> Bool {
                try await webView.callJavaScript(returning: Bool.self, in: frame) {
                    "return matchMedia('(horizontal-viewport-segments: 2)').matches"
                }
            }

            if segmentsSetBeforeLoad {
                splitViewportIntoTwoColumns()
            }
            webView.load(URLRequest(url: try #require(URL(string: "https://example.com/mainframe"))))
            try await navigationDelegate.waitForDidFinishNavigation()
            if !segmentsSetBeforeLoad {
                splitViewportIntoTwoColumns()
            }

            let childFrame = try #require(await webView.firstChildFrame)
            try await waitForCondition("the cross-site iframe to match two horizontal viewport segments") {
                try await matchesTwoHorizontalViewportSegments(in: childFrame)
            }
            #expect(try await matchesTwoHorizontalViewportSegments(in: nil))
        }
    }
}

#endif // HAVE_UIVIEW_RESERVED_REGION
