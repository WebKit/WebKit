/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2011 Igalia S.L.
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

#if ENABLE(WEBGL) && USE(CAIRO)

#include "BitmapImage.h"
#include "CairoUtilities.h"
#include "GraphicsContext.h"
#include "GraphicsContextGLImageExtractor.h"
#include "PixelBuffer.h"
#include "RefPtrCairo.h"
#include <cairo.h>

namespace WebCore {

GraphicsContextGLImageExtractor::~GraphicsContextGLImageExtractor() = default;

bool GraphicsContextGLImageExtractor::extractImage(bool premultiplyAlpha, bool ignoreGammaAndColorProfile, bool)
{
    // We need this to stay in scope because the native image is just a shallow copy of the data.
    AlphaOption alphaOption = premultiplyAlpha ? AlphaOption::Premultiplied : AlphaOption::NotPremultiplied;
    GammaAndColorProfileOption gammaAndColorProfileOption = ignoreGammaAndColorProfile ? GammaAndColorProfileOption::Ignored : GammaAndColorProfileOption::Applied;
    auto image = BitmapImage::create(nullptr, alphaOption, gammaAndColorProfileOption);
    m_alphaOp = AlphaOp::DoNothing;

    if (m_image->data()) {
        image->setData(m_image->data(), true);
        if (!image->frameCount())
            return false;
        m_imageSurface = image->currentNativeImage()->platformImage();
    } else {
        m_imageSurface = m_image->currentNativeImage()->platformImage();
        // 1. For texImage2D with HTMLVideoElment input, assume no PremultiplyAlpha had been applied and the alpha value is 0xFF for each pixel,
        // which is true at present and may be changed in the future and needs adjustment accordingly.
        // 2. For texImage2D with HTMLCanvasElement input in which Alpha is already Premultiplied in this port, 
        // do AlphaDoUnmultiply if UNPACK_PREMULTIPLY_ALPHA_WEBGL is set to false.
        if (!premultiplyAlpha && m_imageHtmlDomSource != DOMSource::Video)
            m_alphaOp = AlphaOp::DoUnmultiply;

        // if m_imageSurface is not an image, extract a copy of the surface
        if (m_imageSurface && cairo_surface_get_type(m_imageSurface.get()) != CAIRO_SURFACE_TYPE_IMAGE) {
            IntSize surfaceSize = cairoSurfaceSize(m_imageSurface.get());
            auto tmpSurface = adoptRef(cairo_image_surface_create(CAIRO_FORMAT_ARGB32, surfaceSize.width(), surfaceSize.height()));
            copyRectFromOneSurfaceToAnother(m_imageSurface.get(), tmpSurface.get(), IntSize(), IntRect(IntPoint(), surfaceSize), IntSize());
            m_imageSurface = WTFMove(tmpSurface);
        }
    }

    if (!m_imageSurface)
        return false;

    ASSERT(cairo_surface_get_type(m_imageSurface.get()) == CAIRO_SURFACE_TYPE_IMAGE);

    IntSize imageSize = cairoSurfaceSize(m_imageSurface.get());
    m_imageWidth = imageSize.width();
    m_imageHeight = imageSize.height();
    if (!m_imageWidth || !m_imageHeight)
        return false;

    if (cairo_image_surface_get_format(m_imageSurface.get()) != CAIRO_FORMAT_ARGB32)
        return false;

    unsigned srcUnpackAlignment = 1;
    size_t bytesPerRow = cairo_image_surface_get_stride(m_imageSurface.get());
    size_t bitsPerPixel = 32;
    unsigned padding = bytesPerRow - bitsPerPixel / 8 * m_imageWidth;
    if (padding) {
        srcUnpackAlignment = padding + 1;
        while (bytesPerRow % srcUnpackAlignment)
            ++srcUnpackAlignment;
    }

    m_imagePixelData = cairo_image_surface_get_data(m_imageSurface.get());
    m_imageSourceFormat = DataFormat::BGRA8;
    m_imageSourceUnpackAlignment = srcUnpackAlignment;
    return true;
}

RefPtr<NativeImage> GraphicsContextGL::createNativeImageFromPixelBuffer(const GraphicsContextGLAttributes& sourceContextAttributes, Ref<PixelBuffer>&& pixelBuffer)
{
    ASSERT(!pixelBuffer->size().isEmpty());

    // Convert RGBA to BGRA. BGRA is CAIRO_FORMAT_ARGB32 on little-endian architectures.
    Ref protectedPixelBuffer = pixelBuffer;
    size_t totalBytes = pixelBuffer->bytes().size();
    uint8_t* pixels = pixelBuffer->bytes().data();
    for (size_t i = 0; i < totalBytes; i += 4)
        std::swap(pixels[i], pixels[i + 2]);

    if (!sourceContextAttributes.premultipliedAlpha) {
        for (size_t i = 0; i < totalBytes; i += 4) {
            pixels[i + 0] = std::min(255, pixels[i + 0] * pixels[i + 3] / 255);
            pixels[i + 1] = std::min(255, pixels[i + 1] * pixels[i + 3] / 255);
            pixels[i + 2] = std::min(255, pixels[i + 2] * pixels[i + 3] / 255);
        }
    }

    auto imageSize = pixelBuffer->size();
    RefPtr<cairo_surface_t> imageSurface = adoptRef(cairo_image_surface_create_for_data(
        pixels, CAIRO_FORMAT_ARGB32, imageSize.width(), imageSize.height(), imageSize.width() * 4));
    static cairo_user_data_key_t dataKey;
    cairo_surface_set_user_data(imageSurface.get(), &dataKey, &protectedPixelBuffer.leakRef(), [](void* buffer) {
        static_cast<PixelBuffer*>(buffer)->deref();
    });
    return NativeImage::create(WTFMove(imageSurface));
}

} // namespace WebCore

#endif // ENABLE(WEBGL) && USE(CAIRO)
