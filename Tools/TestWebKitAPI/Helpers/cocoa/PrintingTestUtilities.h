/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#import <WebCore/CSSUnits.h>
#import <array>
#import <optional>
#import <wtf/RetainPtr.h>

OBJC_CLASS NSData;
OBJC_CLASS NSPrintInfo;
OBJC_CLASS NSPrintOperation;
OBJC_CLASS NSWindow;
OBJC_CLASS TestPDFDocument;
OBJC_CLASS WKWebView;
OBJC_CLASS _WKFrameHandle;

namespace TestWebKitAPI {

constexpr CGSize usLetterPaperSizeInPoints { 8.5 * WebCore::CSS::pointsPerInch, 11 * WebCore::CSS::pointsPerInch };
constexpr CGSize usLetterPaperSizeInCSSPixels { 8.5 * WebCore::CSS::pixelsPerInch, 11 * WebCore::CSS::pixelsPerInch };

constexpr CGFloat pointsFromCSSPixels(CGFloat cssPixels)
{
    return cssPixels * (WebCore::CSS::pointsPerInch / WebCore::CSS::pixelsPerInch);
}

RetainPtr<NSData> drawPagesToPDF(WKWebView *, _WKFrameHandle *, NSUInteger pageCount);

// pageNumber counts from 1, like CGPDFDocumentGetPage().
std::optional<std::array<uint8_t, 3>> pdfPageColorAtCSSPixel(NSData *, size_t pageNumber, CGPoint);

bool pdfPageHasColorAtCSSPixel(NSData *, size_t pageNumber, CGPoint, std::array<uint8_t, 3> color, uint8_t tolerance = 2);

#if PLATFORM(MAC) && HAVE(PDFKIT)

using MakePrintOperation = NSPrintOperation *(^)(NSPrintInfo *);

// Prints on US Letter paper (612x792pt, 816x1056px depending on caller).
RetainPtr<TestPDFDocument> printToPDF(MakePrintOperation, NSWindow *);

// Measured from top left.
CGPoint firstCharacterPosition(TestPDFDocument *);

#endif // PLATFORM(MAC) && HAVE(PDFKIT)

} // namespace TestWebKitAPI
