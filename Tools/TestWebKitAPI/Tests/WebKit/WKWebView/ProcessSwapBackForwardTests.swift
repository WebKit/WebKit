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
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
// PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS
// CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
// EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
// PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
// OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import Foundation
private import TestWebKitAPILibrary
import Testing
import WebKit
private import WebKit_Private.WKWebViewPrivate

import struct Foundation.URL

// The back/forward cache keeps the page a web view navigates away from, and a cross-site navigation leaves the page in
// its process as a suspended page. Going back has to restore it from there. These tests check which process hosts a
// page after a history navigation.

@MainActor
struct ProcessSwapBackForwardTests {
    // Two tabs share a process pool and end up with suspended pages in the same process. Restoring one tab's suspended
    // page must not drop the other tab's.
    @Test
    func crossTabSuspendedPagesInSameProcess() async throws {
        var server = try HTTPServer(protocol: .httpsProxy) {
            Route("/1.html") {
                "<script>addEventListener('pageshow', event => { if (event.persisted) window.wasPersisted = true })</script>"
            }

            Route("/x.html") {
                "plain"
            }

            Route("/b.html") {
                "plain"
            }

            // The back/forward cache holds only two pages. A client redirect doesn't suspend c.com, so only the two
            // suspended pages below take entries, and neither is evicted.
            Route("/c.html") {
                "<script>location.replace('https://www.a.com/x.html')</script>"
            }

            Route("/d.html") {
                "plain"
            }
        }

        try await server.run { serverConfiguration in
            let configuration = WKWebViewConfiguration(httpsProxyFor: serverConfiguration)
            configuration.processPool = .processSwapping(webProcessCacheEnabled: false)
            let frame = CGRect(x: 0, y: 0, width: 800, height: 600)
            let (webViewA, navigationDelegateA) = webViewAndDelegate(
                configuration: configuration,
                frame: frame,
                siteIsolationEnabled: false
            )
            let (webViewB, navigationDelegateB) = webViewAndDelegate(
                configuration: configuration,
                frame: frame,
                siteIsolationEnabled: false
            )

            // Tab A: a.com/1 -> b.com. a.com/1 is suspended in its process.
            webViewA.load(URLRequest(url: try #require(URL(string: "https://www.a.com/1.html"))))
            try await navigationDelegateA.waitForDidFinishNavigation()
            let suspendedProcessIdentifier = webViewA._webProcessIdentifier

            webViewA.load(URLRequest(url: try #require(URL(string: "https://www.b.com/b.html"))))
            try await navigationDelegateA.waitForDidFinishNavigation()
            #expect(webViewA._webProcessIdentifier != suspendedProcessIdentifier)

            // Tab B: c.com redirects to a.com/x, which reuses the suspended page's process.
            webViewB.load(URLRequest(url: try #require(URL(string: "https://www.c.com/c.html"))))
            try await navigationDelegateB.waitForDidFinishNavigation()
            try await navigationDelegateB.waitForDidFinishNavigation()
            #expect(webViewB.url?.absoluteString == "https://www.a.com/x.html")
            #expect(webViewB._webProcessIdentifier == suspendedProcessIdentifier)

            // Tab B: -> d.com suspends a.com/x in the same process.
            webViewB.load(URLRequest(url: try #require(URL(string: "https://www.d.com/d.html"))))
            try await navigationDelegateB.waitForDidFinishNavigation()
            #expect(webViewB._webProcessIdentifier != suspendedProcessIdentifier)

            // Tab B: going back restores a.com/x from its suspended page.
            webViewB.goBack()
            try await navigationDelegateB.waitForDidFinishNavigation()
            #expect(webViewB._webProcessIdentifier == suspendedProcessIdentifier)
            #expect(webViewB.url?.absoluteString == "https://www.a.com/x.html")

            // Tab A: going back must still restore a.com/1 from its own suspended page.
            webViewA.goBack()
            try await navigationDelegateA.waitForDidFinishNavigation()
            #expect(webViewA.url?.absoluteString == "https://www.a.com/1.html")
            #expect(try await webViewA.callJavaScript(returning: Bool.self) { "return !!window.wasPersisted" })
        }
    }
}
