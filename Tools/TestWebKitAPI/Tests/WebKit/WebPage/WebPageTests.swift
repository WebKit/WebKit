// Copyright (C) 2024 Apple Inc. All rights reserved.
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

#if ENABLE_SWIFTUI

import SwiftUI
import Observation
import Security
import Testing
@_spi(CrossImportOverlay) @_spi(Private) @_spi(Testing) import WebKit
import WebKit_Private.WKWebViewPrivate
@_spi(Private) import _WebKit_SwiftUI
private import TestWebKitAPILibrary
#if ENABLE_CXX_INTEROP
private import WebKit_Private._WKWebsiteDataStoreConfiguration
private import WebKit_Private.WKWebsiteDataStorePrivate
#endif

// MARK: Supporting test types

@MainActor
private class TestNavigationDecider: WebPage.NavigationDeciding {
    init() {
        (self.navigationActionStream, self.navigationActionContinuation) = AsyncStream.makeStream(of: WebPage.NavigationAction.self)
        (self.navigationResponseStream, self.navigationResponseContinuation) = AsyncStream.makeStream(of: WebPage.NavigationResponse.self)
    }

    let navigationActionStream: AsyncStream<WebPage.NavigationAction>
    private let navigationActionContinuation: AsyncStream<WebPage.NavigationAction>.Continuation

    let navigationResponseStream: AsyncStream<WebPage.NavigationResponse>
    private let navigationResponseContinuation: AsyncStream<WebPage.NavigationResponse>.Continuation

    var preferencesMutation: (inout WebPage.NavigationPreferences) -> Void = { _ in }

    func decidePolicy(
        for action: WebPage.NavigationAction,
        preferences: inout WebPage.NavigationPreferences
    ) async -> WKNavigationActionPolicy {
        preferencesMutation(&preferences)

        navigationActionContinuation.yield(action)
        return .allow
    }

    func decidePolicy(for response: WebPage.NavigationResponse) async -> WKNavigationResponsePolicy {
        navigationResponseContinuation.yield(response)
        return .allow
    }
}

#if ENABLE_CXX_INTEROP && compiler(>=6.4) && !SWIFT_WEBKIT_TOOLCHAIN
@MainActor
private struct TrustingNavigationDecider: WebPage.NavigationDeciding {
    mutating func decideAuthenticationChallengeDisposition(
        for challenge: URLAuthenticationChallenge
    ) async -> (URLSession.AuthChallengeDisposition, URLCredential?) {
        (.useCredential, challenge.protectionSpace.serverTrust.map(URLCredential.init(trust:)))
    }
}

extension WebPage.Configuration {
    fileprivate init(_ serverConfiguration: HTTPServer.Configuration, qualifiedServerTrustDebugEnabled: Bool = false) {
        self.init()

        let storeConfiguration = _WKWebsiteDataStoreConfiguration(nonPersistentConfiguration: ())
        storeConfiguration.httpsProxy = serverConfiguration.httpsProxy
        storeConfiguration.qualifiedServerTrustDebugEnabledForTesting = qualifiedServerTrustDebugEnabled
        self.websiteDataStore = WKWebsiteDataStore._store(with: storeConfiguration)
    }
}
#endif // ENABLE_CXX_INTEROP && compiler(>=6.4) && !SWIFT_WEBKIT_TOOLCHAIN

// MARK: Tests

@MainActor
struct WebPageTests {
    @Test
    func observableProperties() async throws {
        let page = WebPage()

        #expect(page.url == nil)
        #expect(page.title == "")
        #expect(!page.isLoading)
        #expect(page.estimatedProgress == 0.0)
        #expect(page.serverTrust == nil)
        #expect(page.qualifiedServerTrust == nil)
        #expect(!page.hasOnlySecureContent)
        #if WTF_PLATFORM_MAC || WTF_PLATFORM_IOS
        #expect(page.themeColor == nil)
        #endif

        // FIXME: (283456) Make this test more comprehensive once Observation supports observing a stream of changes to properties.
    }

    #if ENABLE_CXX_INTEROP && compiler(>=6.4) && !SWIFT_WEBKIT_TOOLCHAIN
    @Test
    func qualifiedServerTrust() async throws {
        var server = HTTPServer(protocol: .httpsProxy) {
            Route("/binding-link", headerFields: ["Link": "<https://webkit.org/2qwac>; rel=\"tls-certificate-binding\""]) {
                "hi"
            }

            Route("/2qwac") {
                "This is where the 2QWAC bytes will go."
            }

            Route("/no-link") {
                "hi"
            }
        }

        try await server.run { serverConfiguration in
            let configuration = WebPage.Configuration(serverConfiguration, qualifiedServerTrustDebugEnabled: true)
            let page = WebPage(configuration: configuration, navigationDecider: TrustingNavigationDecider())

            #expect(page.qualifiedServerTrust == nil)

            let changes = Observations { page.qualifiedServerTrust != nil }

            let bindingLinkURL = try #require(URL(string: "https://webkit.org/binding-link"))
            try await page.load(bindingLinkURL).wait()

            // The 2-QWAC is fetched after the navigation commits, so waiting for the navigation to finish
            // is not enough; this waits for the property to be observed changing.
            _ = try await #require(changes.first { @Sendable in $0 })

            let qualifiedServerTrust = try #require(page.qualifiedServerTrust)
            let serverTrust = try #require(page.serverTrust)

            // With debugging enabled the 2-QWAC is the TLS trust of the 2-QWAC fetch, which was served by
            // the same identity as the page itself.
            let qualifiedServerTrustKey = try #require(SecTrustCopyKey(qualifiedServerTrust))
            let serverKey = try #require(SecTrustCopyKey(serverTrust))
            let qualifiedServerTrustKeyData = try #require(unsafe SecKeyCopyExternalRepresentation(qualifiedServerTrustKey, nil))
            let serverKeyData = try #require(unsafe SecKeyCopyExternalRepresentation(serverKey, nil))
            #expect(CFEqual(qualifiedServerTrustKeyData, serverKeyData))

            // Committing a response without a tls-certificate-binding link clears the 2-QWAC.
            let noLinkURL = try #require(URL(string: "https://webkit.org/no-link"))
            try await page.load(noLinkURL).wait()

            #expect(page.qualifiedServerTrust == nil)
        }
    }
    #endif // ENABLE_CXX_INTEROP && compiler(>=6.4) && !SWIFT_WEBKIT_TOOLCHAIN

    @Test
    func decidePolicyForNavigationActionFragment() async throws {
        let decider = TestNavigationDecider()
        let page = WebPage(navigationDecider: decider)

        let html = "<script>window.location.href='#fragment';</script>"

        let baseURL = URL(string: "http://webkit.org")!
        page.load(html: html, baseURL: baseURL)

        let actions = await Array(decider.navigationActionStream.prefix(2))

        #expect(actions[0].request.url!.absoluteString == "http://webkit.org/")
        #expect(actions[1].request.url!.absoluteString == "http://webkit.org/#fragment")
    }

    @Test(arguments: [true, false])
    func globalPrivacyControlEnabledForNavigation(enabled: Bool) async throws {
        let decider = TestNavigationDecider()
        decider.preferencesMutation = { preferences in
            preferences.isGlobalPrivacyControlEnabled = enabled
        }

        let page = WebPage(navigationDecider: decider)
        try await page.load(html: "<body></body>").wait()

        let result = try await page.callJavaScript(returning: Bool.self) {
            """
            return navigator.globalPrivacyControl;
            """
        }
        #expect(result == enabled)
    }

    @Test(arguments: [true, false])
    func allowsJSHandleCreationInPageWorld(enabled: Bool) async throws {
        let decider = TestNavigationDecider()
        decider.preferencesMutation = { preferences in
            preferences.allowsJSHandleCreationInPageWorld = enabled
        }
        let page = WebPage(navigationDecider: decider)
        try await page.load(html: "hi", baseURL: URL(string: "http://webkit.org")!).wait()

        let result = try await page.callJavaScript(returning: Bool.self) {
            """
            return !!window.webkit && !!window.webkit.createJSHandle;
            """
        }
        #expect(result == enabled)
    }

    @Test
    func javaScriptEvaluation() async throws {
        let page = WebPage()

        let arguments = [
            "a": 1,
            "b": 2,
        ]

        let result = try await page.callJavaScript("return a + b;", arguments: arguments) as! Int
        #expect(result == 3)

        let nilResult = try await page.callJavaScript("console.log('hi')")
        #expect(nilResult == nil)
    }

    @Test
    func decidePolicyForNavigationResponse() async throws {
        let decider = TestNavigationDecider()
        let page = WebPage(navigationDecider: decider)

        let simpleURL = Bundle.testResources.url(forResource: "simple", withExtension: "html")!
        let request = URLRequest(url: simpleURL)

        page.load(request)

        let responses = await Array(decider.navigationResponseStream.prefix(1))

        #expect(responses[0].response.url!.absoluteString == simpleURL.absoluteString)
    }

    @Test
    func clearContentWorld() async throws {
        let worldConfiguration = WKContentWorld.Configuration()
        worldConfiguration.nodeSnapshotCreationEnabled = true
        let world = WKContentWorld(configuration: worldConfiguration)

        let page = WebPage()
        try await page.load(html: "<body></body>").wait()

        try await page.callJavaScript("document.body.foo = Number(42)", contentWorld: world)

        let beforeClear = try await page.callJavaScript("return document.body.foo", contentWorld: world) as? Int
        #expect(beforeClear == 42)

        await page.backingWebView._clearContentWorld(world)

        let afterClear = try await page.callJavaScript("return document.body.foo", contentWorld: world)
        #expect(afterClear == nil)
    }
}

#endif // ENABLE_SWIFTUI
