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
#include "GraphicsContext3D.h"

#if ENABLE(GRAPHICS_CONTEXT_3D) && USE(CAIRO)

#include "CairoUtilities.h"
#include "Image.h"
#include "ImageSource.h"
#include "PlatformContextCairo.h"
#include "RefPtrCairo.h"
#include <cairo.h>

namespace WebCore {

GraphicsContext3D::ImageExtractor::~ImageExtractor() = default;

bool GraphicsContext3D::ImageExtractor::extractImage(bool premultiplyAlpha, bool ignoreGammaAndColorProfile)
{
    if (!m_image)
        return false;
    // We need this to stay in scope because the native image is just a shallow copy of the data.
    AlphaOption alphaOption = premultiplyAlpha ? AlphaOption::Premultiplied : AlphaOption::NotPremultiplied;
    GammaAndColorProfileOption gammaAndColorProfileOption = ignoreGammaAndColorProfile ? GammaAndColorProfileOption::Ignored : GammaAndColorProfileOption::Applied;
    auto source = ImageSource::create(nullptr, alphaOption, gammaAndColorProfileOption);
    m_alphaOp = AlphaDoNothing;

    if (m_image->data()) {
        source->setData(m_image->data(), true);
        if (!source->frameCount())
            return false;
        m_imageSurface = source->createFrameImageAtIndex(0);
    } else {
        m_imageSurface = m_image->nativeImageForCurrentFrame();
        // 1. For texImage2D with HTMLVideoElment input, assume no PremultiplyAlpha had been applied and the alpha value is 0xFF for each pixel,
        // which is true at present and may be changed in the future and needs adjustment accordingly.
        // 2. For texImage2D with HTMLCanvasElement input in which Alpha is already Premultiplied in this port, 
        // do AlphaDoUnmultiply if UNPACK_PREMULTIPLY_ALPHA_WEBGL is set to false.
        if (!premultiplyAlpha && m_imageHtmlDomSource != HtmlDomVideo)
            m_alphaOp = AlphaDoUnmultiply;

        // if m_imageSurface is not an image, extract a copy of the surface
        if (m_imageSurface && cairo_surface_get_type(m_imageSurface.get()) != CAIRO_SURFACE_TYPE_IMAGE) {
            IntSize surfaceSize = cairoSurfaceSize(m_imageSurface.get());
            auto tmpSurface = adoptRef(cairo_image_surface_create(CAIRO_FORMAT_ARGB32, surfaceSize.width(), surfaceSize.height()));
            copyRectFromOneSurfaceToAnother(m_imageSurface.get(), tmpSurface.get(), IntSize(), IntRect(IntPoint(), surfaceSize), IntSize(), CAIRO_OPERATOR_SOURCE);
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

    unsigned int srcUnpackAlignment = 1;
    size_t bytesPerRow = cairo_image_surface_get_stride(m_imageSurface.get());
    size_t bitsPerPixel = 32;
    unsigned padding = bytesPerRow - bitsPerPixel / 8 * m_imageWidth;
    if (padding) {
        srcUnpackAlignment = padding + 1;
        while (bytesPerRow % srcUnpackAlignment)
            ++srcUnpackAlignment;
    }

    m_imagePixelData = cairo_image_surface_get_data(m_imageSurface.get());
    m_imageSourceFormat = DataFormatBGRA8;
    m_imageSourceUnpackAlignment = srcUnpackAlignment;
    return true;
}

void GraphicsContext3D::paintToCanvas(const unsigned char* imagePixels, const IntSize& imageSize, const IntSize& canvasSize, GraphicsContext& context)
{
    if (!imagePixels || imageSize.isEmpty() || canvasSize.isEmpty())
        return;

    PlatformContextCairo* platformContext = context.platformContext();
    if (!platformContext)
        return;

    cairo_t* cr = platformContext->cr();
    platformContext->save();

    cairo_rectangle(cr, 0, 0, canvasSize.width(), canvasSize.height());
    cairo_set_operator(cr, CAIRO_OPERATOR_CLEAR);
    cairo_paint(cr);

    RefPtr<cairo_surface_t> imageSurface = adoptRef(cairo_image_surface_create_for_data(
        const_cast<unsigned char*>(imagePixels), CAIRO_FORMAT_ARGB32, imageSize.width(), imageSize.height(), imageSize.width() * 4));

    // OpenGL keeps the pixels stored bottom up, so we need to flip the image here.
    cairo_translate(cr, 0, imageSize.height());
    cairo_scale(cr, 1, -1);

    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
    cairo_set_source_surface(cr, imageSurface.get(), 0, 0);
    cairo_rectangle(cr, 0, 0, canvasSize.width(), -canvasSize.height());

    cairo_fill(cr);
    platformContext->restore();
}

} // namespace WebCore

#endif // ENABLE(GRAPHICS_CONTEXT_3D) && USE(CAIRO)
