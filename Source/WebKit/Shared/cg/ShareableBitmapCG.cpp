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
#include <WebCore/ImageBufferUtilitiesCG.h>
#include <WebCore/PlatformScreen.h>
#include <pal/spi/cg/CoreGraphicsSPI.h>
#include <pal/spi/cocoa/IOSurfaceSPI.h>
#include <wtf/RetainPtr.h>
#include "CGUtilities.h"

namespace WebKit {
using namespace WebCore;
    
static CGColorSpaceRef colorSpace(const ShareableBitmap::Configuration& configuration)
{
    return configuration.colorSpace.cgColorSpace.get() ?: sRGBColorSpaceRef();
}

static bool wantsExtendedRange(const ShareableBitmap::Configuration& configuration)
{
    return CGColorSpaceUsesExtendedRange(colorSpace(configuration));
}

static CGBitmapInfo bitmapInfo(const ShareableBitmap::Configuration& configuration)
{
    CGBitmapInfo info = 0;
    if (wantsExtendedRange(configuration)) {
        info |= kCGBitmapFloatComponents | kCGBitmapByteOrder16Host;

        if (configuration.isOpaque)
            info |= kCGImageAlphaNoneSkipLast;
        else
            info |= kCGImageAlphaPremultipliedLast;
    } else {
        info |= kCGBitmapByteOrder32Host;

        if (configuration.isOpaque)
            info |= kCGImageAlphaNoneSkipFirst;
        else
            info |= kCGImageAlphaPremultipliedFirst;
    }

    return info;
}

Checked<unsigned, RecordOverflow> ShareableBitmap::calculateBytesPerRow(WebCore::IntSize size, const Configuration& configuration)
{
    unsigned bytesPerRow = calculateBytesPerPixel(configuration) * size.width();
#if HAVE(IOSURFACE)
    return IOSurfaceAlignProperty(kIOSurfaceBytesPerRow, bytesPerRow);
#else
    return bytesPerRow;
#endif
}

unsigned ShareableBitmap::calculateBytesPerPixel(const Configuration& configuration)
{
    return wantsExtendedRange(configuration) ? 8 : 4;
}

std::unique_ptr<GraphicsContext> ShareableBitmap::createGraphicsContext()
{
    ref(); // Balanced by deref in releaseBitmapContextData.

    unsigned bytesPerPixel = calculateBytesPerPixel(m_configuration);
    RetainPtr<CGContextRef> bitmapContext = adoptCF(CGBitmapContextCreateWithData(data(), m_size.width(), m_size.height(), bytesPerPixel * 8 / 4, calculateBytesPerRow(m_size, m_configuration).unsafeGet(), colorSpace(m_configuration), bitmapInfo(m_configuration), releaseBitmapContextData, this));
    
    ASSERT(bitmapContext.get());

    // We want the origin to be in the top left corner so we flip the backing store context.
    CGContextTranslateCTM(bitmapContext.get(), 0, m_size.height());
    CGContextScaleCTM(bitmapContext.get(), 1, -1);

    return std::make_unique<GraphicsContext>(bitmapContext.get());
}

void ShareableBitmap::paint(WebCore::GraphicsContext& context, const IntPoint& destination, const IntRect& source)
{
    paintImage(context.platformContext(), makeCGImageCopy().get(), 1, destination, source);
}

void ShareableBitmap::paint(WebCore::GraphicsContext& context, float scaleFactor, const IntPoint& destination, const IntRect& source)
{
    paintImage(context.platformContext(), makeCGImageCopy().get(), scaleFactor, destination, source);
}

RetainPtr<CGImageRef> ShareableBitmap::makeCGImageCopy()
{
    auto graphicsContext = createGraphicsContext();
    RetainPtr<CGImageRef> image = adoptCF(CGBitmapContextCreateImage(graphicsContext->platformContext()));
    return image;
}

RetainPtr<CGImageRef> ShareableBitmap::makeCGImage()
{
    ref(); // Balanced by deref in releaseDataProviderData.
    verifyImageBufferIsBigEnough(data(), sizeInBytes());
    RetainPtr<CGDataProvider> dataProvider = adoptCF(CGDataProviderCreateWithData(this, data(), sizeInBytes(), releaseDataProviderData));
    return createCGImage(dataProvider.get());
}

RetainPtr<CGImageRef> ShareableBitmap::createCGImage(CGDataProviderRef dataProvider) const
{
    ASSERT_ARG(dataProvider, dataProvider);
    unsigned bytesPerPixel = calculateBytesPerPixel(m_configuration);
    RetainPtr<CGImageRef> image = adoptCF(CGImageCreate(m_size.width(), m_size.height(), bytesPerPixel * 8 / 4, bytesPerPixel * 8, calculateBytesPerRow(m_size, m_configuration).unsafeGet(), colorSpace(m_configuration), bitmapInfo(m_configuration), dataProvider, 0, false, kCGRenderingIntentDefault));
    return image;
}

void ShareableBitmap::releaseBitmapContextData(void* typelessBitmap, void* typelessData)
{
    ShareableBitmap* bitmap = static_cast<ShareableBitmap*>(typelessBitmap);
    ASSERT_UNUSED(typelessData, bitmap->data() == typelessData);
    bitmap->deref(); // Balanced by ref in createGraphicsContext.
}

void ShareableBitmap::releaseDataProviderData(void* typelessBitmap, const void* typelessData, size_t)
{
    ShareableBitmap* bitmap = static_cast<ShareableBitmap*>(typelessBitmap);
    ASSERT_UNUSED(typelessData, bitmap->data() == typelessData);
    bitmap->deref(); // Balanced by ref in createCGImage.
}

RefPtr<Image> ShareableBitmap::createImage()
{
    RetainPtr<CGImageRef> platformImage = makeCGImage();
    if (!platformImage)
        return nullptr;

    return BitmapImage::create(WTFMove(platformImage));
}

} // namespace WebKit
