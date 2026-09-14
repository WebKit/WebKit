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
import Foundation
private import TestWebKitAPILibrary
private import TestWebKitAPILibrary.Helpers.cocoa.TestWKWebView
import Testing
import WebKit
private import WebKit_Private.WKPreferencesPrivate
private import WebKit_Private.WKWebViewPrivate

private let html = """
    <!DOCTYPE html>
    <html lang='en-US'>
    <head> <style>
        @font-face { font-family: customFont; src: url(Ahem.ttf); }
        body { font-family: customFont; -webkit-text-size-adjust:none; }
    </style> </head>
    <body> <p> <span id='testspan'>This is a test string</span> </p> </body>
    </html>
    """

@MainActor
struct TextSizeTests {
    private let webView = TestWKWebView(frame: CGRect(x: 0, y: 0, width: 800, height: 600))

    private func spanWidth() async throws -> Int {
        try await webView.callJavaScript(returning: Int.self) {
            "return document.getElementById('testspan').offsetWidth"
        }
    }

    @Test
    func textSize() async throws {
        webView.configuration.preferences._textAutosizingEnabled = false

        try await webView.load(html: html)

        #expect(try await spanWidth() == 336)

        webView._textZoomFactor = 2
        #expect(try await spanWidth() == 672)
    }
}
