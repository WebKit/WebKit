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

#if PLATFORM(IOS_FAMILY)
#include <CoreGraphics/CoreGraphics.h>
#include <ImageIO/ImageIO.h>
#endif

#include "GraphicsContext.h"
#include "ImageBuffer.h"
#include "ImageObserver.h"
#include "IntRect.h"
#include "Length.h"
#include "NotImplemented.h"
#include "SharedBuffer.h"
#include <CoreGraphics/CGContext.h>
#include <CoreGraphics/CGPDFDocument.h>
#include <pal/spi/cg/CoreGraphicsSPI.h>
#include <wtf/MathExtras.h>
#include <wtf/RAMSize.h>
#include <wtf/RetainPtr.h>
#include <wtf/StdLibExtras.h>
#include <wtf/text/TextStream.h>

#if !PLATFORM(COCOA)
#include "ImageSourceCG.h"
#endif

namespace WebCore {

PDFDocumentImage::PDFDocumentImage(ImageObserver* observer)
    : Image(observer)
{
}

PDFDocumentImage::~PDFDocumentImage() = default;

String PDFDocumentImage::filenameExtension() const
{
    return "pdf"_s;
}

FloatSize PDFDocumentImage::size(ImageOrientation) const
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

EncodedDataStatus PDFDocumentImage::dataChanged(bool allDataReceived)
{
    ASSERT(!m_document);
    if (allDataReceived && !m_document) {
        createPDFDocument();

        if (pageCount()) {
            m_hasPage = true;
            computeBoundsForCurrentPage();
        }
    }
    return m_document ? EncodedDataStatus::Complete : EncodedDataStatus::Unknown;
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
    // Old and new source rectangles have to match.
    if (srcRect != m_cachedSourceRect)
        return false;

    // Old and new scaling factors "dest / src" have to match.
    if (dstRect.size() != m_cachedDestinationRect.size())
        return false;

    // m_cachedImageRect can be moved if srcRect and dstRect.size() did not change.
    FloatRect movedCachedImageRect = m_cachedImageRect;
    movedCachedImageRect.move(FloatSize(dstRect.location() - m_cachedDestinationRect.location()));

    // movedCachedImageRect has to contain the whole dirty rectangle.
    FloatRect dirtyRect = intersection(context.clipBounds(), dstRect);
    if (!movedCachedImageRect.contains(dirtyRect))
        return false;

    // Old and new context scaling factors have to match as well.
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
    context.translate(dstRect.location() - srcRect.location());
    context.scale(FloatSize(hScale, -vScale));
    context.translate(0, -srcRect.height());

    // FIXME https://bugs.webkit.org/show_bug.cgi?id=227808: Surely there needs to be a clip operation here,
    // if the srcRect is smaller than the whole PDF.
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
        imageObserver()->decodedSizeChanged(*this, -static_cast<long long>(m_cachedBytes) + newCachedBytes);

    ASSERT(s_allDecodedDataSize >= m_cachedBytes);
    // Update with the difference in two steps to avoid unsigned underflow subtraction.
    s_allDecodedDataSize -= m_cachedBytes;
    s_allDecodedDataSize += newCachedBytes;

    m_cachedBytes = newCachedBytes;
}

void PDFDocumentImage::updateCachedImageIfNeeded(GraphicsContext& context, const FloatRect& dstRect, const FloatRect& srcRect)
{
    // Clipped option is for testing only. Force re-caching the PDF with each draw.
    bool forceUpdateCachedImage = m_pdfImageCachingPolicy == PDFImageCachingPolicy::ClipBoundsOnly || !m_cachedImageBuffer;
    if (!forceUpdateCachedImage && cacheParametersMatch(context, dstRect, srcRect)) {
        // Adjust the view-port rectangles if no re-caching will happen.
        m_cachedImageRect.move(FloatSize(dstRect.location() - m_cachedDestinationRect.location()));
        m_cachedDestinationRect = dstRect;
        return;
    }

    switch (m_pdfImageCachingPolicy) {
    case PDFImageCachingPolicy::Disabled:
        return;
    case PDFImageCachingPolicy::BelowMemoryLimit:
        // Keep the memory used by the cached image below some threshold, otherwise WebKit process
        // will jetsam if it exceeds its memory limit. Only a rectangle from the PDF may be cached.
        m_cachedImageRect = cachedImageRect(context, dstRect);
        break;
    case PDFImageCachingPolicy::ClipBoundsOnly:
        m_cachedImageRect = intersection(context.clipBounds(), dstRect);
        break;
    case PDFImageCachingPolicy::Enabled:
        m_cachedImageRect = dstRect;
        break;
    }

    FloatSize cachedImageSize = FloatRect(enclosingIntRect(m_cachedImageRect)).size();

    // Cache the PDF image only if the size of the new image won't exceed the cache threshold.
    if (m_pdfImageCachingPolicy == PDFImageCachingPolicy::BelowMemoryLimit) {
        auto scaledSize = context.compatibleImageBufferSize(cachedImageSize);
        if (s_allDecodedDataSize + scaledSize.unclampedArea() * 4 - m_cachedBytes > s_maxDecodedDataSize) {
            destroyDecodedData();
            return;
        }
    }

    // Create a local ImageBuffer because decoding the PDF images has to happen in WebProcess.
    m_cachedImageBuffer = context.createAlignedImageBuffer(cachedImageSize, DestinationColorSpace::SRGB(), RenderingMethod::Local);
    if (!m_cachedImageBuffer) {
        destroyDecodedData();
        return;
    }

    auto& bufferContext = m_cachedImageBuffer->context();
    // We need to transform the coordinate system such that top-left of m_cachedImageRect will be mapped to the
    // top-left of dstRect. Although only m_cachedImageRect.size() of the image copied, the sizes of srcRect
    // and dstRect should be passed to this function because they are used to calculate the image scaling.
    // FIXME https://bugs.webkit.org/show_bug.cgi?id=227809: Passing in m_cachedImageRect.location() for the
    // source rect position doesn't seem correct here.
    transformContextForPainting(bufferContext, dstRect, FloatRect(m_cachedImageRect.location(), srcRect.size()));
    drawPDFPage(bufferContext);

    m_cachedTransform = context.getCTM(GraphicsContext::DefinitelyIncludeDeviceScale);
    m_cachedDestinationRect = dstRect;
    m_cachedSourceRect = srcRect;
    ++m_cachingCountForTesting;

    IntSize backendSize = m_cachedImageBuffer->backendSize();
    decodedSizeChanged(backendSize.unclampedArea() * 4);
}

ImageDrawResult PDFDocumentImage::draw(GraphicsContext& context, const FloatRect& dstRect, const FloatRect& srcRect, const ImagePaintingOptions& options)
{
    if (!m_document || !m_hasPage)
        return ImageDrawResult::DidNothing;

    updateCachedImageIfNeeded(context, dstRect, srcRect);

    {
        GraphicsContextStateSaver stateSaver(context);
        context.setCompositeOperation(options.compositeOperator());

        if (m_cachedImageBuffer) {
            // Draw the ImageBuffer 'm_cachedImageBuffer' to the rectangle 'm_cachedImageRect'
            // on the destination context. Since the pixels of the rectangle 'm_cachedImageRect'
            // of the source PDF was copied to 'm_cachedImageBuffer', the sizes of the source
            // and the destination rectangles will be equal and no scaling will be needed here.
            context.drawImageBuffer(*m_cachedImageBuffer, m_cachedImageRect);
        } else if (context.hasPlatformContext()) {
            transformContextForPainting(context, dstRect, srcRect);
            drawPDFPage(context);
        } else {
            // We can't draw the PDF directly because we have no platform context (probably because we're doing display list drawing).
            // We also need to be careful about not getting terminated due to memory pressure.
            //
            // (scalar * width) * (scalar * height) = max number of pixels
            // Solve for scalar...
            // scalar = sqrt(max number of pixels / (width * height))
            auto scalar = std::min(1.f, std::sqrt(static_cast<float>(s_maxCachedImageArea) / (dstRect.width() * dstRect.height())));
            FloatRect localDestinationRect(FloatPoint(), dstRect.size() * scalar);
            if (auto imageBuffer = context.createAlignedImageBuffer(localDestinationRect.size(), DestinationColorSpace::SRGB(), RenderingMethod::Local)) {
                auto& bufferContext = imageBuffer->context();
                transformContextForPainting(bufferContext, localDestinationRect, srcRect);
                drawPDFPage(bufferContext);
                context.drawImageBuffer(*imageBuffer, dstRect);
            } else
                return ImageDrawResult::DidNothing;
        }
    }

    if (imageObserver())
        imageObserver()->didDraw(*this);

    return ImageDrawResult::DidDraw;
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
    RetainPtr<CGDataProviderRef> dataProvider = adoptCF(CGDataProviderCreateWithCFData(data()->makeContiguous()->createCFData().get()));
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
        context.translate(size);
    else if (rotationDegrees == 270)
        context.translate(size.width(), 0);

    context.rotate(-deg2rad(static_cast<float>(rotationDegrees)));
}

void PDFDocumentImage::drawPDFPage(GraphicsContext& context)
{
    applyRotationForPainting(context, size(), m_rotationDegrees);

    context.translate(-m_cropBox.location());

    // CGPDF pages are indexed from 1.
#if PLATFORM(COCOA)
    CGContextDrawPDFPageWithAnnotations(context.platformContext(), CGPDFDocumentGetPage(m_document.get(), 1), nullptr);
#else
    CGContextDrawPDFPage(context.platformContext(), CGPDFDocumentGetPage(m_document.get(), 1));
#endif
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
