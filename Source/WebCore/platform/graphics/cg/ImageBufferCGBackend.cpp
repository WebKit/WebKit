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
#include "IntRect.h"
#include "MIMETypeRegistry.h"
#include "PixelBuffer.h"
#include "RuntimeApplicationChecks.h"
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

unsigned ImageBufferCGBackend::calculateBytesPerRow(const IntSize& backendSize)
{
    ASSERT(!backendSize.isEmpty());
    return CheckedUint32(backendSize.width()) * 4;
}

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

static RetainPtr<CGImageRef> createCroppedImageIfNecessary(CGImageRef image, const IntSize& backendSize)
{
    if (image && (CGImageGetWidth(image) != static_cast<size_t>(backendSize.width()) || CGImageGetHeight(image) != static_cast<size_t>(backendSize.height())))
        return adoptCF(CGImageCreateWithImageInRect(image, CGRectMake(0, 0, backendSize.width(), backendSize.height())));
    return image;
}

static CGColorSpaceRef colorSpaceForBitmap(DestinationColorSpace imageBufferColorSpace)
{
    if (CGColorSpaceGetModel(imageBufferColorSpace.platformColorSpace()) != kCGColorSpaceModelRGB)
        return sRGBColorSpaceRef();
    return imageBufferColorSpace.platformColorSpace();
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

RetainPtr<CGImageRef> ImageBufferCGBackend::copyCGImageForEncoding(CFStringRef destinationUTI, PreserveResolution preserveResolution) const
{
    if (CFEqual(destinationUTI, jpegUTI())) {
        // FIXME: Should this be using the same logic as ImageBufferUtilitiesCG?

        // JPEGs don't have an alpha channel, so we have to manually composite on top of black.
        PixelBufferFormat format { AlphaPremultiplication::Premultiplied, PixelFormat::RGBA8, DestinationColorSpace(colorSpaceForBitmap(colorSpace())) };
        auto pixelBuffer = getPixelBuffer(format, logicalRect());
        if (!pixelBuffer)
            return nullptr;

        Ref protectedPixelBuffer = *pixelBuffer;
        auto dataSize = pixelBuffer->sizeInBytes();
        auto data = pixelBuffer->bytes();

        verifyImageBufferIsBigEnough(data, dataSize);

        auto dataProvider = adoptCF(CGDataProviderCreateWithData(&protectedPixelBuffer.leakRef(), data, dataSize, [] (void* context, const void*, size_t) {
            static_cast<PixelBuffer*>(context)->deref();
        }));
        if (!dataProvider)
            return nullptr;

        auto imageSize = pixelBuffer->size();
        return adoptCF(CGImageCreate(imageSize.width(), imageSize.height(), 8, 32, 4 * imageSize.width(), pixelBuffer->format().colorSpace.platformColorSpace(), static_cast<uint32_t>(kCGBitmapByteOrderDefault) | static_cast<uint32_t>(kCGImageAlphaNoneSkipLast), dataProvider.get(), 0, false, kCGRenderingIntentDefault));
    }

    if (resolutionScale() == 1 || preserveResolution == PreserveResolution::Yes) {
        auto nativeImage = copyNativeImage(CopyBackingStore);
        if (!nativeImage)
            return nullptr;
        return createCroppedImageIfNecessary(nativeImage->platformImage().get(), backendSize());
    }
    
    auto nativeImage = copyNativeImage(DontCopyBackingStore);
    if (!nativeImage)
        return nullptr;
    auto image = nativeImage->platformImage();
    auto context = adoptCF(CGBitmapContextCreate(0, backendSize().width(), backendSize().height(), 8, 4 * backendSize().width(), colorSpaceForBitmap(colorSpace()), static_cast<uint32_t>(kCGImageAlphaPremultipliedFirst) | static_cast<uint32_t>(kCGBitmapByteOrder32Host)));
    CGContextSetBlendMode(context.get(), kCGBlendModeCopy);
    CGContextClipToRect(context.get(), CGRectMake(0, 0, backendSize().width(), backendSize().height()));
    CGContextDrawImage(context.get(), CGRectMake(0, 0, backendSize().width(), backendSize().height()), image.get());
    return adoptCF(CGBitmapContextCreateImage(context.get()));
}

Vector<uint8_t> ImageBufferCGBackend::toData(const String& mimeType, std::optional<double> quality) const
{
#if ENABLE(GPU_PROCESS)
    ASSERT_IMPLIES(!isInGPUProcess(), MIMETypeRegistry::isSupportedImageMIMETypeForEncoding(mimeType));
#else
    ASSERT(MIMETypeRegistry::isSupportedImageMIMETypeForEncoding(mimeType));
#endif

    auto destinationUTI = utiFromImageBufferMIMEType(mimeType);
    auto image = copyCGImageForEncoding(destinationUTI.get(), PreserveResolution::No);

    return WebCore::data(image.get(), destinationUTI.get(), quality);
}

String ImageBufferCGBackend::toDataURL(const String& mimeType, std::optional<double> quality, PreserveResolution preserveResolution) const
{
#if ENABLE(GPU_PROCESS)
    ASSERT_IMPLIES(!isInGPUProcess(), MIMETypeRegistry::isSupportedImageMIMETypeForEncoding(mimeType));
#else
    ASSERT(MIMETypeRegistry::isSupportedImageMIMETypeForEncoding(mimeType));
#endif

    auto destinationUTI = utiFromImageBufferMIMEType(mimeType);
    auto image = copyCGImageForEncoding(destinationUTI.get(), preserveResolution);

    return WebCore::dataURL(image.get(), destinationUTI.get(), mimeType, quality);
}

std::unique_ptr<ThreadSafeImageBufferFlusher> ImageBufferCGBackend::createFlusher()
{
    return makeUnique<ThreadSafeImageBufferFlusherCG>(context().platformContext());
}

bool ImageBufferCGBackend::originAtBottomLeftCorner() const
{
    return isOriginAtBottomLeftCorner;
}

} // namespace WebCore

#endif // USE(CG)
