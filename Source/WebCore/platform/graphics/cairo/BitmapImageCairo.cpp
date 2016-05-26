/*
 * Copyright (C) 2004, 2005, 2006 Apple Inc.  All rights reserved.
 * Copyright (C) 2007 Alp Toker <alp@atoker.com>
 * Copyright (C) 2009 Dirk Schulze <krit@webkit.org>
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
#include "BitmapImage.h"

#if USE(CAIRO)

#include "CairoUtilities.h"
#include "ImageObserver.h"
#include "PlatformContextCairo.h"
#include "Timer.h"
#include <cairo.h>

namespace WebCore {

namespace NativeImage {

IntSize size(const RefPtr<cairo_surface_t>& image)
{
    return cairoSurfaceSize(image.get());
}

bool hasAlpha(const RefPtr<cairo_surface_t>& image)
{
    return cairo_surface_get_content(image.get()) != CAIRO_CONTENT_COLOR;
}

Color singlePixelSolidColor(const RefPtr<cairo_surface_t>& image)
{
    ASSERT(image);
    
    if (NativeImage::size(image) != IntSize(1, 1))
        return Color();

    if (cairo_surface_get_type(image.get()) != CAIRO_SURFACE_TYPE_IMAGE)
        return Color();

    RGBA32* pixel = reinterpret_cast_ptr<RGBA32*>(cairo_image_surface_get_data(image.get()));
    return colorFromPremultipliedARGB(*pixel);
}

}

void BitmapImage::draw(GraphicsContext& context, const FloatRect& dst, const FloatRect& src, CompositeOperator op,
    BlendMode blendMode, ImageOrientationDescription description)
{
    if (!dst.width() || !dst.height() || !src.width() || !src.height())
        return;

    startAnimation();

    auto surface = frameImageAtIndex(m_currentFrame);
    if (!surface) // If it's too early we won't have an image yet.
        return;

    Color color = singlePixelSolidColor();
    if (color.isValid()) {
        fillWithSolidColor(context, dst, color, op);
        return;
    }

    context.save();

    // Set the compositing operation.
    if (op == CompositeSourceOver && blendMode == BlendModeNormal && !frameHasAlphaAtIndex(m_currentFrame))
        context.setCompositeOperation(CompositeCopy);
    else
        context.setCompositeOperation(op, blendMode);

#if ENABLE(IMAGE_DECODER_DOWN_SAMPLING)
    IntSize scaledSize = cairoSurfaceSize(surface.get());
    FloatRect adjustedSrcRect = adjustSourceRectForDownSampling(src, scaledSize);
#else
    FloatRect adjustedSrcRect(src);
#endif

    ImageOrientation frameOrientation(description.imageOrientation());
    if (description.respectImageOrientation() == RespectImageOrientation)
        frameOrientation = frameOrientationAtIndex(m_currentFrame);

    FloatRect dstRect = dst;

    if (frameOrientation != DefaultImageOrientation) {
        // ImageOrientation expects the origin to be at (0, 0).
        context.translate(dstRect.x(), dstRect.y());
        dstRect.setLocation(FloatPoint());
        context.concatCTM(frameOrientation.transformFromDefault(dstRect.size()));
        if (frameOrientation.usesWidthAsHeight()) {
            // The destination rectangle will have it's width and height already reversed for the orientation of
            // the image, as it was needed for page layout, so we need to reverse it back here.
            dstRect = FloatRect(dstRect.x(), dstRect.y(), dstRect.height(), dstRect.width());
        }
    }

    context.platformContext()->drawSurfaceToContext(surface.get(), dstRect, adjustedSrcRect, context);

    context.restore();

    if (imageObserver())
        imageObserver()->didDraw(this);
}

bool FrameData::clear(bool clearMetadata)
{
    if (clearMetadata)
        m_haveMetadata = false;

    if (m_image) {
        m_image = nullptr;
        return true;
    }
    return false;
}

} // namespace WebCore

#endif // USE(CAIRO)
