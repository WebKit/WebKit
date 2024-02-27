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

#include "GraphicsContextSkia.h"
#include "NotImplemented.h"
#include "PlatformDisplay.h"
#include <skia/core/SkData.h>
#include <skia/core/SkImage.h>

IGNORE_CLANG_WARNINGS_BEGIN("cast-align")
#include <skia/core/SkPixmap.h>
IGNORE_CLANG_WARNINGS_END

namespace WebCore {

IntSize PlatformImageNativeImageBackend::size() const
{
    return m_platformImage ? IntSize(m_platformImage->width(), m_platformImage->height()) : IntSize();
}

bool PlatformImageNativeImageBackend::hasAlpha() const
{
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

DestinationColorSpace PlatformImageNativeImageBackend::colorSpace() const
{
    notImplemented();
    return DestinationColorSpace::SRGB();
}

Color NativeImage::singlePixelSolidColor() const
{
    if (size() != IntSize(1, 1))
        return Color();

    auto platformImage = this->platformImage();
    if (platformImage->isTextureBacked()) {
        if (!PlatformDisplay::sharedDisplayForCompositing().skiaGLContext()->makeContextCurrent())
            return Color();

        GrDirectContext* grContext = PlatformDisplay::sharedDisplayForCompositing().skiaGrContext();
        const auto& imageInfo = platformImage->imageInfo();
        uint32_t pixel;
        SkPixmap pixmap(imageInfo, &pixel, imageInfo.minRowBytes());
        if (!platformImage->readPixels(grContext, pixmap, 0, 0))
            return Color();

        return pixmap.getColor(0, 0);
    }

    SkPixmap pixmap;
    if (!platformImage->peekPixels(&pixmap))
        return Color();

    return pixmap.getColor(0, 0);
}

void NativeImage::draw(GraphicsContext& context, const FloatRect& destinationRect, const FloatRect& sourceRect, ImagePaintingOptions options)
{
    context.drawNativeImageInternal(*this, destinationRect, sourceRect, options);
}

void NativeImage::clearSubimages()
{
}

} // namespace WebCore

#endif // USE(SKIA)
