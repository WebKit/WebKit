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
private import WebKit_Private.WKPreferencesPrivate
import WebKit_Private.WKWebViewPrivate
private import WebKit_Private.WKWebViewPrivateForTesting
private import WebKit_Private.WKWebsiteDataStorePrivate
private import WebKit_Private._WKFeature
private import WebKit_Private._WKWebsiteDataStoreConfiguration

import struct Foundation.URL
import struct Foundation.URLRequest
import struct Swift.String

#if ENABLE_SWIFTUI

@_spi(Testing) @_spi(CrossImportOverlay) import WebKit

// MARK: Supporting test types

@MainActor
private class TestNavigationDecider: WebPage.NavigationDeciding {
    var preferencesMutation: (inout WebPage.NavigationPreferences) -> Void = { _ in }

    func decidePolicy(
        for action: WebPage.NavigationAction,
        preferences: inout WebPage.NavigationPreferences
    ) async -> WKNavigationActionPolicy {
        preferencesMutation(&preferences)
        return .allow
    }
}

// MARK: Tests

@MainActor
struct EnhancedSecurityTests {
    @Test(arguments: [
        (mode: WebPage.NavigationPreferences.SecurityRestrictionMode.maximizeCompatibility, expected: true),
        (mode: WebPage.NavigationPreferences.SecurityRestrictionMode.none, expected: false),
    ])
    func securityRestrictionModeFromConfiguration(
        mode: WebPage.NavigationPreferences.SecurityRestrictionMode,
        expected: Bool
    ) async throws {
        var configuration = WebPage.Configuration()
        configuration.defaultNavigationPreferences.securityRestrictionMode = mode

        let page = WebPage(configuration: configuration)
        try await page.load(html: "<body></body>").wait()

        #expect(page.backingWebView._webProcessIdentifier != 0)

        let result = await page.backingWebView._isEnhancedSecurityEnabled()
        #expect(result == expected)
    }

    @Test(arguments: [true, false])
    func securityRestrictionModeFromNavigationDecider(enabled: Bool) async throws {
        let decider = TestNavigationDecider()
        decider.preferencesMutation = { preferences in
            preferences.securityRestrictionMode = enabled ? .maximizeCompatibility : .none
        }

        let page = WebPage(navigationDecider: decider)
        try await page.load(html: "<body></body>").wait()

        #expect(page.backingWebView._webProcessIdentifier != 0)

        let result = await page.backingWebView._isEnhancedSecurityEnabled()
        #expect(result == enabled)
    }
}

#endif

@MainActor
private final class PopupRecorder: NSObject, WKUIDelegate {
    private(set) var popup: WKWebView?

    func webView(
        _ webView: WKWebView,
        createWebViewWith configuration: WKWebViewConfiguration,
        for navigationAction: WKNavigationAction,
        windowFeatures: WKWindowFeatures
    ) -> WKWebView? {
        let popup = WKWebView(frame: .zero, configuration: configuration)
        self.popup = popup
        return popup
    }
}

@MainActor
struct EnhancedSecurityProcessSwapTests {
    @Test
    func declinedSwapDropsLoadCancelledByItsProcess() async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("http://parked-load.internal/popup") { "popup" }
            Route("http://parked-load.internal/ping") { "pong" }
            // Enough of the body for content sniffing, which holds back the response until it has 512 bytes.
            Route(unfinished: "http://parked-load.internal/insecure", headerFields: ["Content-Type": "text/html"]) {
                String(repeating: " ", count: 512)
            }
        }

        try await server.run { serverConfiguration in
            let storeConfiguration = _WKWebsiteDataStoreConfiguration(nonPersistentConfiguration: ())
            storeConfiguration.httpProxy = serverConfiguration.address
            let configuration = WKWebViewConfiguration()
            configuration.websiteDataStore = WKWebsiteDataStore._store(with: storeConfiguration)
            configuration.preferences.javaScriptCanOpenWindowsAutomatically = true
            for feature in WKPreferences._features() {
                if feature.key == "EnhancedSecurityHeuristicsEnabled" {
                    configuration.preferences._setEnabled(true, for: feature)
                }
                if feature.key == "SiteIsolationSharedProcessEnabled" {
                    configuration.preferences._setEnabled(false, for: feature)
                }
            }
            let (webView, navigationDelegate) = siteIsolatedWebViewAndDelegate(configuration: configuration)
            let popupRecorder = PopupRecorder()
            defer { withExtendedLifetime(popupRecorder) {} }
            webView.uiDelegate = popupRecorder

            let baseURL = try #require(URL(string: "http://parked-load.internal/"))
            webView.loadHTMLString("<script>open('/popup')</script>", baseURL: baseURL)
            try await navigationDelegate.waitForDidFinishNavigation()
            try await waitForCondition("the popup to load") {
                guard let popup = popupRecorder.popup else {
                    return false
                }
                return popup.url?.path == "/popup" && !popup.isLoading
            }
            #expect(popupRecorder.popup?._webProcessIdentifier == webView._webProcessIdentifier)

            var swapsProcess: Bool?
            var sendReply: (() -> Void)?
            defer {
                sendReply?()
                webView._setNavigationResponseProcessSwapReplyHandler(forTesting: nil)
            }
            webView._setNavigationResponseProcessSwapReplyHandler(forTesting: { swaps, reply in
                swapsProcess = swaps
                sendReply = reply
            })

            webView.load(URLRequest(url: try #require(URL(string: "http://parked-load.internal/insecure"))))
            try await waitForCondition("the UI process to decide whether to swap processes") { sendReply != nil }
            #expect(swapsProcess == false)
            #expect(webView._provisionalWebProcessIdentifier == 0)

            webView.stopLoading()
            try await webView.callJavaScript { "await fetch('/ping')" }

            sendReply?()
            sendReply = nil

            try await waitForCondition("the network process to cancel the load") {
                serverConfiguration.closedUnfinishedResponseCount == 1
            }
        }
    }
}
