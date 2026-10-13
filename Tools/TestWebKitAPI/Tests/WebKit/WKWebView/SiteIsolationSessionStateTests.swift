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
private import TestWebKitAPILibrary.Helpers.cocoa.TestNavigationDelegate
import Testing
import WebKit
private import WebKit_Private.WKWebViewPrivate
private import WebKit_Private._WKSessionState

import struct Foundation.URL
import struct Swift.String

// Hiding a web view saves the form state of its frames, so that the session state taken afterwards restores it. With
// site isolation, the state of a cross-site iframe is in the iframe's process rather than in the main frame's, so that
// process has to save it too.

@MainActor
struct SiteIsolationSessionStateTests {
    // A web view only holds its delegates weakly, so the test holds them for as long as it runs.
    private let navigationDelegate = TestNavigationDelegate()
    private let alerts = AlertRecorder()

    init() {
        navigationDelegate.allowAnyTLSCertificate()
    }

    @Test(.timeLimit(.minutes(1)))
    func crossSiteIframeFormStateSavedWhenViewIsHiddenIsRestoredFromSessionState() async throws {
        var server = try HTTPServer(protocol: .httpsProxy) {
            Route("/example") {
                "<iframe src='https://webkit.org/source'></iframe>"
            }

            Route("/source") {
                "<script>alert('source')</script>"
            }

            Route("/destination", headerFields: ["Content-Type": "text/html"]) {
                """
                <input id='input'>
                <script>alert('destination:' + input.value)</script>
                """
            }
        }

        try await server.run { serverConfiguration in
            let (webView, _) = siteIsolatedWebViewAndDelegate(configuration: .init(httpsProxyFor: serverConfiguration))
            webView.navigationDelegate = navigationDelegate
            webView.uiDelegate = alerts
            #if WTF_PLATFORM_MAC
            webView._windowOcclusionDetectionEnabled = false
            #endif

            webView.load(URLRequest(url: try #require(URL(string: "https://example.com/example"))))
            try await navigationDelegate.waitForDidFinishNavigation()
            let sourceAlert = try await alerts.nextAlert()
            #expect(sourceAlert == "source")

            // Navigate the iframe so that the current back/forward item comes from the iframe's process. Navigate from a
            // timeout, so the navigation doesn't interrupt the reply to this call.
            let sourceFrame = try #require(await webView.firstChildFrame)
            try await webView.callJavaScript(in: sourceFrame) {
                "setTimeout(() => { location.href = 'https://apple.com/destination' }, 0)"
            }
            let destinationAlert = try await alerts.nextAlert()
            #expect(destinationAlert == "destination:")

            let childFrame = try #require(await webView.firstChildFrame)
            let childVisibilityState = {
                try await webView.callJavaScript(returning: String.self, in: childFrame) { "return document.visibilityState" }
            }
            try await waitForCondition("the iframe to finish loading") {
                try await webView.callJavaScript(returning: String.self, in: childFrame) { "return document.readyState" } == "complete"
            }
            #expect(try await childVisibilityState() == "visible")
            try await webView.callJavaScript(in: childFrame) { "input.value = 'typed'" }

            webView.removeFromTestWindow()
            try await waitForCondition("the iframe to be hidden") {
                try await childVisibilityState() == "hidden"
            }

            let sessionState = try #require(webView._sessionState)
            let (restoredWebView, _) = siteIsolatedWebViewAndDelegate(configuration: .init(httpsProxyFor: serverConfiguration))
            restoredWebView.navigationDelegate = navigationDelegate
            restoredWebView.uiDelegate = alerts
            restoredWebView._restore(sessionState, andNavigate: true)

            let restoredAlert = try await alerts.nextAlert()
            #expect(restoredAlert == "destination:typed")
        }
    }

    @Test(.timeLimit(.minutes(1)))
    func crossSiteIframeThatHasNotNavigatedFormStateSavedWhenViewIsHiddenIsRestoredFromSessionState() async throws {
        var server = try HTTPServer(protocol: .httpsProxy) {
            Route("/example") {
                "<iframe src='https://webkit.org/iframe'></iframe>"
            }

            Route("/iframe", headerFields: ["Content-Type": "text/html"]) {
                """
                <input id='input'>
                <script>alert('iframe:' + input.value)</script>
                """
            }
        }

        try await server.run { serverConfiguration in
            let (webView, _) = siteIsolatedWebViewAndDelegate(configuration: .init(httpsProxyFor: serverConfiguration))
            webView.navigationDelegate = navigationDelegate
            webView.uiDelegate = alerts
            #if WTF_PLATFORM_MAC
            webView._windowOcclusionDetectionEnabled = false
            #endif

            webView.load(URLRequest(url: try #require(URL(string: "https://example.com/example"))))
            try await navigationDelegate.waitForDidFinishNavigation()
            let iframeAlert = try await alerts.nextAlert()
            #expect(iframeAlert == "iframe:")

            let childFrame = try #require(await webView.firstChildFrame)
            let childVisibilityState = {
                try await webView.callJavaScript(returning: String.self, in: childFrame) { "return document.visibilityState" }
            }
            try await waitForCondition("the iframe to finish loading") {
                try await webView.callJavaScript(returning: String.self, in: childFrame) { "return document.readyState" } == "complete"
            }
            #expect(try await childVisibilityState() == "visible")
            try await webView.callJavaScript(in: childFrame) { "input.value = 'typed'" }

            webView.removeFromTestWindow()
            try await waitForCondition("the iframe to be hidden") {
                try await childVisibilityState() == "hidden"
            }

            let sessionState = try #require(webView._sessionState)
            let (restoredWebView, _) = siteIsolatedWebViewAndDelegate(configuration: .init(httpsProxyFor: serverConfiguration))
            restoredWebView.navigationDelegate = navigationDelegate
            restoredWebView.uiDelegate = alerts
            restoredWebView._restore(sessionState, andNavigate: true)

            let restoredAlert = try await alerts.nextAlert()
            #expect(restoredAlert == "iframe:typed")
        }
    }
}
