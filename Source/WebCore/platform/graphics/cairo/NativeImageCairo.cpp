/*
 * Copyright (C) 2016-2020 Apple Inc. All rights reserved.
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
#include "PixelBuffer.h"
#include "RefPtrCairo.h"
#include <algorithm>
#include <cairo.h>

namespace WebCore {

RefPtr<NativeImage> NativeImage::create(Ref<PixelBuffer>&& pixelBuffer)
{
    if (pixelBuffer->size().isEmpty())
        return nullptr;
    auto format = pixelBuffer->format();
    bool hasAlpha = !pixelFormatIsOpaque(format.pixelFormat);
    // The cairo image surface formats are BGRA (CAIRO_FORMAT_ARGB32) and BGRX
    // (CAIRO_FORMAT_RGB24) on little-endian architectures, so contents that have their
    // components in the RGBA order are converted in place.
    bool needsComponentSwap = false;
    switch (format.pixelFormat) {
    case PixelFormat::RGBX8:
    case PixelFormat::RGBA8:
        needsComponentSwap = true;
        break;
    case PixelFormat::BGRX8:
    case PixelFormat::BGRA8:
        needsComponentSwap = false;
        break;
#if ENABLE(PIXEL_FORMAT_RGBA16F)
    case PixelFormat::RGBA16F:
        // cairo has no image surface format for the 16 bit float components.
        return nullptr;
#endif
#if ENABLE(PIXEL_FORMAT_RGB10)
    case PixelFormat::RGB10:
        ASSERT(!PixelBuffer::supportedPixelFormat(format.pixelFormat));
        return nullptr;
#endif
#if ENABLE(PIXEL_FORMAT_RGB10A8)
    case PixelFormat::RGB10A8:
        ASSERT(!PixelBuffer::supportedPixelFormat(format.pixelFormat));
        return nullptr;
#endif
    }

    size_t totalBytes = pixelBuffer->bytes().size();
    uint8_t* pixels = pixelBuffer->bytes().data();
    if (needsComponentSwap) {
        // Convert RGBA to BGRA.
        for (size_t i = 0; i < totalBytes; i += 4)
            std::swap(pixels[i], pixels[i + 2]);
    }

    // The cairo image surfaces are premultiplied. Contents that are drawn as opaque need no
    // premultiplication, as their alpha is ignored.
    if (hasAlpha && format.alphaFormat == AlphaPremultiplication::Unpremultiplied) {
        for (size_t i = 0; i < totalBytes; i += 4) {
            pixels[i + 0] = std::min(255, pixels[i + 0] * pixels[i + 3] / 255);
            pixels[i + 1] = std::min(255, pixels[i + 1] * pixels[i + 3] / 255);
            pixels[i + 2] = std::min(255, pixels[i + 2] * pixels[i + 3] / 255);
        }
    }

    auto imageSize = pixelBuffer->size();
    RefPtr<cairo_surface_t> imageSurface = adoptRef(cairo_image_surface_create_for_data(
        pixels, hasAlpha ? CAIRO_FORMAT_ARGB32 : CAIRO_FORMAT_RGB24, imageSize.width(), imageSize.height(), imageSize.width() * 4));
    if (cairo_surface_status(imageSurface.get()) != CAIRO_STATUS_SUCCESS)
        return nullptr;
    static cairo_user_data_key_t dataKey;
    // On success, the surface owns the pixel buffer reference.
    auto* pixelBufferContext = pixelBuffer.ptr();
    if (cairo_surface_set_user_data(imageSurface.get(), &dataKey, pixelBufferContext, [](void* buffer) {
        static_cast<PixelBuffer*>(buffer)->deref();
    }) != CAIRO_STATUS_SUCCESS)
        return nullptr;
    SUPPRESS_RETAINPTR_CTOR_ADOPT (void) pixelBuffer.leakRef(); // NOLINT
    return create(WTF::move(imageSurface));
}

IntSize NativeImage::size() const
{
    Locker locker { m_lock };
    return cairoSurfaceSize(m_platformImage.get());
}

bool NativeImage::hasAlpha() const
{
    Locker locker { m_lock };
    return cairo_surface_get_content(m_platformImage.get()) != CAIRO_CONTENT_COLOR;
}

ColorSpace NativeImage::colorSpace() const
{
    notImplemented();
    return ColorSpace::SRGB();
}

std::optional<Color> NativeImage::singlePixelSolidColor() const
{
    if (size() != IntSize(1, 1))
        return std::nullopt;
    Locker locker { m_lock };
    if (cairo_surface_get_type(m_platformImage.get()) != CAIRO_SURFACE_TYPE_IMAGE)
        return std::nullopt;

    unsigned* pixel = reinterpret_cast_ptr<unsigned*>(cairo_image_surface_get_data(m_platformImage.get()));
    return unpremultiplied(asSRGBA(PackedColor::ARGB { *pixel }));
}

void NativeImage::clearSubimages()
{
}

#if USE(COORDINATED_GRAPHICS)
uint64_t NativeImage::uniqueID() const
{
    if (auto image = platformImage())
        return getSurfaceUniqueID(image.get());
    return 0;
}
#endif

} // namespace WebCore

#endif // USE(CAIRO)
