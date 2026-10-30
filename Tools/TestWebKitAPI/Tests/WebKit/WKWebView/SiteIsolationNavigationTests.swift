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
private import WebKit_Private._WKFeature
private import WebKit_Private._WKFrameTreeNode
private import WebKit_Private._WKProcessPoolConfiguration

import struct Foundation.URL
import struct Swift.String

// With site isolation, a navigation that changes the site of a frame moves the frame to another process. These tests
// check which processes host each frame after such navigations.

@MainActor
struct SiteIsolationNavigationTests {
    @Test
    func grandchildIframeSameOriginAsGrandparent() async throws {
        var server = try HTTPServer(protocol: .httpsProxy) {
            Route("/example") {
                "<iframe id='webkit_frame' src='https://webkit.org/webkit'></iframe>"
            }

            Route("/webkit") {
                "<iframe src='https://example.com/example_grandchild'></iframe>"
            }

            Route("/example_grandchild") {
                "<script>alert('grandchild loaded successfully')</script>"
            }
        }

        try await server.run { serverConfiguration in
            let (webView, navigationDelegate) = siteIsolatedWebViewAndDelegate(configuration: .init(httpsProxyFor: serverConfiguration))
            let alerts = AlertRecorder()
            webView.uiDelegate = alerts

            webView.load(URLRequest(url: try #require(URL(string: "https://example.com/example"))))
            try await navigationDelegate.waitForDidFinishNavigation()
            let alert = try await alerts.nextAlert()
            #expect(alert == "grandchild loaded successfully")

            #expect(
                await webView.frameTreesInProcesses() == [
                    .local("https://example.com", children: [.remote(children: [.local("https://example.com")])]),
                    .remote(children: [.local("https://webkit.org", children: [.remote])]),
                ]
            )
        }
    }

    @Test
    func parentNavigatingCrossOriginIframeToSameOrigin() async throws {
        var server = try HTTPServer(protocol: .httpsProxy) {
            Route("/example") {
                """
                <iframe id='webkit_frame' src='https://webkit.org/webkit'></iframe>
                <script>onload = () => { document.getElementById('webkit_frame').src = 'https://example.com/example_subframe' }</script>
                """
            }

            Route("/example_subframe") {
                "<script>onload = () => { alert('done') }</script>"
            }

            Route("/webkit") {
                "hi"
            }
        }

        try await server.run { serverConfiguration in
            let (webView, navigationDelegate) = siteIsolatedWebViewAndDelegate(configuration: .init(httpsProxyFor: serverConfiguration))
            let alerts = AlertRecorder()
            webView.uiDelegate = alerts

            webView.load(URLRequest(url: try #require(URL(string: "https://example.com/example"))))
            try await navigationDelegate.waitForDidFinishNavigation()
            let alert = try await alerts.nextAlert()
            #expect(alert == "done")

            let mainFrame = try #require(await webView._frames())
            let childFrame = try #require(mainFrame.childFrames.first)
            #expect(mainFrame.info._processIdentifier != 0)
            #expect(childFrame.info._processIdentifier == mainFrame.info._processIdentifier)
            #expect(mainFrame.info.securityOrigin.host == "example.com")
            #expect(childFrame.info.securityOrigin.host == "example.com")

            #expect(
                await webView.frameTreesInProcesses() == [
                    .local("https://example.com", children: [.local("https://example.com")])
                ]
            )
        }
    }

    @Test
    func shutDownFrameProcessesAfterNavigation() async throws {
        var server = try HTTPServer(protocol: .httpsProxy) {
            Route("/example") {
                "<iframe src='https://webkit.org/webkit'></iframe>"
            }

            Route("/webkit") {
                "hello"
            }

            Route("/apple") {
                "hello"
            }
        }

        try await server.run { serverConfiguration in
            // With the back/forward cache, the iframe's process would stay alive to host the iframe in the cached page.
            let processPoolConfiguration = _WKProcessPoolConfiguration()
            processPoolConfiguration.pageCacheEnabled = false
            let configuration = WKWebViewConfiguration(httpsProxyFor: serverConfiguration)
            configuration.processPool = try #require(WKProcessPool._processPool(with: processPoolConfiguration))
            let (webView, navigationDelegate) = siteIsolatedWebViewAndDelegate(configuration: configuration)

            webView.load(URLRequest(url: try #require(URL(string: "https://example.com/example"))))
            try await navigationDelegate.waitForDidFinishNavigation()

            let frameTrees = await webView.frameTreesInProcesses()
            #expect(
                frameTrees == [
                    .local("https://example.com", children: [.remote]),
                    .remote(children: [.local("https://webkit.org")]),
                ]
            )
            let iframeProcessIdentifier = try #require(frameTrees.processIdentifier(hosting: "https://webkit.org"))

            webView.load(URLRequest(url: try #require(URL(string: "https://apple.com/apple"))))
            try await navigationDelegate.waitForDidFinishNavigation()
            #expect(await webView.frameTreesInProcesses() == [.local("https://apple.com")])

            try await waitForCondition("the iframe's process to exit") {
                !isProcessRunning(iframeProcessIdentifier)
            }
        }
    }

    // The document the main frame leaves, in the back/forward cache, keeps its process for documents with a policy.
    @Test
    func childWithDocumentIsolationPolicyAfterMainFrameLeftProcessForPolicy() async throws {
        let documentIsolationPolicy = Self.documentIsolationPolicy
        var server = try HTTPServer(protocol: .httpsProxy) {
            Route("/isolated", headerFields: documentIsolationPolicy) {
                "isolated"
            }

            Route("/main") {
                "<iframe src='https://example.com/isolated_frame'></iframe>"
            }

            Route("/isolated_frame", headerFields: documentIsolationPolicy) {
                "<script>alert('loaded')</script>"
            }
        }

        try await server.run { serverConfiguration in
            let (webView, navigationDelegate) = siteIsolatedWebViewAndDelegate(
                configuration: Self.configurationWithDocumentIsolationPolicy(serverConfiguration)
            )
            let alerts = AlertRecorder()
            webView.uiDelegate = alerts

            webView.load(URLRequest(url: try #require(URL(string: "https://example.com/isolated"))))
            try await navigationDelegate.waitForDidFinishNavigation()
            let isolatedProcessIdentifier = webView._webProcessIdentifier

            // The next navigation is to this process's site, so it stays here until its response arrives.
            webView.load(URLRequest(url: try #require(URL(string: "about:blank"))))
            try await navigationDelegate.waitForDidFinishNavigation()

            webView.load(URLRequest(url: try #require(URL(string: "https://example.com/main"))))
            try await navigationDelegate.waitForDidFinishNavigation()
            let alert = try await alerts.nextAlert()
            #expect(alert == "loaded")

            let mainFrame = try #require(await webView._frames())
            let childFrame = try #require(mainFrame.childFrames.first)
            #expect(mainFrame.info._processIdentifier != isolatedProcessIdentifier)
            #expect(childFrame.info._processIdentifier != isolatedProcessIdentifier)
            #expect(childFrame.info._processIdentifier != mainFrame.info._processIdentifier)
        }
    }

    // Clearing the back/forward cache leaves the remote pages of the document that replaced the cached one in its group.
    @Test
    func clearingBackForwardCacheKeepsFramesOfDocumentInSameGroup() async throws {
        var server = try HTTPServer(protocol: .httpsProxy) {
            Route("/isolated", headerFields: Self.documentIsolationPolicy) {
                "isolated"
            }

            Route("/main") {
                "<iframe src='https://webkit.org/frame'></iframe>"
            }

            Route("/frame") {
                "<script>alert('loaded')</script>"
            }
        }

        try await server.run { serverConfiguration in
            let (webView, navigationDelegate) = siteIsolatedWebViewAndDelegate(
                configuration: Self.configurationWithDocumentIsolationPolicy(serverConfiguration)
            )
            let alerts = AlertRecorder()
            webView.uiDelegate = alerts

            webView.load(URLRequest(url: try #require(URL(string: "https://example.com/isolated"))))
            try await navigationDelegate.waitForDidFinishNavigation()

            // The same-site navigation stays in the group, and only moves processes when its response arrives.
            webView.load(URLRequest(url: try #require(URL(string: "https://example.com/main"))))
            try await navigationDelegate.waitForDidFinishNavigation()
            let alert = try await alerts.nextAlert()
            #expect(alert == "loaded")

            webView._clearBackForwardCache()

            let mainFrame = try #require(await webView._frames())
            let childFrame = try #require(mainFrame.childFrames.first)
            let host = try await webView.__evaluateJavaScript("location.host", inFrame: childFrame.info, in: .page)
            #expect(host as? String == "webkit.org")
        }
    }

    // A document that the page left for one in the same group, and then for another site's document in another group,
    // goes back into the first group from the back/forward cache. Unless a popup is in that group, which could have
    // frames of the same sites in other processes.
    @Test(arguments: [false, true])
    func goingBackAcrossBrowsingContextGroupChange(openingPopup: Bool) async throws {
        var server = try HTTPServer(protocol: .httpsProxy) {
            Route("/main") {
                "<script>window.token = crypto.randomUUID()</script>"
            }

            Route("/isolated", headerFields: Self.documentIsolationPolicy) {
                "isolated"
            }

            Route("/other_site") {
                "other site"
            }
        }

        try await server.run { serverConfiguration in
            let configuration = Self.configurationWithDocumentIsolationPolicy(serverConfiguration)
            configuration.preferences.javaScriptCanOpenWindowsAutomatically = true
            let (webView, navigationDelegate) = siteIsolatedWebViewAndDelegate(configuration: configuration)
            let popups = PopupOpener()
            webView.uiDelegate = popups

            webView.load(URLRequest(url: try #require(URL(string: "https://example.com/main"))))
            try await navigationDelegate.waitForDidFinishNavigation()
            let token = try #require(await webView.evaluateJavaScript("token") as? String)

            webView.load(URLRequest(url: try #require(URL(string: "https://example.com/isolated"))))
            try await navigationDelegate.waitForDidFinishNavigation()
            if openingPopup {
                _ = try await webView.evaluateJavaScript("open(); true")
                try await waitForCondition("the popup to open") {
                    !popups.webViews.isEmpty
                }
            }

            // Loads the client starts on another site get a new group.
            webView.load(URLRequest(url: try #require(URL(string: "https://webkit.org/other_site"))))
            try await navigationDelegate.waitForDidFinishNavigation()

            webView.go(to: try #require(webView.backForwardList.item(at: -2)))
            try await navigationDelegate.waitForDidFinishNavigation()
            let isSameDocument = try await webView.evaluateJavaScript("token") as? String == token
            #expect(isSameDocument == !openingPopup)
        }
    }

    // When the next document's main frame starts in the process of a frame of the cached document, it takes the WebPage
    // that frame is in, so going back loads the cached document again.
    @Test
    func goingBackToDocumentWithFrameInProcessOfNextDocument() async throws {
        var server = try HTTPServer(protocol: .httpsProxy) {
            Route("/main") {
                "<script>window.token = crypto.randomUUID()</script>"
            }

            Route("/isolated_frame", headerFields: Self.documentIsolationPolicy) {
                "<script>alert('loaded')</script>"
            }

            Route("/isolated", headerFields: Self.documentIsolationPolicy) {
                "isolated"
            }
        }

        try await server.run { serverConfiguration in
            let (webView, navigationDelegate) = siteIsolatedWebViewAndDelegate(
                configuration: Self.configurationWithDocumentIsolationPolicy(serverConfiguration)
            )
            let alerts = AlertRecorder()
            webView.uiDelegate = alerts
            // Added by script, as a frame that loads from its history entry can't move to another process when its response
            // arrives yet.
            let appendChildFrame = "document.body.appendChild(document.createElement('iframe')).src = '/isolated_frame'; true"

            webView.load(URLRequest(url: try #require(URL(string: "https://example.com/main"))))
            try await navigationDelegate.waitForDidFinishNavigation()
            let token = try #require(await webView.evaluateJavaScript("token") as? String)
            _ = try await webView.evaluateJavaScript(appendChildFrame)
            var alert = try await alerts.nextAlert()
            #expect(alert == "loaded")
            let mainFrame = try #require(await webView._frames())
            let childFrameProcessIdentifier = try #require(mainFrame.childFrames.first).info._processIdentifier

            // The response moves the main frame to the process for documents with a policy, which is the child frame's.
            webView.load(URLRequest(url: try #require(URL(string: "https://example.com/isolated"))))
            try await navigationDelegate.waitForDidFinishNavigation()
            #expect(webView._webProcessIdentifier == childFrameProcessIdentifier)

            webView.goBack()
            try await navigationDelegate.waitForDidFinishNavigation()
            let isSameDocument = try await webView.evaluateJavaScript("token") as? String == token
            #expect(!isSameDocument)
            _ = try await webView.evaluateJavaScript(appendChildFrame)
            alert = try await alerts.nextAlert()
            #expect(alert == "loaded")
        }
    }

    private static let documentIsolationPolicy = ["Document-Isolation-Policy": "isolate-and-require-corp"]

    private static func configurationWithDocumentIsolationPolicy(_ server: HTTPServer.Configuration) -> WKWebViewConfiguration {
        let configuration = WKWebViewConfiguration(httpsProxyFor: server)
        let features = ["DocumentIsolationPolicyEnabled", "MultiProcessBackForwardCacheEnabled"]
        for feature in WKPreferences._features() where features.contains(feature.key) {
            configuration.preferences._setEnabled(true, for: feature)
        }
        return configuration
    }

    #if ENABLE_SWIFTUI

    @Test
    func childNavigatingToNewDomain() async throws {
        var server = try HTTPServer(protocol: .httpsProxy) {
            Route("/example") {
                "<iframe id='webkit_frame' src='https://webkit.org/webkit'></iframe>"
            }

            Route("/example_subframe") {
                "<script>alert('done')</script>"
            }

            Route("/webkit") {
                "<script>window.location='https://foo.com/example_subframe'</script>"
            }
        }

        try await server.run { serverConfiguration in
            var configuration = WebPage.Configuration(httpsProxyFor: serverConfiguration)
            configuration.siteIsolationEnabled = true

            let alerts = AlertRecorder()
            let page = WebPage(
                configuration: configuration,
                navigationDecider: NavigationDeciderAllowingAnyTLSCertificate(),
                dialogPresenter: alerts
            )

            try await page.load(URL(string: "https://example.com/example")).wait()
            let alert = try await alerts.nextAlert()
            #expect(alert == "done")

            let mainFrame = try #require(await page.mainFrame)
            let childFrame = try #require(mainFrame.childFrames.first)
            #expect(mainFrame.info._processIdentifier != 0)
            #expect(childFrame.info._processIdentifier != 0)
            #expect(childFrame.info._processIdentifier != mainFrame.info._processIdentifier)
            #expect(mainFrame.info.securityOrigin.host == "example.com")
            #expect(childFrame.info.securityOrigin.host == "foo.com")

            #expect(
                await page.frameTreesInProcesses() == [
                    .local("https://example.com", children: [.remote]),
                    .remote(children: [.local("https://foo.com")]),
                ]
            )
        }
    }

    #endif // ENABLE_SWIFTUI
}

// Opens popups in web views that it keeps alive.
@MainActor
private final class PopupOpener: NSObject, WKUIDelegate {
    private(set) var webViews: [WKWebView] = []

    func webView(
        _ webView: WKWebView,
        createWebViewWith configuration: WKWebViewConfiguration,
        for navigationAction: WKNavigationAction,
        windowFeatures: WKWindowFeatures
    ) -> WKWebView? {
        let popup = WKWebView(frame: .zero, configuration: configuration)
        webViews.append(popup)
        return popup
    }
}
