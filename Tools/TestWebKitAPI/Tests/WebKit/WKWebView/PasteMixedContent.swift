// Copyright (C) 2018-2026 Apple Inc. All rights reserved.
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
private import WebKit_Private.WKWebViewPrivate

import struct Foundation.URL
import struct Swift.String

#if WTF_PLATFORM_IOS_FAMILY
private import UIKit
#else
private import AppKit
#endif

/// A handler for schemes whose pages are only loaded from strings, so it never gets a request.
private final class NoOpURLSchemeHandler: NSObject, WKURLSchemeHandler {
    func webView(_ webView: WKWebView, start urlSchemeTask: any WKURLSchemeTask) {
    }

    func webView(_ webView: WKWebView, stop urlSchemeTask: any WKURLSchemeTask) {
    }
}

#if WTF_PLATFORM_IOS_FAMILY
/// Runs `body` with the general pasteboard's read methods returning nil, and returns whether anything called them.
@MainActor
private func pasteboardWasAccessed(during body: () -> Void) -> Bool {
    var didAccessPasteboard = false
    // One block stands in for all four methods; it ignores the arguments of the two that take them.
    let returnNil: @convention(block) (AnyObject) -> AnyObject? = { _ in
        didAccessPasteboard = true
        return nil
    }

    let selectors = [
        #selector(UIPasteboard.data(forPasteboardType:)),
        #selector(UIPasteboard.data(forPasteboardType:inItemSet:)),
        #selector(getter: UIPasteboard.items),
        #selector(getter: UIPasteboard.itemProviders),
    ]
    let pasteboardClass: AnyClass = type(of: UIPasteboard.general)
    let replacement = unsafe imp_implementationWithBlock(returnNil)
    let originals = unsafe selectors.map { selector in
        guard let method = unsafe class_getInstanceMethod(pasteboardClass, selector) else {
            fatalError("\(pasteboardClass) does not respond to \(selector)")
        }
        return unsafe (method, method_setImplementation(method, replacement))
    }
    defer {
        for unsafe (method, original) in unsafe originals {
            unsafe method_setImplementation(method, original)
        }
        unsafe imp_removeBlock(replacement)
    }

    body()
    return didAccessPasteboard
}
#endif

@MainActor
struct PasteMixedContentTests {
    #if WTF_PLATFORM_MAC
    private let imagePath: String

    // The script tag and mouseover listener attribute in the markup string below should be omitted during
    // sanitization while pasting.
    private let markupString = "<script>foo()</script><strong onmouseover='javascript:void(0)'>HELLO WORLD</strong>"

    init() throws {
        imagePath = try #require(Bundle.testResources.path(forResource: "icon", ofType: "png"))
    }

    /// Declares the types on the general pasteboard, in order, and writes each one's value: `Data` as data, `String`
    /// as a string, and anything else as a property list.
    private func writeTypesAndDataToPasteboard(_ typesAndData: KeyValuePairs<NSPasteboard.PasteboardType, Any>) {
        let pasteboard = NSPasteboard.general
        pasteboard.declareTypes(typesAndData.map(\.key), owner: nil)
        for (type, value) in typesAndData {
            switch value {
            case let data as Data:
                pasteboard.setData(data, forType: type)
            case let string as String:
                pasteboard.setString(string, forType: type)
            default:
                pasteboard.setPropertyList(value, forType: type)
            }
        }
    }

    private func setUpWebView() async throws -> TestWKWebView {
        let webView = TestWKWebView(frame: CGRect(x: 0, y: 0, width: 400, height: 400))
        webView.configuration.setFeature("CustomPasteboardDataEnabled", enabled: true)
        try await webView.load(testPageNamed: "DataTransfer")
        return webView
    }

    @Test
    func imageFileAndPlainText() async throws {
        let webView = try await setUpWebView()
        writeTypesAndDataToPasteboard([.legacyFilenames: [imagePath], .string: imagePath])
        webView.paste(nil)

        #expect(try await webView.callJavaScript(returning: String.self) { "return types.textContent" } == "Files")
        #expect(try await webView.callJavaScript(returning: String.self) { "return items.textContent" } == "(FILE, image/png)")
        #expect(try await webView.callJavaScript(returning: String.self) { "return files.textContent" } == "('icon.png', image/png)")
        #expect(try await webView.callJavaScript(returning: String.self) { "return urlData.textContent" } == "")
        #expect(try await webView.callJavaScript(returning: String.self) { "return textData.textContent" } == "")
        #expect(try await webView.callJavaScript(returning: String.self) { "return htmlData.textContent" } == "")
        #expect(try await webView.callJavaScript(returning: String.self) { "return rawHTMLData.textContent" } == "")
    }

    @Test
    func imageFileAndWebArchive() async throws {
        let webView = try await setUpWebView()
        let mainResourceURL = URL(fileURLWithPath: "/some/nonexistent/file.html")
        let archiveData = makeWebArchiveData(html: Data(markupString.utf8), url: mainResourceURL)
        writeTypesAndDataToPasteboard([.legacyFilenames: [imagePath], .legacyWebArchive: archiveData])
        webView.paste(nil)

        #expect(try await webView.callJavaScript(returning: String.self) { "return types.textContent" } == "Files, text/html")
        #expect(
            try await webView.callJavaScript(returning: String.self) { "return items.textContent" }
                == "(STRING, text/html), (FILE, image/png)"
        )
        #expect(try await webView.callJavaScript(returning: String.self) { "return files.textContent" } == "('icon.png', image/png)")
        #expect(try await webView.callJavaScript(returning: String.self) { "return urlData.textContent" } == "")
        #expect(try await webView.callJavaScript(returning: String.self) { "return textData.textContent" } == "")
        #expect(try await webView.callJavaScript(returning: String.self) { "return htmlData.textContent" } == "HELLO WORLD")
        let rawHTMLData = try await webView.callJavaScript(returning: String.self) { "return rawHTMLData.textContent" }
        #expect(!rawHTMLData.contains("script"))
    }

    @Test
    func imageFileAndHTML() async throws {
        let webView = try await setUpWebView()
        writeTypesAndDataToPasteboard([.legacyFilenames: [imagePath], .html: markupString])
        webView.paste(nil)

        #expect(try await webView.callJavaScript(returning: String.self) { "return types.textContent" } == "Files, text/html")
        #expect(
            try await webView.callJavaScript(returning: String.self) { "return items.textContent" }
                == "(STRING, text/html), (FILE, image/png)"
        )
        #expect(try await webView.callJavaScript(returning: String.self) { "return files.textContent" } == "('icon.png', image/png)")
        #expect(try await webView.callJavaScript(returning: String.self) { "return urlData.textContent" } == "")
        #expect(try await webView.callJavaScript(returning: String.self) { "return textData.textContent" } == "")
        #expect(try await webView.callJavaScript(returning: String.self) { "return htmlData.textContent" } == "HELLO WORLD")
        let rawHTMLData = try await webView.callJavaScript(returning: String.self) { "return rawHTMLData.textContent" }
        #expect(!rawHTMLData.contains("script"))
    }

    @Test
    func imageFileAndRTF() async throws {
        let webView = try await setUpWebView()
        let text = NSMutableAttributedString()
        text.append(NSAttributedString(string: "link to "))
        text.append(
            NSAttributedString(
                string: "apple",
                attributes: [.link: try #require(URL(string: "https://www.apple.com/"))]
            )
        )
        let rtfData = try #require(text.rtf(from: NSRange(location: 0, length: text.length), documentAttributes: [:]))
        writeTypesAndDataToPasteboard([.legacyFilenames: [imagePath], .rtf: rtfData])
        webView.paste(nil)

        #expect(try await webView.callJavaScript(returning: String.self) { "return types.textContent" } == "Files, text/html")
        #expect(
            try await webView.callJavaScript(returning: String.self) { "return items.textContent" }
                == "(STRING, text/html), (FILE, image/png)"
        )
        #expect(try await webView.callJavaScript(returning: String.self) { "return files.textContent" } == "('icon.png', image/png)")
        #expect(try await webView.callJavaScript(returning: String.self) { "return urlData.textContent" } == "")
        #expect(try await webView.callJavaScript(returning: String.self) { "return textData.textContent" } == "")
        #expect(try await webView.callJavaScript(returning: String.self) { "return htmlData.textContent" } == "link to apple")
        #expect(
            try await webView.callJavaScript(returning: String.self) { "return htmlData.querySelector('a').href" }
                == "https://www.apple.com/"
        )
    }

    @Test
    func imageFileAndURL() async throws {
        let webView = try await setUpWebView()
        writeTypesAndDataToPasteboard([.legacyURL: "https://www.webkit.org/", .legacyFilenames: [imagePath]])
        webView.paste(nil)

        #expect(try await webView.callJavaScript(returning: String.self) { "return types.textContent" } == "Files, text/uri-list")
        #expect(
            try await webView.callJavaScript(returning: String.self) { "return items.textContent" }
                == "(STRING, text/uri-list), (FILE, image/png)"
        )
        #expect(try await webView.callJavaScript(returning: String.self) { "return files.textContent" } == "('icon.png', image/png)")
        #expect(try await webView.callJavaScript(returning: String.self) { "return urlData.textContent" } == "https://www.webkit.org/")
        #expect(try await webView.callJavaScript(returning: String.self) { "return textData.textContent" } == "")
        #expect(try await webView.callJavaScript(returning: String.self) { "return htmlData.textContent" } == "")
    }

    @Test
    func imageFileWithHTMLAndURL() async throws {
        let webView = try await setUpWebView()
        writeTypesAndDataToPasteboard([
            .legacyURL: "https://www.webkit.org/", .html: markupString, .legacyFilenames: [imagePath],
        ])
        webView.paste(nil)

        #expect(
            try await webView.callJavaScript(returning: String.self) { "return types.textContent" }
                == "Files, text/uri-list, text/html"
        )
        #expect(
            try await webView.callJavaScript(returning: String.self) { "return items.textContent" }
                == "(STRING, text/uri-list), (STRING, text/html), (FILE, image/png)"
        )
        #expect(try await webView.callJavaScript(returning: String.self) { "return files.textContent" } == "('icon.png', image/png)")
        #expect(try await webView.callJavaScript(returning: String.self) { "return urlData.textContent" } == "https://www.webkit.org/")
        #expect(try await webView.callJavaScript(returning: String.self) { "return textData.textContent" } == "")
        #expect(try await webView.callJavaScript(returning: String.self) { "return htmlData.textContent" } == "HELLO WORLD")
        let rawHTMLData = try await webView.callJavaScript(returning: String.self) { "return rawHTMLData.textContent" }
        #expect(!rawHTMLData.contains("script"))
    }

    @Test
    func imageDataAndPlainText() async throws {
        let webView = try await setUpWebView()
        writeTypesAndDataToPasteboard([.string: "Hello world", .png: try Data(contentsOf: URL(fileURLWithPath: imagePath))])
        webView.paste(nil)

        #expect(try await webView.callJavaScript(returning: String.self) { "return types.textContent" } == "Files, text/plain")
        #expect(
            try await webView.callJavaScript(returning: String.self) { "return items.textContent" }
                == "(STRING, text/plain), (FILE, image/png)"
        )
        #expect(try await webView.callJavaScript(returning: String.self) { "return files.textContent" } == "('image.png', image/png)")
        #expect(try await webView.callJavaScript(returning: String.self) { "return urlData.textContent" } == "")
        #expect(try await webView.callJavaScript(returning: String.self) { "return textData.textContent" } == "Hello world")
        #expect(try await webView.callJavaScript(returning: String.self) { "return htmlData.textContent" } == "")
    }

    @Test
    func imageDataAndPlainTextAndURL() async throws {
        let webView = try await setUpWebView()
        writeTypesAndDataToPasteboard([
            .string: imagePath, .legacyURL: imagePath, .png: try Data(contentsOf: URL(fileURLWithPath: imagePath)),
        ])
        webView.paste(nil)

        #expect(try await webView.callJavaScript(returning: String.self) { "return types.textContent" } == "Files")
        #expect(try await webView.callJavaScript(returning: String.self) { "return items.textContent" } == "(FILE, image/png)")
        #expect(try await webView.callJavaScript(returning: String.self) { "return files.textContent" } == "('image.png', image/png)")
        #expect(try await webView.callJavaScript(returning: String.self) { "return urlData.textContent" } == "")
        #expect(try await webView.callJavaScript(returning: String.self) { "return textData.textContent" } == "")
        #expect(try await webView.callJavaScript(returning: String.self) { "return htmlData.textContent" } == "")
    }

    @Test
    func imageDataAndPlainTextAndURLAndHTML() async throws {
        let webView = try await setUpWebView()
        writeTypesAndDataToPasteboard([
            .string: imagePath, .legacyURL: imagePath, .html: markupString,
            .png: try Data(contentsOf: URL(fileURLWithPath: imagePath)),
        ])
        webView.paste(nil)

        #expect(try await webView.callJavaScript(returning: String.self) { "return types.textContent" } == "Files, text/html")
        #expect(
            try await webView.callJavaScript(returning: String.self) { "return items.textContent" }
                == "(STRING, text/html), (FILE, image/png)"
        )
        #expect(try await webView.callJavaScript(returning: String.self) { "return files.textContent" } == "('image.png', image/png)")
        #expect(try await webView.callJavaScript(returning: String.self) { "return urlData.textContent" } == "")
        #expect(try await webView.callJavaScript(returning: String.self) { "return textData.textContent" } == "")
        #expect(try await webView.callJavaScript(returning: String.self) { "return htmlData.textContent" } == "HELLO WORLD")
        let rawHTMLData = try await webView.callJavaScript(returning: String.self) { "return rawHTMLData.textContent" }
        #expect(!rawHTMLData.contains("script"))
    }

    @Test
    func pasteURLWrittenToPasteboardUsingWriteObjects() async throws {
        let urlToCopy = "https://www.webkit.org/"

        let webView = TestWKWebView(frame: CGRect(x: 0, y: 0, width: 400, height: 400))
        try await webView.load(html: "<body contenteditable></body><script>document.body.focus()</script>")
        NSPasteboard.general.clearContents()
        NSPasteboard.general.writeObjects([try #require(NSURL(string: urlToCopy))])
        webView.paste(nil)

        #expect(try await webView.callJavaScript(returning: String.self) { "return document.querySelector('a').href" } == urlToCopy)
        #expect(
            try await webView.callJavaScript(returning: String.self) { "return document.querySelector('a').textContent" }
                == urlToCopy
        )
    }

    @Test
    func pasteOneOrMoreURLs() async throws {
        let appleURL = try #require(URL(string: "https://www.apple.com/"))
        let webKitURL = try #require(URL(string: "https://webkit.org/"))

        let webView = try await setUpWebView()
        func runTest(
            _ description: Comment,
            expectedURLString: String,
            writeURLsToPasteboard: (NSPasteboard) -> Void,
            sourceLocation: SourceLocation = #_sourceLocation
        ) async throws {
            let pasteboard = NSPasteboard.general

            pasteboard.clearContents()
            writeURLsToPasteboard(pasteboard)
            try await webView.callJavaScript(userGesture: false) { "reset(); document.body.focus()" }
            webView.paste(nil)

            let urlData = try await webView.callJavaScript(returning: String.self) { "return urlData.textContent" }
            let items = try await webView.callJavaScript(returning: String.self) { "return items.textContent" }
            let types = try await webView.callJavaScript(returning: String.self) { "return types.textContent" }
            #expect(urlData == expectedURLString, description, sourceLocation: sourceLocation)
            #expect(items == "(STRING, text/uri-list)", description, sourceLocation: sourceLocation)
            #expect(types == "text/uri-list", description, sourceLocation: sourceLocation)
        }

        try await runTest("Write multiple URLs.", expectedURLString: "https://www.apple.com/\nhttps://webkit.org/") { pasteboard in
            pasteboard.writeObjects([appleURL as NSURL, webKitURL as NSURL])
        }

        try await runTest("Declare legacy URL and write URL to pasteboard.", expectedURLString: "https://www.apple.com/") { pasteboard in
            pasteboard.declareTypes([.legacyURL], owner: nil)
            (appleURL as NSURL).write(to: pasteboard)
        }

        try await runTest("Declare legacy URL and set a URL string.", expectedURLString: "https://www.apple.com/") { pasteboard in
            pasteboard.declareTypes([.legacyURL], owner: nil)
            pasteboard.setString(appleURL.absoluteString, forType: .legacyURL)
        }

        try await runTest("Declare legacy URL and set a property list.", expectedURLString: "https://www.apple.com/") { pasteboard in
            pasteboard.declareTypes([.legacyURL], owner: nil)
            pasteboard.setPropertyList(["/", "https://www.apple.com"], forType: .legacyURL)
        }

        try await runTest("Declare URL UTI and set a URL string.", expectedURLString: "https://www.apple.com/") { pasteboard in
            let urlType = NSPasteboard.PasteboardType(UTType.url.identifier)
            pasteboard.declareTypes([urlType], owner: nil)
            pasteboard.setString(appleURL.absoluteString, forType: urlType)
        }
    }
    #endif // WTF_PLATFORM_MAC

    // FIXME when rdar://162815356 is resolved.
    #if WTF_PLATFORM_MAC
    @Test(.disabled("rdar://162815356"))
    #else
    @Test
    #endif
    func copyAndPasteWithCustomPasteboardDataOnly() async throws {
        let markupForSource = #"<body oncopy="event.preventDefault(); event.clipboardData.setData('foo', 'bar')">hello</body>"#
        let markupForDestination = #"<input autofocus onpaste="event.preventDefault(); this.value = event.clipboardData.getData('foo')">"#

        let schemeHandler = NoOpURLSchemeHandler()
        let configuration = WKWebViewConfiguration()
        configuration.setURLSchemeHandler(schemeHandler, forURLScheme: "same")
        configuration.setURLSchemeHandler(schemeHandler, forURLScheme: "different")
        configuration.setFeature("CustomPasteboardDataEnabled", enabled: true)

        let source = TestWKWebView(frame: CGRect(x: 0, y: 0, width: 400, height: 400), configuration: configuration)
        try await source.load(html: markupForSource, baseURL: URL(string: "same://"))
        source.selectAll(nil)
        _ = await source._executeEditCommand("copy", argument: nil)

        let destination = TestWKWebView(frame: CGRect(x: 0, y: 0, width: 400, height: 400), configuration: configuration)
        try await destination.load(html: markupForDestination, baseURL: URL(string: "same://"))
        #if WTF_PLATFORM_IOS_FAMILY
        let didAccessPasteboardOnSameOrigin = pasteboardWasAccessed {
            #expect(destination.canPerformAction(#selector(UIResponder.paste(_:)), withSender: nil))
        }
        #expect(!didAccessPasteboardOnSameOrigin)
        #endif
        destination.paste(nil)
        #expect(try await destination.callJavaScript(returning: String.self) { "return document.querySelector('input').value" } == "bar")

        try await destination.load(html: markupForDestination, baseURL: URL(string: "different://"))
        #if WTF_PLATFORM_IOS_FAMILY
        let didAccessPasteboardOnDifferentOrigin = pasteboardWasAccessed {
            #expect(destination.canPerformAction(#selector(UIResponder.paste(_:)), withSender: nil))
        }
        #expect(!didAccessPasteboardOnDifferentOrigin)
        #endif
        destination.paste(nil)
        #expect(try await destination.callJavaScript(returning: String.self) { "return document.querySelector('input').value" } == "")
    }
}
