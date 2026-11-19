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

#if ENABLE_WEB_AUTHN && HAVE_DIGITAL_CREDENTIALS_UI && HAVE_DIGITAL_CREDENTIALS_OPENID4VP && WTF_PLATFORM_MAC

import CoreGraphics
private import TestWebKitAPILibrary
private import TestWebKitAPILibrary.Helpers.cocoa.TestNavigationDelegate
private import TestWebKitAPILibrary.Helpers.cocoa.TestWKWebView
import Testing
import WebKit
private import WebKit_Private.WKPreferencesPrivate

import struct Foundation.URL
import struct Foundation.URLRequest
import struct Swift.String

import Foundation

private let useMockPlatformWallet = ["DigitalCredentialsUseMockPlatformWalletForTesting"]

private struct MockPlatformWalletAnswer: Decodable {
    let mockPlatformWallet: Bool
    let origin: String
    let presentation: Int
}

private func mockPlatformWalletAnswer(_ result: String) -> MockPlatformWalletAnswer? {
    guard let data = result.data(using: .utf8),
        let object = try? JSONSerialization.jsonObject(with: data) as? [String: Any],
        let encoded = object["response"] as? String
    else {
        return nil
    }
    var base64 = encoded.replacingOccurrences(of: "-", with: "+").replacingOccurrences(of: "_", with: "/")
    base64 += String(repeating: "=", count: (4 - base64.count % 4) % 4)
    guard let decoded = Data(base64Encoded: base64) else {
        return nil
    }
    return try? JSONDecoder().decode(MockPlatformWalletAnswer.self, from: decoded)
}

@MainActor
private let unsignedOpenID4VPRequest: [String: Any] = [
    "protocol": "openid4vp-v1-unsigned",
    "data": [
        "response_type": "vp_token",
        "response_mode": "dc_api",
        "nonce": "unsigned-test-nonce-0123456789",
        "dcql_query": [
            "credentials": [
                [
                    "id": "mdl",
                    "format": "mso_mdoc",
                    "meta": ["doctype_value": "org.iso.18013.5.1.mDL"],
                    "claims": [["path": ["org.iso.18013.5.1", "family_name"]]],
                ]
            ]
        ],
    ],
]

@MainActor
private let invalidMobileDocumentRequest: [String: Any] = [
    "protocol": "org-iso-mdoc",
    "data": ["deviceRequest": "AAAA", "encryptionInfo": "AAAA"],
]

@MainActor
struct DigitalCredentialsNetworkProcessTests {
    private func withFocusedPage<Result>(features: [String] = [], _ body: (TestWKWebView) async throws -> Result) async throws -> Result {
        var server = try HTTPServer(protocol: .httpsProxy) {
            Route("/") {
                "<!DOCTYPE html><body></body>"
            }
        }

        return try await server.run { serverConfiguration in
            let configuration = WKWebViewConfiguration(httpsProxyFor: serverConfiguration)
            configuration.setFeature("DigitalCredentialsEnabled", enabled: true)
            configuration.setFeature("DigitalCredentialsOpenID4VPEnabled", enabled: true)
            for feature in features {
                configuration.setFeature(feature, enabled: true)
            }
            let webView = TestWKWebView(frame: CGRect(x: 0, y: 0, width: 400, height: 400), configuration: configuration, addToWindow: true)
            let navigationDelegate = TestNavigationDelegate()
            navigationDelegate.allowAnyTLSCertificate()
            webView.navigationDelegate = navigationDelegate

            webView.load(URLRequest(url: try #require(URL(string: "https://example.com/"))))
            try await navigationDelegate.waitForDidFinishNavigation()
            webView.hostWindow()?.makeKey()
            webView.focus()
            try await webView.callJavaScript {
                "if (!document.hasFocus()) await new Promise(resolve => window.addEventListener('focus', resolve, { once: true }))"
            }

            return try await body(webView)
        }
    }

    private func requestCredential(_ request: [String: Any], features: [String] = []) async throws -> String {
        try await withFocusedPage(features: features) { webView in
            try await webView.callJavaScript(returning: String.self, arguments: ["request": request]) {
                """
                try {
                    const credential = await navigator.credentials.get({ digital: { requests: [request] }, mediation: "required" });
                    return JSON.stringify(credential.data);
                } catch (error) {
                    return error.name + ": " + error.message;
                }
                """
            }
        }
    }

    #if !HAVE_IDENTITY_DOCUMENT_SERVICES_SYSTEM_PRESENTMENT
    @Test
    func requestFromAppWithoutEntitlementGoesToNetworkProcess() async throws {
        let result = try await requestCredential(unsignedOpenID4VPRequest)
        #expect(result == "NotSupportedError: Digital credentials are not available in this app.")
    }
    #endif

    @Test
    func networkProcessPresentsTheRequestForTheOrigin() async throws {
        let result = try await requestCredential(unsignedOpenID4VPRequest, features: useMockPlatformWallet)

        let answer = try #require(mockPlatformWalletAnswer(result), "\(result)")
        #expect(answer.mockPlatformWallet)
        #expect(answer.origin == "https://example.com/")
    }

    @Test
    func requestAbortedBeforeReachingTheNetworkProcessIsNotPresented() async throws {
        let results = try await withFocusedPage(features: useMockPlatformWallet) { webView in
            let request = """
                try {
                    return JSON.stringify((await navigator.credentials.get({ digital: { requests: [request] }, mediation: "required" })).data);
                } catch (error) {
                    return error.name + ": " + error.message;
                }
                """
            let first = try await webView.callJavaScript(returning: String.self, arguments: ["request": unsignedOpenID4VPRequest]) {
                request
            }
            let aborted = try await webView.callJavaScript(returning: String.self, arguments: ["request": unsignedOpenID4VPRequest]) {
                """
                const controller = new AbortController();
                const result = navigator.credentials.get({ digital: { requests: [request] }, mediation: "required", signal: controller.signal })
                    .then(() => "resolved", error => error.name);
                controller.abort();
                return await result;
                """
            }
            let next = try await webView.callJavaScript(returning: String.self, arguments: ["request": unsignedOpenID4VPRequest]) {
                request
            }
            return (first, aborted, next)
        }

        #expect(results.1 == "AbortError")
        let first = try #require(mockPlatformWalletAnswer(results.0), "\(results.0)")
        let next = try #require(mockPlatformWalletAnswer(results.2), "\(results.2)")
        #expect(next.presentation == first.presentation + 1)
    }

    @Test
    func invalidRequestIsRefusedBeforeNetworkProcess() async throws {
        let result = try await requestCredential(invalidMobileDocumentRequest)
        #expect(result.hasPrefix("TypeError:"))
    }
}

#endif
