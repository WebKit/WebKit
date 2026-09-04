/*
 * Copyright (C) 2021-2023 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "PixelBufferConversion.h"

#include "AlphaPremultiplication.h"
#include "ColorSpace.h"
#include "IntSize.h"
#include "Logging.h"
#include "PixelBuffer.h"
#include "PixelFormat.h"
#include <array>
#include <wtf/CheckedArithmetic.h>
#include <wtf/StdLibExtras.h>
#include <wtf/text/ParsingUtilities.h>
#include <wtf/text/TextStream.h>

#if USE(ACCELERATE) && USE(CG)
#include <Accelerate/Accelerate.h>
#elif USE(SKIA)
WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_BEGIN
#include <skia/core/SkPixmap.h>
WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_END
#endif

namespace WebCore {

namespace {

enum class AlphaFormat : uint8_t { Opaque, Unpremultiplied, Premultiplied };

// Whether the color components already have the alpha applied to them.
constexpr bool isAlphaApplied(AlphaFormat alphaFormat)
{
    return alphaFormat != AlphaFormat::Unpremultiplied;
}

constexpr AlphaFormat toAlphaFormat(AlphaPremultiplication alphaFormat, PixelFormat pixelFormat)
{
    if (pixelFormatIsOpaque(pixelFormat))
        return AlphaFormat::Opaque;
    if (alphaFormat == AlphaPremultiplication::Premultiplied)
        return AlphaFormat::Premultiplied;
    return AlphaFormat::Unpremultiplied;
}

}

static bool NODELETE isSupportedConversionFormat(PixelFormat pixelFormat)
{
    switch (pixelFormat) {
    case PixelFormat::RGBX8:
    case PixelFormat::RGBA8:
    case PixelFormat::BGRX8:
    case PixelFormat::BGRA8:
#if ENABLE(PIXEL_FORMAT_RGBA16F)
    case PixelFormat::RGBA16F:
#endif
        return true;
    default:
        return false;
    }
}

#if USE(ACCELERATE) && USE(CG)

static inline vImage_CGImageFormat makeVImageCGImageFormat(const PixelBufferFormat& format)
{
    auto [bitsPerComponent, bitsPerPixel, bitmapInfo] = [] (const PixelBufferFormat& format) -> std::tuple<decltype(vImage_CGImageFormat::bitsPerComponent), decltype(vImage_CGImageFormat::bitsPerPixel), CGBitmapInfo> {
        switch (format.pixelFormat) {
        case PixelFormat::RGBX8:
            return std::make_tuple(8u, 32u, static_cast<CGBitmapInfo>(kCGBitmapByteOrder32Big) | static_cast<CGBitmapInfo>(kCGImageAlphaNoneSkipLast));

        case PixelFormat::RGBA8:
            if (format.alphaFormat == AlphaPremultiplication::Premultiplied)
                return std::make_tuple(8u, 32u, static_cast<CGBitmapInfo>(kCGBitmapByteOrder32Big) | static_cast<CGBitmapInfo>(kCGImageAlphaPremultipliedLast));
            return std::make_tuple(8u, 32u, static_cast<CGBitmapInfo>(kCGBitmapByteOrder32Big) | static_cast<CGBitmapInfo>(kCGImageAlphaLast));

        case PixelFormat::BGRX8:
            return std::make_tuple(8u, 32u, static_cast<CGBitmapInfo>(kCGBitmapByteOrder32Little) | static_cast<CGBitmapInfo>(kCGImageAlphaNoneSkipFirst));

        case PixelFormat::BGRA8:
            if (format.alphaFormat == AlphaPremultiplication::Premultiplied)
                return std::make_tuple(8u, 32u, static_cast<CGBitmapInfo>(kCGBitmapByteOrder32Little) | static_cast<CGBitmapInfo>(kCGImageAlphaPremultipliedFirst));
            return std::make_tuple(8u, 32u, static_cast<CGBitmapInfo>(kCGBitmapByteOrder32Little) | static_cast<CGBitmapInfo>(kCGImageAlphaFirst));

#if ENABLE(PIXEL_FORMAT_RGB10)
        case PixelFormat::RGB10:
#endif
#if ENABLE(PIXEL_FORMAT_RGB10A8)
        case PixelFormat::RGB10A8:
#endif
            break;

#if ENABLE(PIXEL_FORMAT_RGBA16F)
        case PixelFormat::RGBA16F:
            if (format.alphaFormat == AlphaPremultiplication::Premultiplied)
                return std::make_tuple(16u, 64u, static_cast<CGBitmapInfo>(kCGBitmapByteOrder16Host) | static_cast<CGBitmapInfo>(kCGBitmapFloatComponents) | static_cast<CGBitmapInfo>(kCGImageAlphaPremultipliedLast));
            return std::make_tuple(16u, 64u, static_cast<CGBitmapInfo>(kCGBitmapByteOrder16Host) | static_cast<CGBitmapInfo>(kCGBitmapFloatComponents) | static_cast<CGBitmapInfo>(kCGImageAlphaLast));
#endif
        }

        // We do not support conversions to or from the 10-bit-per-component formats.

        ASSERT_NOT_REACHED();
        return std::make_tuple(8u, 32u, static_cast<CGBitmapInfo>(kCGBitmapByteOrder32Little) | static_cast<CGBitmapInfo>(kCGImageAlphaFirst));
    }(format);

    vImage_CGImageFormat result;

    result.bitsPerComponent = bitsPerComponent;
    result.bitsPerPixel = bitsPerPixel;
    result.colorSpace = format.colorSpace.platformColorSpace();
    result.bitmapInfo = bitmapInfo;
    result.version = 0;
    result.decode = nullptr;
    result.renderingIntent = kCGRenderingIntentDefault;

    return result;
}

template<typename View>
static vImage_Buffer NODELETE makeVImageBuffer(const View& view, const IntSize& size)
{
    vImage_Buffer result;

    result.height = static_cast<vImagePixelCount>(size.height());
    result.width = static_cast<vImagePixelCount>(size.width());
    result.rowBytes = view.bytesPerRow;
    result.data = const_cast<uint8_t*>(view.rows.data());

    return result;
}

static bool convertImagePixelsAcceleratedAnyToAny(const ConstPixelBufferConversionView& source, const PixelBufferConversionView& destination, const IntSize& destinationSize)
{
    // FIXME: Consider using vImageConvert_AnyToAny for all conversions, not just ones that need a color space
    // or component size conversion, after judiciously performance testing them against each other.

    auto sourceCGImageFormat = makeVImageCGImageFormat(source.format);
    auto destinationCGImageFormat = makeVImageCGImageFormat(destination.format);

    vImage_Error converterCreateError = kvImageNoError;
    RetainPtr converter = adoptCF(vImageConverter_CreateWithCGImageFormat(&sourceCGImageFormat, &destinationCGImageFormat, nullptr, kvImageNoFlags, &converterCreateError));
    if (converterCreateError != kvImageNoError) {
        RELEASE_LOG_ERROR(Images, "%s: vImageConverter_CreateWithCGImageFormat() failed with error: %zd", __FUNCTION__, converterCreateError);
        return false;
    }

    auto sourceVImageBuffer = makeVImageBuffer(source, destinationSize);
    auto destinationVImageBuffer = makeVImageBuffer(destination, destinationSize);

    vImage_Error converterConvertError = vImageConvert_AnyToAny(converter.get(), &sourceVImageBuffer, &destinationVImageBuffer, nullptr, kvImageNoFlags);
    if (converterConvertError != kvImageNoError) {
        RELEASE_LOG_ERROR(Images, "%s: vImageConvert_AnyToAny() failed with error: %zd", __FUNCTION__, converterConvertError);
        return false;
    }

    return true;
}

static bool convertImagePixelsAcceleratedMatchingSize(const ConstPixelBufferConversionView& sourceView, const PixelBufferConversionView& destinationView, const IntSize& destinationSize)
{
    auto sourceVImageBuffer = makeVImageBuffer(sourceView, destinationSize);
    auto destinationVImageBuffer = makeVImageBuffer(destinationView, destinationSize);

    bool swapComponentOrder = pixelComponentOrder(sourceView.format.pixelFormat) != pixelComponentOrder(destinationView.format.pixelFormat);
    auto sourceAlphaFormat = toAlphaFormat(sourceView.format.alphaFormat, sourceView.format.pixelFormat);
    auto destinationAlphaFormat = toAlphaFormat(destinationView.format.alphaFormat, destinationView.format.pixelFormat);

    if (sourceAlphaFormat == AlphaFormat::Opaque) {
        // The component an opaque source has in place of alpha holds no meaningful value, so it must
        // not be carried over. Insert an opaque alpha and possibly reorder.
        constexpr std::array<uint8_t, 4> identityMap { 0, 1, 2, 3 };
        constexpr std::array<uint8_t, 4> swappedMap { 2, 1, 0, 3 };
        constexpr uint8_t lastChannelMask = 0x1; // 0x8 is the first of the four channels, 0x1 the last.
        constexpr std::array<uint8_t, 4> opaqueAlpha { 0, 0, 0, 255 };
        vImagePermuteChannelsWithMaskedInsert_ARGB8888(&sourceVImageBuffer, &destinationVImageBuffer, swapComponentOrder ? swappedMap.data() : identityMap.data(), lastChannelMask, opaqueAlpha.data(), kvImageNoFlags);
        return true;
    }

    if (isAlphaApplied(sourceAlphaFormat) != isAlphaApplied(destinationAlphaFormat)) {
        bool shouldUnpremultiply = !isAlphaApplied(destinationAlphaFormat);
        switch (sourceView.format.pixelFormat) {
#if ENABLE(PIXEL_FORMAT_RGBA16F)
        case PixelFormat::RGBA16F:
            if (shouldUnpremultiply)
                vImageUnpremultiplyData_RGBA16F(&sourceVImageBuffer, &destinationVImageBuffer, kvImageNoFlags);
            else
                vImagePremultiplyData_RGBA16F(&sourceVImageBuffer, &destinationVImageBuffer, kvImageNoFlags);
            break;
#endif
        case PixelFormat::RGBA8:
            if (shouldUnpremultiply)
                vImageUnpremultiplyData_RGBA8888(&sourceVImageBuffer, &destinationVImageBuffer, kvImageNoFlags);
            else
                vImagePremultiplyData_RGBA8888(&sourceVImageBuffer, &destinationVImageBuffer, kvImageNoFlags);
            break;
        default:
            ASSERT(sourceView.format.pixelFormat == PixelFormat::BGRA8);
            if (shouldUnpremultiply)
                vImageUnpremultiplyData_BGRA8888(&sourceVImageBuffer, &destinationVImageBuffer, kvImageNoFlags);
            else
                vImagePremultiplyData_BGRA8888(&sourceVImageBuffer, &destinationVImageBuffer, kvImageNoFlags);
            break;
        }

        sourceVImageBuffer = destinationVImageBuffer;
    }

    if (swapComponentOrder) {
        constexpr std::array<uint8_t, 4> map { 2, 1, 0, 3 };
        vImagePermuteChannels_ARGB8888(&sourceVImageBuffer, &destinationVImageBuffer, map.data(), kvImageNoFlags);
    }

    return true;
}

static bool platformConvertImagePixels(const ConstPixelBufferConversionView& source, const PixelBufferConversionView& destination, const IntSize& destinationSize)
{
    if (source.format.colorSpace == destination.format.colorSpace
        && PixelBuffer::bytesPerPixelComponent(source.format.pixelFormat) == PixelBuffer::bytesPerPixelComponent(destination.format.pixelFormat))
        return convertImagePixelsAcceleratedMatchingSize(source, destination, destinationSize);

    return convertImagePixelsAcceleratedAnyToAny(source, destination, destinationSize);
}

#elif USE(SKIA)

static bool convertImagePixelsSkia(const ConstPixelBufferConversionView& source, const PixelBufferConversionView& destination, const IntSize& destinationSize)
{
    auto toSkiaColorType = [](const PixelFormat& pixelFormat) -> std::optional<SkColorType> {
        switch (pixelFormat) {
        case PixelFormat::RGBA8:
            return SkColorType::kRGBA_8888_SkColorType;
        case PixelFormat::BGRA8:
            return SkColorType::kBGRA_8888_SkColorType;
        default:
            break;
        }
        return std::nullopt;
    };
    auto toSkiaAlphaType = [](const AlphaPremultiplication& alphaFormat) {
        switch (alphaFormat) {
        case AlphaPremultiplication::Premultiplied:
            return SkAlphaType::kPremul_SkAlphaType;
        case AlphaPremultiplication::Unpremultiplied:
            return SkAlphaType::kUnpremul_SkAlphaType;
        }
        ASSERT_NOT_REACHED();
        return SkAlphaType::kUnknown_SkAlphaType;
    };
    auto sourceSkiaColorType = toSkiaColorType(source.format.pixelFormat);
    if (!sourceSkiaColorType)
        return false;
    SkImageInfo sourceImageInfo = SkImageInfo::Make(
        destinationSize.width(),
        destinationSize.height(),
        *sourceSkiaColorType,
        toSkiaAlphaType(source.format.alphaFormat),
        source.format.colorSpace.platformColorSpace()
    );
    auto destinationSkiaColorType = toSkiaColorType(destination.format.pixelFormat);
    if (!destinationSkiaColorType)
        return false;
    // Utilize SkPixmap which is a raw bytes wrapper capable of performing conversions.
    SkPixmap sourcePixmap(sourceImageInfo, source.rows.data(), source.bytesPerRow);
    SkImageInfo destinationImageInfo = SkImageInfo::Make(
        destinationSize.width(),
        destinationSize.height(),
        *destinationSkiaColorType,
        toSkiaAlphaType(destination.format.alphaFormat),
        destination.format.colorSpace.platformColorSpace()
    );
    // Read pixels from source to destination and convert pixels if necessary.
    sourcePixmap.readPixels(destinationImageInfo, destination.rows.data(), destination.bytesPerRow);
    return true;
}

#endif // USE(SKIA)

#if !(USE(ACCELERATE) && USE(CG))

#if ENABLE(PIXEL_FORMAT_RGBA16F)
#error "PixelFormat::RGBA16F unimplemented."
#endif

static constexpr uint8_t NODELETE premultiply(uint8_t unpremultiplied, uint8_t alpha)
{
    // Same as vImagePremultiplyData_ARGB8888: (src * alpha + 127) / 255
    return (unpremultiplied * alpha + 127) / 255;
}

static constexpr uint8_t NODELETE unpremultiply(uint8_t premultiplied, uint8_t alpha)
{
    // Same as vImageUnpremultiplyData_RGBA8888: (MIN(src_color, alpha) * 255 + alpha/2) / alpha
    return (std::min(premultiplied, alpha) * 255 + alpha / 2) / alpha;
}

template <AlphaFormat sourceAlphaFormat, AlphaFormat destinationAlphaFormat, bool swapComponentOrder>
static bool NODELETE convertImagePixelsUnacceleratedFunction(const ConstPixelBufferConversionView& sourceView, const PixelBufferConversionView& destinationView, const IntSize& destinationSize)
{
    // The caller has established that both formats are 8 bits per component, 4 bytes per pixel, with
    // the alpha (or the ignored component that takes its place) in the last byte.
    constexpr bool sourceAlphaApplied = isAlphaApplied(sourceAlphaFormat);
    constexpr bool destinationAlphaApplied = isAlphaApplied(destinationAlphaFormat);

    size_t bytesPerRow = destinationSize.width() * 4;
    for (int y = 0; y < destinationSize.height(); ++y) {
        auto sourceRow = sourceView.rows.subspan(sourceView.bytesPerRow * y);
        auto destinationRow = destinationView.rows.subspan(destinationView.bytesPerRow * y);
        for (size_t x = 0; x < bytesPerRow; x += 4) {
            uint8_t alpha;
            if constexpr (sourceAlphaFormat == AlphaFormat::Opaque)
                alpha = 255;
            else
                alpha = sourceRow[x + 3];

            uint8_t c0, c1, c2;
            if constexpr (!sourceAlphaApplied && !destinationAlphaApplied) {
                c0 = sourceRow[x + 0];
                c1 = sourceRow[x + 1];
                c2 = sourceRow[x + 2];
            } else if constexpr (sourceAlphaApplied && destinationAlphaApplied) {
                if (!alpha) {
                    c0 = 0;
                    c1 = 0;
                    c2 = 0;
                } else {
                    c0 = sourceRow[x + 0];
                    c1 = sourceRow[x + 1];
                    c2 = sourceRow[x + 2];
                }
            } else if constexpr (sourceAlphaApplied && !destinationAlphaApplied) {
                if (!alpha) {
                    c0 = 0;
                    c1 = 0;
                    c2 = 0;
                } else if (alpha == 255) {
                    c0 = sourceRow[x + 0];
                    c1 = sourceRow[x + 1];
                    c2 = sourceRow[x + 2];
                } else {
                    c0 = unpremultiply(sourceRow[x + 0], alpha);
                    c1 = unpremultiply(sourceRow[x + 1], alpha);
                    c2 = unpremultiply(sourceRow[x + 2], alpha);
                }
            } else {
                static_assert(!sourceAlphaApplied && destinationAlphaApplied);
                if (!alpha) {
                    c0 = 0;
                    c1 = 0;
                    c2 = 0;
                } else if (alpha == 255) {
                    c0 = sourceRow[x + 0];
                    c1 = sourceRow[x + 1];
                    c2 = sourceRow[x + 2];
                } else {
                    c0 = premultiply(sourceRow[x + 0], alpha);
                    c1 = premultiply(sourceRow[x + 1], alpha);
                    c2 = premultiply(sourceRow[x + 2], alpha);
                }
            }

            if constexpr (!swapComponentOrder) {
                destinationRow[x + 0] = c0;
                destinationRow[x + 1] = c1;
                destinationRow[x + 2] = c2;
            } else {
                destinationRow[x + 0] = c2;
                destinationRow[x + 1] = c1;
                destinationRow[x + 2] = c0;
            }

            if constexpr (destinationAlphaFormat != AlphaFormat::Opaque)
                destinationRow[x + 3] = alpha;
            else
                destinationRow[x + 3] = 255; // Not strictly necessary, but this prevent exposing uninitialized memory.
        }
    }

    return true;
}

template <bool swapComponentOrder>
static bool convertImagePixelsUnacceleratedSelectAlphaFormats(AlphaFormat sourceAlphaFormat, AlphaFormat destinationAlphaFormat, const ConstPixelBufferConversionView& source, const PixelBufferConversionView& destination, const IntSize& destinationSize)
{
    using enum AlphaFormat;

    switch (sourceAlphaFormat) {
    case Opaque:
        switch (destinationAlphaFormat) {
        case Opaque:
            return convertImagePixelsUnacceleratedFunction<Opaque, Opaque, swapComponentOrder>(source, destination, destinationSize);
        case Unpremultiplied:
            return convertImagePixelsUnacceleratedFunction<Opaque, Unpremultiplied, swapComponentOrder>(source, destination, destinationSize);
        case Premultiplied:
            return convertImagePixelsUnacceleratedFunction<Opaque, Premultiplied, swapComponentOrder>(source, destination, destinationSize);
        }
        break;
    case Unpremultiplied:
        switch (destinationAlphaFormat) {
        case Opaque:
            return convertImagePixelsUnacceleratedFunction<Unpremultiplied, Opaque, swapComponentOrder>(source, destination, destinationSize);
        case Unpremultiplied:
            return convertImagePixelsUnacceleratedFunction<Unpremultiplied, Unpremultiplied, swapComponentOrder>(source, destination, destinationSize);
        case Premultiplied:
            return convertImagePixelsUnacceleratedFunction<Unpremultiplied, Premultiplied, swapComponentOrder>(source, destination, destinationSize);
        }
        break;
    case Premultiplied:
        switch (destinationAlphaFormat) {
        case Opaque:
            return convertImagePixelsUnacceleratedFunction<Premultiplied, Opaque, swapComponentOrder>(source, destination, destinationSize);
        case Unpremultiplied:
            return convertImagePixelsUnacceleratedFunction<Premultiplied, Unpremultiplied, swapComponentOrder>(source, destination, destinationSize);
        case Premultiplied:
            return convertImagePixelsUnacceleratedFunction<Premultiplied, Premultiplied, swapComponentOrder>(source, destination, destinationSize);
        }
        break;
    }

    ASSERT_NOT_REACHED();
    return false;
}

static bool platformConvertImagePixels(const ConstPixelBufferConversionView& source, const PixelBufferConversionView& destination, const IntSize& destinationSize)
{
#if USE(SKIA)
    if (convertImagePixelsSkia(source, destination, destinationSize))
        return true;
#endif

    // FIXME: We don't currently support converting pixel data between different color spaces in the non-accelerated path.
    // This could be added using conversion functions from ColorConversion.h.
    if (source.format.colorSpace != destination.format.colorSpace)
        return false;

    // Without ACCELERATE and CG there are no 16-bits-per-component formats, so every supported format
    // is 8 bits per component and 4 bytes per pixel, which is what the conversion loop assumes.
    ASSERT(PixelBuffer::bytesPerPixel(source.format.pixelFormat) == 4);
    ASSERT(PixelBuffer::bytesPerPixel(destination.format.pixelFormat) == 4);

    auto sourceAlphaFormat = toAlphaFormat(source.format.alphaFormat, source.format.pixelFormat);
    auto destinationAlphaFormat = toAlphaFormat(destination.format.alphaFormat, destination.format.pixelFormat);
    if (pixelComponentOrder(source.format.pixelFormat) != pixelComponentOrder(destination.format.pixelFormat))
        return convertImagePixelsUnacceleratedSelectAlphaFormats<true>(sourceAlphaFormat, destinationAlphaFormat, source, destination, destinationSize);
    return convertImagePixelsUnacceleratedSelectAlphaFormats<false>(sourceAlphaFormat, destinationAlphaFormat, source, destination, destinationSize);
}
#endif // !(USE(ACCELERATE) && USE(CG))

// The rows the conversion reads and writes have to be inside the buffer the view describes.
template<typename View>
static bool NODELETE hasEnoughBytesForConversion(const View& view, const IntSize& destinationSize)
{
    if (destinationSize.width() <= 0 || destinationSize.height() <= 0)
        return true;

    CheckedSize requiredBytes = CheckedSize { static_cast<size_t>(destinationSize.height() - 1) } * view.bytesPerRow;
    requiredBytes += CheckedSize { static_cast<size_t>(destinationSize.width()) } * PixelBuffer::bytesPerPixel(view.format.pixelFormat);
    return !requiredBytes.hasOverflowed() && view.rows.size_bytes() >= requiredBytes.value();
}

static void zeroImagePixels(const PixelBufferConversionView& destination, const IntSize& destinationSize)
{
    size_t rowFillBytes = static_cast<size_t>(destinationSize.width()) * PixelBuffer::bytesPerPixel(destination.format.pixelFormat);
    for (int y = 0; y < destinationSize.height(); ++y)
        zeroSpan(destination.rows.subspan(static_cast<size_t>(y) * destination.bytesPerRow, rowFillBytes));
}

// Whether the contents can be copied verbatim, i.e. the two formats store the same components at
// the same offsets, and the components already relate to the alpha the way the destination
// describes. An opaque destination drops the alpha of the source, which gives the right color only
// if the source has applied it to its color components already.
static bool canCopyPixels(const PixelBufferFormat& sourceFormat, const PixelBufferFormat& destinationFormat)
{
    auto sourceAlphaFormat = toAlphaFormat(sourceFormat.alphaFormat, sourceFormat.pixelFormat);
    auto destinationAlphaFormat = toAlphaFormat(destinationFormat.alphaFormat, destinationFormat.pixelFormat);
#if USE(SKIA)
    // Skia has no native BGRX color type, so the component a BGRX8 destination has in place of
    // alpha is not ignored. Only contents that are opaque themselves can be copied into it.
    if (destinationFormat.pixelFormat == PixelFormat::BGRX8 && sourceAlphaFormat != AlphaFormat::Opaque)
        return false;
#endif
    return sourceFormat.colorSpace == destinationFormat.colorSpace
        && PixelBuffer::bytesPerPixelComponent(sourceFormat.pixelFormat) == PixelBuffer::bytesPerPixelComponent(destinationFormat.pixelFormat)
        && pixelComponentOrder(sourceFormat.pixelFormat) == pixelComponentOrder(destinationFormat.pixelFormat)
        && (sourceAlphaFormat == destinationAlphaFormat || (sourceAlphaFormat == AlphaFormat::Premultiplied && destinationAlphaFormat == AlphaFormat::Opaque));
}

void convertImagePixels(const ConstPixelBufferConversionView& source, const PixelBufferConversionView& destination, const IntSize& destinationSize)
{
    // We currently only support converting between RGBA8, BGRA8, RGBX8, BGRX8, and (where enabled) RGBA16F.
    ASSERT(isSupportedConversionFormat(source.format.pixelFormat));
    ASSERT(isSupportedConversionFormat(destination.format.pixelFormat));

    RELEASE_ASSERT(hasEnoughBytesForConversion(source, destinationSize), "Source buffer is too small for the requested conversion");
    RELEASE_ASSERT(hasEnoughBytesForConversion(destination, destinationSize), "Destination buffer is too small for the requested conversion");

    if (isSupportedConversionFormat(source.format.pixelFormat) && isSupportedConversionFormat(destination.format.pixelFormat)) {
        if (canCopyPixels(source.format, destination.format)) {
            copyRowsInternal(source.bytesPerRow, source.rows, destination.bytesPerRow, destination.rows, destinationSize.height(), destinationSize.width() * PixelBuffer::bytesPerPixel(destination.format.pixelFormat));
            return;
        }

        if (platformConvertImagePixels(source, destination, destinationSize))
            return;
    }

    zeroImagePixels(destination, destinationSize);
}

void copyRowsInternal(unsigned sourceBytesPerRow, std::span<const uint8_t> source, unsigned destinationBytesPerRow, std::span<uint8_t> destination, unsigned rows, unsigned copyBytesPerRow)
{
    if (sourceBytesPerRow == destinationBytesPerRow && copyBytesPerRow == sourceBytesPerRow)
        memcpySpan(destination, source.first(copyBytesPerRow * rows));
    else {
        for (unsigned row = 0; row < rows; ++row) {
            memcpySpan(destination, source.first(copyBytesPerRow));
            if (sourceBytesPerRow > source.size() || destinationBytesPerRow > destination.size())
                break;
            skip(source, sourceBytesPerRow);
            skip(destination, destinationBytesPerRow);
        }
    }
}

}
