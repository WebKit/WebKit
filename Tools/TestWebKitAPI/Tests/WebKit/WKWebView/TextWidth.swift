// Copyright (C) 2019-2026 Apple Inc. All rights reserved.
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
import CoreText
import Foundation
private import TestWebKitAPILibrary
private import TestWebKitAPILibrary.Helpers.cocoa.TestWKWebView
import Testing
import WebKit

import struct Swift.String

@MainActor
struct TextWidthTests {
    private let webView = WKWebView(frame: CGRect(x: 0, y: 0, width: 800, height: 600), configuration: WKWebViewConfiguration())

    @Test
    func textWidth() async throws {
        try await webView.load(testPageNamed: "TextWidth")

        let webKitWidth = try await webView.callJavaScript(returning: Double.self) { "return runTest1()" }

        // Use CFAttributedString so we don't have to deal with NSFont / UIFont and have this code be platform-dependent.
        let font = try #require(CTFontCreateUIFontForLanguage(.system, 24, "en-US" as CFString))
        let attributes = [kCTFontAttributeName: font] as CFDictionary
        let attributedString = CFAttributedStringCreate(kCFAllocatorDefault, "This is a test string" as CFString, attributes)
        let line = CTLineCreateWithAttributedString(try #require(attributedString))
        let coreTextWidth = CTLineGetTypographicBounds(line, nil, nil, nil)

        #expect(abs(webKitWidth - coreTextWidth) <= 3)
    }
}
