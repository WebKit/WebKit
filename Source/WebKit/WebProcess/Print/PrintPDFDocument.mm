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

#import "config.h"
#import "PrintPDFDocument.h"

#if ENABLE(GPU_PROCESS) && PLATFORM(COCOA)

#import "PDFKitSPI.h"
#import <WebCore/FloatRect.h>
#import <WebCore/GraphicsContext.h>
#import <WebCore/SharedMemory.h>
#import <wtf/cocoa/SpanCocoa.h>

#import <pal/cocoa/PDFKitSoftLink.h>

namespace WebKit {
using namespace WebCore;

RefPtr<PrintPDFDocument> PrintPDFDocument::create(SharedMemory& data)
{
    RetainPtr nsData = toNSData(data.span());
    RetainPtr document = adoptNS([PAL::allocPDFDocumentInstance() initWithData:nsData.get()]);
    if (!document)
        return nullptr;
    return adoptRef(*new PrintPDFDocument(WTF::move(document)));
}

PrintPDFDocument::PrintPDFDocument(RetainPtr<PDFDocument>&& document)
    : m_document(WTF::move(document))
{
}

PrintPDFDocument::~PrintPDFDocument() = default;

void PrintPDFDocument::drawPage(GraphicsContext& context, unsigned pageIndex, const FloatRect& source, const FloatRect& destination, bool includeLinks) const
{
    RetainPtr cgContext = context.platformContext();
    if (!cgContext || source.isEmpty() || destination.isEmpty())
        return;

    // PDFKit documents are not to be used on several threads at once.
    Locker locker { m_lock };
    if (pageIndex >= [m_document pageCount])
        return;
    RetainPtr page = [m_document pageAtIndex:pageIndex];
    if (!page)
        return;

    // The size of the page as displayed, which PDFKit draws with its origin at the bottom left.
    CGRect cropBox = [page boundsForBox:kPDFDisplayBoxCropBox];
    NSInteger rotation = [page rotation];
    CGFloat displayedHeight = (rotation == 90 || rotation == 270) ? cropBox.size.width : cropBox.size.height;

    CGContextSaveGState(cgContext.get());
    CGContextClipToRect(cgContext.get(), destination);
    CGContextTranslateCTM(cgContext.get(), destination.x(), destination.y());
    CGContextScaleCTM(cgContext.get(), destination.width() / source.width(), destination.height() / source.height());
    CGContextTranslateCTM(cgContext.get(), -source.x(), -source.y());
    CGContextTranslateCTM(cgContext.get(), 0, displayedHeight);
    CGContextScaleCTM(cgContext.get(), 1, -1);

    [page drawWithBox:kPDFDisplayBoxCropBox toContext:cgContext.get()];

    // The links stay links in the printed PDF.
    if (includeLinks && context.renderingMode() == RenderingMode::PDFDocument) {
        // Annotations are in the space of the page, which PDFKit draws through this.
        CGAffineTransform pageTransform = [page transformForBox:kPDFDisplayBoxCropBox];
        CGAffineTransform transform = CGContextGetCTM(cgContext.get());
        CGRect clip = CGContextGetClipBoundingBox(cgContext.get());
        // FIXME: Remove the suppression once https://github.com/llvm/llvm-project/pull/230818 is deployed (rdar://189710680).
        for (SUPPRESS_UNRETAINED_LOCAL PDFAnnotation *annotation in [page annotations]) {
            if (![[annotation valueForAnnotationKey:PAL::get_PDFKit_PDFAnnotationKeySubtypeSingleton()] isEqualToString:PAL::get_PDFKit_PDFAnnotationSubtypeLinkSingleton()])
                continue;
            RetainPtr<NSURL> url = annotation.URL;
            if (!url)
                continue;
            CGRect bounds = CGRectIntersection(CGRectApplyAffineTransform(annotation.bounds, pageTransform), clip);
            if (CGRectIsEmpty(bounds))
                continue;
            CGPDFContextSetURLForRect(cgContext.get(), bridge_cast(url.get()), CGRectApplyAffineTransform(bounds, transform));
        }
    }

    CGContextRestoreGState(cgContext.get());
}

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS) && PLATFORM(COCOA)
