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

#if WTF_PLATFORM_MAC && ENABLE_SWIFTUI

import AppKit
import SwiftUI
import Testing
@_spi(Testing) import WebKit
import struct Swift.String
import TestWebKitAPILibrary

@MainActor
struct ScrollbarTests {
    private static let contentSize = NSSize(width: 500, height: 500)

    private nonisolated static let arguments = [
        (inset: -5, expectHit: false),
        (inset: 1, expectHit: true),
        (inset: 10, expectHit: true),
        (inset: 20, expectHit: false),
    ]

    private let page = WebPage()

    private let window: NSWindow

    init() async throws {
        self.window = NSWindow(size: Self.contentSize) { [page] in
            WebView(page)
        }
        self.window.setFrameOrigin(.zero)
        self.window.makeKeyAndOrderFront(nil)
    }

    @Test(arguments: Self.arguments)
    func detectsVerticalScrollbarOnRight(inset: Int, expectHit: Bool) async throws {
        let html = """
            <body style="margin: 0; width: 100%; height: 2000px;"></body>
            """

        try await page.load(html: html).wait()

        let point = NSPoint(x: Self.contentSize.width - CGFloat(inset), y: Self.contentSize.height / 2)

        #expect(page.isPointInScrollbar(locationInView: point) == expectHit)
    }

    @Test(arguments: Self.arguments)
    func detectsVerticalScrollbarOnLeft(inset: Int, expectHit: Bool) async throws {
        let html = """
            <html dir="rtl"><body style="margin: 0; width: 100%; height: 2000px;"></body></html>
            """

        try await page.load(html: html).wait()

        let point = NSPoint(x: CGFloat(inset), y: Self.contentSize.height / 2)

        #expect(page.isPointInScrollbar(locationInView: point) == expectHit)
    }

    @Test(arguments: Self.arguments)
    func detectsHorizontalScrollbarOnBottom(inset: Int, expectHit: Bool) async throws {
        let html = """
            <body style="margin: 0; width: 2000px;"></body>
            """

        try await page.load(html: html).wait()

        let point = NSPoint(x: Self.contentSize.width / 2, y: Self.contentSize.height - CGFloat(inset))

        #expect(page.isPointInScrollbar(locationInView: point) == expectHit)
    }

    private static let subscrollerMaxEdge: CGFloat = 400
    private static let subscrollerCenter: CGFloat = 250

    private func loadSubscroller(style: String = "", contents: String = "height: 2000px;", extraMarkup: String = "") async throws {
        let html = """
            <style>\(style)</style>
            <body style="margin: 0;">
                <div id="scroller" style="position: absolute; left: 100px; top: 100px; width: 300px; height: 300px; overflow: scroll;">
                    <div style="\(contents)"></div>
                </div>
                \(extraMarkup)
            </body>
            """

        try await page.load(html: html).wait()
        await page.waitForNextPresentationUpdate()
    }

    @Test(arguments: Self.arguments)
    func detectsVerticalScrollbarInSubscroller(inset: Int, expectHit: Bool) async throws {
        try await loadSubscroller()

        let point = NSPoint(x: Self.subscrollerMaxEdge - CGFloat(inset), y: Self.subscrollerCenter)

        #expect(page.isPointInScrollbar(locationInView: point) == expectHit)
    }

    @Test(arguments: Self.arguments)
    func detectsHorizontalScrollbarInSubscroller(inset: Int, expectHit: Bool) async throws {
        try await loadSubscroller(contents: "width: 2000px; height: 10px;")

        let point = NSPoint(x: Self.subscrollerCenter, y: Self.subscrollerMaxEdge - CGFloat(inset))

        #expect(page.isPointInScrollbar(locationInView: point) == expectHit)
    }

    @Test(arguments: Self.arguments)
    func detectsVerticalScrollbarInSubframe(inset: Int, expectHit: Bool) async throws {
        let html = """
            <body style="margin: 0;">
                <iframe style="position: absolute; left: 100px; top: 100px; width: 300px; height: 300px; border: none;" srcdoc="<body style='margin: 0; height: 2000px;'></body>"></iframe>
            </body>
            """

        try await page.load(html: html).wait()
        await page.waitForNextPresentationUpdate()

        let point = NSPoint(x: Self.subscrollerMaxEdge - CGFloat(inset), y: Self.subscrollerCenter)

        #expect(page.isPointInScrollbar(locationInView: point) == expectHit)
    }

    @Test
    func detectsCustomScrollbarInSubscroller() async throws {
        try await loadSubscroller(
            style: "#scroller::-webkit-scrollbar { width: 20px; height: 20px; } #scroller::-webkit-scrollbar-thumb { background: gray; }"
        )

        let point = NSPoint(x: Self.subscrollerMaxEdge - 10, y: Self.subscrollerCenter)

        #expect(page.isPointInScrollbar(locationInView: point))
    }

    @Test
    func ignoresScrollbarHiddenByStyleInSubscroller() async throws {
        try await loadSubscroller(style: "#scroller { scrollbar-width: none; }")

        let point = NSPoint(x: Self.subscrollerMaxEdge - 5, y: Self.subscrollerCenter)

        #expect(!page.isPointInScrollbar(locationInView: point))
    }

    @Test
    func ignoresSubscrollerScrollbarCoveredByOtherContent() async throws {
        try await loadSubscroller(
            extraMarkup: """
                <div style="position: absolute; left: 350px; top: 100px; width: 100px; height: 300px; background: white;"></div>
                """
        )

        let point = NSPoint(x: Self.subscrollerMaxEdge - 5, y: Self.subscrollerCenter)

        #expect(!page.isPointInScrollbar(locationInView: point))
    }

    @Test
    func ignoresSubscrollerScrollbarClippedByAncestor() async throws {
        let html = """
            <body style="margin: 0;">
                <div style="position: absolute; left: 100px; top: 100px; width: 250px; height: 300px; overflow: hidden;">
                    <div style="width: 300px; height: 300px; overflow: scroll;">
                        <div style="height: 2000px;"></div>
                    </div>
                </div>
            </body>
            """

        try await page.load(html: html).wait()
        await page.waitForNextPresentationUpdate()

        let point = NSPoint(x: Self.subscrollerMaxEdge - 5, y: Self.subscrollerCenter)

        #expect(!page.isPointInScrollbar(locationInView: point))
    }
}

#endif // WTF_PLATFORM_MAC && ENABLE_SWIFTUI
