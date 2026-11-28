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
import UniformTypeIdentifiers
import WebKit

import struct Swift.String

#if WTF_PLATFORM_IOS_FAMILY
private import UIKit
#else
private import AppKit
#endif

private func writeRTFToPasteboard(_ data: Data) {
    #if WTF_PLATFORM_MAC
    NSPasteboard.general.declareTypes([.rtf], owner: nil)
    NSPasteboard.general.setData(data, forType: .rtf)
    #else
    UIPasteboard.general.setItems([[UTType.rtf.identifier: data]])
    #endif
}

private func writeRTFDToPasteboard(_ data: Data) {
    #if WTF_PLATFORM_MAC
    NSPasteboard.general.declareTypes([.legacyRTFD], owner: nil)
    NSPasteboard.general.setData(data, forType: .legacyRTFD)
    #else
    UIPasteboard.general.setItems([[UTType.flatRTFD.identifier: data]])
    #endif
}

private func rtfData(of string: NSAttributedString, sourceLocation: SourceLocation = #_sourceLocation) throws -> Data {
    let range = NSRange(location: 0, length: string.length)
    #if USE_APPKIT
    return try #require(string.rtf(from: range, documentAttributes: [:]), sourceLocation: sourceLocation)
    #else
    // UIKit has RTFFromRange:documentAttributes: too, but only as SPI.
    return try string.data(from: range, documentAttributes: [.documentType: NSAttributedString.DocumentType.rtf])
    #endif
}

private func rtfdData(of string: NSAttributedString, sourceLocation: SourceLocation = #_sourceLocation) throws -> Data {
    let range = NSRange(location: 0, length: string.length)
    #if USE_APPKIT
    return try #require(string.rtfd(from: range, documentAttributes: [:]), sourceLocation: sourceLocation)
    #else
    return try string.data(from: range, documentAttributes: [.documentType: NSAttributedString.DocumentType.rtfd])
    #endif
}

private func makeHelloWorldString() -> NSAttributedString {
    let string = NSMutableAttributedString()
    string.append(NSAttributedString(string: "hello", attributes: [.underlineStyle: NSUnderlineStyle.single.rawValue]))
    string.append(NSAttributedString(string: ", world", attributes: [:]))
    return string
}

private func makeImageAttachmentString(sourceLocation: SourceLocation = #_sourceLocation) throws -> NSAttributedString {
    let pngURL = try #require(
        Bundle.testResources.url(forResource: "sunset-in-cupertino-200px", withExtension: "png"),
        sourceLocation: sourceLocation
    )
    let attachment = NSTextAttachment(data: try Data(contentsOf: pngURL), ofType: UTType.png.identifier)
    return NSAttributedString(attachment: attachment)
}

@MainActor
struct PasteRTFTests {
    @Test
    func exposesHTMLTypeInDataTransfer() async throws {
        let webView = makeWebViewWithCustomPasteboardData()
        try await webView.load(testPageNamed: "paste-rtfd")

        writeRTFToPasteboard(try rtfData(of: makeHelloWorldString()))
        webView.paste(nil)

        #expect(try await webView.callJavaScript(returning: Bool.self) { "return clipboardData.types.includes('text/html')" })
        try await webView.callJavaScript(userGesture: false) { "editor.innerHTML = clipboardData.values[0]; editor.focus()" }
        #expect(try await webView.callJavaScript(returning: Bool.self) { "return document.queryCommandState('underline')" })
        try await webView.callJavaScript(userGesture: false) { "getSelection().modify('move', 'forward', 'lineboundary')" }
        #expect(try await !webView.callJavaScript(returning: Bool.self) { "return document.queryCommandState('underline')" })
        #expect(try await webView.callJavaScript(returning: String.self) { "return editor.textContent" } == "hello, world")
    }
}

@MainActor
struct PasteRTFDTests {
    // This test has no checks; the paste only must not crash.
    @Test
    func emptyRTFD() async throws {
        let webView = makeWebViewWithCustomPasteboardData()
        try await webView.load(html: "<!DOCTYPE html><html><body><div id='editor' contenteditable></div></body></html>")

        writeRTFDToPasteboard(Data())
        try await webView.callJavaScript(userGesture: false) { "document.getElementById('editor').focus()" }
        webView.paste(nil)
    }

    @Test
    func exposesHTMLTypeInDataTransfer() async throws {
        let webView = makeWebViewWithCustomPasteboardData()
        try await webView.load(testPageNamed: "paste-rtfd")

        writeRTFDToPasteboard(try rtfdData(of: makeHelloWorldString()))
        webView.paste(nil)

        #expect(try await webView.callJavaScript(returning: Bool.self) { "return clipboardData.types.includes('text/html')" })
        try await webView.callJavaScript(userGesture: false) { "editor.innerHTML = clipboardData.values[0]; editor.focus()" }
        #expect(try await webView.callJavaScript(returning: Bool.self) { "return document.queryCommandState('underline')" })
        try await webView.callJavaScript(userGesture: false) { "getSelection().modify('move', 'forward', 'lineboundary')" }
        #expect(try await !webView.callJavaScript(returning: Bool.self) { "return document.queryCommandState('underline')" })
        #expect(try await webView.callJavaScript(returning: String.self) { "return editor.textContent" } == "hello, world")
    }

    @Test
    func imageElementUsesBlobURL() async throws {
        let webView = makeWebViewWithCustomPasteboardData()
        try await webView.load(testPageNamed: "paste-rtfd")

        writeRTFDToPasteboard(try rtfdData(of: makeImageAttachmentString()))

        await webView.waitForMessage("loaded") {
            webView.paste(nil)
        }
        #expect(
            try await webView.callJavaScript(returning: Int.self) {
                "imageElement = document.querySelector('img'); return imageElement.width"
            } == 200
        )
        #expect(try await webView.callJavaScript(returning: String.self) { "return url = new URL(imageElement.src).protocol" } == "blob:")
    }

    @Test
    func imageElementUsesBlobURLInHTML() async throws {
        let webView = makeWebViewWithCustomPasteboardData()
        try await webView.load(testPageNamed: "paste-rtfd")

        writeRTFDToPasteboard(try rtfdData(of: makeImageAttachmentString()))

        await webView.waitForMessage("loaded") {
            webView.paste(nil)
        }
        #expect(
            try await webView.callJavaScript(returning: String.self) { "return JSON.stringify(clipboardData.types)" } == "[\"text/html\"]"
        )
        let rawMarkup = try await webView.callJavaScript(returning: String.self) { "return clipboardData.values[0]" }
        #expect(!rawMarkup.contains("<p"))
        #expect(!rawMarkup.contains("<br"))
        #expect(
            try await webView.callJavaScript(returning: Bool.self) {
                "imageElement = (new DOMParser).parseFromString(clipboardData.values[0], 'text/html').querySelector('img'); return !!imageElement"
            }
        )
        #expect(try await webView.callJavaScript(returning: String.self) { "return new URL(imageElement.src).protocol" } == "blob:")
    }

    #if ENABLE_DARK_MODE_CSS

    @Test
    func transformColorsOfDarkContent() async throws {
        let webView = makeWebViewWithCustomPasteboardData(colorFilterEnabled: true)
        webView.forceDarkMode()

        try await webView.load(testPageNamed: "rich-color-filtered")

        let textColor = CocoaColor.lightGray
        let backgroundColor = CocoaColor.darkGray

        let string = NSMutableAttributedString()
        string.append(NSAttributedString(string: "Hello", attributes: [.backgroundColor: backgroundColor, .foregroundColor: textColor]))
        string.append(NSAttributedString(string: " World", attributes: [.foregroundColor: textColor]))

        writeRTFToPasteboard(try rtfData(of: string))

        try await webView.callJavaScript(userGesture: false) { "selectRichText()" }
        webView.paste(nil)

        #if USE_APPKIT
        #expect(
            try await webView.callJavaScript(returning: String.self) { "return rich.querySelector('p').style.color" }
                == "rgb(126, 126, 126)"
        )
        #expect(
            try await webView.callJavaScript(returning: String.self) { "return rich.querySelector('p > span').style.backgroundColor" }
                == "rgb(235, 235, 235)"
        )
        #else
        #expect(
            try await webView.callJavaScript(returning: String.self) { "return rich.querySelector('p').style.color" }
                == "rgb(106, 106, 106)"
        )
        #expect(
            try await webView.callJavaScript(returning: String.self) { "return rich.querySelector('p > span').style.backgroundColor" }
                == "rgb(213, 213, 213)"
        )
        #endif
    }

    @Test
    func doesNotTransformColorsOfLightContent() async throws {
        let webView = makeWebViewWithCustomPasteboardData(colorFilterEnabled: true)
        webView.forceDarkMode()

        try await webView.load(testPageNamed: "rich-color-filtered")

        let textColor = CocoaColor.darkGray
        let backgroundColor = CocoaColor.lightGray

        let string = NSMutableAttributedString()
        string.append(NSAttributedString(string: "Hello", attributes: [.backgroundColor: backgroundColor, .foregroundColor: textColor]))
        string.append(NSAttributedString(string: " World", attributes: [.foregroundColor: textColor]))

        writeRTFToPasteboard(try rtfData(of: string))

        try await webView.callJavaScript(userGesture: false) { "selectRichText()" }
        webView.paste(nil)

        #if USE_APPKIT
        #expect(
            try await webView.callJavaScript(returning: String.self) { "return rich.querySelector('p').style.color" } == "rgb(67, 67, 67)"
        )
        #expect(
            try await webView.callJavaScript(returning: String.self) { "return rich.querySelector('p > span').style.backgroundColor" }
                == "rgb(154, 154, 154)"
        )
        #else
        #expect(
            try await webView.callJavaScript(returning: String.self) { "return rich.querySelector('p').style.color" } == "rgb(85, 85, 85)"
        )
        #expect(
            try await webView.callJavaScript(returning: String.self) { "return rich.querySelector('p > span').style.backgroundColor" }
                == "rgb(170, 170, 170)"
        )
        #endif
    }

    #endif // ENABLE_DARK_MODE_CSS
}
