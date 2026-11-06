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
// AND ANY EXPRESS OR IMPLIED, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
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

import struct Foundation.URL
import struct Swift.String

// With site isolation, each browsing context group has its own processes for the sites of its frames. These tests check
// which group and processes a page ends up in.

@MainActor
struct SiteIsolationBrowsingContextGroupTests {
    // The committed main frame is still a.com while a navigation to b.com waits for its policy decision, so a b.com
    // iframe must not join the main frame's process.
    @Test
    func iframeNavigationToSiteOfPendingMainFrameLoad() async throws {
        var server = try HTTPServer(protocol: .httpsProxy) {
            Route("/main") {
                "<iframe src='https://c.com/iframe'></iframe>"
            }

            Route("/iframe") {
                "hi"
            }

            Route("/pending") {
                "pending"
            }

            Route("/b_iframe") {
                "hi"
            }
        }

        try await server.run { serverConfiguration in
            let configuration = WKWebViewConfiguration(httpsProxyFor: serverConfiguration)
            configuration.setFeature("SiteIsolationSharedProcessEnabled", enabled: false)
            let (webView, navigationDelegate) = siteIsolatedWebViewAndDelegate(configuration: configuration)

            webView.load(URLRequest(url: try #require(URL(string: "https://a.com/main"))))
            try await navigationDelegate.waitForDidFinishNavigation()

            let pendingURL = try #require(URL(string: "https://b.com/pending"))
            let decidePendingNavigationPolicy = await navigationDelegate.holdNextNavigationActionPolicyDecision {
                $0.request.url?.path == "/pending"
            } triggeredBy: {
                webView.load(URLRequest(url: pendingURL))
            }

            let childFrame = try #require(await webView.firstChildFrame)
            webView.startEvaluatingJavaScript("location.href = 'https://b.com/b_iframe'", in: childFrame)
            await navigationDelegate.waitForDidFinishLoadInSubframe()
            let expectedFrameTrees: FrameTreesInProcesses = [
                .local("https://a.com", children: [.remote]),
                .remote(children: [.local("https://b.com")]),
            ]
            #expect(await webView.frameTreesInProcesses() == expectedFrameTrees)

            decidePendingNavigationPolicy(.cancel)
            try await waitForCondition("the main frame navigation to stop") {
                !webView.isLoading
            }
            #expect(await webView.frameTreesInProcesses() == expectedFrameTrees)
        }
    }
}
