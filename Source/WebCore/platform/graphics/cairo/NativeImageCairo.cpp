/*
 * Copyright (C) 2016 Apple Inc.  All rights reserved.
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

#if USE(CAIRO)

#include "CairoUtilities.h"
#include "PlatformContextCairo.h"
#include <cairo.h>

namespace WebCore {

IntSize nativeImageSize(const NativeImagePtr& image)
{
    return image ? cairoSurfaceSize(image.get()) : IntSize();
}

bool nativeImageHasAlpha(const NativeImagePtr& image)
{
    return !image || cairo_surface_get_content(image.get()) != CAIRO_CONTENT_COLOR;
}

Color nativeImageSinglePixelSolidColor(const NativeImagePtr& image)
{
    if (!image || nativeImageSize(image) != IntSize(1, 1))
        return Color();

    if (cairo_surface_get_type(image.get()) != CAIRO_SURFACE_TYPE_IMAGE)
        return Color();

    RGBA32* pixel = reinterpret_cast_ptr<RGBA32*>(cairo_image_surface_get_data(image.get()));
    return colorFromPremultipliedARGB(*pixel);
}

float subsamplingScale(GraphicsContext&, const FloatRect&, const FloatRect&)
{
    return 1;
}

void drawNativeImage(const NativeImagePtr& image, GraphicsContext& context, const FloatRect& destRect, const FloatRect& srcRect, const IntSize&, CompositeOperator op, BlendMode mode, const ImageOrientation& orientation)
{
    context.save();
    
    // Set the compositing operation.
    if (op == CompositeSourceOver && mode == BlendModeNormal && !nativeImageHasAlpha(image))
        context.setCompositeOperation(CompositeCopy);
    else
        context.setCompositeOperation(op, mode);
        
#if ENABLE(IMAGE_DECODER_DOWN_SAMPLING)
    IntSize scaledSize = nativeImageSize(image);
    FloatRect adjustedSrcRect = adjustSourceRectForDownSampling(srcRect, scaledSize);
#else
    FloatRect adjustedSrcRect(srcRect);
#endif
        
    FloatRect adjustedDestRect = destRect;
        
    if (orientation != DefaultImageOrientation) {
        // ImageOrientation expects the origin to be at (0, 0).
        context.translate(destRect.x(), destRect.y());
        adjustedDestRect.setLocation(FloatPoint());
        context.concatCTM(orientation.transformFromDefault(adjustedDestRect.size()));
        if (orientation.usesWidthAsHeight()) {
            // The destination rectangle will have it's width and height already reversed for the orientation of
            // the image, as it was needed for page layout, so we need to reverse it back here.
            adjustedDestRect.setSize(adjustedDestRect.size().transposedSize());
        }
    }

    context.platformContext()->drawSurfaceToContext(image.get(), adjustedDestRect, adjustedSrcRect, context);
    context.restore();
}

void clearNativeImageSubimages(const NativeImagePtr&)
{
}

} // namespace WebCore

#endif // USE(CAIRO)
