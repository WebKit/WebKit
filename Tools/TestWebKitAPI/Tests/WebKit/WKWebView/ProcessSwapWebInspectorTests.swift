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

#if WTF_PLATFORM_MAC

import CoreGraphics
private import TestWebKitAPILibrary
import Testing
import WebKit
private import WebKit_Private.WKPreferencesPrivate
private import WebKit_Private.WKProcessPoolPrivate
private import WebKit_Private.WKWebViewConfigurationPrivate
private import WebKit_Private.WKWebViewPrivate
private import WebKit_Private._WKInspector
private import WebKit_Private._WKInspectorPrivateForTesting

import class Foundation.NSNumber
import struct Foundation.URL
import struct Swift.String

@MainActor
@Suite(.serialized, .timeLimit(.minutes(1)))
struct ProcessSwapWebInspectorTests {
    #if hasAttribute(diagnose)
    @diagnose(DeprecatedDeclaration, as: ignored, reason: "Intentionally uses a custom process pool and a related web view")
    #endif
    @Test
    func webInspectorDoesNotMessageClosedPreviousPage() async throws {
        var server = try Self.makeServer()

        try await server.run { serverConfiguration in
            let configuration = WKWebViewConfiguration(httpsProxyFor: serverConfiguration)
            configuration.processPool = .processSwapping(backForwardCacheEnabled: false)
            configuration.preferences._developerExtrasEnabled = true
            let processPool = configuration.processPool
            let (webView, navigationDelegate) = webViewAndDelegate(
                configuration: configuration,
                frame: CGRect(x: 0, y: 0, width: 800, height: 600),
                siteIsolationEnabled: false
            )
            webView.load(URLRequest(url: try #require(URL(string: "https://example.com/main"))))
            try await navigationDelegate.waitForDidFinishNavigation()
            let pid1 = webView._webProcessIdentifier

            let relatedConfiguration = try #require(configuration.copy() as? WKWebViewConfiguration)
            relatedConfiguration._relatedWebView = webView
            let (relatedView, relatedNavigationDelegate) = webViewAndDelegate(
                configuration: relatedConfiguration,
                frame: CGRect(x: 0, y: 0, width: 800, height: 600),
                siteIsolationEnabled: false
            )
            relatedView.load(URLRequest(url: try #require(URL(string: "https://example.com/main"))))
            try await relatedNavigationDelegate.waitForDidFinishNavigation()
            #expect(relatedView._webProcessIdentifier == pid1)

            let (optionalCountBeforeSwap, namesBeforeSwap) = await Self.unhandledMessagesInWebProcess(processPool, pid1)
            let countBeforeSwap = try #require(optionalCountBeforeSwap)
            #expect(countBeforeSwap.uint64Value == 0, "unhandled messages before the swap: \(namesBeforeSwap.joined(separator: ", "))")

            let inspector = try #require(webView._inspector)
            inspector.show()
            defer { inspector.close() }
            let frontend = try #require(inspector.inspectorWebView())
            try await waitForCondition("the Web Inspector frontend to connect to the page", timeout: .seconds(15)) {
                try await frontend.callJavaScript(returning: Bool.self) { "return !!window.WI?.pageTarget?.identifier" }
            }
            let didReply = try await frontend.callJavaScript(returning: Bool.self) {
                """
                return await Promise.race([
                    WI.pageTarget.RuntimeAgent.evaluate('1').then(() => true),
                    new Promise((resolve) => setTimeout(() => resolve(false), 1000)),
                ]);
                """
            }
            #expect(didReply)

            webView.load(URLRequest(url: try #require(URL(string: "https://webkit.org/main"))))
            try await navigationDelegate.waitForDidFinishNavigation()
            #expect(webView._webProcessIdentifier != pid1)

            let (optionalCount, names) = await Self.unhandledMessagesInWebProcess(processPool, pid1)
            let count = try #require(optionalCount, "previous web process exited although a related view uses it")
            #expect(count.uint64Value == 0, "unhandled messages in the previous web process: \(names.joined(separator: ", "))")

            withExtendedLifetime(relatedView) {}
        }
    }

    #if hasAttribute(diagnose)
    @diagnose(DeprecatedDeclaration, as: ignored, reason: "Intentionally uses a custom process pool")
    #endif
    @Test
    func webInspectorDoesNotMessageClosedPreviousPageWithSiteIsolation() async throws {
        var server = try Self.makeServer()

        try await server.run { serverConfiguration in
            let configuration = WKWebViewConfiguration(httpsProxyFor: serverConfiguration)
            configuration.processPool = .processSwapping(backForwardCacheEnabled: false)
            #expect(configuration.processPool._processCacheCapacity() > 0)
            configuration.preferences._developerExtrasEnabled = true
            let processPool = configuration.processPool
            let (webView, navigationDelegate) = webViewAndDelegate(
                configuration: configuration,
                frame: CGRect(x: 0, y: 0, width: 800, height: 600),
                siteIsolationEnabled: true
            )
            webView.load(URLRequest(url: try #require(URL(string: "https://example.com/main"))))
            try await navigationDelegate.waitForDidFinishNavigation()
            let pid1 = webView._webProcessIdentifier

            let (optionalCountBeforeSwap, namesBeforeSwap) = await Self.unhandledMessagesInWebProcess(processPool, pid1)
            let countBeforeSwap = try #require(optionalCountBeforeSwap)
            #expect(countBeforeSwap.uint64Value == 0, "unhandled messages before the swap: \(namesBeforeSwap.joined(separator: ", "))")

            let inspector = try #require(webView._inspector)
            inspector.show()
            defer { inspector.close() }
            let frontend = try #require(inspector.inspectorWebView())
            try await waitForCondition("the Web Inspector frontend to connect to the page", timeout: .seconds(15)) {
                try await frontend.callJavaScript(returning: Bool.self) { "return !!window.WI?.pageTarget?.identifier" }
            }
            let didReply = try await frontend.callJavaScript(returning: Bool.self) {
                """
                return await Promise.race([
                    WI.pageTarget.RuntimeAgent.evaluate('1').then(() => true),
                    new Promise((resolve) => setTimeout(() => resolve(false), 1000)),
                ]);
                """
            }
            #expect(didReply)

            webView.load(URLRequest(url: try #require(URL(string: "https://webkit.org/main"))))
            try await navigationDelegate.waitForDidFinishNavigation()
            #expect(webView._webProcessIdentifier != pid1)

            let (optionalCount, names) = await Self.unhandledMessagesInWebProcess(processPool, pid1)
            let count = try #require(optionalCount, "previous web process was not kept by the web process cache")
            #expect(count.uint64Value == 0, "unhandled messages in the previous web process: \(names.joined(separator: ", "))")
        }
    }

    private static func makeServer() throws -> HTTPServer {
        try HTTPServer(protocol: .httpsProxy) {
            Route("/main") {
                "main"
            }
        }
    }

    #if hasAttribute(diagnose)
    @diagnose(DeprecatedDeclaration, as: ignored, reason: "Intentionally uses a custom process pool")
    #endif
    private static func unhandledMessagesInWebProcess(
        _ processPool: WKProcessPool,
        _ pid: pid_t
    ) async -> (count: NSNumber?, names: [String]) {
        await withCheckedContinuation { continuation in
            processPool._getUnhandledMessagesInWebProcess(forTesting: pid) { count, names in
                continuation.resume(returning: (count, names ?? []))
            }
        }
    }
}

#endif
