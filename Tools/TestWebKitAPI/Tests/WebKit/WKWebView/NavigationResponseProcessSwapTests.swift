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
private import WebKit_Private.WKProcessPoolPrivate
private import WebKit_Private.WKWebViewConfigurationPrivate
private import WebKit_Private.WKWebViewPrivate
private import WebKit_Private.WKWebsiteDataStorePrivate
private import WebKit_Private._WKProcessPoolConfiguration

import struct Foundation.URL
import struct Swift.String

@MainActor
struct NavigationResponseProcessSwapTests {
    @Test(arguments: [false, true])
    func swapToProcessWithResponseInMemoryCache(siteIsolationEnabled: Bool) async throws {
        var server = try Self.makeServer()

        try await server.run { serverConfiguration in
            let configuration = try Self.makeConfiguration(httpsProxyFor: serverConfiguration)
            let processPool = configuration.processPool
            let keepAliveWebView = try await Self.makeWebViewKeepingDataStoreInUse(
                configuration: configuration,
                siteIsolationEnabled: siteIsolationEnabled
            )

            let (firstWebView, firstNavigationDelegate) = webViewAndDelegate(
                configuration: configuration,
                siteIsolationEnabled: siteIsolationEnabled
            )
            let processIdentifiers = try await Self.loadSourceAndCOOP(in: firstWebView, navigationDelegate: firstNavigationDelegate)

            firstWebView._close()
            try await waitForCondition("the processes of the closed web view to enter the process cache") {
                processPool._processCacheSize() == 2
            }

            let (webView, navigationDelegate) = webViewAndDelegate(configuration: configuration, siteIsolationEnabled: siteIsolationEnabled)
            webView.load(URLRequest(url: try #require(URL(string: "https://example.com/source"))))
            try await navigationDelegate.waitForDidFinishNavigation()
            #expect(webView._webProcessIdentifier == processIdentifiers.source)

            webView.load(URLRequest(url: try #require(URL(string: "https://example.com/coop"))))
            try await navigationDelegate.waitForDidFinishNavigation()
            #expect(webView._webProcessIdentifier == processIdentifiers.coop)
            let parkedLoadCount = await configuration.websiteDataStore._loadersAwaitingWebProcessTransferCountForTesting()
            #expect(parkedLoadCount == 0)

            withExtendedLifetime(keepAliveWebView) {}
        }
    }

    @Test(arguments: [false, true])
    func backForwardSwapToProcessWithResponseInMemoryCache(siteIsolationEnabled: Bool) async throws {
        var server = try Self.makeServer()

        try await server.run { serverConfiguration in
            let configuration = try Self.makeConfiguration(httpsProxyFor: serverConfiguration)
            let processPool = configuration.processPool
            let keepAliveWebView = try await Self.makeWebViewKeepingDataStoreInUse(
                configuration: configuration,
                siteIsolationEnabled: siteIsolationEnabled
            )

            let (firstWebView, firstNavigationDelegate) = webViewAndDelegate(
                configuration: configuration,
                siteIsolationEnabled: siteIsolationEnabled
            )
            let processIdentifiers = try await Self.loadSourceAndCOOP(in: firstWebView, navigationDelegate: firstNavigationDelegate)

            firstWebView.load(URLRequest(url: try #require(URL(string: "https://apple.com/other"))))
            try await firstNavigationDelegate.waitForDidFinishNavigation()
            let interactionState = firstWebView.interactionState

            firstWebView._close()
            try await waitForCondition("the processes of the closed web view to enter the process cache or exit") {
                processPool._webProcessCountIgnoringPrewarmedAndCached() == 1
            }

            let (webView, navigationDelegate) = webViewAndDelegate(configuration: configuration, siteIsolationEnabled: siteIsolationEnabled)
            webView.interactionState = interactionState
            try await navigationDelegate.waitForDidFinishNavigation()

            webView.goBack()
            try await navigationDelegate.waitForDidFinishNavigation()
            #expect(webView.url == URL(string: "https://example.com/coop"))
            #expect(webView._webProcessIdentifier == processIdentifiers.coop)
            let parkedLoadCount = await configuration.websiteDataStore._loadersAwaitingWebProcessTransferCountForTesting()
            #expect(parkedLoadCount == 0)

            withExtendedLifetime(keepAliveWebView) {}
        }
    }

    private static func makeServer() throws -> HTTPServer {
        try HTTPServer(protocol: .httpsProxy) {
            Route("/source") {
                "source"
            }

            Route("/coop", headerFields: ["Cross-Origin-Opener-Policy": "same-origin", "Cache-Control": "max-age=3600"]) {
                "coop"
            }

            Route("/other") {
                "other"
            }
        }
    }

    private static func makeConfiguration(httpsProxyFor serverConfiguration: HTTPServer.Configuration) throws -> WKWebViewConfiguration {
        let processPoolConfiguration = _WKProcessPoolConfiguration()
        processPoolConfiguration.usesWebProcessCache = true
        let configuration = WKWebViewConfiguration(httpsProxyFor: serverConfiguration)
        configuration.processPool = try #require(WKProcessPool._processPool(with: processPoolConfiguration))
        // Without Site Isolation, iOS launches a process when a web view is created by default, and the first load of the
        // web view stays in that process instead of taking one from the process cache.
        configuration._delaysWebProcessLaunchUntilFirstLoad = true
        return configuration
    }

    private static func makeWebViewKeepingDataStoreInUse(
        configuration: WKWebViewConfiguration,
        siteIsolationEnabled: Bool
    ) async throws -> WKWebView {
        let (webView, navigationDelegate) = webViewAndDelegate(configuration: configuration, siteIsolationEnabled: siteIsolationEnabled)
        webView.load(URLRequest(url: try #require(URL(string: "https://webkit.org/other"))))
        try await navigationDelegate.waitForDidFinishNavigation()
        return webView
    }

    private static func loadSourceAndCOOP(
        in webView: WKWebView,
        navigationDelegate: TestNavigationDelegate
    ) async throws -> (source: pid_t, coop: pid_t) {
        webView.load(URLRequest(url: try #require(URL(string: "https://example.com/source"))))
        try await navigationDelegate.waitForDidFinishNavigation()
        let sourceProcessIdentifier = webView._webProcessIdentifier

        webView.load(URLRequest(url: try #require(URL(string: "https://example.com/coop"))))
        try await navigationDelegate.waitForDidFinishNavigation()
        let coopProcessIdentifier = webView._webProcessIdentifier
        #expect(coopProcessIdentifier != sourceProcessIdentifier)

        return (sourceProcessIdentifier, coopProcessIdentifier)
    }
}
