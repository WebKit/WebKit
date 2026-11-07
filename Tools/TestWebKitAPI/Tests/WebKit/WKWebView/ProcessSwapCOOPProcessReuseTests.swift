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

import Foundation
private import TestWebKitAPILibrary
import Testing
import WebKit
private import WebKit_Private.WKPreferencesPrivate
private import WebKit_Private.WKProcessPoolPrivate
private import WebKit_Private.WKWebViewPrivate
private import WebKit_Private._WKProcessPoolConfiguration

import struct Foundation.URL
import struct Swift.String

// A response with Cross-Origin-Opener-Policy: same-origin moves its navigation to a process of a new browsing context
// group. That process is keyed by the origin of the navigation, so it never comes from another origin of the same site.

@MainActor
struct ProcessSwapCOOPProcessReuseTests {
    // The back/forward cache or the process cache keeps a process of site2.example.com. Navigations to a COOP page of
    // site1.example.com in a second tab must not continue in it. Such a swap used to need another swap, again to that
    // process, so the navigation never finished (https://bugs.webkit.org/show_bug.cgi?id=293321).
    @Test(arguments: [false, true], [false, true])
    func navigationsToCOOPPageInSecondTabDoNotUseProcessOfSameSiteOrigin(
        siteIsolationEnabled: Bool,
        backForwardCacheEnabled: Bool
    ) async throws {
        var server = try HTTPServer(protocol: .httpsProxy) {
            Route("/coop", headerFields: ["Content-Type": "text/html", "Cross-Origin-Opener-Policy": "same-origin"]) {
                "coop"
            }

            Route("/other") {
                "other"
            }
        }

        try await server.run { serverConfiguration in
            let configuration = WKWebViewConfiguration(httpsProxyFor: serverConfiguration)
            configuration.processPool = Self.processSwappingProcessPool(backForwardCacheEnabled: backForwardCacheEnabled)
            configuration.setFeature("CrossOriginOpenerPolicyEnabled", enabled: true)
            configuration.setFeature("CrossOriginEmbedderPolicyEnabled", enabled: true)
            let coopURL = try #require(URL(string: "https://site1.example.com/coop"))
            let sameSiteURL = try #require(URL(string: "https://site2.example.com/other"))
            let crossSiteURL = try #require(URL(string: "https://webkit.org/other"))

            // The first tab leaves its process of site2.example.com, which never hosted a COOP page, in the back/forward
            // cache or the process cache.
            let (firstWebView, firstNavigationDelegate) = webViewAndDelegate(
                configuration: configuration,
                frame: CGRect(x: 0, y: 0, width: 800, height: 600),
                siteIsolationEnabled: siteIsolationEnabled
            )
            try await Self.navigate(firstWebView, firstNavigationDelegate) { $0.load(URLRequest(url: sameSiteURL)) }
            let sameSiteProcessIdentifier = firstWebView._webProcessIdentifier
            try await Self.navigate(firstWebView, firstNavigationDelegate) { $0.load(URLRequest(url: coopURL)) }
            #expect(firstWebView._webProcessIdentifier != sameSiteProcessIdentifier)

            // The second tab navigates to the COOP page from its initial empty document, then from a page of another site.
            let (secondWebView, secondNavigationDelegate) = webViewAndDelegate(
                configuration: configuration,
                frame: CGRect(x: 0, y: 0, width: 800, height: 600),
                siteIsolationEnabled: siteIsolationEnabled
            )
            try await Self.navigate(secondWebView, secondNavigationDelegate) { $0.load(URLRequest(url: coopURL)) }
            #expect(secondWebView.url == coopURL)
            #expect(secondWebView._webProcessIdentifier != sameSiteProcessIdentifier)

            try await Self.navigate(secondWebView, secondNavigationDelegate) { $0.load(URLRequest(url: crossSiteURL)) }
            try await Self.navigate(secondWebView, secondNavigationDelegate) { $0.load(URLRequest(url: coopURL)) }
            #expect(secondWebView.url == coopURL)
            #expect(secondWebView._webProcessIdentifier != sameSiteProcessIdentifier)
            #expect(try await secondWebView.callJavaScript(returning: String.self) { "return document.body.textContent" } == "coop")
        }
    }

    /// Creates a process pool that swaps processes on navigation, like the pools of the tests in
    /// ProcessSwapOnNavigation.mm, and keeps processes that no longer host a page in its process cache.
    private static func processSwappingProcessPool(backForwardCacheEnabled: Bool) -> WKProcessPool {
        let configuration = _WKProcessPoolConfiguration()
        configuration.processSwapsOnNavigation = true
        configuration.usesWebProcessCache = true
        configuration.prewarmsProcessesAutomatically = true
        configuration.pageCacheEnabled = backForwardCacheEnabled
        return WKProcessPool._processPool(with: configuration)
    }

    /// Runs a navigation of the web view, failing if it doesn't finish soon, as it wouldn't if it kept moving to
    /// another process.
    private static func navigate(
        _ webView: TestWKWebView,
        _ navigationDelegate: TestNavigationDelegate,
        sourceLocation: SourceLocation = #_sourceLocation,
        _ startNavigation: (TestWKWebView) -> Void
    ) async throws {
        var commits = 0
        var finished = false
        navigationDelegate.didCommitNavigation = { _, _ in
            commits += 1
        }
        navigationDelegate.didFinishNavigation = { _, _ in
            finished = true
        }
        defer {
            navigationDelegate.didCommitNavigation = nil
            navigationDelegate.didFinishNavigation = nil
        }

        startNavigation(webView)
        try await waitForCondition("the navigation to finish", timeout: .seconds(10)) {
            finished && !webView.isLoading
        }
        #expect(commits == 1, sourceLocation: sourceLocation)
    }
}
