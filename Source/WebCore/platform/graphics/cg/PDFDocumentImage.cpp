/*
 * Copyright (C) 2004, 2005, 2006, 2013 Apple Computer, Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#define _USE_MATH_DEFINES 1
#include "config.h"
#include "PDFDocumentImage.h"

#if USE(CG)

#include "GraphicsContext.h"
#include "ImageObserver.h"
#include "Length.h"
#include "SharedBuffer.h"
#include <CoreGraphics/CGContext.h>
#include <CoreGraphics/CGPDFDocument.h>
#include <wtf/MathExtras.h>
#include <wtf/RetainPtr.h>

#if !PLATFORM(MAC)
#include "ImageSourceCG.h"
#endif

namespace WebCore {

PDFDocumentImage::PDFDocumentImage()
    : Image(0) // PDFs don't animate
    , m_rotation(0.0f)
    , m_hasPage(false)
{
}

PDFDocumentImage::~PDFDocumentImage()
{
}

String PDFDocumentImage::filenameExtension() const
{
    return "pdf";
}

IntSize PDFDocumentImage::size() const
{
    const float sina = sinf(-m_rotation);
    const float cosa = cosf(-m_rotation);
    const float width = m_mediaBox.size().width();
    const float height = m_mediaBox.size().height();
    const float rotWidth = fabsf(width * cosa - height * sina);
    const float rotHeight = fabsf(width * sina + height * cosa);
    
    return expandedIntSize(FloatSize(rotWidth, rotHeight));
}

void PDFDocumentImage::computeIntrinsicDimensions(Length& intrinsicWidth, Length& intrinsicHeight, FloatSize& intrinsicRatio)
{
    // FIXME: If we want size negotiation with PDF documents as-image, this is the place to implement it (https://bugs.webkit.org/show_bug.cgi?id=12095).
    Image::computeIntrinsicDimensions(intrinsicWidth, intrinsicHeight, intrinsicRatio);
    intrinsicRatio = FloatSize();
}

bool PDFDocumentImage::dataChanged(bool allDataReceived)
{
    ASSERT(!m_document);
    if (allDataReceived && !m_document) {
        createPDFDocument();

        if (pageCount()) {
            m_hasPage = true;
            computeBoundsForCurrentPage();
        }
    }
    return m_document; // Return true if size is available.
}

void PDFDocumentImage::applyRotationForPainting(GraphicsContext* context) const
{
    // rotate the crop box and calculate bounding box
    float sina = sinf(-m_rotation);
    float cosa = cosf(-m_rotation);
    float width = m_cropBox.width();
    float height = m_cropBox.height();

    // calculate rotated x and y edges of the crop box. if they're negative, it means part of the image has
    // been rotated outside of the bounds and we need to shift over the image so it lies inside the bounds again
    CGPoint rx = CGPointMake(width * cosa, width * sina);
    CGPoint ry = CGPointMake(-height * sina, height * cosa);

    // adjust so we are at the crop box origin
    const CGFloat zero = 0;
    CGContextTranslateCTM(context->platformContext(), floorf(-std::min(zero, std::min(rx.x, ry.x))), floorf(-std::min(zero, std::min(rx.y, ry.y))));

    // rotate -ve to remove rotation
    CGContextRotateCTM(context->platformContext(), -m_rotation);
}

void PDFDocumentImage::draw(GraphicsContext* context, const FloatRect& dstRect, const FloatRect& srcRect, ColorSpace, CompositeOperator op, BlendMode)
{
    if (!m_document || !m_hasPage)
        return;

    {
        GraphicsContextStateSaver stateSaver(*context);

        context->setCompositeOperation(op);

        float hScale = dstRect.width() / srcRect.width();
        float vScale = dstRect.height() / srcRect.height();

        // Scale and translate so the document is rendered in the correct location,
        // accounting for the fact that the GraphicsContext is flipped.
        CGContextTranslateCTM(context->platformContext(), dstRect.x() - srcRect.x() * hScale, dstRect.y() - srcRect.y() * vScale);
        CGContextScaleCTM(context->platformContext(), hScale, vScale);
        CGContextScaleCTM(context->platformContext(), 1, -1);
        CGContextTranslateCTM(context->platformContext(), 0, -srcRect.height());
        CGContextClipToRect(context->platformContext(), CGRectIntegral(srcRect));

        drawPDFPage(context);
    }

    if (imageObserver())
        imageObserver()->didDraw(this);
}

#if !USE(PDFKIT_FOR_PDFDOCUMENTIMAGE)
void PDFDocumentImage::createPDFDocument()
{
    RetainPtr<CGDataProviderRef> dataProvider = adoptCF(CGDataProviderCreateWithCFData(data()->createCFData().get()));
    m_document = CGPDFDocumentCreateWithProvider(dataProvider.get());
}

void PDFDocumentImage::computeBoundsForCurrentPage()
{
    CGPDFPageRef cgPage = CGPDFDocumentGetPage(m_document.get(), 1);

    m_mediaBox = CGPDFPageGetBoxRect(cgPage, kCGPDFMediaBox);

    // Get crop box (not always there). If not, use media box.
    CGRect r = CGPDFPageGetBoxRect(cgPage, kCGPDFCropBox);
    if (!CGRectIsEmpty(r))
        m_cropBox = r;
    else
        m_cropBox = m_mediaBox;

    m_rotation = deg2rad(static_cast<float>(CGPDFPageGetRotationAngle(cgPage)));
}

unsigned PDFDocumentImage::pageCount() const
{
    return CGPDFDocumentGetNumberOfPages(m_document.get());
}

void PDFDocumentImage::drawPDFPage(GraphicsContext* context)
{
    applyRotationForPainting(context);

    CGContextTranslateCTM(context->platformContext(), -m_cropBox.x(), -m_cropBox.y());

    // CGPDF pages are indexed from 1.
    CGContextDrawPDFPage(context->platformContext(), CGPDFDocumentGetPage(m_document.get(), 1));
}
#endif // !USE(PDFKIT_FOR_PDFDOCUMENTIMAGE)

}

#endif // USE(CG)
