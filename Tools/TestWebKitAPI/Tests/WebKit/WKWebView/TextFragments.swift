// Copyright (C) 2022-2026 Apple Inc. All rights reserved.
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
private import WebKit_Private.WKWebViewPrivate

import struct Foundation.URL
import struct Swift.String

@MainActor
struct TextFragmentTests {
    private let webView = TestWKWebView(
        frame: CGRect(x: 0, y: 0, width: 100, height: 100),
        configuration: WKWebViewConfiguration(),
        addToWindow: true
    )

    private func expectTextFragmentMatch(
        pageContent: String,
        textFragment: String,
        equals expectedResult: String?,
        sourceLocation: SourceLocation = #_sourceLocation
    ) async throws {
        // Load an empty baseURL-less string, otherwise using the same baseURL (modulo the fragment) does a same-document navigation.
        try await webView.load(html: "", baseURL: nil)
        try await webView.load(html: pageContent, baseURL: URL(string: "http://example.com/\(textFragment)"))

        #expect(await webView._getTextFragmentMatch() == expectedResult, sourceLocation: sourceLocation)
    }

    @Test
    func getTextFragmentMatch() async throws {
        try await expectTextFragmentMatch(
            pageContent: "hello world",
            textFragment: "#:~:text=hello%20world",
            equals: "hello world"
        )
        try await expectTextFragmentMatch(
            pageContent:
                "<span id='the'>The</span> quick brown fox <span id='jumps'>jumps</span> over the lazy <span id='dog'>dog.</span>",
            textFragment: "#:~:text=quick,jumps",
            equals: "quick brown fox jumps"
        )
        try await expectTextFragmentMatch(
            pageContent: "a the first match b the second match c",
            textFragment: "#:~:text=a-,the,match,-b",
            equals: "the first match"
        )
        try await expectTextFragmentMatch(
            pageContent: "a the first match b the second match c",
            textFragment: "#:~:text=b-,the,match,-c",
            equals: "the second match"
        )
        try await expectTextFragmentMatch(
            pageContent: "no match",
            textFragment: "#:~:text=hello%20world",
            equals: nil
        )
    }
}
