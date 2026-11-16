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
private import WebKit_Private.WKFrameInfoPrivate
private import WebKit_Private.WKProcessPoolPrivate
private import WebKit_Private.WKWebViewPrivate

import struct Foundation.URL
import struct Swift.String

// A response for about:blank has the origin of the navigation's initiator, and the opener policy of the initiator's
// top-level document if the initiator is same origin with that document, or "unsafe-none" otherwise
// (https://html.spec.whatwg.org/multipage/browsing-the-web.html#beginning-navigation, https://github.com/whatwg/html/issues/6913).
// So when a cross-origin iframe navigates its top-level document to about:blank, a document with the opener policy
// "same-origin" is replaced by one that needs a new browsing context group, which leaves the windows of the old group.
// That includes a popup's initial about:blank document, which inherited the opener policy of its opener
// (https://html.spec.whatwg.org/multipage/browsers.html#creating-a-new-browsing-context).
// https://html.spec.whatwg.org/multipage/origin.html#check-browsing-context-group-switch-coop-value
//
// The network process enforces the opener policy of other responses, but there is no response from it for about:blank.

@MainActor
struct CrossOriginOpenerPolicyAboutBlankTests {
    // A popup whose initial about:blank document inherited the opener policy "same-origin" from its opener leaves its
    // opener's browsing context group.
    // https://html.spec.whatwg.org/multipage/origin.html#check-browsing-context-group-switch-coop-value-popup
    @Test(arguments: [true, false])
    func crossOriginIframeNavigatingInitialAboutBlankPopupToAboutBlankSeversOpener(siteIsolationEnabled: Bool) async throws {
        var server = try HTTPServer(protocol: .httpsProxy) {
            Self.routes()
        }

        try await server.run { serverConfiguration in
            let (webView, navigationDelegate) = Self.makeWebView(server: serverConfiguration, siteIsolationEnabled: siteIsolationEnabled)
            let openedWebViews = OpenedWebViewRecorder(siteIsolationEnabled: siteIsolationEnabled)
            webView.uiDelegate = openedWebViews

            webView.load(URLRequest(url: try #require(URL(string: "https://example.com/coop"))))
            try await navigationDelegate.waitForDidFinishNavigation()
            let popup = try await Self.openInitialAboutBlankPopup(from: webView, openedWebViews: openedWebViews)
            let initiator = try await Self.addCrossOriginIframe(toPopupOf: webView, popup: popup.webView)
            let processesOfOldGroup: Set = [webView._webProcessIdentifier, initiator._processIdentifier]

            popup.webView.startEvaluatingJavaScript("top.location = 'about:blank'", in: initiator)
            try await popup.navigationDelegate.waitForDidFinishNavigation()

            #expect(popup.webView.url?.absoluteString == "about:blank")
            #expect(try await Self.openerIsNull(in: popup.webView))
            #expect(try await Self.popupIsClosed(in: webView))
            Self.expectInProcessOfNewGroup(
                popup.webView,
                processesOfOldGroup: processesOfOldGroup,
                siteIsolationEnabled: siteIsolationEnabled
            )
        }
    }

    // The same navigation of a popup whose initial about:blank document inherited "unsafe-none" from its opener needs no
    // new browsing context group: the popup keeps its opener. With site isolation, the about:blank document goes to
    // the process of the iframe that started the navigation.
    @Test(arguments: [true, false])
    func crossOriginIframeNavigatingInitialAboutBlankPopupWithoutCOOPToAboutBlankKeepsOpener(siteIsolationEnabled: Bool) async throws {
        var server = try HTTPServer(protocol: .httpsProxy) {
            Self.routes()
        }

        try await server.run { serverConfiguration in
            let (webView, navigationDelegate) = Self.makeWebView(server: serverConfiguration, siteIsolationEnabled: siteIsolationEnabled)
            let openedWebViews = OpenedWebViewRecorder(siteIsolationEnabled: siteIsolationEnabled)
            webView.uiDelegate = openedWebViews

            webView.load(URLRequest(url: try #require(URL(string: "https://example.com/no-coop"))))
            try await navigationDelegate.waitForDidFinishNavigation()
            let popup = try await Self.openInitialAboutBlankPopup(from: webView, openedWebViews: openedWebViews)
            let initiator = try await Self.addCrossOriginIframe(toPopupOf: webView, popup: popup.webView)
            #expect(siteIsolationEnabled == (initiator._processIdentifier != webView._webProcessIdentifier))

            popup.webView.startEvaluatingJavaScript("top.location = 'about:blank'", in: initiator)
            try await popup.navigationDelegate.waitForDidFinishNavigation()

            #expect(popup.webView.url?.absoluteString == "about:blank")
            #expect(try await !Self.openerIsNull(in: popup.webView))
            // FIXME: Also check that the popup isn't closed for its opener. With site isolation, the UI process gives the
            // about:blank document the origin of its opener while its process gives it an opaque origin, so the opener
            // accessing the popup hits an assertion in DOMWindow::crossDomainAccessErrorMessage().
            #expect(popup.webView._webProcessIdentifier == initiator._processIdentifier)
        }
    }

    // A same-origin initiator gives the about:blank document the popup's origin and opener policy, so the popup keeps
    // its opener.
    @Test(arguments: [true, false])
    func sameOriginInitiatorNavigatingInitialAboutBlankPopupToAboutBlankKeepsOpener(siteIsolationEnabled: Bool) async throws {
        var server = try HTTPServer(protocol: .httpsProxy) {
            Self.routes()
        }

        try await server.run { serverConfiguration in
            let (webView, navigationDelegate) = Self.makeWebView(server: serverConfiguration, siteIsolationEnabled: siteIsolationEnabled)
            let openedWebViews = OpenedWebViewRecorder(siteIsolationEnabled: siteIsolationEnabled)
            webView.uiDelegate = openedWebViews

            webView.load(URLRequest(url: try #require(URL(string: "https://example.com/coop"))))
            try await navigationDelegate.waitForDidFinishNavigation()
            let popup = try await Self.openInitialAboutBlankPopup(from: webView, openedWebViews: openedWebViews)

            webView.startEvaluatingJavaScript("window.popup.location = 'about:blank'")
            try await popup.navigationDelegate.waitForDidFinishNavigation()

            #expect(popup.webView.url?.absoluteString == "about:blank")
            #expect(try await !Self.openerIsNull(in: popup.webView))
            #expect(try await !Self.popupIsClosed(in: webView))
        }
    }

    // A popup whose committed document has the opener policy "same-origin" leaves its opener's browsing context group.
    @Test(arguments: [true, false])
    func crossOriginIframeNavigatingCOOPPopupToAboutBlankSeversOpener(siteIsolationEnabled: Bool) async throws {
        var server = try HTTPServer(protocol: .httpsProxy) {
            Self.routes()
        }

        try await server.run { serverConfiguration in
            let (webView, navigationDelegate) = Self.makeWebView(server: serverConfiguration, siteIsolationEnabled: siteIsolationEnabled)
            let openedWebViews = OpenedWebViewRecorder(siteIsolationEnabled: siteIsolationEnabled)
            webView.uiDelegate = openedWebViews

            webView.load(URLRequest(url: try #require(URL(string: "https://example.com/coop"))))
            try await navigationDelegate.waitForDidFinishNavigation()
            // The popup is same origin with its opener and has the same opener policy, so it stays in its opener's group.
            try await webView.callJavaScript { "window.popup = window.open('/coop-with-iframe'); return undefined" }
            let popup = try await openedWebViews.nextOpenedWebView()
            #expect(try await !Self.openerIsNull(in: popup.webView))
            let initiator = try await Self.iframe(in: popup.webView, withHost: "webkit.org")
            let processesOfOldGroup: Set = [
                webView._webProcessIdentifier,
                popup.webView._webProcessIdentifier,
                initiator._processIdentifier,
            ]

            popup.webView.startEvaluatingJavaScript("top.location = 'about:blank'", in: initiator)
            try await popup.navigationDelegate.waitForDidFinishNavigation()

            #expect(popup.webView.url?.absoluteString == "about:blank")
            #expect(try await Self.openerIsNull(in: popup.webView))
            #expect(try await Self.popupIsClosed(in: webView))
            Self.expectInProcessOfNewGroup(
                popup.webView,
                processesOfOldGroup: processesOfOldGroup,
                siteIsolationEnabled: siteIsolationEnabled
            )
        }
    }

    // A page whose committed document has the opener policy "same-origin" leaves the browsing context group of the
    // window it opened, which loses its opener.
    @Test(arguments: [true, false])
    func crossOriginIframeNavigatingCOOPPageToAboutBlankSeversItsPopup(siteIsolationEnabled: Bool) async throws {
        var server = try HTTPServer(protocol: .httpsProxy) {
            Self.routes()
        }

        try await server.run { serverConfiguration in
            let (webView, navigationDelegate) = Self.makeWebView(server: serverConfiguration, siteIsolationEnabled: siteIsolationEnabled)
            let openedWebViews = OpenedWebViewRecorder(siteIsolationEnabled: siteIsolationEnabled)
            webView.uiDelegate = openedWebViews

            webView.load(URLRequest(url: try #require(URL(string: "https://example.com/coop-with-iframe"))))
            try await navigationDelegate.waitForDidFinishNavigation()
            try await webView.callJavaScript { "window.popup = window.open('/coop'); return undefined" }
            let popup = try await openedWebViews.nextOpenedWebView()
            #expect(try await !Self.openerIsNull(in: popup.webView))
            let initiator = try await Self.iframe(in: webView, withHost: "webkit.org")
            let processesOfOldGroup: Set = [
                webView._webProcessIdentifier,
                popup.webView._webProcessIdentifier,
                initiator._processIdentifier,
            ]

            webView.startEvaluatingJavaScript("top.location = 'about:blank'", in: initiator)
            try await navigationDelegate.waitForDidFinishNavigation()

            #expect(webView.url?.absoluteString == "about:blank")
            #expect(try await Self.openerIsNull(in: popup.webView))
            Self.expectInProcessOfNewGroup(webView, processesOfOldGroup: processesOfOldGroup, siteIsolationEnabled: siteIsolationEnabled)
        }
    }

    // MARK: Helpers

    private static let popupName = "about-blank-popup"

    @RouteBuilder
    private static func routes() -> Route {
        Route("/coop", headerFields: ["Content-Type": "text/html", "Cross-Origin-Opener-Policy": "same-origin"]) {
            "<title>coop</title>"
        }
        Route("/coop-with-iframe", headerFields: ["Content-Type": "text/html", "Cross-Origin-Opener-Policy": "same-origin"]) {
            "<iframe src='https://webkit.org/initiator'></iframe>"
        }
        Route("/no-coop", headerFields: ["Content-Type": "text/html"]) {
            "<title>no-coop</title>"
        }
        Route("/initiator", headerFields: ["Content-Type": "text/html"]) {
            "<title>initiator</title>"
        }
    }

    private static func makeWebView(
        server: HTTPServer.Configuration,
        siteIsolationEnabled: Bool
    ) -> (webView: TestWKWebView, navigationDelegate: TestNavigationDelegate) {
        let configuration = WKWebViewConfiguration(httpsProxyFor: server)
        configuration.processPool = .processSwapping()
        configuration.enableCrossOriginOpenerAndEmbedderPolicies()
        return webViewAndDelegate(configuration: configuration, siteIsolationEnabled: siteIsolationEnabled)
    }

    // window.open('') navigates nowhere, so the popup stays on its initial about:blank document, which inherited the
    // origin and opener policy of its opener.
    private static func openInitialAboutBlankPopup(
        from webView: WKWebView,
        openedWebViews: OpenedWebViewRecorder
    ) async throws -> (webView: TestWKWebView, navigationDelegate: TestNavigationDelegate) {
        try await webView.callJavaScript { "window.popup = window.open('', '\(popupName)'); return undefined" }
        let popup = try await openedWebViews.nextOpenedWebViewWithoutNavigation()
        #expect(try await !openerIsNull(in: popup.webView))
        return popup
    }

    // The opener is same origin with the initial about:blank document of its popup, so it can add a cross-origin iframe
    // to it.
    private static func addCrossOriginIframe(toPopupOf webView: WKWebView, popup: WKWebView) async throws -> WKFrameInfo {
        try await webView.callJavaScript {
            """
            let iframe = window.popup.document.createElement('iframe');
            iframe.src = 'https://webkit.org/initiator';
            (window.popup.document.body || window.popup.document.documentElement).appendChild(iframe);
            return undefined
            """
        }
        return try await iframe(in: popup, withHost: "webkit.org")
    }

    // Waits for the iframe of the page to load its document from a host, and returns the frame.
    private static func iframe(in webView: WKWebView, withHost host: String) async throws -> WKFrameInfo {
        var iframe: WKFrameInfo?
        try await waitForCondition("the iframe to load", timeout: .seconds(10)) {
            iframe = await webView._frames()?.childFrames.first?.info
            return iframe?.securityOrigin.host == host
        }
        return try #require(iframe)
    }

    // With site isolation, the about:blank document goes to a process of its new browsing context group. Without site
    // isolation, the web process emulates the switch in the process of the document it replaces.
    private static func expectInProcessOfNewGroup(
        _ webView: WKWebView,
        processesOfOldGroup: Set<pid_t>,
        siteIsolationEnabled: Bool,
        sourceLocation: SourceLocation = #_sourceLocation
    ) {
        #expect(processesOfOldGroup.contains(webView._webProcessIdentifier) == !siteIsolationEnabled, sourceLocation: sourceLocation)
    }

    private static func openerIsNull(in webView: WKWebView) async throws -> Bool {
        try await webView.callJavaScript(returning: Bool.self) { "return window.opener === null" }
    }

    private static func popupIsClosed(in webView: WKWebView) async throws -> Bool {
        try await webView.callJavaScript(returning: Bool.self) { "return window.popup.closed" }
    }
}
