// Copyright (C) 2017-2026 Apple Inc. All rights reserved.
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
private import TestWebKitAPILibrary.Helpers.cocoa.TestWKWebView
import Testing
import WebKit

import struct Swift.String

#if WTF_PLATFORM_IOS_FAMILY
private import UIKit
#else
private import AppKit
#endif

@MainActor
struct CopyURLTests {
    private func readURLFromPasteboard() -> String? {
        #if WTF_PLATFORM_MAC
        if let url = NSURL(from: .general) {
            return url.absoluteString
        }
        return NSPasteboard.general.string(forType: .legacyURL).flatMap { NSURL(string: $0)?.absoluteString }
        #else
        return UIPasteboard.general.url?.absoluteString
        #endif
    }

    @Test
    func validURL() async throws {
        let webView = makeWebViewWithCustomPasteboardData()
        try await webView.load(testPageNamed: "copy-url")
        try await webView.callJavaScript(userGesture: false) { "URLToCopy = 'http://webkit.org/b/123';" }
        webView.copy(nil)
        webView.paste(nil)
        #expect(try await webView.callJavaScript(returning: Bool.self) { "return window.didCopy" })
        #expect(try await webView.callJavaScript(returning: Bool.self) { "return window.didPaste" })
        #expect(try await webView.callJavaScript(returning: String.self) { "return window.pastedURL" } == "http://webkit.org/b/123")
        #expect((readURLFromPasteboard() ?? "") == "http://webkit.org/b/123")
    }

    @Test
    func unescapedURL() async throws {
        let webView = makeWebViewWithCustomPasteboardData()
        try await webView.load(testPageNamed: "copy-url")
        try await webView.callJavaScript(userGesture: false) { "URLToCopy = 'http://webkit.org/b/\u{4F60}\u{597D};?x=8 + 6';" }
        webView.copy(nil)
        webView.paste(nil)
        #expect(try await webView.callJavaScript(returning: Bool.self) { "return window.didCopy" })
        #expect(try await webView.callJavaScript(returning: Bool.self) { "return window.didPaste" })
        #expect(
            try await webView.callJavaScript(returning: String.self) { "return window.pastedURL" }
                == "http://webkit.org/b/\u{4F60}\u{597D};?x=8 + 6"
        )
        #expect((readURLFromPasteboard() ?? "") == "http://webkit.org/b/%E4%BD%A0%E5%A5%BD;?x=8%20+%206")
    }

    @Test
    func malformedURL() async throws {
        let webView = makeWebViewWithCustomPasteboardData()
        try await webView.load(testPageNamed: "copy-url")
        try await webView.callJavaScript(userGesture: false) { "URLToCopy = 'bad url';" }
        webView.copy(nil)
        webView.paste(nil)
        #expect(try await webView.callJavaScript(returning: Bool.self) { "return window.didCopy" })
        #expect(try await webView.callJavaScript(returning: Bool.self) { "return window.didPaste" })
        #expect(try await webView.callJavaScript(returning: String.self) { "return window.pastedURL" } == "bad url")
        #expect((readURLFromPasteboard() ?? "") == "")
    }
}
