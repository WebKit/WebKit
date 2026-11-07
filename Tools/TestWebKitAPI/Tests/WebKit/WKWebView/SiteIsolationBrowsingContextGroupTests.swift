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

import Foundation
private import TestWebKitAPILibrary
import Testing
import WebKit
private import WebKit_Private.WKProcessPoolPrivate
private import WebKit_Private.WKWebViewPrivate
private import WebKit_Private.WKWebsiteDataStorePrivate

import struct Foundation.URL
import struct Swift.String

// With site isolation, each browsing context group has its own processes for the sites of its frames. A response with
// Cross-Origin-Opener-Policy can move a navigation to a new group, which severs the window's opener, and a cross-origin
// isolated group only uses processes of its own. These tests check which group and processes a page ends up in.

@MainActor
struct SiteIsolationBrowsingContextGroupTests {
    // The opener keeps c.com alive in the shared process of the original group, so the opened page rejoins it when it
    // goes back.
    @Test
    func openedPageReturningToGroupLoadsIframeInSharedProcess() async throws {
        var server = try HTTPServer(protocol: .httpsProxy) {
            Route("/opener") {
                "<iframe src='https://c.com/iframe'></iframe>"
            }

            Route("/opened") {
                "<iframe src='https://c.com/iframe'></iframe>"
            }

            Route("/iframe") {
                "hi"
            }

            Route("/elsewhere") {
                "elsewhere"
            }
        }

        try await server.run { serverConfiguration in
            let configuration = WKWebViewConfiguration(httpsProxyFor: serverConfiguration)
            configuration.websiteDataStore._resourceLoadStatisticsEnabled = true
            configuration.setFeature("SiteIsolationSharedProcessEnabled", enabled: true)
            let (webView, navigationDelegate) = siteIsolatedWebViewAndDelegate(
                configuration: configuration,
                frame: CGRect(x: 0, y: 0, width: 800, height: 600)
            )
            let openedWebViews = OpenedWebViewRecorder()
            webView.uiDelegate = openedWebViews

            webView.load(URLRequest(url: try #require(URL(string: "https://a.com/opener"))))
            try await navigationDelegate.waitForDidFinishNavigation()
            webView.startEvaluatingJavaScript("w = window.open('https://b.com/opened')")
            let (openedWebView, openedNavigationDelegate) = try await openedWebViews.nextOpenedWebView()

            let expectedOpenedFrameTrees: FrameTreesInProcesses = [
                .local("https://b.com", children: [.remote]),
                .remote(children: [.local("https://c.com")]),
                .remote(children: [.remote]),
            ]
            #expect(await openedWebView.frameTreesInProcesses() == expectedOpenedFrameTrees)

            openedWebView.load(URLRequest(url: try #require(URL(string: "https://d.com/elsewhere"))))
            try await openedNavigationDelegate.waitForDidFinishNavigation()
            #expect(await openedWebView.frameTreesInProcesses() == [.local("https://d.com")])

            openedWebView.goBack()
            await openedNavigationDelegate.waitForDidCommitNavigation()
            try? await waitForCondition("the iframe to load in the shared process", timeout: .seconds(10)) {
                await openedWebView.frameTreesInProcesses() == expectedOpenedFrameTrees
            }
            #expect(await openedWebView.frameTreesInProcesses() == expectedOpenedFrameTrees)
        }
    }
}
