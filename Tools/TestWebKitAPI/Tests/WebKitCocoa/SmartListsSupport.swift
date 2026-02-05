// Copyright (C) 2025 Apple Inc. All rights reserved.
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
import Foundation
@_spi(Testing) import WebKit

@objc
@implementation
extension SmartListsTestSelectionConfiguration {
    fileprivate let path: String
    fileprivate let offset: Int

    private init(path: String, offset: Int) {
        self.path = path
        self.offset = offset
    }

    @objc(caretSelectionWithPath:offset:)
    class func caretSelection(withPath path: String, offset: Int) -> SmartListsTestSelectionConfiguration {
        SmartListsTestSelectionConfiguration(path: path, offset: offset)
    }
}

@objc
@implementation
extension SmartListsTestConfiguration {
    fileprivate let expectedHTML: String
    fileprivate let expectedSelection: SmartListsTestSelectionConfiguration
    fileprivate let input: String
    fileprivate let stylesheet: String?

    init(expectedHTML: String, expectedSelection: SmartListsTestSelectionConfiguration, input: String, stylesheet: String?) {
        self.expectedHTML = expectedHTML
        self.expectedSelection = expectedSelection
        self.input = input
        self.stylesheet = stylesheet
    }
}

@objc
@implementation
extension SmartListsTestResult {
    let expectedRenderTree: String?
    let actualRenderTree: String?
    let expectedHTML: String?
    let actualHTML: String?

    fileprivate init(expectedRenderTree: String?, actualRenderTree: String?, expectedHTML: String?, actualHTML: String?) {
        self.expectedRenderTree = expectedRenderTree
        self.actualRenderTree = actualRenderTree
        self.expectedHTML = expectedHTML
        self.actualHTML = actualHTML
    }
}

@objc
@implementation
extension SmartListsSupport {
    @objc(processConfiguration:completionHandler:)
    open class func processConfiguration(_ configuration: SmartListsTestConfiguration) async throws -> SmartListsTestResult {
        let page = WebPage()

        page.setWebFeature("SmartListsAvailable", enabled: true)

        #if os(macOS)
        page.smartListsEnabled = true
        #endif

        let template = """
            <head>
              <meta charset="UTF-8">
            </head>
            \(configuration.stylesheet ?? "")
            """

        try await page.load(html: "\(template)<body contenteditable></body>").wait()

        try await page.callJavaScript("document.body.focus()")

        for character in configuration.input {
            await page.insertText("\(character)")
        }

        guard let actualHTML = try await page.callJavaScript("return document.body.outerHTML") as? String else {
            fatalError()
        }

        let actualTree = try await page.renderTree()

        let collapsedExpectedHTML = configuration.expectedHTML
            .split(separator: "\n")
            .map { $0.trimmingCharacters(in: .whitespacesAndNewlines) }
            .joined()

        let expectedHTMLWithEncoding = template + collapsedExpectedHTML

        try await page.load(html: expectedHTMLWithEncoding).wait()

        try await page.setCaretSelection(path: configuration.expectedSelection.path, offset: configuration.expectedSelection.offset)

        let expectedTree = try await page.renderTree()

        return .init(
            expectedRenderTree: expectedTree,
            actualRenderTree: actualTree,
            expectedHTML: configuration.expectedHTML,
            actualHTML: actualHTML
        )
    }
}

extension WebPage {
    fileprivate func setCaretSelection(path: String, offset: Int) async throws {
        let setSelectionScript = """
            const elementToPositionCaretAfter = document.evaluate(xPath, document, null, XPathResult.FIRST_ORDERED_NODE_TYPE, null).singleNodeValue;

            const range = document.createRange();
            range.setStart(elementToPositionCaretAfter, offset);
            range.setEnd(elementToPositionCaretAfter, offset);

            let selection = window.getSelection();
            selection.removeAllRanges();
            selection.addRange(range);
            """

        try await callJavaScript(setSelectionScript, arguments: ["xPath": path, "offset": offset])
    }
}

#endif // ENABLE_SWIFTUI
