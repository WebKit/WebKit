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

import CoreGraphics
import Foundation
private import TestWebKitAPILibrary
private import TestWebKitAPILibrary.Helpers.cocoa.TestWKWebView
import Testing
import UniformTypeIdentifiers
import WebKit
private import WebKit_Private.NSAttributedStringPrivate
private import WebKit_Private.WKPreferencesPrivate

import struct Foundation.URL
import struct Swift.String

#if WTF_PLATFORM_IOS_FAMILY
private import UIKit
#else
private import AppKit
#endif

@MainActor
struct CopyHTMLTests {
    private func readHTMLDataFromPasteboard() -> Data? {
        #if WTF_PLATFORM_MAC
        NSPasteboard.general.data(forType: .legacyHTML)
        #else
        UIPasteboard.general.data(forPasteboardType: UTType.html.identifier)
        #endif
    }

    private func readHTMLStringFromPasteboard() -> String? {
        #if WTF_PLATFORM_MAC
        NSPasteboard.general.string(forType: .legacyHTML)
        #else
        switch UIPasteboard.general.value(forPasteboardType: UTType.html.identifier) {
        case let data as Data:
            String(data: data, encoding: .utf8)
        case let string as String:
            string
        default:
            nil
        }
        #endif
    }

    private func readWebArchiveDataFromPasteboard() -> Data? {
        #if WTF_PLATFORM_MAC
        NSPasteboard.general.data(forType: NSPasteboard.PasteboardType(UTType.webArchive.identifier))
        #else
        UIPasteboard.general.data(forPasteboardType: UTType.webArchive.identifier)
        #endif
    }

    private func copyAndLoadAttributedStringUsingWebArchive(
        _ webView: TestWKWebView,
        sourceLocation: SourceLocation = #_sourceLocation
    ) async throws -> NSAttributedString {
        webView.copy(nil)
        await webView.nextPresentationUpdate()

        let archiveData = readWebArchiveDataFromPasteboard() ?? Data()
        let mimeType = UTType.webArchive.preferredMIMEType ?? ""
        let blankURL = try #require(URL(string: "about:blank"), sourceLocation: sourceLocation)
        let (string, _) = try await NSAttributedString._loadFromHTML(options: [:]) { loadingWebView in
            // The content loader must return the main frame's navigation, which loading data always starts.
            loadingWebView.load(archiveData, mimeType: mimeType, characterEncodingName: "", baseURL: blankURL)!
        }
        return string
    }

    // When a frameset document is copied, LegacyWebArchive::createFromSelection wraps the frameset in an
    // <iframe> so it can be pasted into a document that has its own body/frameset. The wrapper markup must
    // use a single-percent width/height ("98%"), not a printf-style escaped "98%%".
    @Test
    func framesetSelectionIframeWrapperUsesSinglePercent() async throws {
        let webView = makeWebViewWithCustomPasteboardData()
        try await webView.load(
            html: #"<frameset rows="50%,50%">"# + #"<frame src="about:blank">"# + #"<frame src="about:blank">"# + "</frameset>",
            baseURL: URL(string: "https://webkit.org/frameset.html")
        )
        try await webView.callJavaScript(userGesture: false) { "getSelection().selectAllChildren(document.documentElement)" }
        webView.copy(nil)
        await webView.nextPresentationUpdate()

        let archiveData = readWebArchiveDataFromPasteboard()
        #expect((archiveData?.count ?? 0) > 0)

        let propertyList = archiveData.flatMap { try? unsafe PropertyListSerialization.propertyList(from: $0, options: 0, format: nil) }
        let archive = propertyList as? [String: Any]
        let mainResource = archive?["WebMainResource"] as? [String: Any]
        let mainResourceData = mainResource?["WebResourceData"] as? Data
        let mainMarkup = mainResourceData.flatMap { String(data: $0, encoding: .utf8) } ?? ""

        #expect(mainMarkup.contains("<iframe"))
        #expect(mainMarkup.contains(#"width="98%""#))
        #expect(mainMarkup.contains(#"height="98%""#))
        #expect(!mainMarkup.contains("98%%"))

        // The frameset itself is preserved as a subframe archive of the iframe wrapper.
        #expect(archive?["WebSubframeArchives"] != nil)
    }

    @Test
    func sanitizes() async throws {
        let webView = makeWebViewWithCustomPasteboardData()
        try await webView.load(testPageNamed: "copy-html")
        try await webView.callJavaScript(userGesture: false) {
            #"HTMLToCopy = '<meta content="secret"><b onmouseover="dangerousCode()">hello</b>"#
                + "<!-- secret-->, world<script>dangerousCode()</script>';"
        }
        webView.copy(nil)
        webView.paste(nil)
        #expect(try await webView.callJavaScript(returning: Bool.self) { "return didCopy" })
        #expect(try await webView.callJavaScript(returning: Bool.self) { "return didPaste" })
        #expect(
            try await webView.callJavaScript(returning: String.self) { "return pastedHTML" }
                == #"<meta content="secret"><b onmouseover="dangerousCode()">hello</b><!-- secret-->, world<script>dangerousCode()</script>"#
        )
        let htmlInNativePasteboard = readHTMLStringFromPasteboard() ?? ""
        #expect(htmlInNativePasteboard.contains("hello"))
        #expect(htmlInNativePasteboard.contains(", world"))
        #expect(!htmlInNativePasteboard.contains("secret"))
        #expect(!htmlInNativePasteboard.contains("dangerousCode"))
    }

    @Test
    func sanitizationPreservesCharacterSetInSelectedText() async throws {
        let webView = makeWebViewWithCustomPasteboardData()
        try await webView.load(
            html: """
                <!DOCTYPE html>\
                <body>\
                <meta charset='utf-8'>\
                <span id='copy'>我叫謝文昇</span>\
                <script>getSelection().selectAllChildren(copy);</script>\
                </body>
                """
        )
        webView.copy(nil)
        await webView.nextPresentationUpdate()

        let copiedMarkup = readHTMLStringFromPasteboard() ?? ""
        #expect(copiedMarkup.contains("<span "))
        #expect(copiedMarkup.contains("我叫謝文昇"))
        #expect(copiedMarkup.contains("</span>"))

        let attributedString = try unsafe NSAttributedString(
            data: try #require(readHTMLDataFromPasteboard()),
            options: [.documentType: NSAttributedString.DocumentType.html],
            documentAttributes: nil
        )
        #expect(attributedString.string == "我叫謝文昇")
    }

    // FIXME: rdar://158345822
    #if WTF_PLATFORM_IOS
    @Test(.disabled("rdar://158345822"))
    #else
    @Test
    #endif
    func sanitizationPreservesCharacterSet() async throws {
        var markupStringsAndData: [(String?, Data?)] = []
        let webView = makeWebViewWithCustomPasteboardData()
        for encodingName in ["utf-8", "windows-1252", "bogus-encoding"] {
            try await webView.load(
                html: """
                    <!DOCTYPE html>\
                    <body>\
                    <meta charset='\(encodingName)'>\
                    <p id='copy'>Copy me</p>\
                    <script>\
                    copy.addEventListener('copy', e => {\
                        e.clipboardData.setData('text/html', `<span style='color: red;'>我叫謝文昇</span>`);\
                        e.preventDefault();\
                    });\
                    getSelection().selectAllChildren(copy);\
                    </script>\
                    </body>
                    """
            )
            webView.copy(nil)
            await webView.nextPresentationUpdate()

            markupStringsAndData.append((readHTMLStringFromPasteboard(), readHTMLDataFromPasteboard()))
        }

        for (copiedMarkup, copiedData) in markupStringsAndData {
            let copiedMarkup = copiedMarkup ?? ""
            #expect(copiedMarkup.contains("<span "))
            #expect(copiedMarkup.contains("color: red;"))
            #expect(copiedMarkup.contains("我叫謝文昇"))
            #expect(copiedMarkup.contains("</span>"))

            let attributedString = try unsafe NSAttributedString(
                data: try #require(copiedData),
                options: [.documentType: NSAttributedString.DocumentType.html],
                documentAttributes: nil
            )
            #expect(attributedString.string == "我叫謝文昇")

            var foundColorAttribute = false
            unsafe attributedString.enumerateAttribute(.foregroundColor, in: NSRange(location: 0, length: 5)) { value, _, _ in
                var redComponent: CGFloat = 0
                var greenComponent: CGFloat = 0
                var blueComponent: CGFloat = 0
                unsafe (value as? CocoaColor)?.getRed(&redComponent, green: &greenComponent, blue: &blueComponent, alpha: nil)

                #expect(redComponent == 1)
                #expect(greenComponent == 0)
                #expect(blueComponent == 0)
                foundColorAttribute = true
            }
            #expect(foundColorAttribute)
        }
    }

    @Test
    func sanitizationPreservesRelativeURLInAttributedString() async throws {
        let webView = makeWebViewWithCustomPasteboardData()
        try await webView.load(
            html: "<!DOCTYPE html>" + "<html>" + "<head><base href='https://webkit.org/' /></head>"
                + "<body><a href='/downloads'>Click</a> for downloads</body>" + "</html>"
        )
        try await webView.callJavaScript(userGesture: false) { "getSelection().selectAllChildren(document.body)" }

        let resultString = try await copyAndLoadAttributedStringUsingWebArchive(webView)
        var links: [Any?] = []
        unsafe resultString.enumerateAttribute(.link, in: NSRange(location: 0, length: 5)) { value, _, _ in
            links.append(value)
        }

        #expect(resultString.string == "Click for downloads")
        #expect(links.count == 1)
        #expect(((links.first ?? nil) as? URL)?.absoluteString == "https://webkit.org/downloads")
    }

    @Test
    func copyingRightToLeftTextPreservesDirection() async throws {
        let webView = makeWebViewWithCustomPasteboardData()
        try await webView.load(testPageNamed: "rtl-bidi-text")
        try await webView.callJavaScript(userGesture: false) { "getSelection().selectAllChildren(document.querySelector('p'))" }
        let expectedPlainText = try await webView.callJavaScript(returning: String.self) { "return getSelection().toString()" }

        let resultString = try await copyAndLoadAttributedStringUsingWebArchive(webView)

        var paragraphCount = 0
        let plainText = resultString.string
        unsafe resultString.enumerateAttribute(.paragraphStyle, in: NSRange(location: 0, length: resultString.length)) { value, _, _ in
            #expect((value as? NSParagraphStyle)?.baseWritingDirection == .rightToLeft)
            paragraphCount += 1
        }

        #expect(paragraphCount == 1)
        #expect(plainText == expectedPlainText)
    }

    #if WTF_PLATFORM_MAC

    @Test
    func itemTypesWhenCopyingWebContent() async throws {
        let webView = makeWebViewWithCustomPasteboardData()
        try await webView.load(html: "<strong style='color: rgb(255, 0, 0);'>This is some text to copy.</strong>")
        try await webView.callJavaScript(userGesture: false) { "getSelection().selectAllChildren(document.body)" }
        webView.copy(nil)
        await webView.nextPresentationUpdate()

        let items = NSPasteboard.general.pasteboardItems ?? []
        #expect(items.count == 1)

        let types = items.first?.types ?? []
        #expect(types.contains(NSPasteboard.PasteboardType(UTType.webArchive.identifier)))
        #expect(types.contains(.rtf))
        #expect(types.contains(.string))
        #expect(types.contains(.html))
    }

    @Test
    func writeRichTextSelectionToPasteboard() async throws {
        let webView = makeWebViewWithCustomPasteboardData()
        try await webView.load(html: "<strong style='color: rgb(255, 0, 0);'>This is some text to copy.</strong>")
        try await webView.callJavaScript(userGesture: false) { "getSelection().selectAllChildren(document.body)" }
        await webView.nextPresentationUpdate()

        let webArchiveType = NSPasteboard.PasteboardType(UTType.webArchive.identifier)
        let pasteboard = NSPasteboard.withUniqueName()
        webView.writeSelection(to: pasteboard, types: [webArchiveType])

        #expect((pasteboard.data(forType: webArchiveType)?.count ?? 0) > 0)
    }

    private enum ImageCopySource {
        case user
        case script
    }

    private func copyImageDocument(_ imageURL: URL, source: ImageCopySource) async throws -> [NSPasteboard.PasteboardType] {
        let configuration = WKWebViewConfiguration()
        configuration.preferences._javaScriptCanAccessClipboard = true
        let webView = TestWKWebView(frame: CGRect(x: 0, y: 0, width: 400, height: 400), configuration: configuration)
        try await webView.loadAndWait(URLRequest(url: imageURL))

        NSPasteboard.general.clearContents()
        switch source {
        case .user:
            webView.copy(nil)
        case .script:
            try await webView.callJavaScript(userGesture: false) { "document.execCommand('copy')" }
        }
        await webView.nextPresentationUpdate()

        return NSPasteboard.general.types ?? []
    }

    @Test
    func userInitiatedImageCopyWritesOriginalImageData() async throws {
        let imageURL = try #require(Bundle.testResources.url(forResource: "icon", withExtension: "png"))
        let types = try await copyImageDocument(imageURL, source: .user)

        #expect((types.first?.rawValue ?? "") == UTType.png.identifier)
        #expect(types.contains(.tiff))
        let imageData = try Data(contentsOf: imageURL)
        #expect(NSPasteboard.general.data(forType: .png) == imageData)
    }

    @Test
    func scriptInitiatedImageCopyDoesNotWriteOriginalImageData() async throws {
        let imageURL = try #require(Bundle.testResources.url(forResource: "icon", withExtension: "png"))
        let types = try await copyImageDocument(imageURL, source: .script)

        #expect(types.contains(.tiff))
        #expect(!types.contains(.png))
    }

    @Test
    func userInitiatedTIFFImageCopyOnlyWritesTIFF() async throws {
        let imageURL = try #require(Bundle.testResources.url(forResource: "sunset-in-cupertino-100px", withExtension: "tiff"))
        let types = try await copyImageDocument(imageURL, source: .user)

        #expect(types.contains(.tiff))
        for type in types {
            if let utType = UTType(type.rawValue), utType.conforms(to: .image) {
                #expect(utType.conforms(to: .tiff))
            }
        }
    }

    #endif // WTF_PLATFORM_MAC

    @Test
    func copySelectedTextInTextDocument() async throws {
        let webView = makeWebViewWithCustomPasteboardData()
        let fileURL = try #require(Bundle.testResources.url(forResource: "test", withExtension: "json"))

        try await webView.loadAndWait(URLRequest(url: fileURL))
        try await webView.callJavaScript(userGesture: false) { "getSelection().selectAllChildren(document.body)" }
        #if !WTF_PLATFORM_MAC && !WTF_PLATFORM_WATCHOS && !WTF_PLATFORM_APPLETV
        let expectedString = try await webView.callJavaScript(returning: String.self) { "return getSelection().toString()" }
        #endif
        webView.copy(nil)
        await webView.nextPresentationUpdate()

        #if WTF_PLATFORM_MAC
        // This reads the copied text on macOS without checking it.
        _ = NSPasteboard.general.readObjects(forClasses: [NSAttributedString.self], options: nil)
        #elseif !WTF_PLATFORM_WATCHOS && !WTF_PLATFORM_APPLETV
        let itemProvider = try #require(UIPasteboard.general.itemProviders.first)
        let copiedText: String? = await withCheckedContinuation { continuation in
            // The completion handler runs on a background queue, so it only passes the text back.
            _ = itemProvider.loadObject(ofClass: NSAttributedString.self) { result, _ in
                continuation.resume(returning: (result as? NSAttributedString)?.string)
            }
        }
        #expect((copiedText ?? "") == expectedString)
        #endif
    }

    @Test
    func emojiImgWithSVGSourceReplacedWithAltText() async throws {
        let webView = makeWebViewWithCustomPasteboardData()
        try await webView.load(
            html: """
                <!DOCTYPE html>\
                <body>\
                <div id='copy'>Hello <img class='emoji' alt='\u{1F60C}' src='/assets/emoji.svg'> world</div>\
                <script>getSelection().selectAllChildren(copy);</script>\
                </body>
                """
        )
        webView.copy(nil)
        await webView.nextPresentationUpdate()

        let copiedMarkup = readHTMLStringFromPasteboard() ?? ""
        #expect(copiedMarkup.contains("\u{1F60C}"))
        #expect(copiedMarkup.contains("Hello"))
        #expect(copiedMarkup.contains("world"))
        #expect(!copiedMarkup.contains("<img"))
        #expect(!copiedMarkup.contains(".svg"))
    }

    @Test
    func nonEmojiImgWithSVGSourcePreserved() async throws {
        let webView = makeWebViewWithCustomPasteboardData()
        try await webView.load(
            html: """
                <!DOCTYPE html>\
                <body>\
                <div id='copy'>Hello <img alt='a logo' src='/assets/logo.svg'> world</div>\
                <script>getSelection().selectAllChildren(copy);</script>\
                </body>
                """
        )
        webView.copy(nil)
        await webView.nextPresentationUpdate()

        let copiedMarkup = readHTMLStringFromPasteboard() ?? ""
        #expect(copiedMarkup.contains("<img"))
    }

    @Test
    func emojiImgWithNonSVGSourcePreserved() async throws {
        let webView = makeWebViewWithCustomPasteboardData()
        try await webView.load(
            html: """
                <!DOCTYPE html>\
                <body>\
                <div id='copy'>Hello <img alt='\u{1F60C}' src='/assets/emoji.png'> world</div>\
                <script>getSelection().selectAllChildren(copy);</script>\
                </body>
                """
        )
        webView.copy(nil)
        await webView.nextPresentationUpdate()

        let copiedMarkup = readHTMLStringFromPasteboard() ?? ""
        #expect(copiedMarkup.contains("<img"))
    }
}
