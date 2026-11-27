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
private import WebKit_Private.WKWebViewPrivate

import struct Foundation.URL
import struct Swift.String

#if WTF_PLATFORM_IOS_FAMILY
private import UIKit
#else
private import AppKit
private import WebKit_Private.WKWebsiteDataStorePrivate
#endif

@MainActor
struct PasteImageTests {
    private func testResourceData(
        named name: String,
        withExtension fileExtension: String,
        sourceLocation: SourceLocation = #_sourceLocation
    ) throws -> Data {
        try Data(
            contentsOf: try #require(
                Bundle.testResources.url(forResource: name, withExtension: fileExtension),
                sourceLocation: sourceLocation
            )
        )
    }

    private func writeImageDataToPasteboard(type: String, data: Data) {
        #if WTF_PLATFORM_MAC
        NSPasteboard.general.declareTypes([NSPasteboard.PasteboardType(type)], owner: nil)
        NSPasteboard.general.setData(data, forType: NSPasteboard.PasteboardType(type))
        #else
        UIPasteboard.general.items = [[type: data]]
        #endif
    }

    private func writeImageDataToPasteboard(typesAndData: KeyValuePairs<String, Data>) {
        #if WTF_PLATFORM_MAC
        NSPasteboard.general.declareTypes(typesAndData.map { NSPasteboard.PasteboardType($0.key) }, owner: nil)
        for (type, data) in typesAndData {
            NSPasteboard.general.setData(data, forType: NSPasteboard.PasteboardType(type))
        }
        #else
        // Pasteboard items are dictionaries. Hand UIKit an NSDictionary, as an Objective-C dictionary literal would be:
        // bridging it through a Swift Dictionary would order its types differently on every run.
        let item = NSDictionary(objects: typesAndData.map(\.value), forKeys: typesAndData.map { $0.key as NSString })
        UIPasteboard.general.setValue(NSArray(object: item), forKey: #keyPath(UIPasteboard.items))
        #endif
    }

    private func writeImageDataToPasteboard(items: [[String: Data]]) {
        #if WTF_PLATFORM_MAC
        NSPasteboard.general.clearContents()
        let pasteboardItems = items.map { typesAndData in
            let pasteboardItem = NSPasteboardItem()
            for (type, data) in typesAndData {
                pasteboardItem.setData(data, forType: NSPasteboard.PasteboardType(type))
            }
            return pasteboardItem
        }
        NSPasteboard.general.writeObjects(pasteboardItems)
        #else
        UIPasteboard.general.items = items
        #endif
    }

    #if WTF_PLATFORM_MAC
    private func writeBundleFileToPasteboard(_ url: URL) {
        NSPasteboard.general.declareTypes([.legacyFilenames], owner: nil)
        NSPasteboard.general.writeObjects([url as NSURL])
    }
    #endif

    private func makeWebView() -> TestWKWebView {
        TestWKWebView(frame: CGRect(x: 0, y: 0, width: 400, height: 400))
    }

    @Test
    func pasteGIFImage() async throws {
        let webView = makeWebView()
        try await webView.load(testPageNamed: "paste-image")

        writeImageDataToPasteboard(
            type: UTType.gif.identifier,
            data: try testResourceData(named: "sunset-in-cupertino-400px", withExtension: "gif")
        )
        webView.paste(nil)

        #expect(
            try await webView.callJavaScript(returning: String.self) { "return dataTransfer.types.includes('image/gif').toString()" }
                == "false"
        )
        #expect(
            try await webView.callJavaScript(returning: String.self) { "return dataTransfer.types.includes('image/tiff').toString()" }
                == "false"
        )
        #expect(
            try await webView.callJavaScript(returning: String.self) {
                "gifItem = dataTransfer.items.find((item) => item.type == 'image/gif'); return (!!gifItem).toString()"
            } == "true"
        )
        #expect(try await webView.callJavaScript(returning: String.self) { "return gifItem.kind" } == "file")
        #expect(try await webView.callJavaScript(returning: String.self) { "return gifItem.file.type" } == "image/gif")
        #expect(try await webView.callJavaScript(returning: String.self) { "return gifItem.file.name" } == "image.gif")
        #expect(
            try await webView.callJavaScript(returning: String.self) { "return dataTransfer.files.includes(gifItem.file).toString()" }
                == "true"
        )

        try await webView.waitForMessage("loaded") {
            try await webView.callJavaScript(userGesture: false) { "insertFileAsImage(gifItem.file)" }
        }
        #expect(
            try await webView.callJavaScript(returning: String.self) { "url = new URL(imageElement.src); return url.protocol" } == "blob:"
        )
        #expect(try await webView.callJavaScript(returning: Int.self) { "return imageElement.width" } == 400)
    }

    @Test
    func pasteJPEGImage() async throws {
        let webView = makeWebView()
        try await webView.load(testPageNamed: "paste-image")

        writeImageDataToPasteboard(
            type: UTType.jpeg.identifier,
            data: try testResourceData(named: "sunset-in-cupertino-600px", withExtension: "jpg")
        )
        webView.paste(nil)

        #expect(
            try await webView.callJavaScript(returning: String.self) { "return dataTransfer.types.includes('image/gif').toString()" }
                == "false"
        )
        #expect(
            try await webView.callJavaScript(returning: String.self) { "return dataTransfer.types.includes('image/tiff').toString()" }
                == "false"
        )
        #expect(
            try await webView.callJavaScript(returning: String.self) {
                "jpegItem = dataTransfer.items.find((item) => item.type == 'image/jpeg'); return (!!jpegItem).toString()"
            } == "true"
        )
        #expect(try await webView.callJavaScript(returning: String.self) { "return jpegItem.kind" } == "file")
        #expect(try await webView.callJavaScript(returning: String.self) { "return jpegItem.file.type" } == "image/jpeg")
        #expect(try await webView.callJavaScript(returning: String.self) { "return jpegItem.file.name" } == "image.jpeg")
        #expect(
            try await webView.callJavaScript(returning: String.self) { "return dataTransfer.files.includes(jpegItem.file).toString()" }
                == "true"
        )

        try await webView.waitForMessage("loaded") {
            try await webView.callJavaScript(userGesture: false) { "insertFileAsImage(jpegItem.file)" }
        }
        #expect(
            try await webView.callJavaScript(returning: String.self) { "url = new URL(imageElement.src); return url.protocol" } == "blob:"
        )
        #expect(try await webView.callJavaScript(returning: Int.self) { "return imageElement.width" } == 600)
    }

    @Test
    func pastePNGImage() async throws {
        let webView = makeWebView()
        try await webView.load(testPageNamed: "paste-image")

        writeImageDataToPasteboard(
            type: UTType.png.identifier,
            data: try testResourceData(named: "sunset-in-cupertino-200px", withExtension: "png")
        )
        webView.paste(nil)

        #expect(
            try await webView.callJavaScript(returning: String.self) { "return dataTransfer.types.includes('image/gif').toString()" }
                == "false"
        )
        #expect(
            try await webView.callJavaScript(returning: String.self) { "return dataTransfer.types.includes('image/tiff').toString()" }
                == "false"
        )
        #expect(
            try await webView.callJavaScript(returning: String.self) {
                "pngItem = dataTransfer.items.find((item) => item.type == 'image/png'); return (!!pngItem).toString()"
            } == "true"
        )
        #expect(try await webView.callJavaScript(returning: String.self) { "return pngItem.kind" } == "file")
        #expect(try await webView.callJavaScript(returning: String.self) { "return pngItem.file.type" } == "image/png")
        #expect(try await webView.callJavaScript(returning: String.self) { "return pngItem.file.name" } == "image.png")
        #expect(
            try await webView.callJavaScript(returning: String.self) { "return dataTransfer.files.includes(pngItem.file).toString()" }
                == "true"
        )

        try await webView.waitForMessage("loaded") {
            try await webView.callJavaScript(userGesture: false) { "insertFileAsImage(pngItem.file)" }
        }
        #expect(
            try await webView.callJavaScript(returning: String.self) { "url = new URL(imageElement.src); return url.protocol" } == "blob:"
        )
        #expect(try await webView.callJavaScript(returning: Int.self) { "return imageElement.width" } == 200)
    }

    @Test
    func pasteImageWithMultipleRepresentations() async throws {
        let webView = makeWebView()
        try await webView.load(testPageNamed: "paste-image")

        let pngData = try testResourceData(named: "sunset-in-cupertino-200px", withExtension: "png")
        let jpegData = try testResourceData(named: "sunset-in-cupertino-600px", withExtension: "jpg")
        writeImageDataToPasteboard(typesAndData: [
            UTType.png.identifier: pngData,
            UTType.jpeg.identifier: jpegData,
        ])
        webView.paste(nil)

        #expect(try await webView.callJavaScript(returning: Int.self) { "return dataTransfer.files.length" } == 1)

        writeImageDataToPasteboard(items: [
            [UTType.png.identifier: pngData],
            [UTType.jpeg.identifier: jpegData],
        ])
        webView.paste(nil)

        #expect(try await webView.callJavaScript(returning: Int.self) { "return dataTransfer.files.length" } == 2)
    }

    @Test
    func revealSelectionAfterPastingImage() async throws {
        let webView = makeWebView()
        try await webView.load(
            html: "<meta name='viewport' content='width=device-width, initial-scale=1'><body contenteditable>Hello world</body>"
        )
        try await webView.callJavaScript(userGesture: false) { "document.body.focus()" }
        _ = await webView._executeEditCommand("InsertText", argument: "Hello world")
        _ = await webView._executeEditCommand("InsertParagraph", argument: nil)

        writeImageDataToPasteboard(
            type: UTType.jpeg.identifier,
            data: try testResourceData(named: "sunset-in-cupertino-600px", withExtension: "jpg")
        )
        webView.paste(nil)

        func scrollTop() async throws -> Double {
            try await webView.callJavaScript(returning: Double.self) { "return document.scrollingElement.scrollTop" }
        }

        // Wait with no deadline: TestExpectations/apitests expects this test to time out sometimes on iOS, and a
        // deadline would turn those timeouts into failures.
        while try await scrollTop() <= 0 {
            try await Task.sleep(for: .milliseconds(50))
        }
    }

    #if WTF_PLATFORM_MAC

    @Test
    func pasteGIFFile() async throws {
        let webView = makeWebView()
        try await webView.load(testPageNamed: "paste-image")

        writeBundleFileToPasteboard(try #require(Bundle.testResources.url(forResource: "sunset-in-cupertino-400px", withExtension: "gif")))
        webView.paste(nil)

        #expect(
            try await webView.callJavaScript(returning: String.self) { "return dataTransfer.types.includes('image/png').toString()" }
                == "false"
        )
        #expect(
            try await webView.callJavaScript(returning: Int.self) {
                "return dataTransfer.items.filter((item) => item.kind == 'file').length"
            } == 1
        )
        #expect(try await webView.callJavaScript(returning: Int.self) { "return dataTransfer.files.length" } == 1)
        #expect(
            try await webView.callJavaScript(returning: String.self) { "file = dataTransfer.files[0]; return file.type" } == "image/gif"
        )
        #expect(try await webView.callJavaScript(returning: String.self) { "return file.name" } == "sunset-in-cupertino-400px.gif")
        #expect(try await webView.callJavaScript(returning: String.self) { "return editor.textContent" } == "")

        try await webView.waitForMessage("loaded") {
            try await webView.callJavaScript(userGesture: false) { "insertFileAsImage(file)" }
        }
        #expect(
            try await webView.callJavaScript(returning: String.self) { "url = new URL(imageElement.src); return url.protocol" } == "blob:"
        )
        #expect(try await webView.callJavaScript(returning: Int.self) { "return imageElement.width" } == 400)
    }

    @Test
    func pasteJPEGFile() async throws {
        let webView = makeWebView()
        try await webView.load(testPageNamed: "paste-image")

        writeBundleFileToPasteboard(try #require(Bundle.testResources.url(forResource: "sunset-in-cupertino-600px", withExtension: "jpg")))
        webView.paste(nil)

        #expect(
            try await webView.callJavaScript(returning: String.self) { "return dataTransfer.types.includes('image/jpeg').toString()" }
                == "false"
        )
        #expect(
            try await webView.callJavaScript(returning: Int.self) {
                "return dataTransfer.items.filter((item) => item.kind == 'file').length"
            } == 1
        )
        #expect(try await webView.callJavaScript(returning: Int.self) { "return dataTransfer.files.length" } == 1)
        #expect(
            try await webView.callJavaScript(returning: String.self) { "file = dataTransfer.files[0]; return file.type" } == "image/jpeg"
        )
        #expect(try await webView.callJavaScript(returning: String.self) { "return file.name" } == "sunset-in-cupertino-600px.jpg")
        #expect(try await webView.callJavaScript(returning: String.self) { "return editor.textContent" } == "")

        try await webView.waitForMessage("loaded") {
            try await webView.callJavaScript(userGesture: false) { "insertFileAsImage(file)" }
        }
        #expect(
            try await webView.callJavaScript(returning: String.self) { "url = new URL(imageElement.src); return url.protocol" } == "blob:"
        )
        #expect(try await webView.callJavaScript(returning: Int.self) { "return imageElement.width" } == 600)
    }

    @Test
    func pastePNGFile() async throws {
        let webView = makeWebView()
        try await webView.load(testPageNamed: "paste-image")

        writeBundleFileToPasteboard(try #require(Bundle.testResources.url(forResource: "sunset-in-cupertino-200px", withExtension: "png")))
        webView.paste(nil)

        #expect(
            try await webView.callJavaScript(returning: String.self) { "return dataTransfer.types.includes('image/png').toString()" }
                == "false"
        )
        #expect(
            try await webView.callJavaScript(returning: Int.self) {
                "return dataTransfer.items.filter((item) => item.kind == 'file').length"
            } == 1
        )
        #expect(try await webView.callJavaScript(returning: Int.self) { "return dataTransfer.files.length" } == 1)
        #expect(
            try await webView.callJavaScript(returning: String.self) { "file = dataTransfer.files[0]; return file.type" } == "image/png"
        )
        #expect(try await webView.callJavaScript(returning: String.self) { "return file.name" } == "sunset-in-cupertino-200px.png")
        #expect(try await webView.callJavaScript(returning: String.self) { "return editor.textContent" } == "")

        try await webView.waitForMessage("loaded") {
            try await webView.callJavaScript(userGesture: false) { "insertFileAsImage(file)" }
        }
        #expect(
            try await webView.callJavaScript(returning: String.self) { "url = new URL(imageElement.src); return url.protocol" } == "blob:"
        )
        #expect(try await webView.callJavaScript(returning: Int.self) { "return imageElement.width" } == 200)
    }

    @Test
    func pasteTIFFFile() async throws {
        let webView = makeWebView()
        try await webView.load(testPageNamed: "paste-image")

        writeBundleFileToPasteboard(try #require(Bundle.testResources.url(forResource: "sunset-in-cupertino-100px", withExtension: "tiff")))
        webView.paste(nil)

        #expect(
            try await webView.callJavaScript(returning: String.self) { "return dataTransfer.types.includes('image/tiff').toString()" }
                == "false"
        )
        #expect(
            try await webView.callJavaScript(returning: Int.self) {
                "return dataTransfer.items.filter((item) => item.kind == 'file').length"
            } == 1
        )
        #expect(try await webView.callJavaScript(returning: Int.self) { "return dataTransfer.files.length" } == 1)
        #expect(
            try await webView.callJavaScript(returning: String.self) { "file = dataTransfer.files[0]; return file.type" } == "image/tiff"
        )
        #expect(try await webView.callJavaScript(returning: String.self) { "return file.name" } == "sunset-in-cupertino-100px.tiff")
        #expect(try await webView.callJavaScript(returning: String.self) { "return editor.textContent" } == "")

        try await webView.waitForMessage("loaded") {
            try await webView.callJavaScript(userGesture: false) { "insertFileAsImage(file)" }
        }
        #expect(
            try await webView.callJavaScript(returning: String.self) { "url = new URL(imageElement.src); return url.protocol" } == "blob:"
        )
        #expect(try await webView.callJavaScript(returning: Int.self) { "return imageElement.width" } == 100)
    }

    @Test
    func pasteLegacyTIFFImage() async throws {
        let webView = makeWebView()
        try await webView.load(testPageNamed: "paste-image")

        writeImageDataToPasteboard(
            type: NSPasteboard.PasteboardType.legacyTIFF.rawValue,
            data: try testResourceData(named: "sunset-in-cupertino-100px", withExtension: "tiff")
        )
        webView.paste(nil)

        #expect(
            try await webView.callJavaScript(returning: String.self) { "return dataTransfer.types.includes('image/png').toString()" }
                == "false"
        )
        #expect(
            try await webView.callJavaScript(returning: String.self) { "return dataTransfer.types.includes('image/tiff').toString()" }
                == "false"
        )
        #expect(
            try await webView.callJavaScript(returning: Int.self) {
                "return dataTransfer.items.filter((item) => item.kind == 'file').length"
            } == 1
        )
        #expect(try await webView.callJavaScript(returning: Int.self) { "return dataTransfer.files.length" } == 1)
        #expect(
            try await webView.callJavaScript(returning: String.self) { "file = dataTransfer.files[0]; return file.type" } == "image/png"
        )
        #expect(try await webView.callJavaScript(returning: String.self) { "return file.name" } == "image.png")

        try await webView.waitForMessage("loaded") {
            try await webView.callJavaScript(userGesture: false) { "insertFileAsImage(file)" }
        }
        #expect(
            try await webView.callJavaScript(returning: String.self) { "url = new URL(imageElement.src); return url.protocol" } == "blob:"
        )
        #expect(try await webView.callJavaScript(returning: Int.self) { "return imageElement.width" } == 100)
    }

    @Test
    func pasteTIFFImage() async throws {
        let webView = makeWebView()
        try await webView.load(testPageNamed: "paste-image")

        writeImageDataToPasteboard(
            type: UTType.tiff.identifier,
            data: try testResourceData(named: "sunset-in-cupertino-100px", withExtension: "tiff")
        )
        webView.paste(nil)

        #expect(
            try await webView.callJavaScript(returning: String.self) { "return dataTransfer.types.includes('image/gif').toString()" }
                == "false"
        )
        #expect(
            try await webView.callJavaScript(returning: String.self) { "return dataTransfer.types.includes('image/png').toString()" }
                == "false"
        )
        #expect(
            try await webView.callJavaScript(returning: String.self) {
                "pngItem = dataTransfer.items.find((item) => item.type == 'image/png'); return (!!pngItem).toString()"
            } == "true"
        )
        #expect(try await webView.callJavaScript(returning: String.self) { "return pngItem.kind" } == "file")
        #expect(try await webView.callJavaScript(returning: String.self) { "return pngItem.file.type" } == "image/png")
        #expect(try await webView.callJavaScript(returning: String.self) { "return pngItem.file.name" } == "image.png")
        #expect(
            try await webView.callJavaScript(returning: String.self) { "return dataTransfer.files.includes(pngItem.file).toString()" }
                == "true"
        )

        try await webView.waitForMessage("loaded") {
            try await webView.callJavaScript(userGesture: false) { "insertFileAsImage(pngItem.file)" }
        }
        #expect(
            try await webView.callJavaScript(returning: String.self) { "url = new URL(imageElement.src); return url.protocol" } == "blob:"
        )
        #expect(try await webView.callJavaScript(returning: Int.self) { "return imageElement.width" } == 100)
    }

    @Test
    func pasteFileWhileNetworkProcessIsLaunching() async throws {
        let webView = makeWebView()
        try await webView.load(testPageNamed: "paste-image")

        writeBundleFileToPasteboard(try #require(Bundle.testResources.url(forResource: "sunset-in-cupertino-200px", withExtension: "png")))

        // Terminating existing network process, so network process will be relaunched for paste.
        webView.configuration.websiteDataStore._terminateNetworkProcess()

        webView.paste(nil)

        let fileCountScript = "return dataTransfer.files ? dataTransfer.files.length : 0"
        try await waitForCondition("the pasted file to reach the page", timeout: .seconds(10)) {
            try await webView.callJavaScript(returning: Int.self) { fileCountScript } != 0
        }

        #expect(try await webView.callJavaScript(returning: Int.self) { fileCountScript } == 1)
        #expect(try await webView.callJavaScript(returning: String.self) { "return dataTransfer.files[0].type" } == "image/png")
    }

    #endif // WTF_PLATFORM_MAC
}
