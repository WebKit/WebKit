/*
 * Copyright (C) 2004, 2005, 2006 Apple Computer, Inc.  All rights reserved.
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

#if PLATFORM(CG)

#include "GraphicsContext.h"

using namespace std;

namespace WebCore {

PDFDocumentImage::PDFDocumentImage()
    : Image(0) // PDFs don't animate
    , m_document(0)
    , m_rotation(0.0f)
    , m_currentPage(-1)
{
}

PDFDocumentImage::~PDFDocumentImage()
{
    CGPDFDocumentRelease(m_document);
}

IntSize PDFDocumentImage::size() const
{
    return IntSize((int)m_mediaBox.size().width(), (int)m_mediaBox.size().height());
}

bool PDFDocumentImage::dataChanged(bool allDataReceived)
{
    if (allDataReceived && !m_document) {
        CFDataRef data = (CFDataRef)m_data->createNSData();
        CGDataProviderRef dataProvider = CGDataProviderCreateWithCFData(data);
        CFRelease(data);
        m_document = CGPDFDocumentCreateWithProvider(dataProvider);
        CGDataProviderRelease(dataProvider);
        setCurrentPage(0);
    }
    return m_document; // return true if size is available
}

void PDFDocumentImage::adjustCTM(GraphicsContext* context) const
{
    // rotate the crop box and calculate bounding box
    float sina = sinf(-m_rotation);
    float cosa = cosf(-m_rotation);
    float width = m_cropBox.width();
    float height = m_cropBox.height();

    // calculate rotated x and y edges of the corp box. if they're negative, it means part of the image has
    // been rotated outside of the bounds and we need to shift over the image so it lies inside the bounds again
    CGPoint rx = CGPointMake(width * cosa, width * sina);
    CGPoint ry = CGPointMake(-height * sina, height * cosa);

    // adjust so we are at the crop box origin
    const CGFloat zero = 0;
    CGContextTranslateCTM(context->platformContext(), floorf(-min(zero, min(rx.x, ry.x))), floorf(-min(zero, min(rx.y, ry.y))));

    // rotate -ve to remove rotation
    CGContextRotateCTM(context->platformContext(), -m_rotation);

    // shift so we are completely within media box
    CGContextTranslateCTM(context->platformContext(), m_mediaBox.x() - m_cropBox.x(), m_mediaBox.y() - m_cropBox.y());
}

void PDFDocumentImage::setCurrentPage(int page)
{
    if (!m_document)
        return;

    if (page == m_currentPage)
        return;

    if (!(page >= 0 && page < pageCount()))
        return;

    m_currentPage = page;

    CGPDFPageRef cgPage = CGPDFDocumentGetPage(m_document, page + 1);

    // get media box (guaranteed)
    m_mediaBox = CGPDFPageGetBoxRect(cgPage, kCGPDFMediaBox);

    // get crop box (not always there). if not, use media box
    CGRect r = CGPDFPageGetBoxRect(cgPage, kCGPDFCropBox);
    if (!CGRectIsEmpty(r))
        m_cropBox = r;
    else
        m_cropBox = m_mediaBox;

    // get page rotation angle
    m_rotation = CGPDFPageGetRotationAngle(cgPage) * M_PI / 180.0; // to radians
}

int PDFDocumentImage::pageCount() const
{
    return m_document ? CGPDFDocumentGetNumberOfPages(m_document) : 0;
}

void PDFDocumentImage::draw(GraphicsContext* context, const FloatRect& dstRect, const FloatRect& srcRect, CompositeOperator op)
{
    if (!m_document || m_currentPage == -1)
        return;

    context->save();

    context->setCompositeOperation(op);

    float hScale = dstRect.width() / srcRect.width();
    float vScale = dstRect.height() / srcRect.height();

    // Scale and translate so the document is rendered in the correct location,
    // including accounting for the fact that a GraphicsContext is always flipped
    // and doing appropriate flipping.
    CGContextTranslateCTM(context->platformContext(), dstRect.x() - srcRect.x() * hScale, dstRect.y() - srcRect.y() * vScale);
    CGContextScaleCTM(context->platformContext(), hScale, vScale);
    CGContextScaleCTM(context->platformContext(), 1, -1);
    CGContextTranslateCTM(context->platformContext(), 0, -dstRect.height());
    CGContextClipToRect(context->platformContext(), CGRectIntegral(srcRect));

    // Rotate translate image into position according to doc properties.
    adjustCTM(context);

    // Media box may have non-zero origin which we ignore. Pass 1 for the page number.
    CGContextDrawPDFDocument(context->platformContext(), FloatRect(FloatPoint(), size()),
        m_document, m_currentPage + 1);

    context->restore();
}

}

#endif // PLATFORM(CG)
