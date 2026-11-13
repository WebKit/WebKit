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
// THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
// DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
// (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
// LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
// ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

private import TestWebKitAPILibrary
import Testing
import WebKit
private import WebKit_Private.WKProcessPoolPrivate

import struct Foundation.URL
import struct Swift.String

@MainActor
struct ProcessSwapNavigationAPITests {
    private func navigateEventDestinations(
        startURL: String,
        linkURL: String,
        redirectTarget: String,
        siteIsolationEnabled: Bool
    ) async throws -> String {
        var server = try HTTPServer(protocol: .httpsProxy) {
            Route("/start") {
                """
                <a id='link' href='\(linkURL)'>go</a>
                <script>
                  navigation.addEventListener('navigate', e => {
                    localStorage.setItem('events', (localStorage.getItem('events') || '') + '|' + e.destination.url);
                  });
                </script>
                """
            }

            Route("/redirect", statusCode: 302, headerFields: ["Location": redirectTarget]) {
                "redirecting"
            }

            Route("/end") {
                "done"
            }
        }

        return try await server.run { serverConfiguration -> String in
            let configuration = WKWebViewConfiguration(httpsProxyFor: serverConfiguration)
            configuration.processPool = WKProcessPool.processSwapping()
            let (webView, navigationDelegate) = webViewAndDelegate(
                configuration: configuration,
                siteIsolationEnabled: siteIsolationEnabled
            )

            webView.load(URLRequest(url: try #require(URL(string: startURL))))
            try await navigationDelegate.waitForDidFinishNavigation()

            webView.startEvaluatingJavaScript("document.getElementById('link').click()")
            try await navigationDelegate.waitForDidFinishNavigation()

            return try await webView.callJavaScript(returning: String.self) { "return localStorage.getItem('events')" }
        }
    }

    @Test(arguments: [false, true])
    func navigateEventFiresOnceForRedirectedProcessSwappingNavigation(siteIsolationEnabled: Bool) async throws {
        let events = try await navigateEventDestinations(
            startURL: "https://example.com/start",
            linkURL: "https://webkit.org/redirect",
            redirectTarget: "https://example.com/end",
            siteIsolationEnabled: siteIsolationEnabled
        )
        #expect(events == "|https://webkit.org/redirect")
    }

    @Test(arguments: [false, true])
    func navigateEventFiresOnceForSameSiteRedirectedNavigation(siteIsolationEnabled: Bool) async throws {
        let events = try await navigateEventDestinations(
            startURL: "https://webkit.org/start",
            linkURL: "https://webkit.org/redirect",
            redirectTarget: "https://webkit.org/end",
            siteIsolationEnabled: siteIsolationEnabled
        )
        #expect(events == "|https://webkit.org/redirect")
    }

    @Test(arguments: [false, true])
    func preventDefaultCancelsProcessSwappingNavigation(siteIsolationEnabled: Bool) async throws {
        var server = try HTTPServer(protocol: .httpsProxy) {
            Route("/start") {
                """
                <a id='link' href='https://webkit.org/end'>go</a>
                <script>
                  navigation.addEventListener('navigate', e => {
                    e.preventDefault();
                    alert('navigate event ran');
                  });
                </script>
                """
            }

            Route("/end") {
                "done"
            }
        }

        try await server.run { serverConfiguration in
            let configuration = WKWebViewConfiguration(httpsProxyFor: serverConfiguration)
            configuration.processPool = WKProcessPool.processSwapping()
            let (webView, navigationDelegate) = webViewAndDelegate(
                configuration: configuration,
                siteIsolationEnabled: siteIsolationEnabled
            )
            let alerts = AlertRecorder()
            webView.uiDelegate = alerts

            webView.load(URLRequest(url: try #require(URL(string: "https://example.com/start"))))
            try await navigationDelegate.waitForDidFinishNavigation()

            webView.startEvaluatingJavaScript("document.getElementById('link').click()")
            let alert = try await alerts.nextAlert()
            #expect(alert == "navigate event ran")

            await webView.nextPresentationUpdate()
            #expect(webView.url?.absoluteString == "https://example.com/start")
        }
    }
}
