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
private import WebKit_Private.WKWebViewPrivateForTesting

import struct Foundation.URL
import struct Swift.String

@MainActor
struct ProvisionalNavigationPageStateTests {
    enum PageStateChange: Sendable, CaseIterable {
        case customUserAgent

        case darkAppearance

        case printMediaType

        private static let userAgent = "ProvisionalNavigationPageStateTests"

        var isAppliedScript: String {
            switch self {
            case .customUserAgent: "return navigator.userAgent === '\(Self.userAgent)'"
            case .darkAppearance: "return matchMedia('(prefers-color-scheme: dark)').matches"
            case .printMediaType: "return matchMedia('print').matches"
            }
        }

        @MainActor
        func apply(to webView: WKWebView) {
            switch self {
            case .customUserAgent: webView.customUserAgent = Self.userAgent
            case .darkAppearance: webView._setUseDarkAppearance(forTesting: true)
            case .printMediaType: webView.mediaType = "print"
            }
        }
    }

    @Test(.timeLimit(.minutes(1)), arguments: PageStateChange.allCases, [false, true])
    func stateChangedDuringCrossSiteNavigationReachesNewPage(change: PageStateChange, siteIsolationEnabled: Bool) async throws {
        var server = try HTTPServer(protocol: .httpsProxy) {
            Route("/example") {
                "example.com"
            }

            Route("/webkit") {
                "webkit.org"
            }
        }

        try await server.run { serverConfiguration in
            let (webView, navigationDelegate) = webViewAndDelegate(
                configuration: .init(httpsProxyFor: serverConfiguration),
                siteIsolationEnabled: siteIsolationEnabled
            )
            webView.forceLightMode()

            webView.load(URLRequest(url: try #require(URL(string: "https://example.com/example"))))
            try await navigationDelegate.waitForDidFinishNavigation()
            let isInitiallyApplied = try await webView.callJavaScript(returning: Bool.self) { change.isAppliedScript }
            try #require(!isInitiallyApplied)
            let firstProcessIdentifier = webView._webProcessIdentifier

            // The page for webkit.org already exists in its new process when its response is decided.
            navigationDelegate.decidePolicyForNavigationResponse = { _, decisionHandler in
                change.apply(to: webView)
                decisionHandler(.allow)
            }
            webView.load(URLRequest(url: try #require(URL(string: "https://webkit.org/webkit"))))
            try await navigationDelegate.waitForDidFinishNavigation()
            #expect(webView._webProcessIdentifier != firstProcessIdentifier)

            let isApplied = try await webView.callJavaScript(returning: Bool.self) { change.isAppliedScript }
            #expect(isApplied)
        }
    }
}
