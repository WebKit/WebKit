/*
 * Copyright (C) 2010-2025 Apple Inc. All rights reserved.
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
#include <wtf/cf/VectorCF.h>
#include <wtf/spi/cocoa/IOSurfaceSPI.h>

namespace WebCore {

static std::optional<ShareableBitmapConfiguration> resolveShareableBitmapConfiguration(NativeImage& image)
{
    RetainPtr platformImage = image.platformImage();
    return ShareableBitmapConfiguration::create(image.size(), CGImageGetColorSpace(platformImage.get()), CGImageGetBytesPerRow(platformImage.get()), image.headroom(), CGImageGetBitmapInfo(platformImage.get()), CGImageGetBitsPerComponent(platformImage.get()), CGImageGetBitsPerPixel(platformImage.get()));
}

std::optional<ShareableBitmapConfiguration> ShareableBitmapConfiguration::create(const IntSize& size, PlatformColorSpace&& colorSpace, size_t bytesPerRow, Headroom headroom, CGBitmapInfo bitmapInfo, size_t bitsPerComponent, size_t bitsPerPixel)
{
    if (size.isEmpty())
        return std::nullopt;
    if (!colorSpace)
        return std::nullopt;
    // FIXME: support other color space models.
    if (CGColorSpaceGetModel(colorSpace.get()) != kCGColorSpaceModelRGB)
        return std::nullopt;
    if (headroom < Headroom::None)
        return std::nullopt;
    if (!bytesPerRow)
        return std::nullopt;
    CheckedUint32 bytesPerRowUInt32 { bytesPerRow };
    auto sizeInBytes = bytesPerRowUInt32 * size.height();
    if (sizeInBytes.hasOverflowed())
        return std::nullopt;
    auto bytesPerRowTight = CheckedUint32 { size.width() } * (bitsPerPixel / 8);
    if (bytesPerRowTight.hasOverflowed())
        return std::nullopt;
    if (bytesPerRow < bytesPerRowTight)
        return std::nullopt;
    auto alphaInfo = static_cast<CGImageAlphaInfo>(bitmapInfo & kCGBitmapAlphaInfoMask);
    auto componentInfo = static_cast<CGImageComponentInfo>(bitmapInfo & kCGBitmapComponentInfoMask);
    auto byteOrderInfo = static_cast<CGImageByteOrderInfo>(bitmapInfo & kCGBitmapByteOrderInfoMask);
    auto pixelFormatInfo = static_cast<CGImagePixelFormatInfo>(bitmapInfo & kCGBitmapPixelFormatInfoMask);
    if (bitmapInfo != CGBitmapInfoMake(alphaInfo, componentInfo, byteOrderInfo, pixelFormatInfo))
        return std::nullopt;
    if (bitsPerComponent < 1 || bitsPerComponent > 32)
        return std::nullopt;
    if (bitsPerPixel < 8 || bitsPerPixel > 128)
        return std::nullopt;
    return ShareableBitmapConfiguration { size, WTFMove(colorSpace), bytesPerRow, headroom, bitmapInfo, static_cast<uint8_t>(bitsPerComponent), static_cast<uint8_t>(bitsPerPixel) };
}

ShareableBitmapConfiguration::ShareableBitmapConfiguration(const IntSize& size, PlatformColorSpace&& colorSpace, size_t bytesPerRow, Headroom headroom, CGBitmapInfo bitmapInfo, uint8_t bitsPerComponent, uint8_t bitsPerPixel)
    : m_size(size)
    , m_colorSpace(WTFMove(colorSpace))
    , m_bytesPerRow(bytesPerRow)
    , m_headroom(headroom)
    , m_bitmapInfo(bitmapInfo)
    , m_bitsPerComponent(bitsPerComponent)
    , m_bitsPerPixel(bitsPerPixel)
{
}

RefPtr<ShareableBitmap> ShareableBitmap::create(const IntSize& size, const DestinationColorSpace& colorSpace, Headroom headroom, bool isOpaque)
{
    if (!colorSpace.supportsOutput()) {
        ASSERT_NOT_REACHED();
        return nullptr;
    }

    CGBitmapInfo bitmapInfo;
    size_t bitsPerComponent;
    size_t bitsPerPixel;
    if (colorSpace.usesExtendedRange()) {
        auto alphaInfo = isOpaque ? kCGImageAlphaNoneSkipLast : kCGImageAlphaPremultipliedLast;
        bitmapInfo = CGBitmapInfoMake(alphaInfo, kCGImageComponentFloat, kCGImageByteOrder16Host, kCGImagePixelFormatPacked);
        bitsPerComponent = 16;
        bitsPerPixel = 64;
    } else {
        auto alphaInfo = isOpaque ? kCGImageAlphaNoneSkipFirst : kCGImageAlphaPremultipliedFirst;
        bitmapInfo = CGBitmapInfoMake(alphaInfo, kCGImageComponentInteger, kCGImageByteOrder32Host, kCGImagePixelFormatPacked);
        bitsPerComponent = 8;
        bitsPerPixel = 32;
    }

    auto bytesPerRow = ShareableBitmapConfiguration::calculateBytesPerRow(size, colorSpace);
    if (bytesPerRow.hasOverflowed())
        return nullptr;

    auto configuration = ShareableBitmapConfiguration::create(size, colorSpace.platformColorSpace(), bytesPerRow, headroom, bitmapInfo, bitsPerComponent, bitsPerPixel);
    if (!configuration)
        return nullptr;

    return create(*configuration);
}

CheckedUint32 ShareableBitmapConfiguration::calculateBytesPerRow(const IntSize& size, const DestinationColorSpace& colorSpace)
{
    uint32_t bytesPerPixel = colorSpace.usesExtendedRange() ? 8 : 4;
    CheckedUint32 bytesPerRow = CheckedUint32 { bytesPerPixel } * size.width();
#if HAVE(IOSURFACE)
    size_t alignmentMask = IOSurface::bytesPerRowAlignment() - 1;
    bytesPerRow += bytesPerRow + alignmentMask;
    if (bytesPerRow.hasOverflowed())
        return bytesPerRow;
    bytesPerRow = bytesPerRow & ~alignmentMask;
#endif
    return bytesPerRow;
}

RefPtr<ShareableBitmap> ShareableBitmap::createFromImagePixels(NativeImage& image)
{
    auto colorSpace = image.colorSpace();
    if (CGColorSpaceGetModel(colorSpace.platformColorSpace()) != kCGColorSpaceModelRGB)
        return nullptr;

    auto sourceProvider = CGImageGetDataProvider(image.platformImage().get());
    if (!sourceProvider)
        return nullptr;

    auto configuration = resolveShareableBitmapConfiguration(image);
    if (!configuration)
        return nullptr;

    RetainPtr<CFDataRef> pixels;
    @try {
        pixels = adoptCF(CGDataProviderCopyData(sourceProvider));
    } @catch (id exception) {
        LOG_WITH_STREAM(Images, stream
            << "ShareableBitmap::createFromImagePixels() failed CGDataProviderCopyData "
            << " CGImage size: " << configuration->size()
            << " CGImage bytesPerRow: " << configuration->bytesPerRow()
            << " CGImage sizeInBytes: " << configuration->sizeInBytes());
        return nullptr;
    }

    if (!pixels)
        return nullptr;

    auto bytes = WTF::span(pixels.get());
    if (bytes.empty() || CheckedUint32(bytes.size()).hasOverflowed())
        return nullptr;

    if (configuration->sizeInBytes() != bytes.size()) {
        LOG_WITH_STREAM(Images, stream
            << "ShareableBitmap::createFromImagePixels() " << image.platformImage().get()
            << " CGImage size: " << configuration->size()
            << " CGImage bytesPerRow: " << configuration->bytesPerRow()
            << " CGImage sizeInBytes: " << configuration->sizeInBytes()
            << " CGDataProvider sizeInBytes: " << bytes.size()
            << " CGImage and its CGDataProvider disagree about how many bytes are in pixels buffer. CGImage is a sub-image; bailing.");
        return nullptr;
    }

    RefPtr sharedMemory = SharedMemory::allocate(bytes.size());
    if (!sharedMemory)
        return nullptr;

    memcpySpan(sharedMemory->mutableSpan(), bytes);
    return adoptRef(new ShareableBitmap(*configuration, sharedMemory.releaseNonNull()));
}

std::unique_ptr<GraphicsContext> ShareableBitmap::createGraphicsContext()
{
    auto bitsPerComponent = m_configuration.bitsPerComponent();
    auto bytesPerRow = m_configuration.bytesPerRow();
    auto colorSpace = m_configuration.colorSpace();
    auto size = this->size();

    ref(); // Balanced by deref in releaseBitmapContextData.

    m_releaseBitmapContextDataCalled = false;
    RetainPtr<CGContextRef> bitmapContext = adoptCF(CGBitmapContextCreateWithData(mutableSpan().data(), size.width(), size.height(), bitsPerComponent, bytesPerRow, colorSpace.get(), m_configuration.bitmapInfo(), releaseBitmapContextData, this));
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
    CGContextTranslateCTM(bitmapContext.get(), 0, size.height());
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

    RetainPtr image = createPlatformImage();
    CGFloat imageHeight = CGImageGetHeight(image.get()) / scaleFactor;
    CGFloat imageWidth = CGImageGetWidth(image.get()) / scaleFactor;

    CGFloat destX = destination.x() - source.x();
    CGFloat destY = -imageHeight - destination.y() + source.y();

    CGContextDrawImage(cgContext, CGRectMake(destX, destY, imageWidth, imageHeight), image.get());

    CGContextRestoreGState(cgContext);
}

PlatformImagePtr ShareableBitmap::createPlatformImage(BackingStoreCopy copyBehavior, ShouldInterpolate shouldInterpolate)
{
    verifyImageBufferIsBigEnough(span());

    RetainPtr<CGDataProvider> dataProvider;
    if (copyBehavior == CopyBackingStore) {
        auto data = span();
        dataProvider = adoptCF(CGDataProviderCreateWithCopyOfData(data.data(), data.size()));
        if (!dataProvider)
            return nullptr;
    } else {
        dataProvider = adoptCF(CGDataProviderCreateWithData(this, mutableSpan().data(), sizeInBytes(), [](void* typelessBitmap, const void* typelessData, size_t) {
            auto* bitmap = static_cast<ShareableBitmap*>(typelessBitmap);
            ASSERT_UNUSED(typelessData, bitmap->span().data() == typelessData);
            bitmap->deref();
        }));
        if (!dataProvider)
            return nullptr;
        ref(); // Balanced by deref above.
    }

    auto size = this->size();
    auto bitmapInfo = m_configuration.bitmapInfo();
    auto bitsPerComponent = m_configuration.bitsPerComponent();
    auto bitsPerPixel = m_configuration.bitsPerPixel();
    RetainPtr platformColorSpace = m_configuration.colorSpace();

#if HAVE(SUPPORT_HDR_DISPLAY_APIS)
    if (m_configuration.headroom() > Headroom::None)
        return adoptCF(CGImageCreateWithContentHeadroom(m_configuration.headroom(), size.width(), size.height(), bitsPerComponent, bitsPerPixel, bytesPerRow(), platformColorSpace.get(), bitmapInfo, dataProvider.get(), 0, shouldInterpolate == ShouldInterpolate::Yes, kCGRenderingIntentDefault));
#endif
    return adoptCF(CGImageCreate(size.width(), size.height(), bitsPerComponent, bitsPerPixel, bytesPerRow(), platformColorSpace.get(), bitmapInfo, dataProvider.get(), 0, shouldInterpolate == ShouldInterpolate::Yes, kCGRenderingIntentDefault));
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
    if (RetainPtr platformImage = createPlatformImage(DontCopyBackingStore))
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
