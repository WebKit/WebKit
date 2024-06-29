/*
 * Copyright (C) 2010-2024 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "ShareableBitmap.h"

#include "BitmapImage.h"
#include "GraphicsContextCG.h"
#include "IOSurface.h"
#include "ImageBufferUtilitiesCG.h"
#include "Logging.h"
#include "NativeImage.h"
#include "PlatformScreen.h"
#include <pal/spi/cg/CoreGraphicsSPI.h>
#include <wtf/RetainPtr.h>
#include <wtf/StdLibExtras.h>
#include <wtf/spi/cocoa/IOSurfaceSPI.h>

namespace WebCore {

ShareableBitmapConfiguration::ShareableBitmapConfiguration(NativeImage& image)
    : m_size(image.size())
    , m_colorSpace(image.colorSpace())
    , m_bytesPerPixel(CGImageGetBitsPerPixel(image.platformImage().get()) / 8)
    , m_bytesPerRow(CGImageGetBytesPerRow(image.platformImage().get()))
    , m_bitmapInfo(CGImageGetBitmapInfo(image.platformImage().get()))
{
}

std::optional<DestinationColorSpace> ShareableBitmapConfiguration::validateColorSpace(std::optional<DestinationColorSpace> colorSpace)
{
    if (!colorSpace)
        return std::nullopt;

    if (auto colorSpaceAsRGB = colorSpace->asRGB())
        return colorSpaceAsRGB;

#if HAVE(CORE_GRAPHICS_EXTENDED_SRGB_COLOR_SPACE)
    return DestinationColorSpace(extendedSRGBColorSpaceRef());
#else
    return DestinationColorSpace::SRGB();
#endif
}

static bool wantsExtendedRange(const DestinationColorSpace& colorSpace)
{
    return CGColorSpaceUsesExtendedRange(colorSpace.platformColorSpace());
}

CheckedUint32 ShareableBitmapConfiguration::calculateBytesPerPixel(const DestinationColorSpace& colorSpace)
{
    return wantsExtendedRange(colorSpace) ? 8 : 4;
}

CheckedUint32 ShareableBitmapConfiguration::calculateBytesPerRow(const IntSize& size, const DestinationColorSpace& colorSpace)
{
    CheckedUint32 bytesPerRow = calculateBytesPerPixel(colorSpace) * size.width();
#if HAVE(IOSURFACE)
    if (bytesPerRow.hasOverflowed())
        return bytesPerRow;
    size_t alignmentMask = IOSurface::bytesPerRowAlignment() - 1;
    return (bytesPerRow + alignmentMask) & ~alignmentMask;
#else
    return bytesPerRow;
#endif
}

CGBitmapInfo ShareableBitmapConfiguration::calculateBitmapInfo(const DestinationColorSpace& colorSpace, bool isOpaque)
{
    CGBitmapInfo info = 0;
    if (wantsExtendedRange(colorSpace)) {
        info |= kCGBitmapFloatComponents | static_cast<CGBitmapInfo>(kCGBitmapByteOrder16Host);

        if (isOpaque)
            info |= kCGImageAlphaNoneSkipLast;
        else
            info |= kCGImageAlphaPremultipliedLast;
    } else {
        info |= static_cast<CGBitmapInfo>(kCGBitmapByteOrder32Host);

        if (isOpaque)
            info |= kCGImageAlphaNoneSkipFirst;
        else
            info |= kCGImageAlphaPremultipliedFirst;
    }

    return info;
}

RefPtr<ShareableBitmap> ShareableBitmap::createFromImagePixels(NativeImage& image)
{
    auto colorSpace = image.colorSpace();
    if (colorSpace != DestinationColorSpace::SRGB())
        return nullptr;

    auto configuration = ShareableBitmapConfiguration(image);

    RetainPtr<CFDataRef> pixels;
    @try {
        pixels = adoptCF(CGDataProviderCopyData(CGImageGetDataProvider(image.platformImage().get())));
    } @catch (id exception) {
        LOG_WITH_STREAM(Images, stream
            << "ShareableBitmap::createFromImagePixels() failed CGDataProviderCopyData "
            << " CGImage size: " << configuration.size()
            << " CGImage bytesPerRow: " << configuration.bytesPerRow()
            << " CGImage sizeInBytes: " << configuration.sizeInBytes());
        return nullptr;
    }

    if (!pixels)
        return nullptr;

    const auto* bytes = byteCast<uint8_t>(CFDataGetBytePtr(pixels.get()));
    CheckedUint32 sizeInBytes = CFDataGetLength(pixels.get());
    if (!bytes || !sizeInBytes || sizeInBytes.hasOverflowed())
        return nullptr;

    if (configuration.sizeInBytes() != sizeInBytes) {
        LOG_WITH_STREAM(Images, stream
            << "ShareableBitmap::createFromImagePixels() " << image.platformImage().get()
            << " CGImage size: " << configuration.size()
            << " CGImage bytesPerRow: " << configuration.bytesPerRow()
            << " CGImage sizeInBytes: " << configuration.sizeInBytes()
            << " CGDataProvider sizeInBytes: " << sizeInBytes
            << " CGImage and its CGDataProvider disagree about how many bytes are in pixels buffer. CGImage is a sub-image; bailing.");
        return nullptr;
    }

    RefPtr<SharedMemory> sharedMemory = SharedMemory::allocate(sizeInBytes);
    if (!sharedMemory)
        return nullptr;

    memcpySpan(sharedMemory->mutableSpan(), std::span { bytes, static_cast<size_t>(sizeInBytes) });

    return adoptRef(new ShareableBitmap(configuration, sharedMemory.releaseNonNull()));
}

std::unique_ptr<GraphicsContext> ShareableBitmap::createGraphicsContext()
{
    unsigned bitsPerComponent = m_configuration.bytesPerPixel() * 8 / 4;
    unsigned bytesPerRow = m_configuration.bytesPerRow();

    ref(); // Balanced by deref in releaseBitmapContextData.

    m_releaseBitmapContextDataCalled = false;
    RetainPtr<CGContextRef> bitmapContext = adoptCF(CGBitmapContextCreateWithData(mutableSpan().data(), size().width(), size().height(), bitsPerComponent, bytesPerRow, m_configuration.platformColorSpace(), m_configuration.bitmapInfo(), releaseBitmapContextData, this));
    if (!bitmapContext) {
        // When CGBitmapContextCreateWithData fails and returns null, it will only
        // call the release callback in some circumstances <rdar://82228446>. We
        // work around this by recording whether it was called, and calling it
        // ourselves if needed.
        if (!m_releaseBitmapContextDataCalled)
            releaseBitmapContextData(this, this->mutableSpan().data());
        return nullptr;
    }
    ASSERT(!m_releaseBitmapContextDataCalled);

    // We want the origin to be in the top left corner so we flip the backing store context.
    CGContextTranslateCTM(bitmapContext.get(), 0, size().height());
    CGContextScaleCTM(bitmapContext.get(), 1, -1);

    return makeUnique<GraphicsContextCG>(bitmapContext.get());
}

void ShareableBitmap::paint(GraphicsContext& context, const IntPoint& destination, const IntRect& source)
{
    paint(context, 1, destination, source);
}

void ShareableBitmap::paint(GraphicsContext& context, float scaleFactor, const IntPoint& destination, const IntRect& source)
{
    CGContextRef cgContext = context.platformContext();
    CGContextSaveGState(cgContext);

    CGContextClipToRect(cgContext, CGRectMake(destination.x(), destination.y(), source.width(), source.height()));
    CGContextScaleCTM(cgContext, 1, -1);

    auto image = makeCGImageCopy();
    CGFloat imageHeight = CGImageGetHeight(image.get()) / scaleFactor;
    CGFloat imageWidth = CGImageGetWidth(image.get()) / scaleFactor;

    CGFloat destX = destination.x() - source.x();
    CGFloat destY = -imageHeight - destination.y() + source.y();

    CGContextDrawImage(cgContext, CGRectMake(destX, destY, imageWidth, imageHeight), image.get());

    CGContextRestoreGState(cgContext);
}

RetainPtr<CGImageRef> ShareableBitmap::makeCGImageCopy()
{
    auto graphicsContext = createGraphicsContext();
    if (!graphicsContext)
        return nullptr;

    return adoptCF(CGBitmapContextCreateImage(graphicsContext->platformContext()));
}

RetainPtr<CGImageRef> ShareableBitmap::makeCGImage(ShouldInterpolate shouldInterpolate)
{
    verifyImageBufferIsBigEnough(span());

    auto dataProvider = adoptCF(CGDataProviderCreateWithData(this, mutableSpan().data(), sizeInBytes(), [](void* typelessBitmap, const void* typelessData, size_t) {
        auto* bitmap = static_cast<ShareableBitmap*>(typelessBitmap);
        ASSERT_UNUSED(typelessData, bitmap->span().data() == typelessData);
        bitmap->deref();
    }));

    if (!dataProvider)
        return nullptr;

    ref(); // Balanced by deref above.

    return createCGImage(dataProvider.get(), shouldInterpolate);
}

PlatformImagePtr ShareableBitmap::createPlatformImage(BackingStoreCopy copyBehavior, ShouldInterpolate shouldInterpolate)
{
    if (copyBehavior == CopyBackingStore)
        return makeCGImageCopy();
    return makeCGImage(shouldInterpolate);
}

RetainPtr<CGImageRef> ShareableBitmap::createCGImage(CGDataProviderRef dataProvider, ShouldInterpolate shouldInterpolate) const
{
    ASSERT_ARG(dataProvider, dataProvider);

    unsigned bitsPerPixel = m_configuration.bytesPerPixel() * 8;
    unsigned bytesPerRow = m_configuration.bytesPerRow();

    return adoptCF(CGImageCreate(size().width(), size().height(), bitsPerPixel / 4, bitsPerPixel, bytesPerRow, m_configuration.platformColorSpace(), m_configuration.bitmapInfo(), dataProvider, 0, shouldInterpolate == ShouldInterpolate::Yes, kCGRenderingIntentDefault));
}

void ShareableBitmap::releaseBitmapContextData(void* typelessBitmap, void* typelessData)
{
    ShareableBitmap* bitmap = static_cast<ShareableBitmap*>(typelessBitmap);
    ASSERT_UNUSED(typelessData, bitmap->span().data() == typelessData);
    bitmap->m_releaseBitmapContextDataCalled = true;
    bitmap->deref(); // Balanced by ref in createGraphicsContext.
}

RefPtr<Image> ShareableBitmap::createImage()
{
    if (auto platformImage = makeCGImage())
        return BitmapImage::create(WTFMove(platformImage));
    return nullptr;
}

void ShareableBitmap::setOwnershipOfMemory(const ProcessIdentity& identity)
{
    m_ownershipHandle = m_sharedMemory->createHandle(SharedMemory::Protection::ReadWrite);
    if (!m_ownershipHandle)
        return;
    m_ownershipHandle->setOwnershipOfMemory(identity, MemoryLedger::Graphics);
}

} // namespace WebCore
