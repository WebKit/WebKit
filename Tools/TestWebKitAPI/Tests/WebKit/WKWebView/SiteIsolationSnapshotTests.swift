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

import CoreGraphics
private import TestWebKitAPILibrary
import Testing
import WebKit
private import WebKit_Private.WKWebViewPrivate
private import WebKit_Private._WKTargetedElementInfo
private import WebKit_Private._WKTargetedElementRequest

import struct Foundation.URL
import struct Swift.String

/// A snapshot API that takes a remote snapshot of a page whose frames are hosted by more than one process.
enum RemoteSnapshotAPI: CaseIterable, CustomTestStringConvertible {
    /// `-[_WKTargetedElementInfo takeSnapshotWithCompletionHandler:]`, on every Cocoa platform.
    case targetedElementInfo
    #if WTF_PLATFORM_MAC
    /// `WKWebView.takeSnapshot(configuration:)`. On iOS, it takes a snapshot of the layers instead.
    case webView
    #endif

    var testDescription: String {
        switch self {
        case .targetedElementInfo: "_WKTargetedElementInfo"
        #if WTF_PLATFORM_MAC
        case .webView: "WKWebView"
        #endif
        }
    }
}

@MainActor
@Suite(.timeLimit(.minutes(1)))
struct SiteIsolationSnapshotTests {
    /// A remote snapshot draws a cross-site frame at the frame's own scroll position.
    ///
    /// The process of the frame's parent learns of that scroll position asynchronously, so the frame is scrolled back
    /// and forth with a snapshot right after each scroll, and then once more after the next presentation update, when
    /// both processes know it.
    @Test(arguments: RemoteSnapshotAPI.allCases)
    func snapshotDrawsScrolledCrossSiteFrameAtItsScrollPosition(api: RemoteSnapshotAPI) async throws {
        var server = try HTTPServer(protocol: .httpsProxy) {
            // Element targeting rejects an element that is the page's only content, hence the paragraph.
            Route("/mainframe") {
                """
                <body style='margin: 0'><iframe id='frame' src='https://webkit.org/iframe' \
                style='display: block; width: 300px; height: 200px; border: none'></iframe><p>main frame text</p></body>
                """
            }

            // Red for the first 1000px of the document, green below.
            Route("/iframe") {
                """
                <body style='margin: 0'><div style='height: 1000px; background-color: rgb(255, 0, 0)'></div>\
                <div style='height: 1000px; background-color: rgb(0, 255, 0)'></div></body>
                """
            }
        }

        try await server.run { serverConfiguration in
            let (webView, navigationDelegate) = siteIsolatedWebViewAndDelegate(
                configuration: WKWebViewConfiguration(httpsProxyFor: serverConfiguration),
                frame: CGRect(x: 0, y: 0, width: 800, height: 600)
            )
            webView.load(URLRequest(url: try #require(URL(string: "https://example.com/mainframe"))))
            try await navigationDelegate.waitForDidFinishNavigation()
            await webView.nextPresentationUpdate()

            let frame = try #require(await webView.firstChildFrame)

            let takeSnapshot: () async throws -> CocoaImage
            switch api {
            case .targetedElementInfo:
                let targets = try #require(
                    await webView._requestTargetedElementInfo(_WKTargetedElementRequest(selectors: [["#frame"]]))
                )
                #expect(targets.count == 1)
                let target = try #require(targets.first)
                takeSnapshot = { cocoaImage(await target.takeSnapshot()) }
            #if WTF_PLATFORM_MAC
            case .webView:
                let configuration = WKSnapshotConfiguration()
                configuration.rect = CGRect(x: 0, y: 0, width: 300, height: 200)
                takeSnapshot = { try await webView.takeSnapshot(configuration: configuration) }
            #endif
            }

            func scrollFrame(to y: Int) async throws {
                let scrollY = try await webView.callJavaScript(returning: Int.self, in: frame) {
                    "window.scrollTo(0, \(y)); return window.scrollY"
                }
                #expect(scrollY == y)
            }

            @MainActor
            func expectSnapshotColor(_ expectedColor: CocoaColor) async throws {
                let snapshot = try await takeSnapshot()
                let center = CGPoint(x: snapshot.size.width / 2, y: snapshot.size.height / 2)
                #expect(compareColors(pixelColor(of: snapshot, at: center), expectedColor), "snapshot taken with \(api)")
            }

            let red = CocoaColor(red: 1, green: 0, blue: 0, alpha: 1)
            let green = CocoaColor(red: 0, green: 1, blue: 0, alpha: 1)

            for _ in 0..<3 {
                try await scrollFrame(to: 1000)
                try await expectSnapshotColor(green)
                try await scrollFrame(to: 0)
                try await expectSnapshotColor(red)
            }

            try await scrollFrame(to: 1000)
            await webView.nextPresentationUpdate()
            try await expectSnapshotColor(green)
        }
    }
}

private func cocoaImage(_ image: CGImage) -> CocoaImage {
    #if WTF_PLATFORM_MAC
    NSImage(cgImage: image, size: .zero)
    #else
    UIImage(cgImage: image)
    #endif
}
