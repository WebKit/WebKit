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
#include "NativeImage.h"

#if USE(SKIA)
#include "GLContext.h"
#include "GraphicsContextSkia.h"
#include "PixelBuffer.h"
#include "PlatformDisplay.h"
WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_BEGIN // GLib/Win ports
#include <skia/core/SkColorSpace.h>
#include <skia/core/SkImage.h>
#include <skia/core/SkPixmap.h>
WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_END

namespace WebCore {

RefPtr<NativeImage> NativeImage::create(PlatformImagePtr&& platformImage, std::optional<GainMap>&& gainMap, GrDirectContext* grContext)
{
    if (!platformImage)
        return nullptr;
    return adoptRef(*new NativeImage(WTF::move(platformImage), WTF::move(gainMap), grContext));
}

RefPtr<NativeImage> NativeImage::create(PlatformImagePtr&& platformImage, GrDirectContext* grContext)
{
    return create(WTF::move(platformImage), std::nullopt, grContext);
}

RefPtr<NativeImage> NativeImage::createTransient(PlatformImagePtr&& platformImage, GrDirectContext* grContext)
{
    return create(WTF::move(platformImage), grContext);
}

RefPtr<NativeImage> NativeImage::create(Ref<PixelBuffer>&& pixelBuffer)
{
    if (pixelBuffer->size().isEmpty())
        return nullptr;
    auto format = pixelBuffer->format();
    SkColorType colorType = kRGBA_8888_SkColorType;
    switch (format.pixelFormat) {
    case PixelFormat::RGBX8:
        colorType = kRGB_888x_SkColorType;
        break;
    case PixelFormat::RGBA8:
        colorType = kRGBA_8888_SkColorType;
        break;
    case PixelFormat::BGRX8:
        // Skia has no BGR color type with an ignored fourth component, so the contents are
        // described as BGRA and the alpha component is required to be 255, see below.
        colorType = kBGRA_8888_SkColorType;
        break;
    case PixelFormat::BGRA8:
        colorType = kBGRA_8888_SkColorType;
        break;
#if ENABLE(PIXEL_FORMAT_RGBA16F)
    case PixelFormat::RGBA16F:
        colorType = kRGBA_F16_SkColorType;
        break;
#endif
#if ENABLE(PIXEL_FORMAT_RGB10)
    case PixelFormat::RGB10:
        ASSERT(!PixelBuffer::supportedPixelFormat(format.pixelFormat));
        return nullptr;
#endif
#if ENABLE(PIXEL_FORMAT_RGB10A8)
    case PixelFormat::RGB10A8:
        ASSERT(!PixelBuffer::supportedPixelFormat(format.pixelFormat));
        return nullptr;
#endif
    }
    auto imageSize = pixelBuffer->size();
    // kRGB_888x_SkColorType ignores the fourth component, but kBGRA_8888_SkColorType does not, so
    // BGRX8 contents are required to have 255 in the component that holds no meaningful value.
    SkAlphaType alphaType = kUnpremul_SkAlphaType;
    if (pixelFormatIsOpaque(format.pixelFormat))
        alphaType = kOpaque_SkAlphaType;
    else if (format.alphaFormat == AlphaPremultiplication::Premultiplied)
        alphaType = kPremul_SkAlphaType;
    auto imageInfo = SkImageInfo::Make(imageSize.width(), imageSize.height(), colorType, alphaType, format.colorSpace.platformColorSpace());

    SkPixmap pixmap(imageInfo, pixelBuffer->bytes().data(), imageInfo.minRowBytes());
    // On success, the image owns the pixel buffer reference.
    auto* pixelBufferContext = pixelBuffer.ptr();
    auto image = SkImages::RasterFromPixmap(pixmap, [](const void*, void* context) {
        static_cast<PixelBuffer*>(context)->deref();
    }, pixelBufferContext);
    if (!image)
        return nullptr;
    SUPPRESS_RETAINPTR_CTOR_ADOPT (void) pixelBuffer.leakRef(); // NOLINT
    return create(WTF::move(image));
}

NativeImage::NativeImage(PlatformImagePtr&& platformImage, std::optional<GainMap>&& gainMap, GrDirectContext* grContext)
    : m_platformImage(WTF::move(platformImage))
    , m_gainMap(WTF::move(gainMap))
    , m_grContext(grContext)
{
    ASSERT(!m_platformImage->isTextureBacked() || m_grContext);
    computeHeadroom();
}

IntSize NativeImage::size() const
{
    Locker locker { m_lock };
    return IntSize(m_platformImage->width(), m_platformImage->height());
}

bool NativeImage::hasAlpha() const
{
    Locker locker { m_lock };
    switch (m_platformImage->imageInfo().alphaType()) {
    case kUnknown_SkAlphaType:
    case kOpaque_SkAlphaType:
        return false;
    case kPremul_SkAlphaType:
    case kUnpremul_SkAlphaType:
        return true;
    }
    return false;
}

ColorSpace NativeImage::colorSpace() const
{
    Locker locker { m_lock };
    if (auto colorSpace = m_platformImage->refColorSpace())
        return ColorSpace(colorSpace);
    // No color space means the default - SRGB.
    return ColorSpace::SRGB();
}

std::optional<Color> NativeImage::singlePixelSolidColor() const
{
    if (size() != IntSize(1, 1))
        return std::nullopt;

    auto platformImage = this->platformImage();
    if (platformImage->isTextureBacked()) {
        if (!PlatformDisplay::sharedDisplay().skiaGLContext()->makeContextCurrent())
            return std::nullopt;

        ASSERT(m_grContext);
        const auto& imageInfo = platformImage->imageInfo();
        uint32_t pixel;
        SkPixmap pixmap(imageInfo, &pixel, imageInfo.minRowBytes());
        if (!platformImage->readPixels(m_grContext, pixmap, 0, 0))
            return std::nullopt;

        return pixmap.getColor(0, 0);
    }

    SkPixmap pixmap;
    if (!platformImage->peekPixels(&pixmap))
        return std::nullopt;

    return pixmap.getColor(0, 0);
}

void NativeImage::clearSubimages()
{
}

uint64_t NativeImage::uniqueID() const
{
    return platformImage()->uniqueID();
}

} // namespace WebCore

#endif // USE(SKIA)
