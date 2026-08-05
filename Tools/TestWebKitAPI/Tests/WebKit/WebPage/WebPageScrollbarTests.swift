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
}

#endif // WTF_PLATFORM_MAC && ENABLE_SWIFTUI
