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
#include "NativeImage.h"
#include "NotImplemented.h"
#include "PlatformDisplay.h"
#include "SharedBuffer.h"
#include "SkiaSpanExtras.h"
#include <skia/core/SkData.h>

WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_BEGIN
#include <skia/core/SkImage.h>
#include <skia/core/SkPixmap.h>
WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_END

namespace WebCore {

GraphicsContextGLImageExtractor::~GraphicsContextGLImageExtractor() = default;

static std::optional<GraphicsContextGL::DataFormat> dataFormatForColorType(SkColorType colorType)
{
    switch (colorType) {
    case kRGBA_8888_SkColorType:
        return GraphicsContextGL::DataFormat::RGBA8;
    case kBGRA_8888_SkColorType:
        return GraphicsContextGL::DataFormat::BGRA8;
    default:
        break;
    }
    return std::nullopt;
}

bool GraphicsContextGLImageExtractor::extractImage(AlphaPremultiplication sourceAlphaPremultiplication, bool premultiplyAlpha)
{
    auto platformImage = m_image->platformImage();
    if (!platformImage)
        return false;

    m_imageWidth = platformImage->width();
    m_imageHeight = platformImage->height();
    if (!m_imageWidth || !m_imageHeight)
        return false;

    const auto& imageInfo = platformImage->imageInfo();

    // The SkImage records the premultiplication of its contents, but it may record it incorrectly,
    // so only the presence of an alpha channel is taken from it and the premultiplication from the caller.
    m_alphaOp = AlphaOp::DoNothing;
    switch (imageInfo.alphaType()) {
    case kUnknown_SkAlphaType:
    case kOpaque_SkAlphaType:
        break;
    case kPremul_SkAlphaType:
    case kUnpremul_SkAlphaType:
        m_alphaOp = alphaOpForPremultiplication(sourceAlphaPremultiplication, premultiplyAlpha);
        break;
    }

    unsigned srcUnpackAlignment = 1;
    size_t bytesPerRow = 0;

    // Use the pixels as is when the layout is one the caller understands, otherwise convert to RGBA8.
    auto sourceFormat = dataFormatForColorType(imageInfo.colorType());
    auto readInfo = sourceFormat ? imageInfo : imageInfo.makeColorType(kRGBA_8888_SkColorType);

    if (platformImage->isTextureBacked() || !sourceFormat) {
        auto data = SkData::MakeUninitialized(readInfo.computeMinByteSize());
        bytesPerRow = readInfo.minRowBytes();
        if (platformImage->isTextureBacked() && !PlatformDisplay::sharedDisplay().skiaGLContext()->makeContextCurrent())
            return false;

        auto* grContext = m_image->grContext();
        if (!platformImage->readPixels(grContext, readInfo, static_cast<uint8_t*>(data->writable_data()), bytesPerRow, 0, 0))
            return false;

        m_pixelData = WTF::move(data);
        m_imagePixelData = span(m_pixelData.get());
        m_imageSourceFormat = sourceFormat.value_or(DataFormat::RGBA8);
    } else {
        SkPixmap pixmap;
        if (!platformImage->peekPixels(&pixmap))
            return false;

        bytesPerRow = pixmap.rowBytes();
        m_skImage = WTF::move(platformImage);
        m_imagePixelData = span(pixmap);
        m_imageSourceFormat = *sourceFormat;
    }

    size_t bytesPerPixel = readInfo.bytesPerPixel();
    unsigned padding = bytesPerRow - bytesPerPixel * m_imageWidth;
    if (padding) {
        srcUnpackAlignment = padding + 1;
        while (bytesPerRow % srcUnpackAlignment)
            ++srcUnpackAlignment;
    }

    m_imageSourceUnpackAlignment = srcUnpackAlignment;
    return true;
}

} // namespace WebCore

#endif // ENABLE(WEBGL) && USE(SKIA)
