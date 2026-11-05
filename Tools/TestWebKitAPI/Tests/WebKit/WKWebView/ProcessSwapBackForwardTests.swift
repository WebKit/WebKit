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

// The back/forward cache keeps the page a web view navigates away from, and a cross-site navigation leaves the page in
// its process as a suspended page. Going back has to restore it from there, unless the process can't host the
// navigation, such as when it isn't in the browsing context group. These tests check which process hosts a page after
// a history navigation.

@MainActor
struct ProcessSwapBackForwardTests {
    // A client redirect evicts the page in the in-process back/forward cache without telling the UI process, and the
    // process can then enter the process cache. Going back to that item must not use the cached process.
    @Test
    func inProcessBackForwardCacheProcessInWebProcessCache() async throws {
        var server = try HTTPServer(protocol: .https) {
            Route("/1.html") {
                "one"
            }

            Route("/2.html") {
                "two"
            }

            Route("/other.html") {
                "three"
            }
        }

        try await server.run { serverConfiguration in
            let processPool = WKProcessPool.processSwapping()
            let configuration = WKWebViewConfiguration()
            configuration.processPool = processPool
            let (webView, navigationDelegate) = siteIsolatedWebViewAndDelegate(
                configuration: configuration,
                frame: CGRect(x: 0, y: 0, width: 800, height: 600)
            )
            let firstURL = serverConfiguration.address.appending(path: "1.html")

            webView.load(URLRequest(url: firstURL))
            try await navigationDelegate.waitForDidFinishNavigation()
            let firstProcessIdentifier = webView._webProcessIdentifier

            // A same-process navigation, which puts /1.html in the in-process back/forward cache.
            webView.load(URLRequest(url: serverConfiguration.address.appending(path: "2.html")))
            try await navigationDelegate.waitForDidFinishNavigation()
            #expect(webView._webProcessIdentifier == firstProcessIdentifier)

            // A cross-site client redirect, which doesn't suspend /2.html, so its page in the first process closes.
            let otherURL = serverConfiguration.localhostAddress.appending(path: "other.html")
            webView.startEvaluatingJavaScript("location.replace('\(otherURL.absoluteString)')", withUserGesture: false)
            try await navigationDelegate.waitForDidFinishNavigation()
            #expect(webView._webProcessIdentifier != firstProcessIdentifier)

            // The first process no longer hosts a page, so it should enter the process cache.
            try? await waitForCondition("the first process to enter the process cache", timeout: .seconds(10)) {
                processPool._processCacheSize() > 0
            }
            #expect(processPool._processCacheSize() == 1)

            webView.goBack()
            try await navigationDelegate.waitForDidFinishNavigation()
            #expect(webView._committedURL?.absoluteString == firstURL.absoluteString)
        }
    }
}
