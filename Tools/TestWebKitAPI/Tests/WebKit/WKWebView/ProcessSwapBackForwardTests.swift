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
private import WebKit_Private.WKNavigationDelegatePrivate
private import WebKit_Private.WKProcessPoolPrivate
private import WebKit_Private.WKWebViewPrivate
private import WebKit_Private.WKWebpagePreferencesPrivate

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
        var server = try HTTPServer(protocol: .httpsProxy) {
            Route("/shared") {
                Self.pageshowScript + "shared"
            }

            Route("/isolated", headerFields: Self.crossOriginIsolation) {
                Self.pageshowScript + "isolated"
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

    // Going back in Lockdown Mode doesn't restore a page into its process without Lockdown Mode, even when the process
    // it leaves has Lockdown Mode.
    @Test(arguments: [false, true])
    func navigateBackInLockdownModeAvoidsProcessWithoutLockdownMode(acrossCrossOriginIsolation: Bool) async throws {
        var server = try HTTPServer(protocol: .httpsProxy) {
            Route("/shared") {
                "shared"
            }

            Route("/other") {
                "other"
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
            #expect(await webView._isJITEnabled())
            try await webView.callJavaScript { "window.marker = 'shared'" }

            navigationDelegate.decidePolicyForNavigationActionWithPreferences = { _, preferences, decisionHandler in
                preferences.isLockdownModeEnabled = true
                decisionHandler(.allow, preferences)
            }
            let path = acrossCrossOriginIsolation ? "isolated" : "other"
            webView.load(URLRequest(url: try #require(URL(string: "https://example.com/\(path)"))))
            try await navigationDelegate.waitForDidFinishNavigation()
            #expect(webView._webProcessIdentifier != sharedProcessIdentifier)
            #expect(await !webView._isJITEnabled())

            webView.goBack()
            try await navigationDelegate.waitForDidFinishNavigation()

            #expect(webView._webProcessIdentifier != sharedProcessIdentifier)
            #expect(webView._webProcessIdentifier != 0)
            #expect(await !webView._isJITEnabled())
            #expect(try await webView.callJavaScript(returning: String.self) { "return typeof window.marker" } == "undefined")
            #expect(try await webView.callJavaScript(returning: String.self) { "return document.body.textContent" } == "shared")
        }
    }

    // A client redirect evicts the page in the in-process back/forward cache without telling the UI process, and the
    // process can then enter the process cache. Going back to that item must not use the cached process.
    @Test
    func inProcessBackForwardCacheProcessInWebProcessCache() async throws {
        var server = try HTTPServer(protocol: .https) {
            Route("/1.html") {
                "one"
            }

            Route("/2.html") {
                "two"
            }

            Route("/other.html") {
                "three"
            }
        }

        try await server.run { serverConfiguration in
            let processPool = WKProcessPool.processSwapping()
            let configuration = WKWebViewConfiguration()
            configuration.processPool = processPool
            let (webView, navigationDelegate) = siteIsolatedWebViewAndDelegate(
                configuration: configuration,
                frame: CGRect(x: 0, y: 0, width: 800, height: 600)
            )
            let firstURL = serverConfiguration.address.appending(path: "1.html")

            webView.load(URLRequest(url: firstURL))
            try await navigationDelegate.waitForDidFinishNavigation()
            let firstProcessIdentifier = webView._webProcessIdentifier

            // A same-process navigation, which puts /1.html in the in-process back/forward cache.
            webView.load(URLRequest(url: serverConfiguration.address.appending(path: "2.html")))
            try await navigationDelegate.waitForDidFinishNavigation()
            #expect(webView._webProcessIdentifier == firstProcessIdentifier)

            // A cross-site client redirect, which doesn't suspend /2.html, so its page in the first process closes.
            let otherURL = serverConfiguration.localhostAddress.appending(path: "other.html")
            webView.startEvaluatingJavaScript("location.replace('\(otherURL.absoluteString)')", withUserGesture: false)
            try await navigationDelegate.waitForDidFinishNavigation()
            #expect(webView._webProcessIdentifier != firstProcessIdentifier)

            // The first process no longer hosts a page, so it should enter the process cache.
            try? await waitForCondition("the first process to enter the process cache", timeout: .seconds(10)) {
                processPool._processCacheSize() > 0
            }
            #expect(processPool._processCacheSize() == 1)

            webView.goBack()
            try await navigationDelegate.waitForDidFinishNavigation()
            #expect(webView._committedURL?.absoluteString == firstURL.absoluteString)
        }
    }

    @Test(arguments: [false, true], [false, true])
    func navigateBackAndForwardAskingForNewProcessUsesBackForwardCache(siteIsolationEnabled: Bool, crossSite: Bool) async throws {
        var server = try Self.serverWithPagesAAndB()

        try await server.run { serverConfiguration in
            let (webView, navigationDelegate) = Self.processSwappingWebViewAndDelegate(
                server: serverConfiguration,
                processPool: .processSwapping(),
                siteIsolationEnabled: siteIsolationEnabled
            )

            webView.load(URLRequest(url: try #require(URL(string: "https://example.com/a"))))
            try await navigationDelegate.waitForDidFinishNavigation()
            let firstProcessIdentifier = webView._webProcessIdentifier
            try await webView.callJavaScript { "window.marker = 'a'" }

            let secondHost = crossSite ? "apple.com" : "example.com"
            webView.load(URLRequest(url: try #require(URL(string: "https://\(secondHost)/b"))))
            try await navigationDelegate.waitForDidFinishNavigation()
            let secondProcessIdentifier = webView._webProcessIdentifier
            #expect((secondProcessIdentifier != firstProcessIdentifier) == crossSite)
            try await webView.callJavaScript { "window.marker = 'b'" }

            let backForwardNavigationCount = Self.askForNewProcess(for: navigationDelegate)
            webView.goBack()
            try await navigationDelegate.waitForDidFinishNavigation()
            #expect(backForwardNavigationCount() == 1)
            #expect(webView._webProcessIdentifier == firstProcessIdentifier)
            let documentAfterGoingBack = try await Self.markerAndPersisted(in: webView)
            #expect(documentAfterGoingBack == "a true")

            webView.goForward()
            try await navigationDelegate.waitForDidFinishNavigation()
            #expect(backForwardNavigationCount() == 2)
            #expect(webView._webProcessIdentifier == secondProcessIdentifier)
            let documentAfterGoingForward = try await Self.markerAndPersisted(in: webView)
            #expect(documentAfterGoingForward == "b true")
        }
    }

    @Test(arguments: [false, true])
    func navigateBackAskingForNewProcessWithoutBackForwardCacheStaysInProcess(siteIsolationEnabled: Bool) async throws {
        var server = try Self.serverWithPagesAAndB()

        try await server.run { serverConfiguration in
            let (webView, navigationDelegate) = Self.processSwappingWebViewAndDelegate(
                server: serverConfiguration,
                processPool: .processSwapping(backForwardCacheEnabled: false),
                siteIsolationEnabled: siteIsolationEnabled
            )

            webView.load(URLRequest(url: try #require(URL(string: "https://example.com/a"))))
            try await navigationDelegate.waitForDidFinishNavigation()
            let processIdentifier = webView._webProcessIdentifier
            try await webView.callJavaScript { "window.marker = 'a'" }

            webView.load(URLRequest(url: try #require(URL(string: "https://example.com/b"))))
            try await navigationDelegate.waitForDidFinishNavigation()
            #expect(webView._webProcessIdentifier == processIdentifier)

            let backForwardNavigationCount = Self.askForNewProcess(for: navigationDelegate)
            webView.goBack()
            try await navigationDelegate.waitForDidFinishNavigation()
            #expect(backForwardNavigationCount() == 1)
            #expect(webView._committedURL?.absoluteString == "https://example.com/a")
            #expect(webView._webProcessIdentifier == processIdentifier)
            #expect(try await webView.callJavaScript(returning: String.self) { "return typeof window.marker" } == "undefined")
        }
    }

    @Test(arguments: [false, true])
    func navigateBackAcrossSitesAskingForNewProcessWithoutBackForwardCacheReturnsToItsProcess(siteIsolationEnabled: Bool) async throws {
        var server = try Self.serverWithPagesAAndB()

        try await server.run { serverConfiguration in
            let (webView, navigationDelegate) = Self.processSwappingWebViewAndDelegate(
                server: serverConfiguration,
                processPool: .processSwapping(backForwardCacheEnabled: false),
                siteIsolationEnabled: siteIsolationEnabled
            )
            let openedWebViews = OpenedWebViewRecorder(siteIsolationEnabled: siteIsolationEnabled)
            webView.uiDelegate = openedWebViews

            webView.load(URLRequest(url: try #require(URL(string: "https://example.com/a"))))
            try await navigationDelegate.waitForDidFinishNavigation()
            let processIdentifier = webView._webProcessIdentifier
            try await webView.callJavaScript { "window.open('https://example.com/b'); return undefined" }
            let popup = try await openedWebViews.nextOpenedWebView()
            #expect(popup.webView._webProcessIdentifier == processIdentifier)

            webView.load(URLRequest(url: try #require(URL(string: "https://apple.com/b"))))
            try await navigationDelegate.waitForDidFinishNavigation()
            #expect(webView._webProcessIdentifier != processIdentifier)

            let backForwardNavigationCount = Self.askForNewProcess(for: navigationDelegate)
            webView.goBack()
            try await navigationDelegate.waitForDidFinishNavigation()
            #expect(backForwardNavigationCount() == 1)
            #expect(webView._committedURL?.absoluteString == "https://example.com/a")
            #expect(webView._webProcessIdentifier == processIdentifier)
            withExtendedLifetime(popup) {}
        }
    }

    @Test(arguments: [false, true])
    func navigateBackToOtherWebsiteDataStoreAskingForNewProcessUsesNewProcess(siteIsolationEnabled: Bool) async throws {
        var server = try Self.serverWithPagesAAndB()

        try await server.run { serverConfiguration in
            let (webView, navigationDelegate) = Self.processSwappingWebViewAndDelegate(
                server: serverConfiguration,
                processPool: .processSwapping(backForwardCacheEnabled: false),
                siteIsolationEnabled: siteIsolationEnabled
            )

            webView.load(URLRequest(url: try #require(URL(string: "https://example.com/a"))))
            try await navigationDelegate.waitForDidFinishNavigation()
            let processIdentifier = webView._webProcessIdentifier

            webView.load(URLRequest(url: try #require(URL(string: "https://example.com/b"))))
            try await navigationDelegate.waitForDidFinishNavigation()
            #expect(webView._webProcessIdentifier == processIdentifier)

            let otherWebsiteDataStore = WKWebsiteDataStore.nonPersistent()
            navigationDelegate.decidePolicyForNavigationActionWithPreferences = { navigationAction, preferences, decisionHandler in
                if navigationAction.navigationType == .backForward {
                    preferences._websiteDataStore = otherWebsiteDataStore
                }
                decisionHandler(_WKNavigationActionPolicyAllowInNewProcess, preferences)
            }
            webView.goBack()
            try await navigationDelegate.waitForDidFinishNavigation()
            #expect(webView._committedURL?.absoluteString == "https://example.com/a")
            #expect(webView._webProcessIdentifier != processIdentifier)
            #expect(webView._webProcessIdentifier != 0)
        }
    }

    @Test(arguments: [false, true])
    func sameSiteNavigationAskingForNewProcessUsesNewProcess(siteIsolationEnabled: Bool) async throws {
        var server = try Self.serverWithPagesAAndB()

        try await server.run { serverConfiguration in
            let (webView, navigationDelegate) = Self.processSwappingWebViewAndDelegate(
                server: serverConfiguration,
                processPool: .processSwapping(),
                siteIsolationEnabled: siteIsolationEnabled
            )

            webView.load(URLRequest(url: try #require(URL(string: "https://example.com/a"))))
            try await navigationDelegate.waitForDidFinishNavigation()
            let firstProcessIdentifier = webView._webProcessIdentifier

            let backForwardNavigationCount = Self.askForNewProcess(for: navigationDelegate)
            webView.load(URLRequest(url: try #require(URL(string: "https://example.com/b"))))
            try await navigationDelegate.waitForDidFinishNavigation()
            #expect(backForwardNavigationCount() == 0)
            #expect(webView._webProcessIdentifier != firstProcessIdentifier)
            #expect(webView._webProcessIdentifier != 0)
        }
    }

    private static let pageshowScript = "<script>addEventListener('pageshow', event => { window.persisted = event.persisted })</script>"

    private static func serverWithPagesAAndB() throws -> HTTPServer {
        try HTTPServer(protocol: .httpsProxy) {
            Route("/a") {
                Self.pageshowScript + "a"
            }

            Route("/b") {
                Self.pageshowScript + "b"
            }
        }
    }

    private static func processSwappingWebViewAndDelegate(
        server: HTTPServer.Configuration,
        processPool: WKProcessPool,
        siteIsolationEnabled: Bool
    ) -> (webView: TestWKWebView, navigationDelegate: TestNavigationDelegate) {
        let configuration = WKWebViewConfiguration(httpsProxyFor: server)
        configuration.processPool = processPool
        return webViewAndDelegate(
            configuration: configuration,
            frame: CGRect(x: 0, y: 0, width: 800, height: 600),
            siteIsolationEnabled: siteIsolationEnabled
        )
    }

    private static func askForNewProcess(for navigationDelegate: TestNavigationDelegate) -> () -> Int {
        var backForwardNavigationCount = 0
        navigationDelegate.decidePolicyForNavigationAction = { navigationAction, decisionHandler in
            if navigationAction.navigationType == .backForward {
                backForwardNavigationCount += 1
            }
            decisionHandler(_WKNavigationActionPolicyAllowInNewProcess)
        }
        return { backForwardNavigationCount }
    }

    private static func markerAndPersisted(in webView: WKWebView) async throws -> String {
        try await webView.callJavaScript(returning: String.self) { "return `${window.marker} ${window.persisted}`" }
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
