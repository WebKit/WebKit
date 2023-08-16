/*
 * Copyright (C) 2004-2023 Apple Inc.  All rights reserved.
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

#include "CachedSubimage.h"
#include "GraphicsContext.h"
#include "ImageObserver.h"
#include <CoreGraphics/CGContext.h>
#include <CoreGraphics/CGPDFDocument.h>
#include <pal/spi/cg/CoreGraphicsSPI.h>
#include <wtf/RetainPtr.h>
#include <wtf/text/TextStream.h>

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

void PDFDocumentImage::decodedSizeChanged(size_t newCachedBytes)
{
    if (!m_cachedBytes && !newCachedBytes)
        return;

    if (imageObserver())
        imageObserver()->decodedSizeChanged(*this, -static_cast<long long>(m_cachedBytes) + newCachedBytes);

    m_cachedBytes = newCachedBytes;
}

bool PDFDocumentImage::shouldDrawFromCachedSubimage(GraphicsContext& context) const
{
    if (mustDrawFromCachedSubimage(context))
        return true;

    auto scaledImageSize = size() * context.scaleFactor();
    return scaledImageSize.area() > CachedSubimage::maxArea;
}

bool PDFDocumentImage::mustDrawFromCachedSubimage(GraphicsContext& context) const
{
    return !context.hasPlatformContext();
}

ImageDrawResult PDFDocumentImage::draw(GraphicsContext& context, const FloatRect& destinationRect, const FloatRect& sourceRect, const ImagePaintingOptions& options)
{
    ASSERT(context.hasPlatformContext());

    if (!m_document || !m_hasPage)
        return ImageDrawResult::DidNothing;

    GraphicsContextStateSaver stateSaver(context);
    context.setCompositeOperation(options.compositeOperator());

    context.clip(destinationRect);

    // Transform context such that sourceRect is mapped to destinationRect.
    auto scaleFactor = destinationRect.size() / sourceRect.size();
    auto scaledSourceRect = sourceRect;
    scaledSourceRect.scale(scaleFactor);
    context.translate(destinationRect.location() - scaledSourceRect.location());
    context.scale(scaleFactor);

    // Flip the coords.
    context.translate(0, size().height());
    context.scale({ 1, -1 });
    drawPDFPage(context);

    if (imageObserver())
        imageObserver()->didDraw(*this);

    return ImageDrawResult::DidDraw;
}

void PDFDocumentImage::destroyDecodedData(bool destroyAll)
{
    decodedSizeChanged(0);
    Image::destroyDecodedData(destroyAll);
}

#if !USE(PDFKIT_FOR_PDFDOCUMENTIMAGE)

void PDFDocumentImage::createPDFDocument()
{
    auto dataProvider = adoptCF(CGDataProviderCreateWithCFData(data()->makeContiguous()->createCFData().get()));
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
