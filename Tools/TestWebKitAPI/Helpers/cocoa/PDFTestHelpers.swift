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

import CoreGraphics
import CoreText
import Foundation
private import TestWebKitAPILibrary.Helpers.cocoa.PDFTestHelpers
private import TestWebKitAPILibrary.Helpers.cocoa.WKWebViewConfigurationExtras
import WebKit
private import WebKit_Private
private import WebKit_Private.WKPreferencesPrivate

import struct Foundation.URL
import struct Swift.String

@objc
@implementation
extension TestPDFBuilder {
    class func pdfData() -> Data {
        guard let url = Bundle.testResources.url(forResource: "test", withExtension: "pdf"),
            let data = try? Data(contentsOf: url)
        else {
            fatalError("Unable to load test.pdf from the test resources bundle")
        }

        return data
    }

    class func pdfDataWithLink() -> Data {
        let pdfData = NSMutableData()

        // swift-format-ignore: NeverForceUnwrap
        let consumer = CGDataConsumer(data: pdfData)!
        var mediaBox = CGRect(x: 0, y: 0, width: 400, height: 200)
        // swift-format-ignore: NeverForceUnwrap
        let context = unsafe CGContext(consumer: consumer, mediaBox: &mediaBox, nil)!

        context.beginPDFPage(nil)

        let font = CTFontCreateWithName("Helvetica" as CFString, 24, nil)
        let text = NSAttributedString(
            string: "Visit our website for details",
            attributes: [kCTFontAttributeName as NSAttributedString.Key: font]
        )
        let line = CTLineCreateWithAttributedString(text)

        let baselineX: CGFloat = 20
        let baselineY: CGFloat = 100
        context.textPosition = CGPoint(x: baselineX, y: baselineY)
        CTLineDraw(line, context)

        // "Visit our website for details"
        //  0123456789...
        // "our website" spans [6, 17).
        let linkStartX = baselineX + CTLineGetOffsetForStringIndex(line, 6, nil)
        let linkEndX = baselineX + CTLineGetOffsetForStringIndex(line, 17, nil)

        let lineBounds = CTLineGetBoundsWithOptions(line, [])
        let linkRect = CGRect(
            x: linkStartX,
            y: baselineY + lineBounds.minY,
            width: linkEndX - linkStartX,
            height: lineBounds.height
        )

        // swift-format-ignore: NeverForceUnwrap
        context.setURL(URL(string: "https://www.example.com/")! as CFURL, for: linkRect)

        context.endPDFPage()
        context.closePDF()

        return pdfData as Data
    }

    @MainActor
    class func configurationForUnifiedPDF(withHUDEnabled hudEnabled: Bool) -> WKWebViewConfiguration {
        // swift-format-ignore: NeverForceUnwrap
        let configuration = WKWebViewConfiguration._test_configurationWithTestPlugInClassName(
            "WebProcessPlugInWithInternals",
            configureJSCForTesting: true
        )!

        for feature in WKPreferences._features() {
            switch feature.key {
            case "UnifiedPDFEnabled":
                configuration.preferences._setEnabled(true, for: feature)
            case "PDFPluginHUDEnabled":
                configuration.preferences._setEnabled(hudEnabled, for: feature)
            default:
                break
            }
        }

        return configuration
    }
}
