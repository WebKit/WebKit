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
private import WebKit_Private.WKWebViewPrivate

import struct Foundation.URL
import struct Swift.String

// The back/forward cache keeps the page a web view navigates away from, and a cross-site navigation leaves the page in
// its process as a suspended page. Going back has to restore it from there, unless the process can't host the
// navigation, such as when it isn't in the browsing context group or the Lockdown Mode state the navigation needs.
// These tests check which process hosts a page after a history navigation.

@MainActor
struct ProcessSwapBackForwardTests {
    @Test
    func navigateBackAndForwardAcrossCrossOriginIsolationUsesBackForwardCache() async throws {
        let pageshowScript = "<script>addEventListener('pageshow', event => { window.persisted = event.persisted })</script>"
        var server = try HTTPServer(protocol: .httpsProxy) {
            Route("/shared") {
                pageshowScript + "shared"
            }

            Route("/isolated", headerFields: Self.crossOriginIsolation) {
                pageshowScript + "isolated"
            }
        }

        try await server.run { serverConfiguration in
            let processPool = WKProcessPool.processSwapping()
            let (webView, navigationDelegate) = Self.crossOriginIsolationWebViewAndDelegate(
                server: serverConfiguration,
                processPool: processPool
            )

            webView.load(URLRequest(url: try #require(URL(string: "https://example.com/shared"))))
            try await navigationDelegate.waitForDidFinishNavigation()
            let sharedProcessIdentifier = webView._webProcessIdentifier
            try await webView.callJavaScript { "window.marker = 'shared'" }

            webView.load(URLRequest(url: try #require(URL(string: "https://example.com/isolated"))))
            try await navigationDelegate.waitForDidFinishNavigation()
            let isolatedProcessIdentifier = webView._webProcessIdentifier
            #expect(isolatedProcessIdentifier != sharedProcessIdentifier)
            #expect(processPool.crossOriginIsolatedProcessIdentifiers.contains(isolatedProcessIdentifier))
            try await webView.callJavaScript { "window.marker = 'isolated'" }

            webView.goBack()
            try await navigationDelegate.waitForDidFinishNavigation()
            #expect(webView._webProcessIdentifier == sharedProcessIdentifier)
            #expect(try await webView.callJavaScript(returning: String.self) { "return String(window.marker)" } == "shared")
            #expect(try await webView.callJavaScript(returning: String.self) { "return String(window.persisted)" } == "true")
            #expect(!processPool.crossOriginIsolatedProcessIdentifiers.contains(webView._webProcessIdentifier))
            #expect(try await !webView.callJavaScript(returning: Bool.self) { "return self.crossOriginIsolated" })
            #expect(try await webView.callJavaScript(returning: String.self) { "return typeof SharedArrayBuffer" } == "undefined")

            webView.goForward()
            try await navigationDelegate.waitForDidFinishNavigation()
            #expect(webView._webProcessIdentifier == isolatedProcessIdentifier)
            #expect(try await webView.callJavaScript(returning: String.self) { "return String(window.marker)" } == "isolated")
            #expect(try await webView.callJavaScript(returning: String.self) { "return String(window.persisted)" } == "true")
            #expect(try await webView.callJavaScript(returning: Bool.self) { "return self.crossOriginIsolated" })
        }
    }

    // A control: going back across cross-origin isolation still uses a new process when it turns on Lockdown Mode.
    @Test
    func navigateBackAcrossCrossOriginIsolationIntoLockdownModeUsesNewProcess() async throws {
        var server = try HTTPServer(protocol: .httpsProxy) {
            Route("/shared") {
                "shared"
            }

            Route("/isolated", headerFields: Self.crossOriginIsolation) {
                "isolated"
            }
        }

        try await server.run { serverConfiguration in
            let (webView, navigationDelegate) = Self.crossOriginIsolationWebViewAndDelegate(
                server: serverConfiguration,
                processPool: .processSwapping()
            )

            webView.load(URLRequest(url: try #require(URL(string: "https://example.com/shared"))))
            try await navigationDelegate.waitForDidFinishNavigation()
            let sharedProcessIdentifier = webView._webProcessIdentifier
            try await webView.callJavaScript { "window.marker = 'shared'" }

            webView.load(URLRequest(url: try #require(URL(string: "https://example.com/isolated"))))
            try await navigationDelegate.waitForDidFinishNavigation()
            let isolatedProcessIdentifier = webView._webProcessIdentifier
            #expect(isolatedProcessIdentifier != sharedProcessIdentifier)

            navigationDelegate.decidePolicyForNavigationActionWithPreferences = { _, preferences, decisionHandler in
                preferences.isLockdownModeEnabled = true
                decisionHandler(.allow, preferences)
            }
            webView.goBack()
            try await navigationDelegate.waitForDidFinishNavigation()

            #expect(webView._webProcessIdentifier != sharedProcessIdentifier)
            #expect(webView._webProcessIdentifier != isolatedProcessIdentifier)
            #expect(webView._webProcessIdentifier != 0)
            #expect(await !webView._isJITEnabled())
            #expect(try await webView.callJavaScript(returning: String.self) { "return typeof window.marker" } == "undefined")
            #expect(try await webView.callJavaScript(returning: String.self) { "return document.body.textContent" } == "shared")
        }
    }

    // The history item of a document that location.replace() committed through a COOP swap has to record the group the
    // document committed into, so that going back restores the document from the back/forward cache.
    @Test(arguments: [false, true])
    func navigateBackToItemReplacedThroughCOOPSwap(siteIsolationEnabled: Bool) async throws {
        try await Self.navigateBackToItemReplacedThroughCOOPSwap(siteIsolationEnabled: siteIsolationEnabled, includingCOEP: true)
    }

    // A control: without COEP, the replacement swaps between two groups that aren't cross-origin isolated, so a stale
    // group in the history item doesn't change which process can restore it.
    @Test
    func navigateBackToItemReplacedThroughCOOPSwapWithoutCOEP() async throws {
        try await Self.navigateBackToItemReplacedThroughCOOPSwap(siteIsolationEnabled: false, includingCOEP: false)
    }

    private static func navigateBackToItemReplacedThroughCOOPSwap(siteIsolationEnabled: Bool, includingCOEP: Bool) async throws {
        var isolatedHeaderFields = ["Content-Type": "text/html", "Cross-Origin-Opener-Policy": "same-origin"]
        if includingCOEP {
            isolatedHeaderFields["Cross-Origin-Embedder-Policy"] = "require-corp"
        }
        var server = try HTTPServer(protocol: .https) {
            Route("/source.html") {
                "foo"
            }

            Route("/isolated.html", headerFields: isolatedHeaderFields) {
                "<script>addEventListener('pageshow', event => { if (event.persisted) window.wasPersisted = true })</script>"
            }

            Route("/other.html") {
                "bar"
            }
        }

        try await server.run { serverConfiguration in
            let processPool = WKProcessPool.processSwapping()
            let configuration = WKWebViewConfiguration()
            configuration.processPool = processPool
            configuration.enableCrossOriginOpenerAndEmbedderPolicies()
            let (webView, navigationDelegate) = webViewAndDelegate(
                configuration: configuration,
                frame: CGRect(x: 0, y: 0, width: 800, height: 600),
                siteIsolationEnabled: siteIsolationEnabled
            )
            let isolatedURL = serverConfiguration.address.appending(path: "isolated.html")

            webView.load(URLRequest(url: serverConfiguration.address.appending(path: "source.html")))
            try await navigationDelegate.waitForDidFinishNavigation()
            let sourceProcessIdentifier = webView._webProcessIdentifier

            // Replaces the history item of /source.html with a document that commits into a new group.
            webView.startEvaluatingJavaScript("location.replace('/isolated.html')", withUserGesture: false)
            try await navigationDelegate.waitForDidFinishNavigation()

            let isolatedProcessIdentifier = webView._webProcessIdentifier
            #expect(isolatedProcessIdentifier != sourceProcessIdentifier)
            if includingCOEP {
                #expect(processPool.crossOriginIsolatedProcessIdentifiers.contains(isolatedProcessIdentifier))
            }
            #expect(webView._committedURL?.absoluteString == isolatedURL.absoluteString)
            #expect(webView.backForwardList.backList.isEmpty)

            webView.load(URLRequest(url: serverConfiguration.address.appending(path: "other.html")))
            try await navigationDelegate.waitForDidFinishNavigation()
            #expect(webView._webProcessIdentifier != isolatedProcessIdentifier)

            webView.goBack()
            try await navigationDelegate.waitForDidFinishNavigation()
            #expect(webView._webProcessIdentifier == isolatedProcessIdentifier)
            #expect(webView._committedURL?.absoluteString == isolatedURL.absoluteString)
            #expect(try await webView.callJavaScript(returning: Bool.self) { "return !!window.wasPersisted" })
        }
    }

    private static let crossOriginIsolation = [
        "Content-Type": "text/html",
        "Cross-Origin-Opener-Policy": "same-origin",
        "Cross-Origin-Embedder-Policy": "require-corp",
    ]

    private static func crossOriginIsolationWebViewAndDelegate(
        server: HTTPServer.Configuration,
        processPool: WKProcessPool
    ) -> (webView: TestWKWebView, navigationDelegate: TestNavigationDelegate) {
        let configuration = WKWebViewConfiguration(httpsProxyFor: server)
        configuration.processPool = processPool
        configuration.enableCrossOriginOpenerAndEmbedderPolicies()
        return webViewAndDelegate(
            configuration: configuration,
            frame: CGRect(x: 0, y: 0, width: 800, height: 600),
            siteIsolationEnabled: false
        )
    }
}
