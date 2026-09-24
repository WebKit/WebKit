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
private import WebKit_Private.WKWebViewConfigurationPrivate

import struct Foundation.URL
import struct Swift.String

// The network process rejects a cookie IPC whose URL names a registrable domain that the sending web process doesn't
// host a document for. These tests check the other direction: that an iframe's access to its own cookies isn't blocked.
// A denied read gets an empty cookie string rather than an error or a process termination, so each test sets a cookie
// and checks that the iframe reads exactly that.

@MainActor
struct SiteIsolationCookieTests {
    /// Routes for a main frame that alerts whatever its iframe posts, and an iframe that posts its `document.cookie`.
    ///
    /// The iframe's `document.cookie` sends the network process a cookie IPC that names the iframe's own URL.
    @RouteBuilder
    private static func cookieReadingRoutes(iframeURL: String) -> Route {
        Route("/mainframe") {
            """
            <script>addEventListener('message', (event) => alert(String(event.data)))</script>
            <iframe src='\(iframeURL)'></iframe>
            """
        }

        Route("/iframe") {
            "<script>parent.postMessage(String(document.cookie), '*')</script>"
        }
    }

    private static func setSecureCookie(name: String, value: String, domain: String, in dataStore: WKWebsiteDataStore) async throws {
        let cookie = try #require(
            HTTPCookie(properties: [
                .name: name,
                .value: value,
                .domain: domain,
                .path: "/",
                .secure: true,
            ])
        )
        await dataStore.httpCookieStore.setCookie(cookie)
    }

    // With site isolation, the cross-site iframe has its own process, whose hosted sites are exactly webkit.org, so
    // the check applies and has to allow webkit.org's cookies. Without site isolation, which is the shipping default,
    // one process hosts both frames and may name any site; if it were limited to example.com instead, every cross-site
    // iframe would read no cookies.
    @Test(arguments: [true, false])
    func crossSiteIframeReadsOwnCookies(siteIsolationEnabled: Bool) async throws {
        var server = try HTTPServer(protocol: .httpsProxy) {
            Self.cookieReadingRoutes(iframeURL: "https://webkit.org/iframe")
        }

        try await server.run { serverConfiguration in
            let configuration = WKWebViewConfiguration(httpsProxyFor: serverConfiguration)
            // Third-party cookie blocking would otherwise hide webkit.org's cookies from the iframe, whatever this check does.
            configuration._shouldRelaxThirdPartyCookieBlocking = true
            try await Self.setSecureCookie(name: "k", value: "v", domain: "webkit.org", in: configuration.websiteDataStore)

            let (webView, navigationDelegate) = webViewAndDelegate(configuration: configuration, siteIsolationEnabled: siteIsolationEnabled)
            let alerts = AlertRecorder()
            webView.uiDelegate = alerts

            webView.load(URLRequest(url: try #require(URL(string: "https://example.com/mainframe"))))
            try await navigationDelegate.waitForDidFinishNavigation()
            let cookies = try await alerts.nextAlert()
            #expect(cookies == "k=v")

            // Had site isolation stopped putting the iframe in its own process, the two cases would be the same, and the
            // first would keep passing without testing the check.
            let expectedFrameTrees: FrameTreesInProcesses =
                siteIsolationEnabled
                ? [
                    .local("https://example.com", children: [.remote]),
                    .remote(children: [.local("https://webkit.org")]),
                ]
                : [
                    .local("https://example.com", children: [.local("https://webkit.org")])
                ]
            #expect(await webView.frameTreesInProcesses() == expectedFrameTrees)
        }
    }

    // A same-site iframe from a subdomain shares the main frame's process, whose hosted sites are exactly example.com,
    // so the check has to match the iframe's URL by registrable domain rather than by origin.
    @Test
    func sameSiteSubdomainIframeReadsOwnCookies() async throws {
        var server = try HTTPServer(protocol: .httpsProxy) {
            Self.cookieReadingRoutes(iframeURL: "https://sub.example.com/iframe")
        }

        try await server.run { serverConfiguration in
            let configuration = WKWebViewConfiguration(httpsProxyFor: serverConfiguration)
            try await Self.setSecureCookie(name: "k", value: "v", domain: "sub.example.com", in: configuration.websiteDataStore)

            let (webView, navigationDelegate) = siteIsolatedWebViewAndDelegate(configuration: configuration)
            let alerts = AlertRecorder()
            webView.uiDelegate = alerts

            webView.load(URLRequest(url: try #require(URL(string: "https://example.com/mainframe"))))
            try await navigationDelegate.waitForDidFinishNavigation()
            let cookies = try await alerts.nextAlert()
            #expect(cookies == "k=v")

            #expect(
                await webView.frameTreesInProcesses() == [
                    .local("https://example.com", children: [.local("https://sub.example.com")])
                ]
            )
        }
    }
}
