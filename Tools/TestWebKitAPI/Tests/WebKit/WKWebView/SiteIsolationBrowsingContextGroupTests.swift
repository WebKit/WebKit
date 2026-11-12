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
private import WebKit_Private.WKProcessPoolPrivate
private import WebKit_Private.WKWebViewPrivate
private import WebKit_Private.WKWebsiteDataStorePrivate

import struct Foundation.URL
import struct Swift.String

// With site isolation, each browsing context group has its own processes for the sites of its frames. A response with
// Cross-Origin-Opener-Policy can move a navigation to a new group, which severs the window's opener, and a cross-origin
// isolated group only uses processes of its own. These tests check which group and processes a page ends up in.

@MainActor
struct SiteIsolationBrowsingContextGroupTests {
    // The opener keeps c.com alive in the shared process of the original group, so the opened page rejoins it when it
    // goes back.
    @Test
    func openedPageReturningToGroupLoadsIframeInSharedProcess() async throws {
        var server = try HTTPServer(protocol: .httpsProxy) {
            Route("/opener") {
                "<iframe src='https://c.com/iframe'></iframe>"
            }

            Route("/opened") {
                "<iframe src='https://c.com/iframe'></iframe>"
            }

            Route("/iframe") {
                "hi"
            }

            Route("/elsewhere") {
                "elsewhere"
            }
        }

        try await server.run { serverConfiguration in
            let configuration = WKWebViewConfiguration(httpsProxyFor: serverConfiguration)
            configuration.websiteDataStore._resourceLoadStatisticsEnabled = true
            configuration.setFeature("SiteIsolationSharedProcessEnabled", enabled: true)
            let (webView, navigationDelegate) = siteIsolatedWebViewAndDelegate(
                configuration: configuration,
                frame: CGRect(x: 0, y: 0, width: 800, height: 600)
            )
            let openedWebViews = OpenedWebViewRecorder()
            webView.uiDelegate = openedWebViews

            webView.load(URLRequest(url: try #require(URL(string: "https://a.com/opener"))))
            try await navigationDelegate.waitForDidFinishNavigation()
            webView.startEvaluatingJavaScript("w = window.open('https://b.com/opened')")
            let (openedWebView, openedNavigationDelegate) = try await openedWebViews.nextOpenedWebView()

            let expectedOpenedFrameTrees: FrameTreesInProcesses = [
                .local("https://b.com", children: [.remote]),
                .remote(children: [.local("https://c.com")]),
                .remote(children: [.remote]),
            ]
            #expect(await openedWebView.frameTreesInProcesses() == expectedOpenedFrameTrees)

            openedWebView.load(URLRequest(url: try #require(URL(string: "https://d.com/elsewhere"))))
            try await openedNavigationDelegate.waitForDidFinishNavigation()
            #expect(await openedWebView.frameTreesInProcesses() == [.local("https://d.com")])

            openedWebView.goBack()
            await openedNavigationDelegate.waitForDidCommitNavigation()
            try? await waitForCondition("the iframe to load in the shared process", timeout: .seconds(10)) {
                await openedWebView.frameTreesInProcesses() == expectedOpenedFrameTrees
            }
            #expect(await openedWebView.frameTreesInProcesses() == expectedOpenedFrameTrees)
        }
    }

    // The committed main frame is still a.com while a navigation to b.com waits for its policy decision, so a b.com
    // iframe must not join the main frame's process.
    @Test
    func iframeNavigationToSiteOfPendingMainFrameLoad() async throws {
        var server = try HTTPServer(protocol: .httpsProxy) {
            Route("/main") {
                "<iframe src='https://c.com/iframe'></iframe>"
            }

            Route("/iframe") {
                "hi"
            }

            Route("/pending") {
                "pending"
            }

            Route("/b_iframe") {
                "hi"
            }
        }

        try await server.run { serverConfiguration in
            let configuration = WKWebViewConfiguration(httpsProxyFor: serverConfiguration)
            configuration.setFeature("SiteIsolationSharedProcessEnabled", enabled: false)
            let (webView, navigationDelegate) = siteIsolatedWebViewAndDelegate(configuration: configuration)

            webView.load(URLRequest(url: try #require(URL(string: "https://a.com/main"))))
            try await navigationDelegate.waitForDidFinishNavigation()

            let pendingURL = try #require(URL(string: "https://b.com/pending"))
            let decidePendingNavigationPolicy = await navigationDelegate.holdNextNavigationActionPolicyDecision {
                $0.request.url?.path == "/pending"
            } triggeredBy: {
                webView.load(URLRequest(url: pendingURL))
            }

            let childFrame = try #require(await webView.firstChildFrame)
            webView.startEvaluatingJavaScript("location.href = 'https://b.com/b_iframe'", in: childFrame)
            await navigationDelegate.waitForDidFinishLoadInSubframe()
            let expectedFrameTrees: FrameTreesInProcesses = [
                .local("https://a.com", children: [.remote]),
                .remote(children: [.local("https://b.com")]),
            ]
            #expect(await webView.frameTreesInProcesses() == expectedFrameTrees)

            decidePendingNavigationPolicy(.cancel)
            try await waitForCondition("the main frame navigation to stop") {
                !webView.isLoading
            }
            #expect(await webView.frameTreesInProcesses() == expectedFrameTrees)
        }
    }

    // Killing the web content process doesn't cancel the policy decision of an iframe that was already removed from
    // the page, so the decision can arrive when the page has no main frame. The UI process must survive it.
    @Test
    func removedIframeNavigationAllowedAfterWebContentProcessIsKilled() async throws {
        var server = try HTTPServer(protocol: .httpsProxy) {
            Route("/main") {
                "<iframe src='https://c.com/iframe'></iframe>"
            }

            Route("/iframe") {
                "hi"
            }

            Route("/b_iframe") {
                "hi"
            }
        }

        try await server.run { serverConfiguration in
            let configuration = WKWebViewConfiguration(httpsProxyFor: serverConfiguration)
            configuration.setFeature("SiteIsolationSharedProcessEnabled", enabled: false)
            let (webView, navigationDelegate) = siteIsolatedWebViewAndDelegate(configuration: configuration)

            let mainURL = try #require(URL(string: "https://a.com/main"))
            webView.load(URLRequest(url: mainURL))
            try await navigationDelegate.waitForDidFinishNavigation()

            let childFrame = try #require(await webView.firstChildFrame)
            let decideIframeNavigationPolicy = await navigationDelegate.holdNextNavigationActionPolicyDecision {
                $0.request.url?.path == "/b_iframe"
            } triggeredBy: {
                webView.startEvaluatingJavaScript("location.href = 'https://b.com/b_iframe'", in: childFrame)
            }

            try await webView.callJavaScript { "document.querySelector('iframe').remove()" }
            try await waitForCondition("the iframe to be removed") {
                await webView.firstChildFrame == nil
            }

            webView._killWebContentProcessAndResetState()
            decideIframeNavigationPolicy(.allow)
            // WebKit handles the decision on a later run loop iteration, with no callback to wait for. Give it time to
            // arrive before the next load creates a new main frame.
            try await Task.sleep(for: .milliseconds(100))

            webView.load(URLRequest(url: mainURL))
            try await navigationDelegate.waitForDidFinishNavigation()
            let expectedFrameTrees: FrameTreesInProcesses = [
                .local("https://a.com", children: [.remote]),
                .remote(children: [.local("https://c.com")]),
            ]
            #expect(await webView.frameTreesInProcesses() == expectedFrameTrees)
        }
    }

    @Test(arguments: [true, false])
    func cancelledCoopNavigationKeepsOpener(siteIsolationEnabled: Bool) async throws {
        try await assertCoopNavigationThatDoesNotCommitKeepsOpener(
            siteIsolationEnabled: siteIsolationEnabled,
            coopPath: "/coop",
            coopResponsePolicy: .cancel
        )
    }

    @Test(arguments: [true, false])
    func coopNavigationTurnedIntoDownloadKeepsOpener(siteIsolationEnabled: Bool) async throws {
        try await assertCoopNavigationThatDoesNotCommitKeepsOpener(
            siteIsolationEnabled: siteIsolationEnabled,
            coopPath: "/coop",
            coopResponsePolicy: .download
        )
    }

    @Test(arguments: [true, false])
    func coopNavigationReceivingNoContentResponseKeepsOpener(siteIsolationEnabled: Bool) async throws {
        try await assertCoopNavigationThatDoesNotCommitKeepsOpener(
            siteIsolationEnabled: siteIsolationEnabled,
            coopPath: "/coop-204"
        )
    }

    @Test(arguments: [true, false])
    func committedCoopNavigationSeversOpener(siteIsolationEnabled: Bool) async throws {
        var server = try HTTPServer(protocol: .httpsProxy) {
            Self.openerRoutes

            Route("/coop", headerFields: ["Cross-Origin-Opener-Policy": "same-origin"]) {
                "coop"
            }
        }

        try await server.run { serverConfiguration in
            let (opener, opened) = try await Self.openerAndOpenedWebViews(
                siteIsolationEnabled: siteIsolationEnabled,
                server: serverConfiguration
            )
            defer { withExtendedLifetime(opener) {} }
            #expect(try await Self.hasOpener(opened.webView))

            try await Self.navigate(opened.webView, to: "https://example.com/coop", andWaitWith: opened.navigationDelegate)
            #expect(try await !Self.hasOpener(opened.webView))
            #expect(try await Self.windowName(opened.webView) == "")
        }
    }

    @Test(arguments: [true, false])
    func clientStartedCrossSiteNavigationSeversOpener(siteIsolationEnabled: Bool) async throws {
        var server = try HTTPServer(protocol: .httpsProxy) {
            Self.openerRoutes

            Route("/target") {
                "target"
            }
        }

        try await server.run { serverConfiguration in
            let (opener, opened) = try await Self.openerAndOpenedWebViews(
                siteIsolationEnabled: siteIsolationEnabled,
                server: serverConfiguration
            )
            defer { withExtendedLifetime(opener) {} }
            #expect(try await Self.hasOpener(opened.webView))

            opened.webView.load(URLRequest(url: try #require(URL(string: "https://apple.com/target"))))
            try await opened.navigationDelegate.waitForDidFinishNavigation()
            #expect(try await !Self.hasOpener(opened.webView))
            #expect(try await Self.windowName(opened.webView) == "")
        }
    }

    private func assertCoopNavigationThatDoesNotCommitKeepsOpener(
        siteIsolationEnabled: Bool,
        coopPath: String,
        coopResponsePolicy: WKNavigationResponsePolicy? = nil
    ) async throws {
        var server = try HTTPServer(protocol: .httpsProxy) {
            Self.openerRoutes

            Route("/target") {
                "target"
            }

            Route("/coop", headerFields: ["Cross-Origin-Opener-Policy": "same-origin"]) {
                "coop"
            }

            Route("/coop-204", statusCode: 204, headerFields: ["Cross-Origin-Opener-Policy": "same-origin"]) {
                ""
            }
        }

        try await server.run { serverConfiguration in
            let (opener, opened) = try await Self.openerAndOpenedWebViews(
                siteIsolationEnabled: siteIsolationEnabled,
                server: serverConfiguration
            )
            defer { withExtendedLifetime(opener) {} }
            #expect(try await Self.hasOpener(opened.webView))

            if let coopResponsePolicy {
                opened.navigationDelegate.decidePolicyForNavigationResponse = { _, decisionHandler in
                    decisionHandler(coopResponsePolicy)
                }
            }
            opened.webView.startEvaluatingJavaScript("location.href = 'https://example.com\(coopPath)'")
            _ = await opened.navigationDelegate.waitForDidFailProvisionalNavigation()
            opened.navigationDelegate.decidePolicyForNavigationResponse = nil

            try await Self.navigate(opened.webView, to: "https://apple.com/target", andWaitWith: opened.navigationDelegate)
            #expect(try await Self.hasOpener(opened.webView))
            #expect(try await Self.windowName(opened.webView) == "popup")
        }
    }

    private typealias WebViewAndDelegate = (webView: TestWKWebView, navigationDelegate: TestNavigationDelegate)

    @RouteBuilder
    private static var openerRoutes: Route {
        Route("/shared-opener") {
            "<script>onload = () => { w = window.open('https://example.com/shared-plain', 'popup'); }</script>"
        }

        Route("/shared-plain") {
            "shared"
        }
    }

    private static func openerAndOpenedWebViews(
        siteIsolationEnabled: Bool,
        server: HTTPServer.Configuration
    ) async throws -> (opener: WebViewAndDelegate, opened: WebViewAndDelegate) {
        let configuration = WKWebViewConfiguration(httpsProxyFor: server)
        configuration.preferences.javaScriptCanOpenWindowsAutomatically = true
        let opener = webViewAndDelegate(
            configuration: configuration,
            frame: CGRect(x: 0, y: 0, width: 800, height: 600),
            siteIsolationEnabled: siteIsolationEnabled
        )
        let openedWebViews = OpenedWebViewRecorder(siteIsolationEnabled: siteIsolationEnabled)
        opener.webView.uiDelegate = openedWebViews

        opener.webView.load(URLRequest(url: try #require(URL(string: "https://webkit.org/shared-opener"))))
        let opened = try await openedWebViews.nextOpenedWebView()
        return (opener, opened)
    }

    private static func hasOpener(_ webView: WKWebView) async throws -> Bool {
        try await webView.callJavaScript(returning: Bool.self) { "return !!opener" }
    }

    private static func windowName(_ webView: WKWebView) async throws -> String {
        try await webView.callJavaScript(returning: String.self) { "return name" }
    }

    private static func navigate(
        _ webView: TestWKWebView,
        to urlString: String,
        andWaitWith navigationDelegate: TestNavigationDelegate
    ) async throws {
        webView.startEvaluatingJavaScript("location.href = '\(urlString)'")
        try await navigationDelegate.waitForDidFinishNavigation()
    }
}
