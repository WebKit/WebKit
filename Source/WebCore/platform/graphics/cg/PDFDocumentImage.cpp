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
#include "ImageBuffer.h"
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

PDFDocumentImage::PDFDocumentImage(ImageObserver* observer)
    : Image(observer)
    , m_cachedBytes(0)
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
    // Rotate the crop box and calculate bounding box.
    float sina = sinf(-m_rotation);
    float cosa = cosf(-m_rotation);
    float width = m_cropBox.width();
    float height = m_cropBox.height();

    // Calculate rotated x and y edges of the crop box. If they're negative, it means part of the image has
    // been rotated outside of the bounds and we need to shift over the image so it lies inside the bounds again.
    CGPoint rx = CGPointMake(width * cosa, width * sina);
    CGPoint ry = CGPointMake(-height * sina, height * cosa);

    // Adjust so we are at the crop box origin.
    const CGFloat zero = 0;
    context->translate(floorf(-std::min(zero, std::min(rx.x, ry.x))), floorf(-std::min(zero, std::min(rx.y, ry.y))));

    context->rotate(-m_rotation);
}

bool PDFDocumentImage::cacheParametersMatch(GraphicsContext* context, const FloatRect& dstRect, const FloatRect& srcRect) const
{
    if (dstRect.size() != m_cachedDestinationSize)
        return false;

    if (srcRect != m_cachedSourceRect)
        return false;

    AffineTransform::DecomposedType decomposedTransform;
    context->getCTM(GraphicsContext::DefinitelyIncludeDeviceScale).decompose(decomposedTransform);

    AffineTransform::DecomposedType cachedDecomposedTransform;
    m_cachedTransform.decompose(cachedDecomposedTransform);
    if (decomposedTransform.scaleX != cachedDecomposedTransform.scaleX || decomposedTransform.scaleY != cachedDecomposedTransform.scaleY)
        return false;

    return true;
}

static void transformContextForPainting(GraphicsContext* context, const FloatRect& dstRect, const FloatRect& srcRect)
{
    float hScale = dstRect.width() / srcRect.width();
    float vScale = dstRect.height() / srcRect.height();
    context->translate(srcRect.x() * hScale, srcRect.y() * vScale);
    context->scale(FloatSize(hScale, -vScale));
    context->translate(0, -srcRect.height());
}

void PDFDocumentImage::updateCachedImageIfNeeded(GraphicsContext* context, const FloatRect& dstRect, const FloatRect& srcRect)
{
    // If we have an existing image, reuse it if we're doing a low-quality paint, even if cache parameters don't match;
    // we'll rerender when we do the subsequent high-quality paint.
    InterpolationQuality interpolationQuality = context->imageInterpolationQuality();
    bool useLowQualityInterpolation = interpolationQuality == InterpolationNone || interpolationQuality == InterpolationLow;

    if (!m_cachedImageBuffer || (!cacheParametersMatch(context, dstRect, srcRect) && !useLowQualityInterpolation)) {
        m_cachedImageBuffer = context->createCompatibleBuffer(enclosingIntRect(dstRect).size());
        if (!m_cachedImageBuffer)
            return;
        GraphicsContext* bufferContext = m_cachedImageBuffer->context();
        if (!bufferContext) {
            m_cachedImageBuffer = nullptr;
            return;
        }

        transformContextForPainting(bufferContext, dstRect, srcRect);
        drawPDFPage(bufferContext);

        m_cachedTransform = context->getCTM(GraphicsContext::DefinitelyIncludeDeviceScale);
        m_cachedDestinationSize = dstRect.size();
        m_cachedSourceRect = srcRect;

        IntSize internalSize = m_cachedImageBuffer->internalSize();
        size_t oldCachedBytes = m_cachedBytes;
        m_cachedBytes = internalSize.width() * internalSize.height() * 4;

        if (imageObserver())
            imageObserver()->decodedSizeChanged(this, safeCast<int>(m_cachedBytes) - safeCast<int>(oldCachedBytes));
    }
}

void PDFDocumentImage::draw(GraphicsContext* context, const FloatRect& dstRect, const FloatRect& srcRect, ColorSpace, CompositeOperator op, BlendMode)
{
    if (!m_document || !m_hasPage)
        return;

    updateCachedImageIfNeeded(context, dstRect, srcRect);

    {
        GraphicsContextStateSaver stateSaver(*context);
        context->setCompositeOperation(op);

        if (m_cachedImageBuffer)
            context->drawImageBuffer(m_cachedImageBuffer.get(), ColorSpaceDeviceRGB, dstRect);
        else {
            transformContextForPainting(context, dstRect, srcRect);
            drawPDFPage(context);
        }
    }

    if (imageObserver())
        imageObserver()->didDraw(this);
}

void PDFDocumentImage::destroyDecodedData(bool)
{
    m_cachedImageBuffer = nullptr;

    if (imageObserver())
        imageObserver()->decodedSizeChanged(this, -safeCast<int>(m_cachedBytes));

    m_cachedBytes = 0;
}

unsigned PDFDocumentImage::decodedSize() const
{
    // FIXME: PDFDocumentImage is underreporting decoded sizes because this
    // only includes the cached image and nothing else.

    return m_cachedBytes;
}

#if !USE(PDFKIT_FOR_PDFDOCUMENTIMAGE)
void PDFDocumentImage::createPDFDocument()
{
    RetainPtr<CFDataRef> data = adoptCF(this->data()->createCFData());
    RetainPtr<CGDataProviderRef> dataProvider = adoptCF(CGDataProviderCreateWithCFData(data.get()));
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

    context->translate(-m_cropBox.x(), -m_cropBox.y());

    // CGPDF pages are indexed from 1.
    CGContextDrawPDFPage(context->platformContext(), CGPDFDocumentGetPage(m_document.get(), 1));
}
#endif // !USE(PDFKIT_FOR_PDFDOCUMENTIMAGE)

}

#endif // USE(CG)
