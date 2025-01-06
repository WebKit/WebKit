/*
 * Copyright (C) 2024 Igalia S.L.
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
#include "FontRenderOptions.h"
#include "GraphicsContextSkia.h"
#include "NotImplemented.h"

WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_BEGIN // GLib/Win ports
#include <skia/core/SkSurface.h>
WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_END

namespace WebCore {

std::optional<DestinationColorSpace> ShareableBitmapConfiguration::validateColorSpace(std::optional<DestinationColorSpace> colorSpace)
{
    return colorSpace;
}

CheckedUint32 ShareableBitmapConfiguration::calculateBytesPerPixel(const DestinationColorSpace& colorSpace)
{
    return SkImageInfo::MakeN32Premul(1, 1, colorSpace.platformColorSpace()).bytesPerPixel();
}

CheckedUint32 ShareableBitmapConfiguration::calculateBytesPerRow(const IntSize& size, const DestinationColorSpace& colorSpace)
{
    return SkImageInfo::MakeN32Premul(size.width(), size.height(), colorSpace.platformColorSpace()).minRowBytes();
}

std::unique_ptr<GraphicsContext> ShareableBitmap::createGraphicsContext()
{
    ref();
    SkSurfaceProps properties = { 0, FontRenderOptions::singleton().subpixelOrder() };
    auto surface = SkSurfaces::WrapPixels(m_configuration.imageInfo(), mutableSpan().data(), bytesPerRow(), [](void*, void* context) {
        static_cast<ShareableBitmap*>(context)->deref();
    }, this, &properties);

    auto* canvas = surface->getCanvas();
    if (!canvas)
        return nullptr;

    return makeUnique<GraphicsContextSkia>(*canvas, RenderingMode::Unaccelerated, RenderingPurpose::ShareableSnapshot, [surface = WTFMove(surface)] { });
}

void ShareableBitmap::paint(GraphicsContext& context, const IntPoint& dstPoint, const IntRect& srcRect)
{
    paint(context, 1, dstPoint, srcRect);
}

void ShareableBitmap::paint(GraphicsContext& context, float scaleFactor, const IntPoint& dstPoint, const IntRect& srcRect)
{
    FloatRect scaledSrcRect(srcRect);
    scaledSrcRect.scale(scaleFactor);
    FloatRect scaledDestRect(dstPoint, srcRect.size());
    scaledDestRect.scale(scaleFactor);
    auto image = createPlatformImage(BackingStoreCopy::DontCopyBackingStore);
    context.platformContext()->drawImageRect(image.get(), scaledSrcRect, scaledDestRect, { }, nullptr, { });
}

RefPtr<Image> ShareableBitmap::createImage()
{
    return BitmapImage::create(createPlatformImage(BackingStoreCopy::DontCopyBackingStore));
}

PlatformImagePtr ShareableBitmap::createPlatformImage(BackingStoreCopy backingStoreCopy, ShouldInterpolate)
{
    sk_sp<SkData> pixelData;
    if (backingStoreCopy == BackingStoreCopy::CopyBackingStore)
        pixelData = SkData::MakeWithCopy(span().data(), sizeInBytes());
    else {
        ref();
        pixelData = SkData::MakeWithProc(span().data(), sizeInBytes(), [](const void*, void* bitmap) -> void {
            static_cast<ShareableBitmap*>(bitmap)->deref();
        }, this);
    }
    return SkImages::RasterFromData(m_configuration.imageInfo(), pixelData, bytesPerRow());
}

void ShareableBitmap::setOwnershipOfMemory(const ProcessIdentity&)
{
}

} // namespace WebCore
