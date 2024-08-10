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
#include "GraphicsContextGL.h"

#if ENABLE(WEBGL) && USE(SKIA)
#include "BitmapImage.h"
#include "GLContext.h"
#include "GraphicsContextGLImageExtractor.h"
#include "NotImplemented.h"
#include "PixelBuffer.h"
#include "PlatformDisplay.h"
#include "SharedBuffer.h"
#include <skia/core/SkData.h>
#include <skia/core/SkImage.h>

IGNORE_CLANG_WARNINGS_BEGIN("cast-align")
#include <skia/core/SkPixmap.h>
IGNORE_CLANG_WARNINGS_END

namespace WebCore {

GraphicsContextGLImageExtractor::~GraphicsContextGLImageExtractor() = default;

bool GraphicsContextGLImageExtractor::extractImage(bool premultiplyAlpha, bool ignoreGammaAndColorProfile, bool ignoreNativeImageAlphaPremultiplication)
{
    PlatformImagePtr platformImage;
    bool hasAlpha = !m_image->currentFrameKnownToBeOpaque();
    if ((ignoreGammaAndColorProfile || (hasAlpha && !premultiplyAlpha)) && m_image->data()) {
        auto image = BitmapImage::create(nullptr,  AlphaOption::NotPremultiplied, ignoreGammaAndColorProfile ? GammaAndColorProfileOption::Ignored : GammaAndColorProfileOption::Applied);
        image->setData(m_image->data(), true);
        if (!image->frameCount())
            return false;

        platformImage = image->currentNativeImage()->platformImage();
    } else
        platformImage = m_image->currentNativeImage()->platformImage();

    if (!platformImage)
        return false;

    m_imageWidth = platformImage->width();
    m_imageHeight = platformImage->height();
    if (!m_imageWidth || !m_imageHeight)
        return false;

    const auto& imageInfo = platformImage->imageInfo();
    m_alphaOp = AlphaOp::DoNothing;
    switch (imageInfo.alphaType()) {
    case kUnknown_SkAlphaType:
    case kOpaque_SkAlphaType:
        break;
    case kPremul_SkAlphaType:
        if (!premultiplyAlpha)
            m_alphaOp = AlphaOp::DoUnmultiply;
        else if (ignoreNativeImageAlphaPremultiplication)
            m_alphaOp = AlphaOp::DoPremultiply;
        break;
    case kUnpremul_SkAlphaType:
        if (premultiplyAlpha)
            m_alphaOp = AlphaOp::DoPremultiply;
        break;
    }

    unsigned srcUnpackAlignment = 1;
    size_t bytesPerRow = imageInfo.minRowBytes();
    size_t bytesPerPixel = imageInfo.bytesPerPixel();
    unsigned padding = bytesPerRow - bytesPerPixel * m_imageWidth;
    if (padding) {
        srcUnpackAlignment = padding + 1;
        while (bytesPerRow % srcUnpackAlignment)
            ++srcUnpackAlignment;
    }

    if (platformImage->isTextureBacked()) {
        auto data = SkData::MakeUninitialized(imageInfo.computeMinByteSize());
        if (!PlatformDisplay::sharedDisplay().skiaGLContext()->makeContextCurrent())
            return false;

        GrDirectContext* grContext = PlatformDisplay::sharedDisplay().skiaGrContext();
        if (!platformImage->readPixels(grContext, imageInfo, static_cast<uint8_t*>(data->writable_data()), bytesPerRow, 0, 0))
            return false;

        m_pixelData = WTFMove(data);
        m_imagePixelData = m_pixelData->data();

        // SkSurfaces backed by textures have RGBA format.
        m_imageSourceFormat = DataFormat::RGBA8;
    } else {
        SkPixmap pixmap;
        if (!platformImage->peekPixels(&pixmap))
            return false;

        m_skImage = WTFMove(platformImage);
        m_imagePixelData = pixmap.addr();

        // Raster SkSurfaces have BGRA format.
        m_imageSourceFormat = DataFormat::BGRA8;
    }

    m_imageSourceUnpackAlignment = srcUnpackAlignment;
    return true;
}

RefPtr<NativeImage> GraphicsContextGL::createNativeImageFromPixelBuffer(const GraphicsContextGLAttributes& sourceContextAttributes, Ref<PixelBuffer>&& pixelBuffer)
{
    ASSERT(!pixelBuffer->size().isEmpty());
    auto imageSize = pixelBuffer->size();
    SkAlphaType alphaType = kUnpremul_SkAlphaType;
    if (!sourceContextAttributes.alpha)
        alphaType = kOpaque_SkAlphaType;
    else if (sourceContextAttributes.premultipliedAlpha)
        alphaType = kPremul_SkAlphaType;
    auto imageInfo = SkImageInfo::Make(imageSize.width(), imageSize.height(), kRGBA_8888_SkColorType, alphaType, SkColorSpace::MakeSRGB());

    Ref protectedPixelBuffer = pixelBuffer;
    SkPixmap pixmap(imageInfo, pixelBuffer->bytes().data(), imageInfo.minRowBytes());
    auto image = SkImages::RasterFromPixmap(pixmap, [](const void*, void* context) {
        static_cast<PixelBuffer*>(context)->deref();
    }, &protectedPixelBuffer.leakRef());
    return NativeImage::create(WTFMove(image));
}

} // namespace WebCore

#endif // ENABLE(WEBGL) && USE(SKIA)
