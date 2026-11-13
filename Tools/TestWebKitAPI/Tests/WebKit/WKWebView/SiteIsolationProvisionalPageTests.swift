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
@_spi(Testing) import WebKit
private import WebKit_Private.WKFrameInfoPrivate
private import WebKit_Private.WKPreferencesPrivate
private import WebKit_Private.WKProcessPoolPrivate
private import WebKit_Private.WKWebViewPrivate
private import WebKit_Private.WKWebpagePreferencesPrivate
private import WebKit_Private._WKFeature

import struct Foundation.URL
import struct Swift.String

// With site isolation, a main frame navigation loads in a provisional page until it commits, which can be in a process
// that already hosts frames of the page, and can move to another process when its response arrives. These tests check
// what the page keeps in each process when such a navigation moves on, fails, or shares a process with other frames.

@MainActor
struct SiteIsolationProvisionalPageTests {
    // The navigation's response moves it to a new browsing context group, whose process the web process cache can keep.
    #if hasAttribute(diagnose)
    @diagnose(DeprecatedDeclaration, as: ignored, reason: "Intentionally uses a custom process pool")
    #endif
    @Test
    func cancelledNavigationToNewBrowsingContextGroupClosesItsPage() async throws {
        var server = try HTTPServer(protocol: .httpsProxy) {
            Route("/main") {
                "main"
            }

            Route("/coop", headerFields: ["Cross-Origin-Opener-Policy": "same-origin"]) {
                "coop"
            }
        }

        try await server.run { serverConfiguration in
            let configuration = Self.configuration(serverConfiguration)
            configuration.processPool = .processSwapping(backForwardCacheEnabled: false)
            let (webView, navigationDelegate) = siteIsolatedWebViewAndDelegate(configuration: configuration)

            webView.load(URLRequest(url: try #require(URL(string: "https://example.com/main"))))
            try await navigationDelegate.waitForDidFinishNavigation()

            navigationDelegate.decidePolicyForNavigationResponse = { _, decisionHandler in
                decisionHandler(.cancel)
            }
            let coopURL = try #require(URL(string: "https://webkit.org/coop"))
            webView.load(URLRequest(url: coopURL))
            _ = await navigationDelegate.waitForDidFailProvisionalNavigation()
            navigationDelegate.decidePolicyForNavigationResponse = nil

            let processPool = configuration.processPool
            let closedPageOfCancelledNavigation =
                (try? await waitForCondition("the web processes to have one page") {
                    await processPool._countWebPagesInAllProcessesForTesting() == 1
                }) != nil
            #expect(closedPageOfCancelledNavigation)

            webView.load(URLRequest(url: coopURL))
            try await navigationDelegate.waitForDidFinishNavigation()
            #expect(webView.url == coopURL)
            let closedPageOfPreviousDocument =
                (try? await waitForCondition("the web processes to have one page") {
                    await processPool._countWebPagesInAllProcessesForTesting() == 1
                }) != nil
            #expect(closedPageOfPreviousDocument)
        }
    }

    private static func configuration(_ server: HTTPServer.Configuration) -> WKWebViewConfiguration {
        let configuration = WKWebViewConfiguration(httpsProxyFor: server)
        configuration.setFeature("DocumentIsolationPolicyEnabled", enabled: true)
        configuration.enableCrossOriginOpenerAndEmbedderPolicies()
        configuration.preferences.javaScriptCanOpenWindowsAutomatically = true
        return configuration
    }

    @Test
    func popupStaysReachableFromOpenersIframeAfterCancelledNavigationMovesProcess() async throws {
        var server = try HTTPServer(protocol: .httpsProxy) {
            Self.openerWithIframeRoutes
        }

        try await server.run { serverConfiguration in
            let alerts = AlertRecorder()
            let (opener, popup) = try await Self.openerWithIframeAndPopup(serverConfiguration, popupAlerts: alerts)
            defer { withExtendedLifetime(opener) {} }
            var source = try await Self.messageSourceInOpenersIframe(from: popup.webView, alerts: alerts)
            #expect(source == "has source")

            popup.navigationDelegate.decidePolicyForNavigationResponse = { _, decisionHandler in
                decisionHandler(.cancel)
            }
            popup.webView.startEvaluatingJavaScript("location.href = 'https://webkit.org/isolated'", withUserGesture: false)
            _ = await popup.navigationDelegate.waitForDidFailProvisionalNavigation()
            popup.navigationDelegate.decidePolicyForNavigationResponse = nil

            source = try await Self.messageSourceInOpenersIframe(from: popup.webView, alerts: alerts)
            #expect(source == "has source")
        }
    }

    @Test
    func popupStaysReachableFromOpenersIframeAfterNavigationMovesProcess() async throws {
        var server = try HTTPServer(protocol: .httpsProxy) {
            Self.openerWithIframeRoutes
        }

        try await server.run { serverConfiguration in
            let alerts = AlertRecorder()
            let (opener, popup) = try await Self.openerWithIframeAndPopup(serverConfiguration, popupAlerts: alerts)
            let openerFrameTrees = await opener.webView.frameTreesInProcesses()
            let iframeProcessIdentifier = try #require(openerFrameTrees.processIdentifier(hosting: "https://webkit.org"))

            popup.webView.startEvaluatingJavaScript("location.href = 'https://webkit.org/isolated'", withUserGesture: false)
            try await popup.navigationDelegate.waitForDidFinishNavigation()
            #expect(popup.webView._webProcessIdentifier != iframeProcessIdentifier)

            let source = try await Self.messageSourceInOpenersIframe(from: popup.webView, alerts: alerts)
            #expect(source == "has source")
        }
    }

    // A redirect moves the popup's navigation out of the process of the opener's iframe, where the landing page's iframe
    // then loads, so the popup only finishes loading if that process still has a page for it.
    @Test
    func popupLoadsIframeInOpenersIframeProcessAfterRedirectMovesProcess() async throws {
        var server = try HTTPServer(protocol: .httpsProxy) {
            Self.openerWithIframeRoutes
        }

        try await server.run { serverConfiguration in
            let alerts = AlertRecorder()
            let (opener, popup) = try await Self.openerWithIframeAndPopup(serverConfiguration, popupAlerts: alerts)
            defer { withExtendedLifetime(opener) {} }
            let openerFrameTrees = await opener.webView.frameTreesInProcesses()
            let iframeProcessIdentifier = try #require(openerFrameTrees.processIdentifier(hosting: "https://webkit.org"))

            popup.webView.startEvaluatingJavaScript("location.href = 'https://webkit.org/redirect'", withUserGesture: false)
            let message = try await alerts.nextAlert()
            #expect(message == "loaded")
            #expect(popup.webView._webProcessIdentifier != iframeProcessIdentifier)

            let popupFrameTrees = await popup.webView.frameTreesInProcesses()
            #expect(popupFrameTrees.processIdentifier(hosting: "https://webkit.org") == iframeProcessIdentifier)
        }
    }

    private typealias WebViewAndDelegate = (webView: TestWKWebView, navigationDelegate: TestNavigationDelegate)

    private static let documentIsolationPolicy = ["Document-Isolation-Policy": "isolate-and-require-corp"]

    private static let openerWithIframeRoutes = Route("") {
        Route("/opener") {
            """
            <iframe src='https://webkit.org/frame'></iframe>
            <script>onmessage = (event) => popup.postMessage(event.data, '*')</script>
            """
        }

        Route("/frame") {
            "<script>onmessage = (event) => parent.postMessage(event.source ? 'has source' : 'no source', '*')</script>"
        }

        Route("/popup") {
            "<script>onmessage = (event) => alert(event.data)</script>"
        }

        Route("/isolated", headerFields: documentIsolationPolicy) {
            "<script>onmessage = (event) => alert(event.data)</script>"
        }

        Route("/redirect", statusCode: 302, headerFields: ["Location": "https://apple.com/landing"]) {
            ""
        }

        Route("/landing") {
            """
            <iframe src='https://webkit.org/frame'></iframe>
            <script>onload = () => alert('loaded')</script>
            """
        }
    }

    private static func openerWithIframeAndPopup(
        _ server: HTTPServer.Configuration,
        popupAlerts: AlertRecorder? = nil
    ) async throws -> (opener: WebViewAndDelegate, popup: WebViewAndDelegate) {
        let opener = siteIsolatedWebViewAndDelegate(configuration: configuration(server))
        let openedWebViews = OpenedWebViewRecorder()
        opener.webView.uiDelegate = openedWebViews

        opener.webView.load(URLRequest(url: try #require(URL(string: "https://example.com/opener"))))
        try await opener.navigationDelegate.waitForDidFinishNavigation()
        opener.webView.startEvaluatingJavaScript("window.popup = open('/popup')", withUserGesture: false)
        let popup = try await openedWebViews.nextOpenedWebView()
        popup.webView.uiDelegate = popupAlerts

        let frameTrees = await opener.webView.frameTreesInProcesses()
        #expect(
            frameTrees == [
                .local("https://example.com", children: [.remote]),
                .remote(children: [.local("https://webkit.org")]),
            ]
        )
        return (opener, popup)
    }

    private static func messageSourceInOpenersIframe(from popup: WKWebView, alerts: AlertRecorder) async throws -> String {
        popup.startEvaluatingJavaScript("opener.frames[0].postMessage('ping', '*')")
        return try await alerts.nextAlert()
    }
}
