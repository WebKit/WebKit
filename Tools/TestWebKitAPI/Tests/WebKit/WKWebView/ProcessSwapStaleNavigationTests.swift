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

// A navigation to another site loads in a provisional page, in another web content process, until it commits. These
// tests start another navigation in that time, and check that the web view ends up where that navigation took it.
@MainActor
struct ProcessSwapStaleNavigationTests {
    // Going back to the same site is a traversal in the committed process, which moves the back/forward list's cursor
    // as it starts.
    @Test(arguments: [false, true])
    func goingBackWhileCrossSiteNavigationWaitsForItsResponse(siteIsolationEnabled: Bool) async throws {
        try await goBackWhileCrossSiteNavigationWaitsForItsResponse(
            to: "https://example.com/first",
            siteIsolationEnabled: siteIsolationEnabled
        )
    }

    // Going back to another site loads in a provisional page of its own, whose process moves the cursor as well.
    @Test(arguments: [false, true])
    func goingBackToAnotherSiteWhileCrossSiteNavigationWaitsForItsResponse(siteIsolationEnabled: Bool) async throws {
        try await goBackWhileCrossSiteNavigationWaitsForItsResponse(
            to: "https://apple.com/first",
            siteIsolationEnabled: siteIsolationEnabled
        )
    }

    #if hasAttribute(diagnose)
    @diagnose(DeprecatedDeclaration, as: ignored, reason: "Intentionally uses a custom process pool")
    #endif
    private func goBackWhileCrossSiteNavigationWaitsForItsResponse(to backURL: String, siteIsolationEnabled: Bool) async throws {
        var server = try HTTPServer(protocol: .httpsProxy) {
            Route("/first") { "first" }
            Route("/second") { "second" }
            Route("/target") { "target" }
        }

        try await server.run { serverConfiguration in
            let configuration = WKWebViewConfiguration(httpsProxyFor: serverConfiguration)
            configuration.processPool = WKProcessPool.processSwapping()

            let (webView, navigationDelegate) = webViewAndDelegate(configuration: configuration, siteIsolationEnabled: siteIsolationEnabled)

            webView.load(URLRequest(url: try #require(URL(string: backURL))))
            try await navigationDelegate.waitForDidFinishNavigation()
            webView.load(URLRequest(url: try #require(URL(string: "https://example.com/second"))))
            try await navigationDelegate.waitForDidFinishNavigation()

            var finishedURLs: [URL] = []
            navigationDelegate.didFinishNavigation = { webView, _ in
                if let url = webView.url {
                    finishedURLs.append(url)
                }
            }

            // Holding the response's policy decision keeps the navigation in its provisional page.
            var decideTargetResponsePolicy: ((WKNavigationResponsePolicy) -> Void)?
            navigationDelegate.decidePolicyForNavigationResponse = { navigationResponse, decisionHandler in
                guard navigationResponse.response.url?.path == "/target" else {
                    decisionHandler(.allow)
                    return
                }

                decideTargetResponsePolicy = decisionHandler
            }

            webView.load(URLRequest(url: try #require(URL(string: "https://webkit.org/target"))))
            try await waitForCondition("The navigation to /target waits for its response policy") {
                decideTargetResponsePolicy != nil
            }
            #expect(webView._provisionalWebProcessIdentifier != 0)
            #expect(webView._committedURL?.path == "/second")

            webView.goBack()
            try await waitForCondition("The back navigation finishes") { !finishedURLs.isEmpty }

            #expect(finishedURLs.map(\.absoluteString) == [backURL])
            #expect(webView._provisionalWebProcessIdentifier == 0)
            #expect(webView.url?.absoluteString == backURL)
            #expect(webView.backForwardList.currentItem?.url.absoluteString == backURL)
            #expect(webView.backForwardList.forwardItem?.url.path == "/second")
            #expect(try await webView.callJavaScript(returning: String.self) { "return document.body.textContent" } == "first")

            decideTargetResponsePolicy?(.cancel)
            withExtendedLifetime(navigationDelegate) {}
        }
    }
}
