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

#if WTF_PLATFORM_IOS_FAMILY

import CoreGraphics
private import TestWebKitAPILibrary
private import TestWebKitAPILibrary.Helpers.cocoa.TestWKWebView
import Testing
import WebKit
private import WebKit_Private.WKWebViewPrivate
private import WebKit_Private._WKViewportFitEdges

import struct Swift.String

private func fits(_ edges: _WKViewportFitEdges) -> [_WKViewportFitEdge] {
    [edges.top, edges.left, edges.bottom, edges.right]
}

@MainActor
struct ViewportFitEdgesTests {
    private let webView = TestWKWebView(frame: CGRect(x: 0, y: 0, width: 320, height: 500))

    private func load(viewport: String) async throws {
        try await webView.load(html: "<meta name='viewport' content='\(viewport)'>")
        await webView.nextPresentationUpdate()
    }

    private func setViewport(_ content: String) async throws {
        try await webView.callJavaScript(arguments: ["content": content]) {
            "document.querySelector('meta[name=viewport]').content = content"
        }
        await webView.nextPresentationUpdate()
    }

    @Test(arguments: [
        "width=device-width",
        "width=device-width, viewport-fit=auto",
        "width=device-width, viewport-fit=contain",
    ])
    func edgesContainUnlessViewportFitIsCover(viewport: String) async throws {
        try await load(viewport: viewport)

        #expect(fits(webView._viewportFitEdges) == Array(repeating: .contain, count: 4))
    }

    @Test
    func viewportFitCoverImplicitlyCoversEveryEdge() async throws {
        try await load(viewport: "width=device-width, viewport-fit=cover")

        #expect(fits(webView._viewportFitEdges) == Array(repeating: .implicitCover, count: 4))
    }

    @Test
    func observersAreNotifiedWhenViewportFitChanges() async throws {
        try await load(viewport: "width=device-width")

        let (changes, continuation) = AsyncStream.makeStream(of: [_WKViewportFitEdge].self)
        let observation = webView.observe(\._viewportFitEdges, options: .new) { _, change in
            guard let edges = change.newValue ?? nil else {
                return
            }
            continuation.yield(fits(edges))
        }

        try await setViewport("width=device-width, viewport-fit=cover")
        try await setViewport("width=device-width")

        observation.invalidate()
        continuation.finish()

        let expectedChanges: [[_WKViewportFitEdge]] = [
            Array(repeating: .implicitCover, count: 4),
            Array(repeating: .contain, count: 4),
        ]
        #expect(await Array(changes) == expectedChanges)
    }
}

#endif // WTF_PLATFORM_IOS_FAMILY
