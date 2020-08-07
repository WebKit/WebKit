/*
 * Copyright (C) 2020 Apple Inc.  All rights reserved.
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
#include "ImageBufferCGBackend.h"

#if USE(CG)

#include "BitmapImage.h"
#include "GraphicsContextCG.h"
#include "ImageBufferUtilitiesCG.h"
#include "ImageData.h"
#include "IntRect.h"
#include "MIMETypeRegistry.h"

#if USE(ACCELERATE)
#include <Accelerate/Accelerate.h>
#endif
#include <CoreGraphics/CoreGraphics.h>
#include <pal/spi/cg/CoreGraphicsSPI.h>

namespace WebCore {

RetainPtr<CGColorSpaceRef> ImageBufferCGBackend::contextColorSpace(const GraphicsContext& context)
{
#if PLATFORM(COCOA)
    CGContextRef cgContext = context.platformContext();

    if (CGContextGetType(cgContext) == kCGContextTypeBitmap)
        return CGBitmapContextGetColorSpace(cgContext);
    
    return adoptCF(CGContextCopyDeviceColorSpace(cgContext));
#else
    UNUSED_PARAM(context);
    return nullptr;
#endif
}
#

void ImageBufferCGBackend::setupContext()
{
    context().scale(FloatSize(1, -1));
    context().translate(0, -m_backendSize.height());
    context().applyDeviceScaleFactor(m_resolutionScale);
}

static RetainPtr<CGImageRef> createCroppedImageIfNecessary(CGImageRef image, const IntSize& backendSize)
{
    if (image && (CGImageGetWidth(image) != static_cast<size_t>(backendSize.width()) || CGImageGetHeight(image) != static_cast<size_t>(backendSize.height())))
        return adoptCF(CGImageCreateWithImageInRect(image, CGRectMake(0, 0, backendSize.width(), backendSize.height())));
    return image;
}

static RefPtr<Image> createBitmapImageAfterScalingIfNeeded(RetainPtr<CGImageRef>&& image, const IntSize& logicalSize, const IntSize& backendSize, float resolutionScale, PreserveResolution preserveResolution)
{
    if (resolutionScale == 1 || preserveResolution == PreserveResolution::Yes)
        image = createCroppedImageIfNecessary(image.get(), backendSize);
    else {
        auto context = adoptCF(CGBitmapContextCreate(0, logicalSize.width(), logicalSize.height(), 8, 4 * logicalSize.width(), sRGBColorSpaceRef(), kCGImageAlphaPremultipliedLast));
        CGContextSetBlendMode(context.get(), kCGBlendModeCopy);
        CGContextClipToRect(context.get(), FloatRect(FloatPoint::zero(), logicalSize));
        FloatSize imageSizeInUserSpace = logicalSize;
        CGContextDrawImage(context.get(), FloatRect(FloatPoint::zero(), imageSizeInUserSpace), image.get());
        image = adoptCF(CGBitmapContextCreateImage(context.get()));
    }

    if (!image)
        return nullptr;

    return BitmapImage::create(WTFMove(image));
}

RefPtr<Image> ImageBufferCGBackend::copyImage(BackingStoreCopy copyBehavior, PreserveResolution preserveResolution) const
{
    NativeImagePtr image;
    if (m_resolutionScale == 1 || preserveResolution == PreserveResolution::Yes)
        image = copyNativeImage(copyBehavior);
    else
        image = copyNativeImage(DontCopyBackingStore);
    return createBitmapImageAfterScalingIfNeeded(WTFMove(image), m_logicalSize, m_backendSize, m_resolutionScale, preserveResolution);
}

RefPtr<Image> ImageBufferCGBackend::sinkIntoImage(PreserveResolution preserveResolution)
{
    return createBitmapImageAfterScalingIfNeeded(sinkIntoNativeImage(), m_logicalSize, m_backendSize, m_resolutionScale, preserveResolution);
}

void ImageBufferCGBackend::draw(GraphicsContext& destContext, const FloatRect& destRect, const FloatRect& srcRect, const ImagePaintingOptions& options)
{
    FloatRect srcRectScaled = srcRect;
    srcRectScaled.scale(m_resolutionScale);

    if (auto image = copyNativeImage(&destContext == &context() ? CopyBackingStore : DontCopyBackingStore))
        destContext.drawNativeImage(image.get(), m_backendSize, destRect, srcRectScaled, options);
}

void ImageBufferCGBackend::drawPattern(GraphicsContext& destContext, const FloatRect& destRect, const FloatRect& srcRect, const AffineTransform& patternTransform, const FloatPoint& phase, const FloatSize& spacing, const ImagePaintingOptions& options)
{
    FloatRect adjustedSrcRect = srcRect;
    adjustedSrcRect.scale(m_resolutionScale);

    if (auto image = copyImage(&destContext == &context() ? CopyBackingStore : DontCopyBackingStore))
        image->drawPattern(destContext, destRect, adjustedSrcRect, patternTransform, phase, spacing, options);
}

AffineTransform ImageBufferCGBackend::baseTransform() const
{
    return AffineTransform(1, 0, 0, -1, 0, m_logicalSize.height());
}

RetainPtr<CFDataRef> ImageBufferCGBackend::toCFData(const String& mimeType, Optional<double> quality, PreserveResolution preserveResolution) const
{
    ASSERT(MIMETypeRegistry::isSupportedImageMIMETypeForEncoding(mimeType));

    auto uti = utiFromImageBufferMIMEType(mimeType);
    ASSERT(uti);

    RetainPtr<CGImageRef> image;
    RefPtr<Uint8ClampedArray> protectedPixelArray;

    if (CFEqual(uti.get(), jpegUTI())) {
        // JPEGs don't have an alpha channel, so we have to manually composite on top of black.
        auto imageData = getImageData(AlphaPremultiplication::Premultiplied, logicalRect());
        if (!imageData)
            return nullptr;

        protectedPixelArray = makeRefPtr(imageData->data());
        size_t dataSize = protectedPixelArray->byteLength();
        IntSize pixelArrayDimensions = imageData->size();

        verifyImageBufferIsBigEnough(protectedPixelArray->data(), dataSize);
        auto dataProvider = adoptCF(CGDataProviderCreateWithData(nullptr, protectedPixelArray->data(), dataSize, nullptr));
        if (!dataProvider)
            return nullptr;

        image = adoptCF(CGImageCreate(pixelArrayDimensions.width(), pixelArrayDimensions.height(), 8, 32, 4 * pixelArrayDimensions.width(), sRGBColorSpaceRef(), kCGBitmapByteOrderDefault | kCGImageAlphaNoneSkipLast, dataProvider.get(), 0, false, kCGRenderingIntentDefault));
    } else if (m_resolutionScale == 1 || preserveResolution == PreserveResolution::Yes) {
        image = copyNativeImage(CopyBackingStore);
        image = createCroppedImageIfNecessary(image.get(), backendSize());
    } else {
        image = copyNativeImage(DontCopyBackingStore);
        auto context = adoptCF(CGBitmapContextCreate(0, backendSize().width(), backendSize().height(), 8, 4 * backendSize().width(), sRGBColorSpaceRef(), kCGImageAlphaPremultipliedLast));
        CGContextSetBlendMode(context.get(), kCGBlendModeCopy);
        CGContextClipToRect(context.get(), CGRectMake(0, 0, backendSize().width(), backendSize().height()));
        CGContextDrawImage(context.get(), CGRectMake(0, 0, backendSize().width(), backendSize().height()), image.get());
        image = adoptCF(CGBitmapContextCreateImage(context.get()));
    }

    auto cfData = adoptCF(CFDataCreateMutable(kCFAllocatorDefault, 0));
    if (!encodeImage(image.get(), uti.get(), quality, cfData.get()))
        return nullptr;

    return WTFMove(cfData);
}

Vector<uint8_t> ImageBufferCGBackend::toData(const String& mimeType, Optional<double> quality) const
{
    if (auto data = toCFData(mimeType, quality, PreserveResolution::No))
        return dataVector(data.get());
    return { };
}

String ImageBufferCGBackend::toDataURL(const String& mimeType, Optional<double> quality, PreserveResolution preserveResolution) const
{
    if (auto data = toCFData(mimeType, quality, preserveResolution))
        return dataURL(data.get(), mimeType);
    return "data:,"_s;
}

#if USE(ACCELERATE)
static inline vImage_Buffer makeVImageBuffer(unsigned bytesPerRow, uint8_t* rows, const IntSize& size)
{
    vImage_Buffer vImageBuffer;

    vImageBuffer.height = static_cast<vImagePixelCount>(size.height());
    vImageBuffer.width = static_cast<vImagePixelCount>(size.width());
    vImageBuffer.rowBytes = bytesPerRow;
    vImageBuffer.data = rows;
    return vImageBuffer;
}

static inline void copyImagePixelsAccelerated(
    AlphaPremultiplication srcAlphaFormat, ColorFormat srcColorFormat, vImage_Buffer& src,
    AlphaPremultiplication destAlphaFormat, ColorFormat destColorFormat, vImage_Buffer& dest)
{
    if (srcAlphaFormat == destAlphaFormat) {
        ASSERT(srcColorFormat != destColorFormat);
        // The destination alpha format can be unpremultiplied in the
        // case of an ImageBitmap created from an ImageData with
        // premultiplyAlpha=="none".

        // Swap pixel channels BGRA <-> RGBA.
        const uint8_t map[4] = { 2, 1, 0, 3 };
        vImagePermuteChannels_ARGB8888(&src, &dest, map, kvImageNoFlags);
        return;
    }

    if (destAlphaFormat == AlphaPremultiplication::Unpremultiplied) {
        if (srcColorFormat == ColorFormat::RGBA)
            vImageUnpremultiplyData_RGBA8888(&src, &dest, kvImageNoFlags);
        else
            vImageUnpremultiplyData_BGRA8888(&src, &dest, kvImageNoFlags);
    } else {
        if (srcColorFormat == ColorFormat::RGBA)
            vImagePremultiplyData_RGBA8888(&src, &dest, kvImageNoFlags);
        else
            vImagePremultiplyData_BGRA8888(&src, &dest, kvImageNoFlags);
    }

    if (srcColorFormat != destColorFormat) {
        // Swap pixel channels BGRA <-> RGBA.
        const uint8_t map[4] = { 2, 1, 0, 3 };
        vImagePermuteChannels_ARGB8888(&dest, &dest, map, kvImageNoFlags);
    }
}

void ImageBufferCGBackend::copyImagePixels(
    AlphaPremultiplication srcAlphaFormat, ColorFormat srcColorFormat, unsigned srcBytesPerRow, uint8_t* srcRows,
    AlphaPremultiplication destAlphaFormat, ColorFormat destColorFormat, unsigned destBytesPerRow, uint8_t* destRows, const IntSize& size) const
{
    if (srcAlphaFormat == destAlphaFormat && srcColorFormat == destColorFormat) {
        ImageBufferBackend::copyImagePixels(srcAlphaFormat, srcColorFormat, srcBytesPerRow, srcRows, destAlphaFormat, destColorFormat, destBytesPerRow, destRows, size);
        return;
    }

    vImage_Buffer src = makeVImageBuffer(srcBytesPerRow, srcRows, size);
    vImage_Buffer dest = makeVImageBuffer(destBytesPerRow, destRows, size);

    copyImagePixelsAccelerated(srcAlphaFormat, srcColorFormat, src, destAlphaFormat, destColorFormat, dest);
}
#endif

} // namespace WebCore

#endif // USE(CG)
