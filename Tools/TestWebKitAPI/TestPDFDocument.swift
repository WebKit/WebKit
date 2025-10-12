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

#if HAVE_PDFKIT

import Foundation
import WebKit
import PDFKit
import CoreGraphics

#if canImport(UIKit)
import UIKit
typealias CocoaColor = UIColor
#else
import AppKit
typealias CocoaColor = NSColor
#endif

@_objcImplementation extension TestPDFAnnotation {
    @nonobjc private var annotation: PDFAnnotation

    var isLink: Bool {
        let link = annotation.type == "Link"
        assert(!link || annotation.action is PDFActionURL)
        return link
    }

    var bounds: CGRect {
        annotation.bounds
    }

    var linkURL: URL? {
        guard isLink else {
            return nil
        }

        return (annotation.action as? PDFActionURL)?.url
    }

    @objc(initWithPDFAnnotation:)
    init(pdfAnnotation: PDFAnnotation) {
        self.annotation = pdfAnnotation
    }
}

@_objcImplementation extension TestPDFPage {
    @nonobjc private var page: PDFPage

    lazy var text: String = {
        page.string?.trimmingCharacters(in: .whitespacesAndNewlines) ?? ""
    }()

    var bounds: CGRect {
        page.bounds(for: .mediaBox)
    }
    
    var characterCount: Int {
        text.count
    }

    lazy var annotations: [TestPDFAnnotation] = { @MainActor in
        page.annotations.map(TestPDFAnnotation.init(pdfAnnotation:))
    }()

    @objc(characterIndexAtPoint:)
    func characterIndex(at point: CGPoint) -> Int {
        page.characterIndex(at: point)
    }
    
    @objc(colorAtPoint:)
    func color(at point: CGPoint) -> CocoaColor {
        let boundsRect = bounds
        let colorSpace = CGColorSpace(name: CGColorSpace.sRGB)

        #if swift(>=6.2)
        // FIXME: document safety invariants here.
        #if HAVE_CGCONTEXT_INIT_WITH_BITMAP_INFO_AND_NULLABLE_COLORSPACE
        let context = unsafe CGContext(
            data: nil,
            width: Int(boundsRect.size.width),
            height: Int(boundsRect.size.height),
            bitsPerComponent: 8,
            bytesPerRow: 0,
            space: colorSpace,
            bitmapInfo: CGBitmapInfo(alpha: .premultipliedLast, byteOrder: .order32Big)
        )
        #else
        let context = unsafe CGContext(
            data: nil,
            width: Int(boundsRect.size.width),
            height: Int(boundsRect.size.height),
            bitsPerComponent: 8,
            bytesPerRow: 0,
            space: colorSpace!,
            bitmapInfo: CGBitmapInfo(rawValue: CGImageAlphaInfo.premultipliedLast.rawValue).union(.byteOrder32Big).rawValue
        )
        #endif
        #else
        #if HAVE_CGCONTEXT_INIT_WITH_BITMAP_INFO_AND_NULLABLE_COLORSPACE
        let context = CGContext(
            data: nil,
            width: Int(boundsRect.size.width),
            height: Int(boundsRect.size.height),
            bitsPerComponent: 8,
            bytesPerRow: 0,
            space: colorSpace,
            bitmapInfo: CGBitmapInfo(alpha: .premultipliedLast, byteOrder: .order32Big)
        )
        #else
        let context = CGContext(
            data: nil,
            width: Int(boundsRect.size.width),
            height: Int(boundsRect.size.height),
            bitsPerComponent: 8,
            bytesPerRow: 0,
            space: colorSpace!,
            bitmapInfo: CGBitmapInfo(rawValue: CGImageAlphaInfo.premultipliedLast.rawValue).union(.byteOrder32Big).rawValue
        )
        #endif
        #endif

        guard let context, let cgPage = page.pageRef else {
            fatalError()
        }

        CGContextDrawPDFPageWithAnnotations(context, cgPage, nil)

        let x = Int(point.x)
        let y = Int(point.y)
        let index = (y * x * 4) + (x * 4)

        #if swift(>=6.2)
        // FIXME: document safety invariants here. Is CGContext always guaranteed to
        // contain a context.data of the right size at this point?
        let pixels = unsafe UnsafeMutableRawBufferPointer(start: context.data, count: context.width * context.height * 4)
        let r = unsafe pixels[index]
        let g = unsafe pixels[index + 1]
        let b = unsafe pixels[index + 2]
        let a = unsafe pixels[index + 3]
        #else
        let pixels = UnsafeMutableRawBufferPointer(start: context.data, count: context.width * context.height * 4)
        let r = pixels[index]
        let g = pixels[index + 1]
        let b = pixels[index + 2]
        let a = pixels[index + 3]
        #endif

        if a == 0 {
            return .clear
        }

        let red = CGFloat(r) * 255 / CGFloat(a) / 255.0
        let green = CGFloat(g) * 255 / CGFloat(a) / 255.0
        let blue = CGFloat(b) * 255 / CGFloat(a) / 255.0
        let alpha = CGFloat(a) / 255.0

        return CocoaColor(red: red, green: green, blue: blue, alpha: alpha)
    }
    
    @objc(rectForCharacterAtIndex:)
    func rectForCharacter(at index: Int) -> CGRect {
        page.characterBounds(at: index)
    }
    
    @objc(initWithPDFPage:)
    init(pdfPage: PDFPage) {
        self.page = pdfPage
    }
}

@_objcImplementation extension TestPDFDocument {
    @nonobjc private let document: PDFDocument
    @nonobjc private var pages: [TestPDFPage?]

    var pageCount: Int {
        document.pageCount
    }

    @objc(initFromData:)
    init(from data: Data) {
        let document = PDFDocument(data: data)!
        self.document = document
        self.pages = .init(repeating: nil, count: document.pageCount)
    }

    @objc(pageAtIndex:)
    func page(at index: Int) -> TestPDFPage? {
        precondition((0..<pageCount).contains(index))

        if pages[index] == nil {
            guard let page = document.page(at: index) else {
                return nil
            }
            pages[index] = TestPDFPage(pdfPage: page)
        }

        return pages[index]
    }
}

#endif // HAVE_PDFKIT
