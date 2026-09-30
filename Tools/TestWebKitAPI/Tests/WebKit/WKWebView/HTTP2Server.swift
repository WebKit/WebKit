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

#if HAVE_NETWORK_FRAMEWORK_HTTP_MESSAGING

import Foundation
@_spi(HTTP) private import TestWebKitAPILibrary
private import TestWebKitAPILibrary.Helpers.cocoa.TestNavigationDelegate
private import TestWebKitAPILibrary.Helpers.cocoa.TestWKWebView
import Testing
import WebKit
private import WebKit_Private
private import WebKit_Private.WKWebsiteDataStorePrivate
private import WebKit_Private._WKWebsiteDataStoreConfiguration

#if USE_APPLE_INTERNAL_SDK
@_spi(HTTP) private import Network
#else
private import Network
private import Network_SPI
#endif

import struct Foundation.URL
import struct Swift.String

@MainActor
struct HTTP2ServerTests {
    private let webView: TestWKWebView

    init() {
        let configuration = WKWebViewConfiguration()
        configuration.websiteDataStore = .nonPersistent()
        webView = TestWKWebView(frame: CGRect(x: 0, y: 0, width: 800, height: 600), configuration: configuration)
    }

    @Test
    func roundTrip() async throws {
        var server = try HTTPServer(protocol: .http2) {
            Route("/") {
                "hello"
            }
        }

        try await server.run { configuration in
            try await webView.loadAndWaitIgnoringSSLErrors(URLRequest(url: configuration.address))

            let bodyText = try await webView.callJavaScript(returning: String.self) { "return document.body.textContent" }
            #expect(bodyText == "hello")

            let nextHopProtocol = try await webView.callJavaScript(returning: String.self) {
                "return performance.getEntriesByType('navigation')[0].nextHopProtocol"
            }
            #expect(nextHopProtocol == "h2")
        }

        #expect(server.totalRequests == 1)
    }

    @Test
    func requestMethodPathAndHeaders() async throws {
        var server = try HTTPServer(protocol: .http2) {
            Route("/test") {
                "hello"
            }
        }

        try await server.run { configuration in
            var request = URLRequest(url: configuration.address.appending(path: "test"))
            request.setValue("testvalue", forHTTPHeaderField: "X-Test-Header")

            try await webView.loadAndWaitIgnoringSSLErrors(request)

            let bodyText = try await webView.callJavaScript(returning: String.self) { "return document.body.textContent" }
            #expect(bodyText == "hello")

            let nextHopProtocol = try await webView.callJavaScript(returning: String.self) {
                "return performance.getEntriesByType('navigation')[0].nextHopProtocol"
            }
            #expect(nextHopProtocol == "h2")
        }

        let request = try #require(server.receivedRequests.first?.request)
        let testHeader = try #require(HTTPField.Name("x-test-header"))
        #expect(request.method == .get)
        #expect(request.path == "/test")
        #expect(request.headerFields[testHeader] == "testvalue")
    }

    @Test
    func postBody() async throws {
        var server = try HTTPServer(protocol: .http2) {
            Route("/") {
                "hello"
            }
            Route("/submit") {
                "ok"
            }
        }

        try await server.run { configuration in
            try await webView.loadAndWaitIgnoringSSLErrors(URLRequest(url: configuration.address))

            let responseText = try await webView.callJavaScript(returning: String.self) {
                "const response = await fetch('/submit', { method: 'POST', body: 'hello world' }); return await response.text()"
            }
            #expect(responseText == "ok")

            let nextHopProtocol = try await webView.callJavaScript(returning: String.self) {
                "return performance.getEntriesByType('resource')[0].nextHopProtocol"
            }
            #expect(nextHopProtocol == "h2")
        }

        let (request, body) = try #require(server.receivedRequests.first { $0.request.path == "/submit" })
        #expect(request.method == .post)
        #expect(body == Data("hello world".utf8))
    }

    @Test
    func nonDefaultStatusCodeAndResponseHeader() async throws {
        var server = try HTTPServer(protocol: .http2) {
            Route("/") {
                "hello"
            }
            Route("/missing", statusCode: 404, headerFields: ["X-Custom-Response-Header": "responsevalue"]) {
                ""
            }
        }

        try await server.run { configuration in
            try await webView.loadAndWaitIgnoringSSLErrors(URLRequest(url: configuration.address))

            let statusAndHeader = try await webView.callJavaScript(returning: String.self) {
                "const response = await fetch('/missing'); return `${response.status}:${response.headers.get('X-Custom-Response-Header')}`"
            }
            #expect(statusAndHeader == "404:responsevalue")

            let nextHopProtocol = try await webView.callJavaScript(returning: String.self) {
                "return performance.getEntriesByType('resource')[0].nextHopProtocol"
            }
            #expect(nextHopProtocol == "h2")
        }
    }

    @Test
    func multipleRequestsOverOneConnection() async throws {
        var server = try HTTPServer(protocol: .http2) {
            Route("/") {
                "hello"
            }
            Route("/a") {
                "a"
            }
            Route("/b") {
                "b"
            }
        }

        try await server.run { configuration in
            try await webView.loadAndWaitIgnoringSSLErrors(URLRequest(url: configuration.address))

            let responseTexts = try await webView.callJavaScript(returning: String.self) {
                """
                const [a, b] = await Promise.all([fetch('/a').then(response => response.text()), fetch('/b').then(response => response.text())]);
                return `${a}:${b}`
                """
            }
            #expect(responseTexts == "a:b")

            let allResourcesUsedHTTP2 = try await webView.callJavaScript(returning: Bool.self) {
                "return performance.getEntriesByType('resource').every(entry => entry.nextHopProtocol === 'h2')"
            }
            #expect(allResourcesUsedHTTP2)
        }

        #expect(server.totalRequests == 3)
    }

    @Test
    func multipleCookiesSetBeforeLoad() async throws {
        // RFC 7540 8.1.2.5 "cookie crumbling": a client may split one logical Cookie header into
        // multiple header fields on the wire. These must be rejoined with "; ", not the "," used
        // for other repeated header fields (RFC 7230 3.2.2), to reconstruct the Cookie header value.
        var server = try HTTPServer(protocol: .http2) {
            Route("/") {
                "hello"
            }
        }

        let firstCookie = try #require(
            HTTPCookie(properties: [
                .path: "/",
                .name: "firstCookie",
                .value: "firstValue",
                .domain: "127.0.0.1",
            ])
        )
        let secondCookie = try #require(
            HTTPCookie(properties: [
                .path: "/",
                .name: "secondCookie",
                .value: "secondValue",
                .domain: "127.0.0.1",
            ])
        )

        let cookieStore = webView.configuration.websiteDataStore.httpCookieStore
        await cookieStore.setCookie(firstCookie)
        await cookieStore.setCookie(secondCookie)

        try await server.run { configuration in
            try await webView.loadAndWaitIgnoringSSLErrors(URLRequest(url: configuration.address))
        }

        let request = try #require(server.receivedRequests.first?.request)
        #expect(request.headerFields[.cookie] == "firstCookie=firstValue; secondCookie=secondValue")
    }

    @Test
    func throughProxy() async throws {
        var server = try HTTPServer(protocol: .http2Proxy) {
            Route("/") {
                "hello"
            }
        }

        try await server.run { configuration in
            let storeConfiguration = _WKWebsiteDataStoreConfiguration(nonPersistentConfiguration: ())
            storeConfiguration.httpsProxy = configuration.httpsProxy
            let dataStore = WKWebsiteDataStore._store(with: storeConfiguration)

            let viewConfiguration = WKWebViewConfiguration()
            viewConfiguration.websiteDataStore = dataStore

            let navigationDelegate = TestNavigationDelegate()
            navigationDelegate.allowAnyTLSCertificate()

            let webView = WKWebView(frame: .zero, configuration: viewConfiguration)
            webView.navigationDelegate = navigationDelegate

            let url = try #require(URL(string: "https://a.example/"))
            webView.load(URLRequest(url: url))
            try await navigationDelegate.waitForDidFinishNavigation()
        }

        let request = try #require(server.receivedRequests.first?.request)
        #expect(request.authority == "a.example")
    }
}

#endif // HAVE_NETWORK_FRAMEWORK_HTTP_MESSAGING
