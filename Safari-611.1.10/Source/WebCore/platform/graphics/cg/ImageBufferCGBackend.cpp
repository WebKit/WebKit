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

class ThreadSafeImageBufferFlusherCG : public ThreadSafeImageBufferFlusher {
    WTF_MAKE_FAST_ALLOCATED;
public:
    ThreadSafeImageBufferFlusherCG(CGContextRef context)
        : m_context(context)
    {
    }

    void flush() override
    {
        CGContextFlush(m_context.get());
    }

private:
    RetainPtr<CGContextRef> m_context;
};

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

void ImageBufferCGBackend::setupContext() const
{
    // The initial CTM matches DisplayList::Recorder::clipToDrawingCommands()'s initial CTM.
    context().scale(FloatSize(1, -1));
    context().translate(0, -backendSize().height());
    context().applyDeviceScaleFactor(resolutionScale());
}

static RetainPtr<CGImageRef> createCroppedImageIfNecessary(CGImageRef image, const IntSize& backendSize)
{
    if (image && (CGImageGetWidth(image) != static_cast<size_t>(backendSize.width()) || CGImageGetHeight(image) != static_cast<size_t>(backendSize.height())))
        return adoptCF(CGImageCreateWithImageInRect(image, CGRectMake(0, 0, backendSize.width(), backendSize.height())));
    return image;
}

static RefPtr<Image> createBitmapImageAfterScalingIfNeeded(RefPtr<NativeImage>&& image, const IntSize& logicalSize, const IntSize& backendSize, float resolutionScale, PreserveResolution preserveResolution)
{
    if (resolutionScale == 1 || preserveResolution == PreserveResolution::Yes)
        image = NativeImage::create(createCroppedImageIfNecessary(image->platformImage().get(), backendSize));
    else {
        auto context = adoptCF(CGBitmapContextCreate(0, logicalSize.width(), logicalSize.height(), 8, 4 * logicalSize.width(), sRGBColorSpaceRef(), kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Host));
        CGContextSetBlendMode(context.get(), kCGBlendModeCopy);
        CGContextClipToRect(context.get(), FloatRect(FloatPoint::zero(), logicalSize));
        FloatSize imageSizeInUserSpace = logicalSize;
        CGContextDrawImage(context.get(), FloatRect(FloatPoint::zero(), imageSizeInUserSpace), image->platformImage().get());
        image = NativeImage::create(adoptCF(CGBitmapContextCreateImage(context.get())));
    }

    if (!image)
        return nullptr;

    return BitmapImage::create(WTFMove(image));
}

RefPtr<Image> ImageBufferCGBackend::copyImage(BackingStoreCopy copyBehavior, PreserveResolution preserveResolution) const
{
    RefPtr<NativeImage> image;
    if (resolutionScale() == 1 || preserveResolution == PreserveResolution::Yes)
        image = copyNativeImage(copyBehavior);
    else
        image = copyNativeImage(DontCopyBackingStore);
    return createBitmapImageAfterScalingIfNeeded(WTFMove(image), logicalSize(), backendSize(), resolutionScale(), preserveResolution);
}

RefPtr<Image> ImageBufferCGBackend::sinkIntoImage(PreserveResolution preserveResolution)
{
    // Get the backend size before sinking the it into a NativeImage. sinkIntoNativeImage() sets the IOSurface to null if it's an accelerated backend.
    auto backendSize = this->backendSize();
    return createBitmapImageAfterScalingIfNeeded(sinkIntoNativeImage(), logicalSize(), backendSize, resolutionScale(), preserveResolution);
}

void ImageBufferCGBackend::draw(GraphicsContext& destContext, const FloatRect& destRect, const FloatRect& srcRect, const ImagePaintingOptions& options)
{
    FloatRect srcRectScaled = srcRect;
    srcRectScaled.scale(resolutionScale());

    if (auto image = copyNativeImage(&destContext == &context() ? CopyBackingStore : DontCopyBackingStore))
        destContext.drawNativeImage(*image, backendSize(), destRect, srcRectScaled, options);
}

void ImageBufferCGBackend::drawPattern(GraphicsContext& destContext, const FloatRect& destRect, const FloatRect& srcRect, const AffineTransform& patternTransform, const FloatPoint& phase, const FloatSize& spacing, const ImagePaintingOptions& options)
{
    FloatRect adjustedSrcRect = srcRect;
    adjustedSrcRect.scale(resolutionScale());

    if (auto image = copyImage(&destContext == &context() ? CopyBackingStore : DontCopyBackingStore))
        image->drawPattern(destContext, destRect, adjustedSrcRect, patternTransform, phase, spacing, options);
}

void ImageBufferCGBackend::clipToMask(GraphicsContext& destContext, const FloatRect& destRect)
{
    auto nativeImage = copyNativeImage(DontCopyBackingStore);
    if (!nativeImage)
        return;
    
    CGContextRef cgContext = destContext.platformContext();

    // FIXME: This image needs to be grayscale to be used as an alpha mask here.
    CGContextTranslateCTM(cgContext, destRect.x(), destRect.maxY());
    CGContextScaleCTM(cgContext, 1, -1);
    CGContextClipToRect(cgContext, { { }, destRect.size() });
    CGContextClipToMask(cgContext, { { }, destRect.size() }, nativeImage->platformImage().get());
    CGContextScaleCTM(cgContext, 1, -1);
    CGContextTranslateCTM(cgContext, -destRect.x(), -destRect.maxY());
}

RetainPtr<CFDataRef> ImageBufferCGBackend::toCFData(const String& mimeType, Optional<double> quality, PreserveResolution preserveResolution) const
{
    ASSERT(MIMETypeRegistry::isSupportedImageMIMETypeForEncoding(mimeType));

    auto uti = utiFromImageBufferMIMEType(mimeType);
    ASSERT(uti);

    PlatformImagePtr image;
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
    } else if (resolutionScale() == 1 || preserveResolution == PreserveResolution::Yes) {
        image = copyNativeImage(CopyBackingStore)->platformImage();
        image = createCroppedImageIfNecessary(image.get(), backendSize());
    } else {
        image = copyNativeImage(DontCopyBackingStore)->platformImage();
        auto context = adoptCF(CGBitmapContextCreate(0, backendSize().width(), backendSize().height(), 8, 4 * backendSize().width(), sRGBColorSpaceRef(), kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Host));
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

std::unique_ptr<ThreadSafeImageBufferFlusher> ImageBufferCGBackend::createFlusher()
{
    return WTF::makeUnique<ThreadSafeImageBufferFlusherCG>(context().platformContext());
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
    AlphaPremultiplication srcAlphaFormat, PixelFormat srcPixelFormat, vImage_Buffer& src,
    AlphaPremultiplication destAlphaFormat, PixelFormat destPixelFormat, vImage_Buffer& dest)
{
    if (srcAlphaFormat == destAlphaFormat) {
        ASSERT(srcPixelFormat != destPixelFormat);
        // The destination alpha format can be unpremultiplied in the
        // case of an ImageBitmap created from an ImageData with
        // premultiplyAlpha=="none".

        // Swap pixel channels BGRA <-> RGBA.
        const uint8_t map[4] = { 2, 1, 0, 3 };
        vImagePermuteChannels_ARGB8888(&src, &dest, map, kvImageNoFlags);
        return;
    }

    if (destAlphaFormat == AlphaPremultiplication::Unpremultiplied) {
        if (srcPixelFormat == PixelFormat::RGBA8)
            vImageUnpremultiplyData_RGBA8888(&src, &dest, kvImageNoFlags);
        else
            vImageUnpremultiplyData_BGRA8888(&src, &dest, kvImageNoFlags);
    } else {
        if (srcPixelFormat == PixelFormat::RGBA8)
            vImagePremultiplyData_RGBA8888(&src, &dest, kvImageNoFlags);
        else
            vImagePremultiplyData_BGRA8888(&src, &dest, kvImageNoFlags);
    }

    if (srcPixelFormat != destPixelFormat) {
        // Swap pixel channels BGRA <-> RGBA.
        const uint8_t map[4] = { 2, 1, 0, 3 };
        vImagePermuteChannels_ARGB8888(&dest, &dest, map, kvImageNoFlags);
    }
}

void ImageBufferCGBackend::copyImagePixels(
    AlphaPremultiplication srcAlphaFormat, PixelFormat srcPixelFormat, unsigned srcBytesPerRow, uint8_t* srcRows,
    AlphaPremultiplication destAlphaFormat, PixelFormat destPixelFormat, unsigned destBytesPerRow, uint8_t* destRows, const IntSize& size) const
{
    // We don't currently support getting or putting pixel data with deep color buffers.
    ASSERT(srcPixelFormat == PixelFormat::RGBA8 || srcPixelFormat == PixelFormat::BGRA8);
    ASSERT(destPixelFormat == PixelFormat::RGBA8 || destPixelFormat == PixelFormat::BGRA8);

    if (srcAlphaFormat == destAlphaFormat && srcPixelFormat == destPixelFormat) {
        ImageBufferBackend::copyImagePixels(srcAlphaFormat, srcPixelFormat, srcBytesPerRow, srcRows, destAlphaFormat, destPixelFormat, destBytesPerRow, destRows, size);
        return;
    }

    vImage_Buffer src = makeVImageBuffer(srcBytesPerRow, srcRows, size);
    vImage_Buffer dest = makeVImageBuffer(destBytesPerRow, destRows, size);

    copyImagePixelsAccelerated(srcAlphaFormat, srcPixelFormat, src, destAlphaFormat, destPixelFormat, dest);
}
#endif

} // namespace WebCore

#endif // USE(CG)
