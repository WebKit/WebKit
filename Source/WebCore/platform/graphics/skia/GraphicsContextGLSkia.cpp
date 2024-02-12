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
#include "GLContext.h"
#include "GraphicsContextGLImageExtractor.h"
#include "Image.h"
#include "ImageSource.h"
#include "NotImplemented.h"
#include "PixelBuffer.h"
#include "PlatformDisplay.h"
#include <skia/core/SkData.h>
#include <skia/core/SkImage.h>

IGNORE_CLANG_WARNINGS_BEGIN("cast-align")
#include <skia/core/SkPixmap.h>
IGNORE_CLANG_WARNINGS_END

namespace WebCore {

GraphicsContextGLImageExtractor::~GraphicsContextGLImageExtractor() = default;

bool GraphicsContextGLImageExtractor::extractImage(bool premultiplyAlpha, bool ignoreGammaAndColorProfile, bool)
{
    if (!m_image)
        return false;

    RefPtr<NativeImage> decodedImage;
    bool hasAlpha = !m_image->currentFrameKnownToBeOpaque();
    if ((ignoreGammaAndColorProfile || (hasAlpha && !premultiplyAlpha)) && m_image->data()) {
        auto source = ImageSource::create(nullptr, AlphaOption::NotPremultiplied, ignoreGammaAndColorProfile ? GammaAndColorProfileOption::Ignored : GammaAndColorProfileOption::Applied);
        source->setData(m_image->data(), true);
        if (!source->frameCount())
            return false;

        decodedImage = source->createFrameImageAtIndex(0);
    } else
        decodedImage = m_image->nativeImageForCurrentFrame();

    if (!decodedImage)
        return false;

    auto image = decodedImage->platformImage();
    m_imageWidth = image->width();
    m_imageHeight = image->height();
    if (!m_imageWidth || !m_imageHeight)
        return false;

    m_alphaOp = AlphaOp::DoNothing;

    const auto& imageInfo = image->imageInfo();
    unsigned srcUnpackAlignment = 1;
    size_t bytesPerRow = imageInfo.minRowBytes();
    size_t bytesPerPixel = imageInfo.bytesPerPixel();
    unsigned padding = bytesPerRow - bytesPerPixel * m_imageWidth;
    if (padding) {
        srcUnpackAlignment = padding + 1;
        while (bytesPerRow % srcUnpackAlignment)
            ++srcUnpackAlignment;
    }

    if (image->isTextureBacked()) {
        auto data = SkData::MakeUninitialized(imageInfo.computeMinByteSize());
        GrDirectContext* grContext = PlatformDisplay::sharedDisplayForCompositing().skiaGrContext();
        auto* glContext = PlatformDisplay::sharedDisplayForCompositing().skiaGLContext();
        GLContext::ScopedGLContextCurrent scopedCurrent(*glContext);
        if (!image->readPixels(grContext, imageInfo, static_cast<uint8_t*>(data->writable_data()), bytesPerRow, 0, 0))
            return false;

        m_pixelData = WTFMove(data);
        m_imagePixelData = m_pixelData->data();
    } else {
        SkPixmap pixmap;
        if (!image->peekPixels(&pixmap))
            return false;

        m_skImage = WTFMove(image);
        m_imagePixelData = pixmap.addr();
    }

    m_imageSourceFormat = DataFormat::BGRA8;
    m_imageSourceUnpackAlignment = srcUnpackAlignment;
    return true;
}

RefPtr<NativeImage> GraphicsContextGL::createNativeImageFromPixelBuffer(const GraphicsContextGLAttributes& /*sourceContextAttributes*/, Ref<PixelBuffer>&& pixelBuffer)
{
    ASSERT_UNUSED(pixelBuffer, !pixelBuffer->size().isEmpty());
    notImplemented();
    return nullptr;
}

} // namespace WebCore

#endif // ENABLE(WEBGL) && USE(SKIA)
