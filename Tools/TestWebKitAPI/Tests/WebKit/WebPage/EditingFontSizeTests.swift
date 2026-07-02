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

#if os(macOS) && ENABLE_SWIFTUI

import AppKit
import struct Swift.String
import Testing
@_spi(CrossImportOverlay) import WebKit
import WebKit_Private.WKPreferencesPrivate
import WebKit_Private.WKWebViewPrivate
private import TestWebKitAPILibrary

@MainActor
struct EditingFontSizeTests {
    @Test(.bug("rdar://15292320", "https://bugs.webkit.org/show_bug.cgi?id=312755"))
    func fontSizeMatchingLegacySizePreservesCSSValue() async throws {
        let page = WebPage()

        try await page.load(html: "<div id='target' contenteditable>Hello world</div>").wait()

        let webView = page.backingWebView

        try await page.callJavaScript(
            """
            getSelection().selectAllChildren(document.getElementById("target"));
            """
        )

        // 13px matches legacy font size 2 ("small") when the default font size is 16 (the default).
        webView._setFont(NSFont(name: "Helvetica", size: 13), sender: nil)

        let innerHTML = try #require(await page.callJavaScript("return document.getElementById('target').innerHTML") as? String)
        // The markup should contain a <font> element with size="2" for legacy compatibility.
        #expect(innerHTML.contains(#"size="2""#))
        // The markup should also contain the explicit CSS font-size for renderers that respect CSS.
        #expect(innerHTML.contains("font-size: 13px"))

        // Change the default font size so <font size="2"> alone would resolve to 10px.
        webView.configuration.preferences._defaultFontSize = 12

        let computedSize =
            try await page.callJavaScript(
                "return getComputedStyle(document.getElementById('target').querySelector('[style]')).fontSize"
            ) as? String
        #expect(computedSize == "13px")
    }
}

#endif
