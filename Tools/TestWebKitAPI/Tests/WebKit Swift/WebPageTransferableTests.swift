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

#if ENABLE_SWIFTUI && canImport(Testing) && compiler(>=6.2)

import Testing
@_spi(Testing) import WebKit
import _WebKit_SwiftUI
import UniformTypeIdentifiers
import SwiftUI

@MainActor
struct WebPageTransferableTests {
    @Test
    func transferableContentTypes() async throws {
        let expectedTypes: [UTType] = [.webArchive, .pdf, .png, .url, .flatRTFD, .rtf, .utf8PlainText]
        let exportedContentTypes = WebPage.exportedContentTypes()
        #expect(exportedContentTypes == expectedTypes)

        let importedContentTypes = WebPage.importedContentTypes()
        #expect(importedContentTypes.isEmpty)

        let webPage = WebPage()

        let instanceExportedContentTypes = webPage.exportedContentTypes()
        #expect(instanceExportedContentTypes == expectedTypes)

        let instanceImportedContentTypes = webPage.importedContentTypes()
        #expect(instanceImportedContentTypes.isEmpty)
    }

    @Test
    func exportToPDFWithFullContent() async throws {
        let webPage = WebPage()
        try await webPage.load(html: "<meta name='viewport' content='width=device-width'><body bgcolor=#00ff00>Hello</body>").wait()

        let data = try await webPage.exported(as: .pdf)
        let pdf = TestPDFDocument(from: data)
        #expect(pdf.pageCount == 1)

        let page = try #require(pdf.page(at: 0))

        #expect(page.bounds == .init(x: 0, y: 0, width: 1024, height: 768))
        #expect(page.text == "Hello")
        #expect(page.color(at: .init(x: 400, y: 300)) == .green)
    }

    @Test
    func exportToPDFWithSubRegion() async throws {
        let webPage = WebPage()
        try await webPage.load(html: "<meta name='viewport' content='width=device-width'><body bgcolor=#00ff00>Hello</body>").wait()

        let region = CGRect(x: 200, y: 150, width: 400, height: 300)
        let data = try await webPage.exported(as: .pdf(region: .rect(region)))
        let pdf = TestPDFDocument(from: data)
        #expect(pdf.pageCount == 1)

        let page = try #require(pdf.page(at: 0))

        #expect(page.bounds == .init(x: 0, y: 0, width: 400, height: 300))
        #expect(page.characterCount == 0)
        #expect(page.color(at: .init(x: 200, y: 150)) == .green)
    }

    @Test
    func exportToPDFWithoutLoadingAnyWebContentFails() async throws {
        // FIXME: Either make both macOS and iOS throw an error, or have neither throw an error.
        #if os(macOS)
        let webPage = WebPage()

        await #expect(throws: (any Error).self) {
            let _ = try await webPage.exported(as: .pdf)
        }
        #endif // os(macOS)
    }

    @Test
    func exportToURL() async throws {
        let webPage = WebPage()
        try await webPage.load(html: "<meta name='viewport' content='width=device-width'><body bgcolor=#00ff00>Hello</body>").wait()

        let data = try await webPage.exported(as: .url)
        let actualURL = try await URL(importing: data, contentType: .url)

        #expect(actualURL == URL(string: "about:blank"))
    }

    @Test(arguments: [UTType.png, UTType.image])
    func exportToImage(type: UTType) async throws {
        let defaultFrame = CGRect(x: 0, y: 0, width: 1024, height: 768)

        #if os(macOS)
        let scaleFactor: CGFloat = 2
        #else
        let scaleFactor: CGFloat = 3
        #endif

        let webPage = WebPage()
        try await webPage.load(html: "<body style='background-color: red;'></body>").wait()

        let imageData = try await webPage.exported(as: type)

        #if os(macOS)
        let image = try await NSImage(importing: imageData, contentType: .image)
        #else
        let image = try #require(UIImage(data: imageData))
        #endif

        #expect(image.size.width == defaultFrame.size.width * scaleFactor)
        #expect(image.size.height == defaultFrame.size.height * scaleFactor)
    }

    @Test(arguments: [UTType.plainText, UTType.utf8PlainText])
    func exportToPlainText(type: UTType) async throws {
        let webPage = WebPage()
        try await webPage.load(
            html: """
                <body>
                    <div>beep <b>bop</b></div>
                    <iframe srcdoc='meep'>herp</iframe>
                    <iframe srcdoc='moop'>derp</iframe>
                </body>
                """
        )
        .wait()

        let expected = "beep bop\n  \n\nmeep\n\nmoop"

        let textData = try await webPage.exported(as: type)
        let string = try await String(importing: textData, contentType: type)
        #expect(string == expected)
    }

    @Test(arguments: [UTType.flatRTFD, UTType.rtf])
    func exportToRichText(type: UTType) async throws {
        let webPage = WebPage()
        try await webPage.load(html: "<body style='font-family: system-ui; font-size: 16px;'>Hello <b>World!</b>").wait()

        let textData = try await webPage.exported(as: type)
        let attributedString = try await AttributedString(importing: textData, contentType: type)

        let context = EnvironmentValues().fontResolutionContext

        #if os(macOS)
        let expectedFontSize: CGFloat = 16
        #else
        let expectedFontSize: CGFloat = 23
        #endif

        let firstRange = try #require(attributedString.range(of: "Hello "))
        let firstFont = try #require(Font(attributedString[firstRange].font)).resolve(in: context)
        #expect(!firstFont.isBold)
        #expect(firstFont.pointSize == expectedFontSize)

        let secondRange = try #require(attributedString.range(of: "World!"))
        let secondFont = try #require(Font(attributedString[secondRange].font)).resolve(in: context)
        #expect(secondFont.isBold)
        #expect(secondFont.pointSize == expectedFontSize)
    }
}

#endif // ENABLE_SWIFTUI && canImport(Testing) && compiler(>=6.2)
