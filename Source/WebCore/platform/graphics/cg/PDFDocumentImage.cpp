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

#if PLATFORM(IOS)
#import <CoreGraphics/CGContextPrivate.h>
#import <CoreGraphics/CGContextGState.h>
#import <CoreGraphics/CoreGraphics.h>
#import <ImageIO/ImageIO.h>
#endif

#include "GraphicsContext.h"
#include "ImageBuffer.h"
#include "ImageObserver.h"
#include "Length.h"
#include "SharedBuffer.h"
#include <CoreGraphics/CGContext.h>
#include <CoreGraphics/CGPDFDocument.h>
#include <wtf/MathExtras.h>
#include <wtf/RetainPtr.h>

#if !PLATFORM(COCOA)
#include "ImageSourceCG.h"
#endif

namespace WebCore {

PDFDocumentImage::PDFDocumentImage(ImageObserver* observer)
    : Image(observer)
    , m_cachedBytes(0)
    , m_rotationDegrees(0)
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
    IntSize expandedCropBoxSize = expandedIntSize(m_cropBox.size());

    if (m_rotationDegrees == 90 || m_rotationDegrees == 270)
        return expandedCropBoxSize.transposedSize();
    return expandedCropBoxSize;
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

    if (hScale != vScale) {
        float minimumScale = std::max((dstRect.width() - 0.5) / srcRect.width(), (dstRect.height() - 0.5) / srcRect.height());
        float maximumScale = std::min((dstRect.width() + 0.5) / srcRect.width(), (dstRect.height() + 0.5) / srcRect.height());

        // If the difference between the two scales is due to integer rounding of image sizes,
        // use the smaller of the two original scales to ensure that the image fits inside the
        // space originally allocated for it.
        if (minimumScale <= maximumScale) {
            hScale = std::min(hScale, vScale);
            vScale = hScale;
        }
    }

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

void PDFDocumentImage::draw(GraphicsContext* context, const FloatRect& dstRect, const FloatRect& srcRect, ColorSpace, CompositeOperator op, BlendMode, ImageOrientationDescription)
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

#if !USE(PDFKIT_FOR_PDFDOCUMENTIMAGE)
void PDFDocumentImage::createPDFDocument()
{
    RetainPtr<CGDataProviderRef> dataProvider = adoptCF(CGDataProviderCreateWithCFData(data()->createCFData().get()));
    m_document = CGPDFDocumentCreateWithProvider(dataProvider.get());
}

void PDFDocumentImage::computeBoundsForCurrentPage()
{
    CGPDFPageRef cgPage = CGPDFDocumentGetPage(m_document.get(), 1);
    CGRect mediaBox = CGPDFPageGetBoxRect(cgPage, kCGPDFMediaBox);

    // Get crop box (not always there). If not, use media box.
    CGRect r = CGPDFPageGetBoxRect(cgPage, kCGPDFCropBox);
    if (!CGRectIsEmpty(r))
        m_cropBox = r;
    else
        m_cropBox = mediaBox;

    m_rotationDegrees = CGPDFPageGetRotationAngle(cgPage);
}

unsigned PDFDocumentImage::pageCount() const
{
    return CGPDFDocumentGetNumberOfPages(m_document.get());
}

static void applyRotationForPainting(GraphicsContext* context, IntSize size, int rotationDegrees)
{
    if (rotationDegrees == 90)
        context->translate(0, size.height());
    else if (rotationDegrees == 180)
        context->translate(size.width(), size.height());
    else if (rotationDegrees == 270)
        context->translate(size.width(), 0);

    context->rotate(-deg2rad(static_cast<float>(rotationDegrees)));
}

void PDFDocumentImage::drawPDFPage(GraphicsContext* context)
{
    applyRotationForPainting(context, size(), m_rotationDegrees);

    context->translate(-m_cropBox.x(), -m_cropBox.y());

    // CGPDF pages are indexed from 1.
    CGContextDrawPDFPage(context->platformContext(), CGPDFDocumentGetPage(m_document.get(), 1));
}
#endif // !USE(PDFKIT_FOR_PDFDOCUMENTIMAGE)

}

#endif // USE(CG)
