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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
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
#include "PlatformContextCairo.h"

#if ENABLE(WEBGL)

#include "Image.h"
#include "RefPtrCairo.h"
#include <cairo.h>
#include <wtf/PassOwnPtr.h>

namespace WebCore {

bool GraphicsContext3D::getImageData(Image* image, unsigned int format, unsigned int type, bool premultiplyAlpha, bool ignoreGammaAndColorProfile, Vector<uint8_t>& outputVector)
{
    if (!image)
        return false;
    // We need this to stay in scope because the native image is just a shallow copy of the data.
    ImageSource decoder(premultiplyAlpha ? ImageSource::AlphaPremultiplied : ImageSource::AlphaNotPremultiplied,
                        ignoreGammaAndColorProfile ? ImageSource::GammaAndColorProfileIgnored : ImageSource::GammaAndColorProfileApplied);
    AlphaOp alphaOp = AlphaDoNothing;
    RefPtr<cairo_surface_t> imageSurface;
    if (image->data()) {
        decoder.setData(image->data(), true);
        if (!decoder.frameCount() || !decoder.frameIsCompleteAtIndex(0))
            return false;
        imageSurface = decoder.createFrameAtIndex(0);
    } else {
        imageSurface = image->nativeImageForCurrentFrame();
        if (!premultiplyAlpha)
            alphaOp = AlphaDoUnmultiply;
    }

    if (!imageSurface)
        return false;

    int width = cairo_image_surface_get_width(imageSurface.get());
    int height = cairo_image_surface_get_height(imageSurface.get());
    if (!width || !height)
        return false;

    if (cairo_image_surface_get_format(imageSurface.get()) != CAIRO_FORMAT_ARGB32)
        return false;

    unsigned int srcUnpackAlignment = 1;
    size_t bytesPerRow = cairo_image_surface_get_stride(imageSurface.get());
    size_t bitsPerPixel = 32;
    unsigned int padding = bytesPerRow - bitsPerPixel / 8 * width;
    if (padding) {
        srcUnpackAlignment = padding + 1;
        while (bytesPerRow % srcUnpackAlignment)
            ++srcUnpackAlignment;
    }

    outputVector.resize(width * height * 4);
    return packPixels(cairo_image_surface_get_data(imageSurface.get()), SourceFormatBGRA8,
                      width, height, srcUnpackAlignment, format, type, alphaOp, outputVector.data());
}

void GraphicsContext3D::paintToCanvas(const unsigned char* imagePixels, int imageWidth, int imageHeight, int canvasWidth, int canvasHeight, PlatformContextCairo* context)
{
    if (!imagePixels || imageWidth <= 0 || imageHeight <= 0 || canvasWidth <= 0 || canvasHeight <= 0 || !context)
        return;

    cairo_t *cr = context->cr();
    context->save();

    cairo_rectangle(cr, 0, 0, canvasWidth, canvasHeight);
    cairo_set_operator(cr, CAIRO_OPERATOR_CLEAR);
    cairo_paint(cr);

    RefPtr<cairo_surface_t> imageSurface = adoptRef(cairo_image_surface_create_for_data(
        const_cast<unsigned char*>(imagePixels), CAIRO_FORMAT_ARGB32, imageWidth, imageHeight, imageWidth * 4));

    // OpenGL keeps the pixels stored bottom up, so we need to flip the image here.
    cairo_translate(cr, 0, imageHeight);
    cairo_scale(cr, 1, -1);

    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
    cairo_set_source_surface(cr, imageSurface.get(), 0, 0);
    cairo_rectangle(cr, 0, 0, canvasWidth, -canvasHeight);

    cairo_fill(cr);
    context->restore();
}

void GraphicsContext3D::setContextLostCallback(PassOwnPtr<ContextLostCallback>)
{
}

#if USE(ACCELERATED_COMPOSITING)
PlatformLayer* GraphicsContext3D::platformLayer() const
{
    return 0;
}
#endif

} // namespace WebCore

#endif // ENABLE(WEBGL)
