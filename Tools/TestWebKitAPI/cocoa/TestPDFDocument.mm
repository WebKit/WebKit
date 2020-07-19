/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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

#import "config.h"
#import "TestPDFDocument.h"

#if HAVE(PDFKIT)

#import <WebCore/ColorMac.h>
#import <pal/spi/cg/CoreGraphicsSPI.h>

namespace TestWebKitAPI {

#if PLATFORM(MAC)
CGRect toCGRect(NSRect rect) { return NSRectToCGRect(rect); }
NSPoint toPlatformPoint(CGPoint point) { return NSPointFromCGPoint(point); }
#else
CGRect toCGRect(CGRect rect) { return rect; }
NSPoint toPlatformPoint(CGPoint point) { return point; }
#endif

// Annotations
TestPDFAnnotation::TestPDFAnnotation(PDFAnnotation *annotation)
    : m_annotation(annotation)
{
}

bool TestPDFAnnotation::isLink() const
{
    bool isLink = [m_annotation.get().type isEqualToString:@"Link"];
    if (isLink)
        ASSERT([m_annotation.get().action isKindOfClass:[PDFActionURL class]]);
    return isLink;
}

CGRect TestPDFAnnotation::bounds() const
{
    return toCGRect(m_annotation.get().bounds);
}

NSURL *TestPDFAnnotation::linkURL() const
{
    if (!isLink())
        return nil;
    return ((PDFActionURL *)m_annotation.get().action).URL;
}

// Pages

Ref<TestPDFPage> TestPDFPage::create(PDFPage *page)
{
    return adoptRef(*new TestPDFPage(page));
}

TestPDFPage::TestPDFPage(PDFPage *page)
    : m_page(page)
{
}

const Vector<TestPDFAnnotation>& TestPDFPage::annotations()
{
    if (!m_annotations) {
        m_annotations = Vector<TestPDFAnnotation>();
        for (PDFAnnotation *annotation in m_page.get().annotations)
            m_annotations->append({ annotation });
    }

    return *m_annotations;
}

size_t TestPDFPage::characterCount() const
{
    return text().length();
}

inline bool shouldStrip(UChar character)
{
    // This is a list of trailing and leading white space characters we've seen in PDF generation.
    // It can be expanded if we see more popup.
    return character == ' ' || character == '\n';
}

String TestPDFPage::text() const
{
    if (!m_textWithoutSurroundingWhitespace)
        m_textWithoutSurroundingWhitespace = String(m_page.get().string).stripLeadingAndTrailingCharacters(shouldStrip);

    return *m_textWithoutSurroundingWhitespace;
}

CGRect TestPDFPage::rectForCharacterAtIndex(size_t index) const
{
    return toCGRect([m_page characterBoundsAtIndex:index]);
}

size_t TestPDFPage::characterIndexAtPoint(CGPoint point) const
{
    return [m_page characterIndexAtPoint:toPlatformPoint(point)];
}

CGRect TestPDFPage::bounds() const
{
    return toCGRect([m_page boundsForBox:kPDFDisplayBoxMediaBox]);
}


WebCore::Color TestPDFPage::colorAtPoint(int x, int y) const
{
    auto boundsRect = bounds();
    auto colorSpace = adoptCF(CGColorSpaceCreateWithName(kCGColorSpaceSRGB));
    auto context = adoptCF(CGBitmapContextCreate(NULL, boundsRect.size.width, boundsRect.size.height, 8, 0, colorSpace.get(), kCGImageAlphaPremultipliedLast | kCGBitmapByteOrder32Big));

    auto cgPage = m_page.get().pageRef;
    CGContextDrawPDFPageWithAnnotations(context.get(), cgPage, nullptr);

    const unsigned char* pixel = (const unsigned char*)CGBitmapContextGetData(context.get());
    size_t i = (y * x * 4) + (x * 4);

    auto r = pixel[i];
    auto g = pixel[i + 1];
    auto b = pixel[i + 2];
    auto a = pixel[i + 3];

    if (!a)
        return WebCore::Color::transparentBlack;
    return WebCore::clampToComponentBytes<WebCore::SRGBA>(r * 255 / a, g * 255 / a, b * 255 / a, a);
}

// Documents

Ref<TestPDFDocument> TestPDFDocument::createFromData(NSData *data)
{
    return adoptRef(*new TestPDFDocument(data));
}

TestPDFDocument::TestPDFDocument(NSData *data)
    : m_document(adoptNS([[PDFDocument alloc] initWithData:data]))
    , m_pages(pageCount())
{
}

size_t TestPDFDocument::pageCount() const
{
    return [m_document pageCount];
}

TestPDFPage* TestPDFDocument::page(size_t index)
{
    if (index >= pageCount())
        return nullptr;
    if (!m_pages[index])
        m_pages[index] = TestPDFPage::create([m_document pageAtIndex:index]);
    return m_pages[index].get();
}

} // namespace TestWebKitAPI

#endif // HAVE(PDFKIT)
