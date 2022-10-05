/*
 * Copyright (C) 2010, 2011 Apple Inc. All rights reserved.
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

#include <WebCore/BitmapImage.h>
#include <WebCore/GraphicsContextCG.h>
#include <WebCore/IOSurface.h>
#include <WebCore/ImageBufferUtilitiesCG.h>
#include <WebCore/NativeImage.h>
#include <WebCore/PlatformScreen.h>
#include <pal/spi/cg/CoreGraphicsSPI.h>
#include <pal/spi/cocoa/IOSurfaceSPI.h>
#include <wtf/RetainPtr.h>

namespace WebKit {
using namespace WebCore;

void ShareableBitmap::validateConfiguration(ShareableBitmapConfiguration& configuration)
{
    if (!configuration.colorSpace)
        return;

    configuration.colorSpace = configuration.colorSpace->asRGB();

    if (!configuration.colorSpace) {
#if HAVE(CORE_GRAPHICS_EXTENDED_SRGB_COLOR_SPACE)
        configuration.colorSpace = DestinationColorSpace(extendedSRGBColorSpaceRef());
#else
        configuration.colorSpace = DestinationColorSpace::SRGB();
#endif
    }
}

static CGColorSpaceRef colorSpace(const ShareableBitmapConfiguration& configuration)
{
    return configuration.colorSpace ? configuration.colorSpace->platformColorSpace() : sRGBColorSpaceRef();
}

static bool wantsExtendedRange(const ShareableBitmapConfiguration& configuration)
{
    return CGColorSpaceUsesExtendedRange(colorSpace(configuration));
}

static CGBitmapInfo bitmapInfo(const ShareableBitmapConfiguration& configuration)
{
    CGBitmapInfo info = 0;
    if (wantsExtendedRange(configuration)) {
        info |= kCGBitmapFloatComponents | static_cast<CGBitmapInfo>(kCGBitmapByteOrder16Host);

        if (configuration.isOpaque)
            info |= kCGImageAlphaNoneSkipLast;
        else
            info |= kCGImageAlphaPremultipliedLast;
    } else {
        info |= static_cast<CGBitmapInfo>(kCGBitmapByteOrder32Host);

        if (configuration.isOpaque)
            info |= kCGImageAlphaNoneSkipFirst;
        else
            info |= kCGImageAlphaPremultipliedFirst;
    }

    return info;
}

CheckedUint32 ShareableBitmap::calculateBytesPerRow(WebCore::IntSize size, const ShareableBitmapConfiguration& configuration)
{
    CheckedUint32 bytesPerRow = calculateBytesPerPixel(configuration) * size.width();
#if HAVE(IOSURFACE)
    if (bytesPerRow.hasOverflowed())
        return bytesPerRow;
    size_t alignmentMask = WebCore::IOSurface::bytesPerRowAlignment() - 1;
    return (bytesPerRow + alignmentMask) & ~alignmentMask;
#else
    return bytesPerRow;
#endif
}

CheckedUint32 ShareableBitmap::calculateBytesPerPixel(const ShareableBitmapConfiguration& configuration)
{
    return wantsExtendedRange(configuration) ? 8 : 4;
}

std::unique_ptr<GraphicsContext> ShareableBitmap::createGraphicsContext()
{
    auto bitsPerComponent = calculateBytesPerPixel(m_configuration) * 8 / 4;
    if (bitsPerComponent.hasOverflowed())
        return nullptr;

    auto bytesPerRow = calculateBytesPerRow(m_size, m_configuration);
    if (bytesPerRow.hasOverflowed())
        return nullptr;

    ref(); // Balanced by deref in releaseBitmapContextData.

    m_releaseBitmapContextDataCalled = false;
    RetainPtr<CGContextRef> bitmapContext = adoptCF(CGBitmapContextCreateWithData(data(), m_size.width(), m_size.height(), bitsPerComponent, bytesPerRow, colorSpace(m_configuration), bitmapInfo(m_configuration), releaseBitmapContextData, this));
    if (!bitmapContext) {
        // When CGBitmapContextCreateWithData fails and returns null, it will only
        // call the release callback in some circumstances <rdar://82228446>. We
        // work around this by recording whether it was called, and calling it
        // ourselves if needed.
        if (!m_releaseBitmapContextDataCalled)
            releaseBitmapContextData(this, this->data());
        return nullptr;
    }
    ASSERT(!m_releaseBitmapContextDataCalled);

    // We want the origin to be in the top left corner so we flip the backing store context.
    CGContextTranslateCTM(bitmapContext.get(), 0, m_size.height());
    CGContextScaleCTM(bitmapContext.get(), 1, -1);

    return makeUnique<GraphicsContextCG>(bitmapContext.get());
}

void ShareableBitmap::paint(WebCore::GraphicsContext& context, const IntPoint& destination, const IntRect& source)
{
    paint(context, 1, destination, source);
}

void ShareableBitmap::paint(WebCore::GraphicsContext& context, float scaleFactor, const IntPoint& destination, const IntRect& source)
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
    verifyImageBufferIsBigEnough(data(), sizeInBytes());

    auto dataProvider = adoptCF(CGDataProviderCreateWithData(this, data(), sizeInBytes(), [](void* typelessBitmap, const void* typelessData, size_t) {
        auto* bitmap = static_cast<ShareableBitmap*>(typelessBitmap);
        ASSERT_UNUSED(typelessData, bitmap->data() == typelessData);
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
    auto bitsPerPixel = calculateBytesPerPixel(m_configuration) * 8;
    if (bitsPerPixel.hasOverflowed())
        return nullptr;

    auto bytesPerRow = calculateBytesPerRow(m_size, m_configuration);
    if (bytesPerRow.hasOverflowed())
        return nullptr;

    return adoptCF(CGImageCreate(m_size.width(), m_size.height(), bitsPerPixel / 4, bitsPerPixel, bytesPerRow, colorSpace(m_configuration), bitmapInfo(m_configuration), dataProvider, 0, shouldInterpolate == ShouldInterpolate::Yes ? true : false, kCGRenderingIntentDefault));
}

void ShareableBitmap::releaseBitmapContextData(void* typelessBitmap, void* typelessData)
{
    ShareableBitmap* bitmap = static_cast<ShareableBitmap*>(typelessBitmap);
    ASSERT_UNUSED(typelessData, bitmap->data() == typelessData);
    bitmap->m_releaseBitmapContextDataCalled = true;
    bitmap->deref(); // Balanced by ref in createGraphicsContext.
}

RefPtr<Image> ShareableBitmap::createImage()
{
    if (auto platformImage = makeCGImage())
        return BitmapImage::create(WTFMove(platformImage));
    return nullptr;
}

} // namespace WebKit
