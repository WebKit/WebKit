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
private import WebKit_Private.WKUIDelegatePrivate
private import WebKit_Private.WKWebViewPrivate
private import WebKit_Private._WKViewportFitEdges

import struct Swift.String

private let edgeNames = ["top", "left", "bottom", "right"]

private func fits(_ edges: _WKViewportFitEdges) -> [_WKViewportFitEdge] {
    [edges.top, edges.left, edges.bottom, edges.right]
}

@MainActor
private final class SafeAreaUIDelegate: NSObject, WKUIDelegatePrivate {
    private(set) var safeAreaShouldAffectObscuredInsetsChanges: [Bool] = []

    // swift-format-ignore: NoLeadingUnderscores
    func _webView(_ webView: WKWebView, didChangeSafeAreaShouldAffectObscuredInsets safeAreaShouldAffectObscuredInsets: Bool) {
        safeAreaShouldAffectObscuredInsetsChanges.append(safeAreaShouldAffectObscuredInsets)
    }
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

    private func enablePerEdgeViewportFit() {
        webView.configuration.setFeature("PerEdgeViewportFitEnabled", enabled: true)
    }

    private func viewportFitEdgesChanges(during body: () async throws -> Void) async throws -> [[_WKViewportFitEdge]] {
        let (changes, continuation) = AsyncStream.makeStream(of: [_WKViewportFitEdge].self)
        let observation = webView.observe(\._viewportFitEdges, options: .new) { _, change in
            guard let edges = change.newValue ?? nil else {
                return
            }
            continuation.yield(fits(edges))
        }

        try await body()

        observation.invalidate()
        continuation.finish()
        return await Array(changes)
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

        let changes = try await viewportFitEdgesChanges {
            try await setViewport("width=device-width, viewport-fit=cover")
            try await setViewport("width=device-width")
        }

        let expectedChanges: [[_WKViewportFitEdge]] = [
            Array(repeating: .implicitCover, count: 4),
            Array(repeating: .contain, count: 4),
        ]
        #expect(changes == expectedChanges)
    }

    @Test
    func perEdgeViewportFitIsIgnoredWhenDisabled() async throws {
        try await load(viewport: "width=device-width, viewport-fit-top=cover, viewport-fit-bottom=cover")

        #expect(fits(webView._viewportFitEdges) == Array(repeating: .contain, count: 4))
    }

    @Test(arguments: edgeNames)
    func perEdgeViewportFitExplicitlyCoversOnlyThatEdge(edge: String) async throws {
        enablePerEdgeViewportFit()
        try await load(viewport: "width=device-width, viewport-fit-\(edge)=cover")

        let expected: [_WKViewportFitEdge] = edgeNames.map { $0 == edge ? .explicitCover : .contain }
        #expect(fits(webView._viewportFitEdges) == expected)
    }

    @Test(arguments: ["contain", "auto"])
    func perEdgeViewportFitOverridesViewportFitCover(value: String) async throws {
        enablePerEdgeViewportFit()
        try await load(viewport: "width=device-width, viewport-fit=cover, viewport-fit-bottom=\(value)")

        let expected: [_WKViewportFitEdge] = [.implicitCover, .implicitCover, .contain, .implicitCover]
        #expect(fits(webView._viewportFitEdges) == expected)
    }

    @Test
    func perEdgeViewportFitCoverIsExplicitEvenWithViewportFitCover() async throws {
        enablePerEdgeViewportFit()
        try await load(viewport: "width=device-width, viewport-fit=cover, viewport-fit-top=cover")

        let expected: [_WKViewportFitEdge] = [.explicitCover, .implicitCover, .implicitCover, .implicitCover]
        #expect(fits(webView._viewportFitEdges) == expected)
    }

    @Test
    func invalidPerEdgeViewportFitIsIgnored() async throws {
        enablePerEdgeViewportFit()
        try await load(viewport: "width=device-width, viewport-fit=cover, viewport-fit-bottom=invalid")

        #expect(fits(webView._viewportFitEdges) == Array(repeating: .implicitCover, count: 4))
    }

    @Test
    func overriddenViewportArgumentsCanSpecifyPerEdgeViewportFit() async throws {
        enablePerEdgeViewportFit()
        try await load(viewport: "width=device-width")

        webView._overrideViewport(withArguments: ["width": "device-width", "viewport-fit-left": "cover"])
        await webView.nextPresentationUpdate()

        let expected: [_WKViewportFitEdge] = [.contain, .explicitCover, .contain, .contain]
        #expect(fits(webView._viewportFitEdges) == expected)
    }

    @Test
    func observersAreNotifiedWhenPerEdgeViewportFitChanges() async throws {
        enablePerEdgeViewportFit()
        try await load(viewport: "width=device-width")

        let changes = try await viewportFitEdgesChanges {
            try await setViewport("width=device-width, viewport-fit-top=cover")
            try await setViewport("width=device-width, viewport-fit=cover, viewport-fit-top=cover")
        }

        let expectedChanges: [[_WKViewportFitEdge]] = [
            [.explicitCover, .contain, .contain, .contain],
            [.explicitCover, .implicitCover, .implicitCover, .implicitCover],
        ]
        #expect(changes == expectedChanges)
    }

    @Test(arguments: [
        "width=device-width",
        "width=device-width, viewport-fit-top=cover",
        "width=device-width, viewport-fit=cover, viewport-fit-bottom=contain",
    ])
    func safeAreaAffectsObscuredInsetsUnlessEveryEdgeIsCovered(viewport: String) async throws {
        enablePerEdgeViewportFit()
        try await load(viewport: viewport)

        #expect(webView._safeAreaShouldAffectObscuredInsets)
    }

    @Test(arguments: [
        "width=device-width, viewport-fit=cover",
        "width=device-width, viewport-fit=cover, viewport-fit-top=cover",
        "width=device-width, viewport-fit-top=cover, viewport-fit-left=cover, viewport-fit-bottom=cover, viewport-fit-right=cover",
    ])
    func safeAreaDoesNotAffectObscuredInsetsWhenEveryEdgeIsCovered(viewport: String) async throws {
        enablePerEdgeViewportFit()
        try await load(viewport: viewport)

        #expect(!webView._safeAreaShouldAffectObscuredInsets)
    }

    @Test
    func uiDelegateIsNotifiedWhenSafeAreaShouldAffectObscuredInsetsChanges() async throws {
        enablePerEdgeViewportFit()
        try await load(viewport: "width=device-width, viewport-fit-top=cover")

        let uiDelegate = SafeAreaUIDelegate()
        webView.uiDelegate = uiDelegate

        try await setViewport("width=device-width, viewport-fit-top=cover, viewport-fit-left=cover")
        try await setViewport("width=device-width, viewport-fit=cover")
        try await setViewport("width=device-width, viewport-fit=cover, viewport-fit-bottom=contain")

        #expect(uiDelegate.safeAreaShouldAffectObscuredInsetsChanges == [false, true])
    }
}

#endif // WTF_PLATFORM_IOS_FAMILY
