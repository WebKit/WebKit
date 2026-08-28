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
#include "GLContext.h"
#include "GraphicsContextSkia.h"
#include "PixelBuffer.h"
#include "PixelBufferConversion.h"
#include "PlatformDisplay.h"
#include "SkiaSpanExtras.h"
#include <limits>
WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_BEGIN // GLib/Win ports
#include <skia/core/SkPixmap.h>
WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_END

namespace WebCore {

RefPtr<NativeImage> NativeImage::create(PlatformImagePtr&& platformImage, std::optional<GainMap>&& gainMap, GrDirectContext* grContext)
{
    if (!platformImage)
        return nullptr;
    return adoptRef(*new NativeImage(WTF::move(platformImage), WTF::move(gainMap), grContext));
}

RefPtr<NativeImage> NativeImage::create(PlatformImagePtr&& platformImage, GrDirectContext* grContext)
{
    return create(WTF::move(platformImage), std::nullopt, grContext);
}

RefPtr<NativeImage> NativeImage::createTransient(PlatformImagePtr&& platformImage, GrDirectContext* grContext)
{
    return create(WTF::move(platformImage), grContext);
}

NativeImage::NativeImage(PlatformImagePtr&& platformImage, std::optional<GainMap>&& gainMap, GrDirectContext* grContext)
    : m_platformImage(WTF::move(platformImage))
    , m_gainMap(WTF::move(gainMap))
    , m_grContext(grContext)
{
    ASSERT(!m_platformImage->isTextureBacked() || m_grContext);
    computeHeadroom();
}

IntSize NativeImage::size() const
{
    Locker locker { m_lock };
    return IntSize(m_platformImage->width(), m_platformImage->height());
}

bool NativeImage::hasAlpha() const
{
    Locker locker { m_lock };
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

DestinationColorSpace NativeImage::colorSpace() const
{
    Locker locker { m_lock };
    if (auto colorSpace = m_platformImage->refColorSpace())
        return DestinationColorSpace(colorSpace);
    // No color space means the default - SRGB.
    return DestinationColorSpace::SRGB();
}

// Maps an SkImageInfo onto a PixelBufferFormat. nullopt for color types PixelBufferFormat
// cannot name; those images reach withPixels() through readPixels() instead.
//
// Deliberately not exported: NativeImage is meant to be the only place that maps a platform
// image layout to a pixel format. GraphicsContextGLSkia has a copy of this as
// dataFormatForColorType(), which should be deleted in favour of this one.
static std::optional<PixelBufferFormat> pixelBufferFormat(const SkImageInfo& imageInfo)
{
    std::optional<PixelFormat> pixelFormat;
    switch (imageInfo.colorType()) {
    case kRGBA_8888_SkColorType:
        pixelFormat = PixelFormat::RGBA8;
        break;
    case kBGRA_8888_SkColorType:
        pixelFormat = PixelFormat::BGRA8;
        break;
    default:
        return std::nullopt;
    }

    std::optional<AlphaPremultiplication> alphaFormat;
    switch (imageInfo.alphaType()) {
    case kPremul_SkAlphaType:
    // An opaque image's alpha is effectively 255, so either alpha format describes it;
    // convertImagePixels() treats premultiplied as the no-op case.
    case kOpaque_SkAlphaType:
        alphaFormat = AlphaPremultiplication::Premultiplied;
        break;
    case kUnpremul_SkAlphaType:
        alphaFormat = AlphaPremultiplication::Unpremultiplied;
        break;
    case kUnknown_SkAlphaType:
        return std::nullopt;
    }

    if (auto colorSpace = imageInfo.refColorSpace())
        return PixelBufferFormat { *alphaFormat, *pixelFormat, DestinationColorSpace(colorSpace) };
    return PixelBufferFormat { *alphaFormat, *pixelFormat, DestinationColorSpace::SRGB() };
}

std::optional<NativeImage::PixelSourceInfo> NativeImage::pixelSourceInfo() const
{
    auto platformImage = this->platformImage();
    if (!platformImage || platformImage->isTextureBacked())
        return std::nullopt;

    auto format = pixelBufferFormat(platformImage->imageInfo());
    if (!format)
        return std::nullopt;

    SkPixmap pixmap;
    if (!platformImage->peekPixels(&pixmap))
        return std::nullopt;
    if (pixmap.rowBytes() > std::numeric_limits<unsigned>::max())
        return std::nullopt;

    return PixelSourceInfo { *format, static_cast<unsigned>(pixmap.rowBytes()) };
}

bool NativeImage::withBorrowedPixels(NOESCAPE const PixelSourceFunctor& functor) const
{
    auto platformImage = this->platformImage();
    if (!platformImage || platformImage->isTextureBacked())
        return false;

    auto format = pixelBufferFormat(platformImage->imageInfo());
    if (!format)
        return false;

    SkPixmap pixmap;
    if (!platformImage->peekPixels(&pixmap))
        return false;
    if (pixmap.rowBytes() > std::numeric_limits<unsigned>::max())
        return false;

    auto view = validatedConversionView(*format, IntSize(pixmap.width(), pixmap.height()), static_cast<unsigned>(pixmap.rowBytes()), span(pixmap));
    if (!view)
        return false;

    functor(*view);
    return true;
}

bool NativeImage::readPixels(const PixelBufferFormat& format, std::span<uint8_t> destination, unsigned bytesPerRow) const
{
    auto platformImage = this->platformImage();
    if (!platformImage)
        return false;

    auto size = this->size();
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

    std::optional<SkColorType> colorType;
    switch (format.pixelFormat) {
    case PixelFormat::RGBA8:
        colorType = kRGBA_8888_SkColorType;
        break;
    case PixelFormat::BGRA8:
    // BGRX8 is BGRA8 with the alpha ignored; Skia has no skip-alpha color type, so read
    // BGRA8 and let the caller's conversion treat the result as opaque.
    case PixelFormat::BGRX8:
        colorType = kBGRA_8888_SkColorType;
        break;
    default:
        return false;
    }

    auto alphaType = format.alphaFormat == AlphaPremultiplication::Premultiplied ? kPremul_SkAlphaType : kUnpremul_SkAlphaType;
    if (pixelFormatIsOpaque(format.pixelFormat))
        alphaType = kOpaque_SkAlphaType;
    auto imageInfo = SkImageInfo::Make(size.width(), size.height(), *colorType, alphaType, format.colorSpace.platformColorSpace());

    if (platformImage->isTextureBacked() && !PlatformDisplay::sharedDisplay().skiaGLContext()->makeContextCurrent())
        return false;

    return platformImage->readPixels(m_grContext, imageInfo, destination.data(), bytesPerRow, 0, 0);
}

void NativeImage::clearSubimages()
{
}

uint64_t NativeImage::uniqueID() const
{
    return platformImage()->uniqueID();
}

} // namespace WebCore

#endif // USE(SKIA)
