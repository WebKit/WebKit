/*
 * Copyright (C) 2016-2020 Apple Inc.  All rights reserved.
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

#include "CairoOperations.h"
#include "CairoUtilities.h"
#include "NotImplemented.h"
#include <cairo.h>

namespace WebCore {

RefPtr<NativeImage> NativeImage::create(Ref<PixelBuffer>&& pixelBuffer, bool premultipliedAlpha)
{
    ASSERT(!pixelBuffer->size().isEmpty());

    // Convert RGBA to BGRA. BGRA is CAIRO_FORMAT_ARGB32 on little-endian architectures.
    size_t totalBytes = pixelBuffer->sizeInBytes();
    uint8_t* pixels = pixelBuffer->bytes();
    for (size_t i = 0; i < totalBytes; i += 4)
        std::swap(pixels[i], pixels[i + 2]);

    if (!premultipliedAlpha) {
        for (size_t i = 0; i < totalBytes; i += 4) {
            pixels[i + 0] = std::min(255, pixels[i + 0] * pixels[i + 3] / 255);
            pixels[i + 1] = std::min(255, pixels[i + 1] * pixels[i + 3] / 255);
            pixels[i + 2] = std::min(255, pixels[i + 2] * pixels[i + 3] / 255);
        }
    }

    auto imageSize = pixelBuffer->size();
    RefPtr<cairo_surface_t> imageSurface = adoptRef(cairo_image_surface_create_for_data(
        pixelBuffer->bytes(), CAIRO_FORMAT_ARGB32, imageSize.width(), imageSize.height(), imageSize.width() * 4));

    return adoptRef(*new NativeImage(WTFMove(imageSurface), RenderingResourceIdentifier::generate(), WTFMove(pixelBuffer)));
}

NativeImage::NativeImage(PlatformImagePtr&& platformImage, RenderingResourceIdentifier renderingResourceIdentifier, Ref<PixelBuffer>&& pixelBuffer)
    : RenderingResource(renderingResourceIdentifier)
    , m_platformImage(WTFMove(platformImage))
    , m_pixelBuffer(WTFMove(pixelBuffer))
{
}

IntSize NativeImage::size() const
{
    return cairoSurfaceSize(m_platformImage.get());
}

bool NativeImage::hasAlpha() const
{
    return cairo_surface_get_content(m_platformImage.get()) != CAIRO_CONTENT_COLOR;
}

Color NativeImage::singlePixelSolidColor() const
{
    if (size() != IntSize(1, 1))
        return Color();

    if (cairo_surface_get_type(m_platformImage.get()) != CAIRO_SURFACE_TYPE_IMAGE)
        return Color();

    unsigned* pixel = reinterpret_cast_ptr<unsigned*>(cairo_image_surface_get_data(m_platformImage.get()));
    return unpremultiplied(asSRGBA(PackedColor::ARGB { *pixel }));
}

DestinationColorSpace NativeImage::colorSpace() const
{
    notImplemented();
    return DestinationColorSpace::SRGB();
}

void NativeImage::draw(GraphicsContext& context, const FloatSize& imageSize, const FloatRect& destinationRect, const FloatRect& sourceRect, const ImagePaintingOptions& options)
{
    context.drawNativeImageInternal(*this, imageSize, destinationRect, sourceRect, options);
}

void NativeImage::clearSubimages()
{
}

} // namespace WebCore

#endif // USE(CAIRO)
