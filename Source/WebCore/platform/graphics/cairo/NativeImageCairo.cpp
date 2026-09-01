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
#include "IntRect.h"
#include "NotImplemented.h"
#include "PixelBuffer.h"
#include "PixelBufferConversion.h"
#include "RefPtrCairo.h"
#include <cairo.h>

namespace WebCore {

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

DestinationColorSpace NativeImage::colorSpace() const
{
    notImplemented();
    return DestinationColorSpace::SRGB();
}

std::optional<NativeImage::PixelSourceInfo> NativeImage::pixelSourceInfo() const
{
    auto image = platformImage();
    if (!image)
        return std::nullopt;
    // Only image surfaces expose their pixels; anything else must be drawn into one.
    if (cairo_surface_get_type(image.get()) != CAIRO_SURFACE_TYPE_IMAGE)
        return std::nullopt;
    // CAIRO_FORMAT_ARGB32 is premultiplied BGRA8 on little-endian architectures, which is
    // the only cairo format PixelBufferFormat can name.
    if (cairo_image_surface_get_format(image.get()) != CAIRO_FORMAT_ARGB32)
        return std::nullopt;

    auto stride = cairo_image_surface_get_stride(image.get());
    if (stride < 0)
        return std::nullopt;

    // Cairo has no colour management, so its surfaces are treated as sRGB throughout.
    PixelBufferFormat format { AlphaPremultiplication::Premultiplied, PixelFormat::BGRA8, DestinationColorSpace::SRGB() };
    return PixelSourceInfo { format, static_cast<unsigned>(stride) };
}

bool NativeImage::withBorrowedPixels(NOESCAPE const PixelSourceFunctor& functor) const
{
    auto info = pixelSourceInfo();
    if (!info)
        return false;
    auto image = platformImage();
    if (!image)
        return false;

    cairo_surface_flush(image.get());
    auto view = validatedConversionView(info->format, cairoSurfaceSize(image.get()), info->bytesPerRow, span(image.get()));
    if (!view)
        return false;

    functor(*view);
    return true;
}

bool NativeImage::readPixels(const PixelBufferFormat& format, std::span<uint8_t> destination, unsigned bytesPerRow) const
{
    auto image = platformImage();
    if (!image)
        return false;
    // Cairo can only produce premultiplied BGRA8; withPixels() converts from there.
    if (format.pixelFormat != PixelFormat::BGRA8 || format.alphaFormat != AlphaPremultiplication::Premultiplied)
        return false;

    auto size = cairoSurfaceSize(image.get());
    if (size.isEmpty()) {
        ASSERT_NOT_REACHED();
        return false;
    }
    // The whole stride of every row is written, including any padding on the last row, so the
    // destination has to be larger than PixelBuffer::minimumBufferSize() would require.
    auto tightlyPackedBytesPerRow = PixelBuffer::tightlyPackedBytesPerRow(format.pixelFormat, size.width());
    if (tightlyPackedBytesPerRow.hasOverflowed() || bytesPerRow < tightlyPackedBytesPerRow.value()) {
        ASSERT_NOT_REACHED();
        return false;
    }
    auto requiredBytes = CheckedSize { bytesPerRow } * static_cast<size_t>(size.height());
    if (requiredBytes.hasOverflowed() || destination.size() < requiredBytes.value()) {
        ASSERT_NOT_REACHED();
        return false;
    }

    RefPtr surface = adoptRef(cairo_image_surface_create_for_data(destination.data(), CAIRO_FORMAT_ARGB32, size.width(), size.height(), static_cast<int>(bytesPerRow)));
    if (!surface || cairo_surface_status(surface.get()) != CAIRO_STATUS_SUCCESS)
        return false;

    copyRectFromOneSurfaceToAnother(image.get(), surface.get(), IntSize(), IntRect(IntPoint(), size), IntSize());
    cairo_surface_flush(surface.get());
    return true;
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
