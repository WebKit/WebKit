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

#if ENABLE_SWIFTUI

import struct Swift.String
import Testing
import WebKit
private import TestWebKitAPILibrary

@MainActor
struct JavaScriptExpressionTests {
    private static let text = "Here's to the crazy ones."

    private static let html = """
        <div id="div" contenteditable style="font-size: 30px;">\(text)</div>
        """

    @Test
    func setAndGetSelectionWithCollapsedPositionRoundTrip() async throws {
        let page = WebPage()
        try await page.load(html: Self.html).wait()

        let selection: JavaScriptSelection = .collapsed(.init(in: "div", at: 5))

        try await page.callJavaScript(JavaScriptMessages.SetSelection(selection))

        let actualSelection = try await page.callJavaScript(JavaScriptMessages.GetSelection())

        #expect(actualSelection == selection)
    }

    @Test
    func setSelectionAppliesRangeSelection() async throws {
        let page = WebPage()
        try await page.load(html: Self.html).wait()

        let text = Self.text
        let range = try #require(text.range(of: "crazy"))
        let start = range.lowerBound.utf16Offset(in: text)
        let end = range.upperBound.utf16Offset(in: text)

        try await page.callJavaScript(
            JavaScriptMessages.SetSelection(.range(base: .init(in: "div", at: start), extent: .init(in: "div", at: end)))
        )

        let selectedText = try #require(await page.callJavaScript("return getSelection().toString();") as? String)
        #expect(selectedText == "crazy")
    }

    @Test
    func getSelectionReturnsCollapsedAfterCollapsedIsSet() async throws {
        let page = WebPage()
        try await page.load(html: Self.html).wait()

        try await page.callJavaScript(
            """
            const node = document.getElementById("div").firstChild;
            getSelection().setPosition(node, 3);
            """
        )

        let actualSelection = try await page.callJavaScript(JavaScriptMessages.GetSelection())

        let expectedSelection: JavaScriptSelection = .collapsed(.init(in: "div", at: 3))

        #expect(actualSelection == expectedSelection)
    }

    @Test
    func getSelectionReturnsRangeAfterRangeIsSet() async throws {
        let page = WebPage()
        try await page.load(html: Self.html).wait()

        let text = Self.text
        let range = try #require(text.range(of: "crazy"))
        let start = range.lowerBound.utf16Offset(in: text)
        let end = range.upperBound.utf16Offset(in: text)

        try await page.callJavaScript(
            """
            const node = document.getElementById("div").firstChild;
            getSelection().setBaseAndExtent(node, \(start), node, \(end));
            """
        )

        let actualSelection = try await page.callJavaScript(JavaScriptMessages.GetSelection())

        let expectedSelection: JavaScriptSelection = .range(
            base: .init(in: "div", at: start),
            extent: .init(in: "div", at: end)
        )

        #expect(actualSelection == expectedSelection)
    }

    @Test
    func setAndGetSelectionRangeRoundTrip() async throws {
        let page = WebPage()
        try await page.load(html: Self.html).wait()

        let selection: JavaScriptSelection = .range(
            base: .init(in: "div", at: 10),
            extent: .init(in: "div", at: 15)
        )

        try await page.callJavaScript(JavaScriptMessages.SetSelection(selection))

        let actualSelection = try await page.callJavaScript(JavaScriptMessages.GetSelection())

        #expect(actualSelection == selection)
    }

    @Test
    func selectionBoundingClientRectReturnsNonEmptyRectForRangeSelection() async throws {
        let page = WebPage()
        try await page.load(html: Self.html).wait()

        let text = Self.text
        let range = try #require(text.range(of: "crazy"))
        let start = range.lowerBound.utf16Offset(in: text)
        let end = range.upperBound.utf16Offset(in: text)

        let selection: JavaScriptSelection = .range(base: .init(in: "div", at: start), extent: .init(in: "div", at: end))

        try await page.callJavaScript(JavaScriptMessages.SetSelection(selection))

        let rect = try await page.callJavaScript(JavaScriptMessages.BoundingClientRect(selection))

        #expect(rect.x > 0)
        #expect(rect.y > 0)
        #expect(rect.width > 0)
        #expect(rect.height > 0)
    }

    @Test
    func selectionBoundingClientRectReturnsZeroWidthRectForCollapsedSelection() async throws {
        let page = WebPage()
        try await page.load(html: Self.html).wait()

        let selection: JavaScriptSelection = .collapsed(.init(in: "div", at: 0))

        try await page.callJavaScript(JavaScriptMessages.SetSelection(selection))

        let rect = try await page.callJavaScript(JavaScriptMessages.BoundingClientRect(selection))

        #expect(rect.width == 0)
        #expect(rect.height > 0)
    }
}

#endif // ENABLE_SWIFTUI
