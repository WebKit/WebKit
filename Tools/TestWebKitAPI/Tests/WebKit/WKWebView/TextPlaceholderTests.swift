// Copyright (C) 2024-2026 Apple Inc. All rights reserved.
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

#if HAVE_NSTEXTPLACEHOLDER_RECTS

import CoreGraphics
private import TestWebKitAPILibrary
private import TestWebKitAPILibrary.Helpers.cocoa.TestWKWebView
private import TestWebKitAPILibrary.Helpers.cocoa.WKWebViewConfigurationExtras
private import TestWebKitAPILibrary.Helpers.mac.AppKitSPI
private import pal.spi.mac.NSTextInputContextSPI
import Testing
import WebKit

import struct Swift.String

private let placeholderSize = CGSize(width: 50, height: 100)

private let bodyHTML = "Test<script>document.body.focus()</script>"

private let placeholderHTML = """
    <div style="display: inline-block; vertical-align: top; \
    visibility: hidden !important; width: 50px; height: 100px;"></div>
    """

@MainActor
struct NSTextPlaceholderTests {
    private let webView: TestWKWebView

    init() async throws {
        let configuration = WKWebViewConfiguration._test_configurationWithTestPlugInClassName(
            "WebProcessPlugInWithInternals",
            configureJSCForTesting: true
        )

        webView = TestWKWebView(frame: CGRect(x: 0, y: 0, width: 320, height: 500), configuration: configuration)

        try await webView.load(html: "<body contenteditable>Test</body><script>document.body.focus()</script>")
    }

    private func bodyInnerHTML() async throws -> String {
        try await webView.callJavaScript(returning: String.self) { "return document.body.innerHTML" }
    }

    private func isCaretVisible() async throws -> Bool {
        try await webView.callJavaScript(returning: Bool.self) { "return internals.isCaretVisible()" }
    }

    private func insertPlaceholder() async throws -> NSTextPlaceholder {
        // `NSTextInputClient_Async` is not `NS_SWIFT_UI_ACTOR`, so `await`-ing the functions directly does not work.

        struct UncheckedSendable<Value>: @unchecked Sendable {
            let value: Value
        }

        let client = try #require(webView as? any NSTextInputClient_Async)

        let placeholder = await withCheckedContinuation { continuation in
            client.insertTextPlaceholder?(with: placeholderSize) {
                continuation.resume(returning: UncheckedSendable(value: $0))
            }
        }

        return try #require(placeholder.value)
    }

    private func removePlaceholder(_ placeholder: NSTextPlaceholder, willInsertText: Bool) async throws {
        // `NSTextInputClient_Async` is not `NS_SWIFT_UI_ACTOR`, so `await`-ing the functions directly does not work.

        let client = try #require(webView as? any NSTextInputClient_Async)

        await withCheckedContinuation { continuation in
            client.remove?(placeholder, willInsertText: willInsertText) {
                continuation.resume()
            }
        }
    }

    @Test(arguments: [true, false])
    func insertAndRemoveTextPlaceholder(willInsertText: Bool) async throws {
        let placeholder = try await insertPlaceholder()

        #expect(try await bodyInnerHTML() == placeholderHTML + bodyHTML)
        #expect(placeholder.rects.first?.rect.size == placeholderSize)
        #expect(try await !isCaretVisible())

        try await removePlaceholder(placeholder, willInsertText: willInsertText)

        #expect(try await bodyInnerHTML() == bodyHTML)
        #expect(try await isCaretVisible())
    }
}

#endif // HAVE_NSTEXTPLACEHOLDER_RECTS
