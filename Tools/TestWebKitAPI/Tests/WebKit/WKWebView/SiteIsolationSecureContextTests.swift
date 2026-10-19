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
private import TestWebKitAPILibrary.Helpers.cocoa.TestNavigationDelegate
private import TestWebKitAPILibrary.Helpers.cocoa.TestWKWebView
import Testing
import WebKit
private import WebKit_Private.WKWebsiteDataStorePrivate
private import WebKit_Private._WKWebsiteDataStoreConfiguration

import struct Foundation.URL
import struct Swift.String

// A web process reports whether a document it commits is a secure context, and the UI process syncs that to the
// frame's children in other processes, which decide their own state from it. The UI process computes the state as
// well, and has to agree with the web process.

extension WKWebViewConfiguration {
    /// Creates a configuration whose website data store sends HTTP loads through one server and HTTPS loads through
    /// another, so a test can use non-secure origins other than localhost, which is potentially trustworthy.
    fileprivate convenience init(
        httpProxyFor plaintextServer: HTTPServer.Configuration,
        httpsProxyFor secureServer: HTTPServer.Configuration
    ) {
        self.init()

        let storeConfiguration = _WKWebsiteDataStoreConfiguration(nonPersistentConfiguration: ())
        storeConfiguration.httpProxy = plaintextServer.address
        storeConfiguration.httpsProxy = secureServer.httpsProxy
        websiteDataStore = WKWebsiteDataStore._store(with: storeConfiguration)
    }
}

/// A UI delegate that records the message of each JavaScript alert, and opens each popup in a site-isolated web view.
@MainActor
private final class PopupOpeningAlertRecorder: NSObject, WKUIDelegate {
    private var messages: [String] = []
    private var popups: [(webView: TestWKWebView, navigationDelegate: TestNavigationDelegate)] = []

    func nextAlert() async throws -> String {
        try await waitForCondition("a JavaScript alert", timeout: .seconds(10)) {
            !messages.isEmpty
        }
        return messages.removeFirst()
    }

    func webView(
        _ webView: WKWebView,
        runJavaScriptAlertPanelWithMessage message: String,
        initiatedByFrame frame: WKFrameInfo
    ) async {
        messages.append(message)
    }

    func webView(
        _ webView: WKWebView,
        createWebViewWith configuration: WKWebViewConfiguration,
        for navigationAction: WKNavigationAction,
        windowFeatures: WKWindowFeatures
    ) -> WKWebView? {
        // A web view only holds its navigation delegate weakly, so keep both alive until the end of the test.
        let popup = siteIsolatedWebViewAndDelegate(configuration: configuration)
        popups.append(popup)
        return popup.webView
    }
}

private let frameReportingSecureContext = "<script>(top.opener || top).postMessage(isSecureContext, '*')</script>"
private let parentOfFrame = "<script>onmessage = event => alert(event.data)</script><iframe src='https://b.com/frame'></iframe>"
private let parentOfDataFrame =
    "<script>onmessage = event => alert(event.data)</script><iframe src=\"data:text/html,<iframe src='https://b.com/frame'></iframe>\"></iframe>"
private let openerOfPopupWithFrame =
    "<script>onmessage = event => alert(event.data); onload = () => open('about:blank').document.write(\"<iframe src='https://b.com/frame'></iframe>\")</script>"

@MainActor
struct SiteIsolationSecureContextTests {
    // The web process treats the scheme of a loadHTMLString() base URL as potentially trustworthy, and the UI process,
    // which decides whether the frame is origin-keyed, has to agree. That lasts for the rest of the UI process, so the
    // scheme is named after this test.
    // FIXME: Use site isolation once a document loaded with such a base URL loads under it.
    @Test
    func frameUnderLoadDataBaseURLWithCustomSchemeHonorsOriginAgentCluster() async throws {
        var server = try HTTPServer(protocol: .httpsProxy) {
            Route("/frame", headerFields: ["Content-Type": "text/html", "Origin-Agent-Cluster": "?1"]) {
                "<script>alert(`${originAgentCluster} ${isSecureContext}`)</script>"
            }
        }

        try await server.run { serverConfiguration in
            let (webView, navigationDelegate) = webViewAndDelegate(
                configuration: .init(httpsProxyFor: serverConfiguration),
                siteIsolationEnabled: false
            )
            defer { withExtendedLifetime(navigationDelegate) {} }
            let alerts = AlertRecorder()
            webView.uiDelegate = alerts

            webView.loadHTMLString(
                "<iframe src='https://example.com/frame'></iframe>",
                baseURL: try #require(URL(string: "frame-under-load-data-base-url-with-custom-scheme://example.org/"))
            )
            #expect(try await alerts.nextAlert() == "true true")
        }
    }

    // The frame's process learns from the UI process whether its parent is a secure context.
    @Test(arguments: ["http://a.com/", "http://a.com/data", "https://a.com/", "https://a.com/data"])
    func frameInAnotherProcess(url: String) async throws {
        var secureServer = try HTTPServer(protocol: .httpsProxy) {
            Route("/") { parentOfFrame }
            Route("/data") { parentOfDataFrame }
            Route("/frame") { frameReportingSecureContext }
        }
        var plaintextServer = try HTTPServer(protocol: .http) {
            Route("http://a.com/") { parentOfFrame }
            Route("http://a.com/data") { parentOfDataFrame }
        }

        try await secureServer.run { secureConfiguration in
            try await plaintextServer.run { plaintextConfiguration in
                let (webView, navigationDelegate) = siteIsolatedWebViewAndDelegate(
                    configuration: .init(httpProxyFor: plaintextConfiguration, httpsProxyFor: secureConfiguration)
                )
                defer { withExtendedLifetime(navigationDelegate) {} }
                let alerts = AlertRecorder()
                webView.uiDelegate = alerts

                webView.load(URLRequest(url: try #require(URL(string: url))))
                #expect(try await alerts.nextAlert() == (url.hasPrefix("https:") ? "true" : "false"))
            }
        }
    }

    // A popup's initial about:blank document is as secure as its opener.
    @Test(arguments: ["http://a.com/popup", "https://a.com/popup"])
    func frameInPopupInAnotherProcess(url: String) async throws {
        var secureServer = try HTTPServer(protocol: .httpsProxy) {
            Route("/popup") { openerOfPopupWithFrame }
            Route("/frame") { frameReportingSecureContext }
        }
        var plaintextServer = try HTTPServer(protocol: .http) {
            Route("http://a.com/popup") { openerOfPopupWithFrame }
        }

        try await secureServer.run { secureConfiguration in
            try await plaintextServer.run { plaintextConfiguration in
                let configuration = WKWebViewConfiguration(httpProxyFor: plaintextConfiguration, httpsProxyFor: secureConfiguration)
                configuration.preferences.javaScriptCanOpenWindowsAutomatically = true
                let (webView, navigationDelegate) = siteIsolatedWebViewAndDelegate(configuration: configuration)
                defer { withExtendedLifetime(navigationDelegate) {} }
                let uiDelegate = PopupOpeningAlertRecorder()
                webView.uiDelegate = uiDelegate

                webView.load(URLRequest(url: try #require(URL(string: url))))
                #expect(try await uiDelegate.nextAlert() == (url.hasPrefix("https:") ? "true" : "false"))
            }
        }
    }
}
