/*
 * Copyright (C) 2004, 2005, 2006, 2013 Apple Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "PDFDocumentImage.h"

#if USE(CG)

#if PLATFORM(IOS)
#include <CoreGraphics/CoreGraphics.h>
#include <ImageIO/ImageIO.h>
#endif

#include "GraphicsContext.h"
#include "ImageBuffer.h"
#include "ImageObserver.h"
#include "IntRect.h"
#include "Length.h"
#include "SharedBuffer.h"
#include "TextStream.h"
#include <CoreGraphics/CGContext.h>
#include <CoreGraphics/CGPDFDocument.h>
#include <wtf/MathExtras.h>
#include <wtf/RAMSize.h>
#include <wtf/RetainPtr.h>
#include <wtf/StdLibExtras.h>

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

FloatSize PDFDocumentImage::size() const
{
    FloatSize expandedCropBoxSize = FloatSize(expandedIntSize(m_cropBox.size()));

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

void PDFDocumentImage::setPdfImageCachingPolicy(PDFImageCachingPolicy pdfImageCachingPolicy)
{
    if (m_pdfImageCachingPolicy == pdfImageCachingPolicy)
        return;

    m_pdfImageCachingPolicy = pdfImageCachingPolicy;
    destroyDecodedData();
}

bool PDFDocumentImage::cacheParametersMatch(GraphicsContext& context, const FloatRect& dstRect, const FloatRect& srcRect) const
{
    if (dstRect.size() != m_cachedDestinationSize)
        return false;

    if (srcRect != m_cachedSourceRect)
        return false;

    if (!m_cachedImageRect.contains(context.clipBounds()))
        return false;

    AffineTransform::DecomposedType decomposedTransform;
    context.getCTM(GraphicsContext::DefinitelyIncludeDeviceScale).decompose(decomposedTransform);

    AffineTransform::DecomposedType cachedDecomposedTransform;
    m_cachedTransform.decompose(cachedDecomposedTransform);
    if (decomposedTransform.scaleX != cachedDecomposedTransform.scaleX || decomposedTransform.scaleY != cachedDecomposedTransform.scaleY)
        return false;

    return true;
}

static void transformContextForPainting(GraphicsContext& context, const FloatRect& dstRect, const FloatRect& srcRect)
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

    // drawPDFPage() relies on drawing the whole PDF into a context starting at (0, 0). We need
    // to transform the destination context such that srcRect of the source context will be drawn
    // in dstRect of destination context.
    context.translate(dstRect.x() - srcRect.x(), dstRect.y() - srcRect.y());
    context.scale(FloatSize(hScale, -vScale));
    context.translate(0, -srcRect.height());
}

// To avoid the jetsam on iOS, we are going to limit the size of all the PDF cachedImages to be 64MB.
static const size_t s_maxCachedImageSide = 4 * 1024;
static const size_t s_maxCachedImageArea = s_maxCachedImageSide * s_maxCachedImageSide;

static const size_t s_maxDecodedDataSize = s_maxCachedImageArea * 4;
static size_t s_allDecodedDataSize = 0;

static FloatRect cachedImageRect(GraphicsContext& context, const FloatRect& dstRect)
{
    FloatRect dirtyRect = context.clipBounds();

    // Calculate the maximum rectangle we can cache around the center of the clipping bounds.
    FloatSize maxSize = s_maxCachedImageSide / context.scaleFactor();
    FloatPoint minLocation = FloatPoint(dirtyRect.center() - maxSize / 2);

    // Ensure the clipping bounds are all included but within the bounds of the dstRect
    return intersection(unionRect(dirtyRect, FloatRect(minLocation, maxSize)), dstRect);
}

void PDFDocumentImage::decodedSizeChanged(size_t newCachedBytes)
{
    if (!m_cachedBytes && !newCachedBytes)
        return;

    if (imageObserver())
        imageObserver()->decodedSizeChanged(this, -safeCast<long long>(m_cachedBytes) + newCachedBytes);

    ASSERT(s_allDecodedDataSize >= m_cachedBytes);
    // Update with the difference in two steps to avoid unsigned underflow subtraction.
    s_allDecodedDataSize -= m_cachedBytes;
    s_allDecodedDataSize += newCachedBytes;

    m_cachedBytes = newCachedBytes;
}

void PDFDocumentImage::updateCachedImageIfNeeded(GraphicsContext& context, const FloatRect& dstRect, const FloatRect& srcRect)
{
#if PLATFORM(IOS)
    // On iOS, some clients use low-quality image interpolation always, which throws off this optimization,
    // as we never get the subsequent high-quality paint. Since live resize is rare on iOS, disable the optimization.
    // FIXME (136593): It's also possible to do the wrong thing here if CSS specifies low-quality interpolation via the "image-rendering"
    // property, on all platforms. We should only do this optimization if we're actually in a ImageQualityController live resize,
    // and are guaranteed to do a high-quality paint later.
    bool repaintIfNecessary = true;
#else
    // If we have an existing image, reuse it if we're doing a low-quality paint, even if cache parameters don't match;
    // we'll rerender when we do the subsequent high-quality paint.
    InterpolationQuality interpolationQuality = context.imageInterpolationQuality();
    bool repaintIfNecessary = interpolationQuality != InterpolationNone && interpolationQuality != InterpolationLow;
#endif

    // Clipped option is for testing only. Force recaching the PDF with each draw.
    if (m_pdfImageCachingPolicy != PDFImageCachingClipBoundsOnly) {
        if (m_cachedImageBuffer && (!repaintIfNecessary || cacheParametersMatch(context, dstRect, srcRect)))
            return;
    }

    switch (m_pdfImageCachingPolicy) {
    case PDFImageCachingDisabled:
        return;
    case PDFImageCachingBelowMemoryLimit:
        // Keep the memory used by the cached image below some threshold, otherwise WebKit process
        // will jetsam if it exceeds its memory limit. Only a rectangle from the PDF may be cached.
        m_cachedImageRect = cachedImageRect(context, dstRect);
        break;
    case PDFImageCachingClipBoundsOnly:
        m_cachedImageRect = context.clipBounds();
        break;
    case PDFImageCachingEnabled:
        m_cachedImageRect = dstRect;
        break;
    }

    FloatSize cachedImageSize = FloatRect(enclosingIntRect(m_cachedImageRect)).size();

    // Cache the PDF image only if the size of the new image won't exceed the cache threshold.
    if (m_pdfImageCachingPolicy == PDFImageCachingBelowMemoryLimit) {
        IntSize scaledSize = ImageBuffer::compatibleBufferSize(cachedImageSize, context);
        if (s_allDecodedDataSize + safeCast<size_t>(scaledSize.width()) * scaledSize.height() * 4 - m_cachedBytes > s_maxDecodedDataSize) {
            destroyDecodedData();
            return;
        }
    }

    m_cachedImageBuffer = ImageBuffer::createCompatibleBuffer(cachedImageSize, context);
    if (!m_cachedImageBuffer) {
        destroyDecodedData();
        return;
    }

    auto& bufferContext = m_cachedImageBuffer->context();
    // We need to transform the coordinate system such that top-left of m_cachedImageRect will be mapped to the
    // top-left of dstRect. Although only m_cachedImageRect.size() of the image copied, the sizes of srcRect
    // and dstRect should be passed to this function because they are used to calculate the image scaling.
    transformContextForPainting(bufferContext, dstRect, FloatRect(m_cachedImageRect.location(), srcRect.size()));
    drawPDFPage(bufferContext);

    m_cachedTransform = context.getCTM(GraphicsContext::DefinitelyIncludeDeviceScale);
    m_cachedDestinationSize = dstRect.size();
    m_cachedSourceRect = srcRect;

    IntSize internalSize = m_cachedImageBuffer->internalSize();
    decodedSizeChanged(safeCast<size_t>(internalSize.width()) * internalSize.height() * 4);
}

void PDFDocumentImage::draw(GraphicsContext& context, const FloatRect& dstRect, const FloatRect& srcRect, CompositeOperator op, BlendMode, ImageOrientationDescription)
{
    if (!m_document || !m_hasPage)
        return;

    updateCachedImageIfNeeded(context, dstRect, srcRect);

    {
        GraphicsContextStateSaver stateSaver(context);
        context.setCompositeOperation(op);

        if (m_cachedImageBuffer) {
            // Draw the ImageBuffer 'm_cachedImageBuffer' to the rectangle 'm_cachedImageRect'
            // on the destination context. Since the pixels of the rectangle 'm_cachedImageRect'
            // of the source PDF was copied to 'm_cachedImageBuffer', the sizes of the source
            // and the destination rectangles will be equal and no scaling will be needed here.
            context.drawImageBuffer(*m_cachedImageBuffer, m_cachedImageRect);
        }
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
    m_cachedImageRect = FloatRect();
    decodedSizeChanged(0);
}

#if !USE(PDFKIT_FOR_PDFDOCUMENTIMAGE)

void PDFDocumentImage::createPDFDocument()
{
    RetainPtr<CGDataProviderRef> dataProvider = adoptCF(CGDataProviderCreateWithCFData(data()->createCFData().get()));
    m_document = adoptCF(CGPDFDocumentCreateWithProvider(dataProvider.get()));
}

void PDFDocumentImage::computeBoundsForCurrentPage()
{
    ASSERT(pageCount() > 0);
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

static void applyRotationForPainting(GraphicsContext& context, FloatSize size, int rotationDegrees)
{
    if (rotationDegrees == 90)
        context.translate(0, size.height());
    else if (rotationDegrees == 180)
        context.translate(size.width(), size.height());
    else if (rotationDegrees == 270)
        context.translate(size.width(), 0);

    context.rotate(-deg2rad(static_cast<float>(rotationDegrees)));
}

void PDFDocumentImage::drawPDFPage(GraphicsContext& context)
{
    applyRotationForPainting(context, size(), m_rotationDegrees);

    context.translate(-m_cropBox.x(), -m_cropBox.y());

    // CGPDF pages are indexed from 1.
    CGContextDrawPDFPage(context.platformContext(), CGPDFDocumentGetPage(m_document.get(), 1));
}

#endif // !USE(PDFKIT_FOR_PDFDOCUMENTIMAGE)

void PDFDocumentImage::dump(TextStream& ts) const
{
    Image::dump(ts);
    ts.dumpProperty("page-count", pageCount());
    ts.dumpProperty("crop-box", m_cropBox);
    if (m_rotationDegrees)
        ts.dumpProperty("rotation", m_rotationDegrees);
}

}

#endif // USE(CG)
