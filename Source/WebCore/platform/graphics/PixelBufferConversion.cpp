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
#include "DestinationColorSpace.h"
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

#if ENABLE(PIXEL_FORMAT_RGBA16F) && !(USE(ACCELERATE) && USE(CG))
// PixelFormat::RGBA16F is only enabled under HAVE(SUPPORT_HDR_DISPLAY), which is Cocoa-only, and
// PLATFORM(COCOA) implies both USE(CG) and USE(ACCELERATE). RGBA16F conversions are therefore
// handled entirely by convertImagePixelsAccelerated(), and the unaccelerated single-pixel functions
// below only ever see 8-bits-per-component formats. If RGBA16F is ever enabled somewhere without
// ACCELERATE or CG, those functions need to grow 16-bits-per-component variants.
#error "PixelFormat::RGBA16F requires USE(ACCELERATE) && USE(CG)."
#endif

#if USE(ACCELERATE) && USE(CG)

static inline vImage_CGImageFormat makeVImageCGImageFormat(const PixelBufferFormat& format)
{
    auto [bitsPerComponent, bitsPerPixel, bitmapInfo] = [] (const PixelBufferFormat& format) -> std::tuple<decltype(vImage_CGImageFormat::bitsPerComponent), decltype(vImage_CGImageFormat::bitsPerPixel), CGBitmapInfo> {
        switch (format.pixelFormat) {
        case PixelFormat::RGBA8:
            if (format.alphaFormat == AlphaPremultiplication::Premultiplied)
                return std::make_tuple(8u, 32u, static_cast<CGBitmapInfo>(kCGBitmapByteOrder32Big) | static_cast<CGBitmapInfo>(kCGImageAlphaPremultipliedLast));
            return std::make_tuple(8u, 32u, static_cast<CGBitmapInfo>(kCGBitmapByteOrder32Big) | static_cast<CGBitmapInfo>(kCGImageAlphaLast));

        case PixelFormat::BGRA8:
            if (format.alphaFormat == AlphaPremultiplication::Premultiplied)
                return std::make_tuple(8u, 32u, static_cast<CGBitmapInfo>(kCGBitmapByteOrder32Little) | static_cast<CGBitmapInfo>(kCGImageAlphaPremultipliedFirst));
            return std::make_tuple(8u, 32u, static_cast<CGBitmapInfo>(kCGBitmapByteOrder32Little) | static_cast<CGBitmapInfo>(kCGImageAlphaFirst));

        case PixelFormat::BGRX8:
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

        // We currently only support 8- and 16-bit pixel formats with alpha for these conversions.

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

template<typename View> static vImage_Buffer NODELETE makeVImageBuffer(const View& view, const IntSize& size)
{
    vImage_Buffer result;

    result.height = static_cast<vImagePixelCount>(size.height());
    result.width = static_cast<vImagePixelCount>(size.width());
    result.rowBytes = view.bytesPerRow;
    result.data = const_cast<uint8_t*>(view.rows.data());

    return result;
}

static void convertImagePixelsAccelerated(const ConstPixelBufferConversionView& source, const PixelBufferConversionView& destination, const IntSize& destinationSize)
{
    auto sourceVImageBuffer = makeVImageBuffer(source, destinationSize);
    auto destinationVImageBuffer = makeVImageBuffer(destination, destinationSize);

    auto zeroFillDestination = [&] {
        size_t rowFillBytes = static_cast<size_t>(destinationSize.width()) * PixelBuffer::bytesPerPixel(destination.format.pixelFormat);
        for (int y = 0; y < destinationSize.height(); ++y)
            zeroSpan(destination.rows.subspan(static_cast<size_t>(y) * destination.bytesPerRow, rowFillBytes));
    };

    if (source.format.colorSpace != destination.format.colorSpace
        || PixelBuffer::bytesPerPixelComponent(source.format.pixelFormat) != PixelBuffer::bytesPerPixelComponent(destination.format.pixelFormat)) {
        // FIXME: Consider using vImageConvert_AnyToAny for all conversions, not just ones that need a color space
        // or component size conversion, after judiciously performance testing them against each other.

        auto sourceCGImageFormat = makeVImageCGImageFormat(source.format);
        auto destinationCGImageFormat = makeVImageCGImageFormat(destination.format);

        vImage_Error converterCreateError = kvImageNoError;
        auto converter = adoptCF(vImageConverter_CreateWithCGImageFormat(&sourceCGImageFormat, &destinationCGImageFormat, nullptr, kvImageNoFlags, &converterCreateError));
        if (converterCreateError != kvImageNoError) {
            RELEASE_LOG_ERROR(Images, "%s: vImageConverter_CreateWithCGImageFormat() failed with error: %zd", __FUNCTION__, converterCreateError);
            // The destination may be uninitialized; ensure no stale heap is exposed to callers.
            zeroFillDestination();
            return;
        }

        vImage_Error converterConvertError = vImageConvert_AnyToAny(converter.get(), &sourceVImageBuffer, &destinationVImageBuffer, nullptr, kvImageNoFlags);
        if (converterConvertError != kvImageNoError) {
            RELEASE_LOG_ERROR(Images, "%s: vImageConvert_AnyToAny() failed with error: %zd", __FUNCTION__, converterConvertError);
            // The destination may be uninitialized; ensure no stale heap is exposed to callers.
            zeroFillDestination();
        }

        return;
    }

    if (source.format.alphaFormat != destination.format.alphaFormat) {
#if ENABLE(PIXEL_FORMAT_RGBA16F)
        if (source.format.pixelFormat == PixelFormat::RGBA16F) {
            if (destination.format.alphaFormat == AlphaPremultiplication::Unpremultiplied)
                vImageUnpremultiplyData_RGBA16F(&sourceVImageBuffer, &destinationVImageBuffer, kvImageNoFlags);
            else
                vImagePremultiplyData_RGBA16F(&sourceVImageBuffer, &destinationVImageBuffer, kvImageNoFlags);
        } else
#endif
        if (destination.format.alphaFormat == AlphaPremultiplication::Unpremultiplied) {
            if (source.format.pixelFormat == PixelFormat::RGBA8)
                vImageUnpremultiplyData_RGBA8888(&sourceVImageBuffer, &destinationVImageBuffer, kvImageNoFlags);
            else
                vImageUnpremultiplyData_BGRA8888(&sourceVImageBuffer, &destinationVImageBuffer, kvImageNoFlags);
        } else {
            if (source.format.pixelFormat == PixelFormat::RGBA8)
                vImagePremultiplyData_RGBA8888(&sourceVImageBuffer, &destinationVImageBuffer, kvImageNoFlags);
            else
                vImagePremultiplyData_BGRA8888(&sourceVImageBuffer, &destinationVImageBuffer, kvImageNoFlags);
        }

        sourceVImageBuffer = destinationVImageBuffer;
    }

    if (source.format.pixelFormat != destination.format.pixelFormat) {
        ASSERT(source.format.pixelFormat != PixelFormat::RGBA16F && destination.format.pixelFormat != PixelFormat::RGBA16F, "Conversion to/from RGBA16F should have been handled above.");

        // Swap pixel channels BGRA <-> RGBA.
        constexpr std::array<uint8_t, 4> map { 2, 1, 0, 3 };
        vImagePermuteChannels_ARGB8888(&sourceVImageBuffer, &destinationVImageBuffer, map.data(), kvImageNoFlags);
    }
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

#endif

enum class PixelFormatConversion { None, Permute };

template<PixelFormatConversion pixelFormatConversion>
static void NODELETE convertSinglePixelPremultipliedToPremultiplied(std::span<const uint8_t, 4> sourcePixel, std::span<uint8_t, 4> destinationPixel)
{
    uint8_t alpha = sourcePixel[3];
    if (!alpha) {
        reinterpretCastSpanStartTo<uint32_t>(destinationPixel) = 0;
        return;
    }

    if constexpr (pixelFormatConversion == PixelFormatConversion::None)
        reinterpretCastSpanStartTo<uint32_t>(destinationPixel) = reinterpretCastSpanStartTo<const uint32_t>(sourcePixel);
    else {
        // Swap pixel channels BGRA <-> RGBA.
        destinationPixel[0] = sourcePixel[2];
        destinationPixel[1] = sourcePixel[1];
        destinationPixel[2] = sourcePixel[0];
        destinationPixel[3] = sourcePixel[3];
    }
}

template<PixelFormatConversion pixelFormatConversion>
static void convertSinglePixelPremultipliedToUnpremultiplied(std::span<const uint8_t, 4> sourcePixel, std::span<uint8_t, 4> destinationPixel)
{
    uint8_t alpha = sourcePixel[3];
    if (!alpha || alpha == 255) {
        convertSinglePixelPremultipliedToPremultiplied<pixelFormatConversion>(sourcePixel, destinationPixel);
        return;
    }

    if constexpr (pixelFormatConversion == PixelFormatConversion::None) {
        destinationPixel[0] = (sourcePixel[0] * 255) / alpha;
        destinationPixel[1] = (sourcePixel[1] * 255) / alpha;
        destinationPixel[2] = (sourcePixel[2] * 255) / alpha;
        destinationPixel[3] = alpha;
    } else {
        // Swap pixel channels BGRA <-> RGBA.
        destinationPixel[0] = (sourcePixel[2] * 255) / alpha;
        destinationPixel[1] = (sourcePixel[1] * 255) / alpha;
        destinationPixel[2] = (sourcePixel[0] * 255) / alpha;
        destinationPixel[3] = alpha;
    }
}

template<PixelFormatConversion pixelFormatConversion>
static void convertSinglePixelUnpremultipliedToPremultiplied(std::span<const uint8_t, 4> sourcePixel, std::span<uint8_t, 4> destinationPixel)
{
    uint8_t alpha = sourcePixel[3];
    if (!alpha || alpha == 255) {
        convertSinglePixelPremultipliedToPremultiplied<pixelFormatConversion>(sourcePixel, destinationPixel);
        return;
    }

    if constexpr (pixelFormatConversion == PixelFormatConversion::None) {
        destinationPixel[0] = (sourcePixel[0] * alpha + 254) / 255;
        destinationPixel[1] = (sourcePixel[1] * alpha + 254) / 255;
        destinationPixel[2] = (sourcePixel[2] * alpha + 254) / 255;
        destinationPixel[3] = alpha;
    } else {
        // Swap pixel channels BGRA <-> RGBA.
        destinationPixel[0] = (sourcePixel[2] * alpha + 254) / 255;
        destinationPixel[1] = (sourcePixel[1] * alpha + 254) / 255;
        destinationPixel[2] = (sourcePixel[0] * alpha + 254) / 255;
        destinationPixel[3] = alpha;
    }
}

template<PixelFormatConversion pixelFormatConversion>
static void NODELETE convertSinglePixelUnpremultipliedToUnpremultiplied(std::span<const uint8_t, 4> sourcePixel, std::span<uint8_t, 4> destinationPixel)
{
    if constexpr (pixelFormatConversion == PixelFormatConversion::None)
        reinterpretCastSpanStartTo<uint32_t>(destinationPixel) = reinterpretCastSpanStartTo<const uint32_t>(sourcePixel);
    else {
        // Swap pixel channels BGRA <-> RGBA.
        destinationPixel[0] = sourcePixel[2];
        destinationPixel[1] = sourcePixel[1];
        destinationPixel[2] = sourcePixel[0];
        destinationPixel[3] = sourcePixel[3];
    }
}

template<void (*convertFunctor)(std::span<const uint8_t, 4>, std::span<uint8_t, 4>)>
static void NODELETE convertImagePixelsUnaccelerated(const ConstPixelBufferConversionView& source, const PixelBufferConversionView& destination, const IntSize& destinationSize)
{
    size_t bytesPerRow = destinationSize.width() * 4;
    for (int y = 0; y < destinationSize.height(); ++y) {
        auto sourceRow = source.rows.subspan(source.bytesPerRow * y);
        auto destinationRow = destination.rows.subspan(destination.bytesPerRow * y);
        for (size_t x = 0; x < bytesPerRow; x += 4)
            convertFunctor(sourceRow.subspan(x).subspan<0, 4>(), destinationRow.subspan(x).subspan<0, 4>());
    }
}

#if ENABLE(PIXEL_FORMAT_RGBA16F) || !(USE(ACCELERATE) && USE(CG))
static void copyImagePixels(const ConstPixelBufferConversionView& source, const PixelBufferConversionView& destination, const IntSize& destinationSize)
{
    ASSERT(source.format.pixelFormat == destination.format.pixelFormat);
    size_t bytesPerRow = static_cast<size_t>(destinationSize.width()) * PixelBuffer::bytesPerPixel(destination.format.pixelFormat);

    if (bytesPerRow == source.bytesPerRow && bytesPerRow == destination.bytesPerRow) {
        memcpySpan(destination.rows, source.rows.first(bytesPerRow * destinationSize.height()));
        return;
    }

    for (int y = 0; y < destinationSize.height(); ++y) {
        auto sourceRow = source.rows.subspan(source.bytesPerRow * y);
        auto destinationRow = destination.rows.subspan(destination.bytesPerRow * y);
        memcpySpan(destinationRow, sourceRow.first(bytesPerRow));
    }
}
#endif

static bool UNUSED_FUNCTION NODELETE isSupportedConversionFormat(PixelFormat pixelFormat)
{
    switch (pixelFormat) {
    case PixelFormat::RGBA8:
    case PixelFormat::BGRA8:
    case PixelFormat::BGRX8:
#if ENABLE(PIXEL_FORMAT_RGBA16F)
    case PixelFormat::RGBA16F:
#endif
        return true;
    default:
        return false;
    }
}

#if ENABLE(PIXEL_FORMAT_RGBA16F)
template<typename View> static bool NODELETE hasEnoughBytesForConversion(const View& view, const IntSize& destinationSize)
{
    if (destinationSize.width() <= 0 || destinationSize.height() <= 0)
        return true;

    CheckedSize requiredBytes = CheckedSize { static_cast<size_t>(destinationSize.height() - 1) } * view.bytesPerRow;
    requiredBytes += CheckedSize { static_cast<size_t>(destinationSize.width()) } * PixelBuffer::bytesPerPixel(view.format.pixelFormat);
    return !requiredBytes.hasOverflowed() && view.rows.size_bytes() >= requiredBytes.value();
}
#endif

void convertImagePixels(const ConstPixelBufferConversionView& source, const PixelBufferConversionView& destination, const IntSize& destinationSize)
{
    // We currently only support converting between RGBA8, BGRA8, BGRX8, and — where enabled — RGBA16F.
    ASSERT(isSupportedConversionFormat(source.format.pixelFormat));
    ASSERT(isSupportedConversionFormat(destination.format.pixelFormat));

#if ENABLE(PIXEL_FORMAT_RGBA16F)
    if (source.format.pixelFormat == PixelFormat::RGBA16F)
        RELEASE_ASSERT(hasEnoughBytesForConversion(source, destinationSize), "Source buffer is too small for the requested conversion");
    if (destination.format.pixelFormat == PixelFormat::RGBA16F)
        RELEASE_ASSERT(hasEnoughBytesForConversion(destination, destinationSize), "Destination buffer is too small for the requested conversion");
#endif

#if USE(ACCELERATE) && USE(CG)
    bool formatsAreIdentical = source.format.alphaFormat == destination.format.alphaFormat && source.format.pixelFormat == destination.format.pixelFormat && source.format.colorSpace == destination.format.colorSpace;
#if ENABLE(PIXEL_FORMAT_RGBA16F)
    // The single-pixel functors below only handle 8 bits per component, so copy RGBA16F verbatim.
    if (formatsAreIdentical && source.format.pixelFormat == PixelFormat::RGBA16F) {
        copyImagePixels(source, destination, destinationSize);
        return;
    }
#endif
    if (formatsAreIdentical) {
        // FIXME: Can thes both just use per-row memcpy?
        if (source.format.alphaFormat == AlphaPremultiplication::Premultiplied)
            convertImagePixelsUnaccelerated<convertSinglePixelPremultipliedToPremultiplied<PixelFormatConversion::None>>(source, destination, destinationSize);
        else
            convertImagePixelsUnaccelerated<convertSinglePixelUnpremultipliedToUnpremultiplied<PixelFormatConversion::None>>(source, destination, destinationSize);
    } else
        convertImagePixelsAccelerated(source, destination, destinationSize);
#else
    if (source.format.alphaFormat == destination.format.alphaFormat && source.format.pixelFormat == destination.format.pixelFormat && source.format.colorSpace == destination.format.colorSpace) {
        copyImagePixels(source, destination, destinationSize);
        return;
    }
#if USE(SKIA)
    if (convertImagePixelsSkia(source, destination, destinationSize))
        return;
#endif
    // FIXME: We don't currently support converting pixel data between different color spaces in the non-accelerated path.
    // This could be added using conversion functions from ColorConversion.h.
    ASSERT(source.format.colorSpace == destination.format.colorSpace);

    // FIXME: In Linux platform the following paths could be optimized with ORC.

    if (source.format.alphaFormat == destination.format.alphaFormat) {
        if (source.format.pixelFormat == destination.format.pixelFormat) {
            if (source.format.alphaFormat == AlphaPremultiplication::Premultiplied)
                convertImagePixelsUnaccelerated<convertSinglePixelPremultipliedToPremultiplied<PixelFormatConversion::None>>(source, destination, destinationSize);
            else
                convertImagePixelsUnaccelerated<convertSinglePixelUnpremultipliedToUnpremultiplied<PixelFormatConversion::None>>(source, destination, destinationSize);
        } else {
            if (destination.format.alphaFormat == AlphaPremultiplication::Premultiplied)
                convertImagePixelsUnaccelerated<convertSinglePixelPremultipliedToPremultiplied<PixelFormatConversion::Permute>>(source, destination, destinationSize);
            else
                convertImagePixelsUnaccelerated<convertSinglePixelUnpremultipliedToUnpremultiplied<PixelFormatConversion::Permute>>(source, destination, destinationSize);
        }
    } else {
        if (source.format.pixelFormat == destination.format.pixelFormat) {
            if (source.format.alphaFormat == AlphaPremultiplication::Premultiplied)
                convertImagePixelsUnaccelerated<convertSinglePixelPremultipliedToUnpremultiplied<PixelFormatConversion::None>>(source, destination, destinationSize);
            else
                convertImagePixelsUnaccelerated<convertSinglePixelUnpremultipliedToPremultiplied<PixelFormatConversion::None>>(source, destination, destinationSize);
        } else {
            if (destination.format.alphaFormat == AlphaPremultiplication::Premultiplied)
                convertImagePixelsUnaccelerated<convertSinglePixelUnpremultipliedToPremultiplied<PixelFormatConversion::Permute>>(source, destination, destinationSize);
            else
                convertImagePixelsUnaccelerated<convertSinglePixelPremultipliedToUnpremultiplied<PixelFormatConversion::Permute>>(source, destination, destinationSize);
        }
    }
#endif
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
