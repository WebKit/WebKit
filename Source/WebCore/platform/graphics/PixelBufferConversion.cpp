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
#include "PixelFormat.h"
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

#if USE(ACCELERATE) && USE(CG)

static inline vImage_CGImageFormat makeVImageCGImageFormat(const PixelBufferFormat& format)
{
    auto [bitsPerComponent, bitsPerPixel, bitmapInfo] = [] (const PixelBufferFormat& format) -> std::tuple<unsigned, unsigned, CGBitmapInfo> {
        switch (format.pixelFormat) {
        case PixelFormat::RGBA8:
            if (format.alphaFormat == AlphaPremultiplication::Premultiplied)
                return std::make_tuple(8u, 32u, static_cast<CGBitmapInfo>(kCGBitmapByteOrder32Big) | static_cast<CGBitmapInfo>(kCGImageAlphaPremultipliedLast));
            else
                return std::make_tuple(8u, 32u, static_cast<CGBitmapInfo>(kCGBitmapByteOrder32Big) | static_cast<CGBitmapInfo>(kCGImageAlphaLast));

        case PixelFormat::BGRA8:
            if (format.alphaFormat == AlphaPremultiplication::Premultiplied)
                return std::make_tuple(8u, 32u, static_cast<CGBitmapInfo>(kCGBitmapByteOrder32Little) | static_cast<CGBitmapInfo>(kCGImageAlphaPremultipliedFirst));
            else
                return std::make_tuple(8u, 32u, static_cast<CGBitmapInfo>(kCGBitmapByteOrder32Little) | static_cast<CGBitmapInfo>(kCGImageAlphaFirst));

        case PixelFormat::BGRX8:
#if ENABLE(PIXEL_FORMAT_RGB10)
        case PixelFormat::RGB10:
#endif
#if ENABLE(PIXEL_FORMAT_RGB10A8)
        case PixelFormat::RGB10A8:
#endif
#if ENABLE(PIXEL_FORMAT_RGBA16F)
        case PixelFormat::RGBA16F:
#endif
            break;
        }

        // We currently only support 8 bit pixel formats with alpha for these conversions.

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

template<typename View> static vImage_Buffer makeVImageBuffer(const View& view, const IntSize& size)
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

    if (source.format.colorSpace != destination.format.colorSpace) {
        // FIXME: Consider using vImageConvert_AnyToAny for all conversions, not just ones that need a color space conversion,
        // after judiciously performance testing them against each other.

        auto sourceCGImageFormat = makeVImageCGImageFormat(source.format);
        auto destinationCGImageFormat = makeVImageCGImageFormat(destination.format);

        vImage_Error converterCreateError = kvImageNoError;
        auto converter = adoptCF(vImageConverter_CreateWithCGImageFormat(&sourceCGImageFormat, &destinationCGImageFormat, nullptr, kvImageNoFlags, &converterCreateError));
        if (converterCreateError != kvImageNoError)
            return;

        vImage_Error converterConvertError = vImageConvert_AnyToAny(converter.get(), &sourceVImageBuffer, &destinationVImageBuffer, nullptr, kvImageNoFlags);
        ASSERT_WITH_MESSAGE_UNUSED(converterConvertError, converterConvertError == kvImageNoError, "vImageConvert_AnyToAny failed conversion with error: %zd", converterConvertError);
        return;
    }

    if (source.format.alphaFormat != destination.format.alphaFormat) {
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
        // Swap pixel channels BGRA <-> RGBA.
        const uint8_t map[4] = { 2, 1, 0, 3 };
        vImagePermuteChannels_ARGB8888(&sourceVImageBuffer, &destinationVImageBuffer, map, kvImageNoFlags);
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
static void convertSinglePixelPremultipliedToPremultiplied(std::span<const uint8_t, 4> sourcePixel, std::span<uint8_t, 4> destinationPixel)
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
static void convertSinglePixelUnpremultipliedToUnpremultiplied(std::span<const uint8_t, 4> sourcePixel, std::span<uint8_t, 4> destinationPixel)
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
static void convertImagePixelsUnaccelerated(const ConstPixelBufferConversionView& source, const PixelBufferConversionView& destination, const IntSize& destinationSize)
{
    size_t bytesPerRow = destinationSize.width() * 4;
    for (int y = 0; y < destinationSize.height(); ++y) {
        auto sourceRow = source.rows.subspan(source.bytesPerRow * y);
        auto destinationRow = destination.rows.subspan(destination.bytesPerRow * y);
        for (size_t x = 0; x < bytesPerRow; x += 4)
            convertFunctor(sourceRow.subspan(x).subspan<0, 4>(), destinationRow.subspan(x).subspan<0, 4>());
    }
}

#if !(USE(ACCELERATE) && USE(CG))
static void copyImagePixels(const ConstPixelBufferConversionView& source, const PixelBufferConversionView& destination, const IntSize& destinationSize)
{
    size_t bytesPerRow = destinationSize.width() * 4;

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

#if ENABLE(PIXEL_FORMAT_RGBA16F)
static Float16 readFloat16(const std::span<const uint8_t>& span8, size_t offset)
{
    union {
        Float16 float16 { };
        std::array<uint8_t, sizeof(Float16)> bytes;
    } float16OrBytesUnion;
    for (size_t i = 0; i < sizeof(Float16); ++i)
        float16OrBytesUnion.bytes[i] = span8[offset + i];
    return float16OrBytesUnion.float16;
}

static void writeFloat16(Float16 f16, const std::span<uint8_t>& spanFloat16, size_t offset)
{
    union {
        Float16 float16 { };
        std::array<uint8_t, sizeof(Float16)> bytes;
    } float16OrBytesUnion(f16);
    for (size_t i = 0; i < sizeof(Float16); ++i)
        spanFloat16[offset + i] = float16OrBytesUnion.bytes[i];
}

static void convertImagePixelsFromFloat16ToFloat16(const ConstPixelBufferConversionView& source, const PixelBufferConversionView& destination, const IntSize& destinationSize)
{
    if (source.format.colorSpace != destination.format.colorSpace)
        return;

    auto sourceBytes = source.rows.size_bytes();
    auto sourcePixelComponents = sourceBytes / 2;
    auto sourcePixels = sourcePixelComponents / 4;
    auto sourceHeight = sourceBytes / source.bytesPerRow;
    auto sourceWidth = sourcePixels / sourceHeight;

    auto destinationBytes = destination.rows.size_bytes();
    auto destinationPixelComponents = destinationBytes / 2;
    auto destinationPixels = destinationPixelComponents / 4;
    auto destinationHeight = destinationBytes / destination.bytesPerRow;
    auto destinationWidth = destinationPixels / destinationHeight;

    if (destinationSize.height() >= 0 && size_t(destinationSize.height()) < destinationHeight)
        destinationHeight = size_t(destinationSize.height());
    if (destinationSize.width() >= 0 && size_t(destinationSize.width()) < destinationWidth)
        destinationWidth = size_t(destinationSize.width());

    auto sourceRowStartOffset = 0;
    auto destinationRowStartOffset = 0;
    for (size_t y = 0; y < sourceHeight && y < destinationHeight; ++y) {
        size_t offset = 0;
        for (size_t x = 0; x < sourceWidth && x < destinationWidth; ++x) {
            struct Pixel16 {
                Float16 r = { };
                Float16 g = { };
                Float16 b = { };
                Float16 a = { };
            };
            static_assert(sizeof(Float16) == 2);
            static_assert(sizeof(Pixel16) == 4 * sizeof(Float16));
            union {
                Pixel16 pixel16 { };
                std::array<uint8_t, sizeof(Pixel16)> bytes;
            } pixel16OrBytesUnion;
            for (size_t byte = 0; byte < sizeof(Pixel16); ++byte)
                pixel16OrBytesUnion.bytes[byte] = source.rows[sourceRowStartOffset + offset + byte];
            if (source.format.alphaFormat != destination.format.alphaFormat) {
                if (source.format.alphaFormat == AlphaPremultiplication::Unpremultiplied && destination.format.alphaFormat == AlphaPremultiplication::Premultiplied) {
                    auto fa = float(pixel16OrBytesUnion.pixel16.a);
                    pixel16OrBytesUnion.pixel16.r = Float16(float(pixel16OrBytesUnion.pixel16.r) * fa);
                    pixel16OrBytesUnion.pixel16.g = Float16(float(pixel16OrBytesUnion.pixel16.g) * fa);
                    pixel16OrBytesUnion.pixel16.b = Float16(float(pixel16OrBytesUnion.pixel16.b) * fa);
                } else if (source.format.alphaFormat == AlphaPremultiplication::Premultiplied && destination.format.alphaFormat == AlphaPremultiplication::Unpremultiplied) {
                    if (auto fa = float(pixel16OrBytesUnion.pixel16.a)) {
                        pixel16OrBytesUnion.pixel16.r = Float16(float(pixel16OrBytesUnion.pixel16.r) / fa);
                        pixel16OrBytesUnion.pixel16.g = Float16(float(pixel16OrBytesUnion.pixel16.g) / fa);
                        pixel16OrBytesUnion.pixel16.b = Float16(float(pixel16OrBytesUnion.pixel16.b) / fa);
                    }
                } else
                    RELEASE_ASSERT_NOT_REACHED();
            }
            for (size_t byte = 0; byte < sizeof(Pixel16); ++byte)
                destination.rows[destinationRowStartOffset + offset + byte] = pixel16OrBytesUnion.bytes[byte];
            offset += sizeof(Pixel16);
        }
        sourceRowStartOffset += source.bytesPerRow;
        destinationRowStartOffset += destination.bytesPerRow;
    }
}

static void convertImagePixelsFromFloat16(const ConstPixelBufferConversionView& source, const PixelBufferConversionView& destination, const IntSize& destinationSize)
{
    auto pixelComponents = source.rows.size_bytes() / sizeof(Float16);

    Vector<uint8_t> rgba8;
    rgba8.reserveInitialCapacity(pixelComponents);

    for (size_t i = 0; i < pixelComponents; ++i) {
        auto f16 = readFloat16(source.rows, i * sizeof(Float16));
        float f = float(f16);
        auto u8 = (f <= 0.f) ? uint8_t(0) : ((f >= 1.f) ? uint8_t(255) : uint8_t(f * 255.f + 0.5f));
        rgba8.append(u8);
    }

    ConstPixelBufferConversionView rgba8ConversionView {
        .format = PixelBufferFormat {
            .alphaFormat = source.format.alphaFormat,
            .pixelFormat = PixelFormat::RGBA8,
            .colorSpace = source.format.colorSpace
        },
        .bytesPerRow = source.bytesPerRow / unsigned(sizeof(Float16)),
        .rows = rgba8.span()
    };

    convertImagePixels(rgba8ConversionView, destination, destinationSize);
}

// [[noreturn]]
static void convertImagePixelsToFloat16(const ConstPixelBufferConversionView& source, const PixelBufferConversionView& destination, const IntSize& destinationSize)
{
    auto pixelComponents = destination.rows.size_bytes() / sizeof(Float16);

    Vector<uint8_t> rgba8;
    rgba8.reserveInitialCapacity(pixelComponents);
    rgba8.fill(uint8_t(0), pixelComponents);

    PixelBufferConversionView rgba8ConversionView {
        .format = PixelBufferFormat {
            .alphaFormat = destination.format.alphaFormat,
            .pixelFormat = PixelFormat::RGBA8,
            .colorSpace = destination.format.colorSpace
        },
        .bytesPerRow = destination.bytesPerRow / unsigned(sizeof(Float16)),
        .rows = rgba8.mutableSpan()
    };

    convertImagePixels(source, rgba8ConversionView, destinationSize);

    for (size_t i = 0; i < pixelComponents; ++i) {
        auto u8 = rgba8[i];
        float f = float(u8) / 255.f;
        Float16 f16 = f;
        writeFloat16(f16, destination.rows, i * 2);
    }

}
#endif // ENABLE(PIXEL_FORMAT_RGBA16F)

void convertImagePixels(const ConstPixelBufferConversionView& source, const PixelBufferConversionView& destination, const IntSize& destinationSize)
{
#if ENABLE(PIXEL_FORMAT_RGBA16F)
    auto isSourceFloat = source.format.pixelFormat == PixelFormat::RGBA16F;
    auto isDestinationFloat = destination.format.pixelFormat == PixelFormat::RGBA16F;
    if (isSourceFloat && isDestinationFloat)
        return convertImagePixelsFromFloat16ToFloat16(source, destination, destinationSize);
    if (isSourceFloat)
        return convertImagePixelsFromFloat16(source, destination, destinationSize);
    if (isDestinationFloat)
        return convertImagePixelsToFloat16(source, destination, destinationSize);
#endif // ENABLE(PIXEL_FORMAT_RGBA16F)

    // We currently only support converting between RGBA8, BGRA8, and BGRX8; and on some platforms RGBA16F (see above).
    ASSERT(source.format.pixelFormat == PixelFormat::RGBA8 || source.format.pixelFormat == PixelFormat::BGRA8 || source.format.pixelFormat == PixelFormat::BGRX8);
    ASSERT(destination.format.pixelFormat == PixelFormat::RGBA8 || destination.format.pixelFormat == PixelFormat::BGRA8 || destination.format.pixelFormat == PixelFormat::BGRX8);

#if USE(ACCELERATE) && USE(CG)
    if (source.format.alphaFormat == destination.format.alphaFormat && source.format.pixelFormat == destination.format.pixelFormat && source.format.colorSpace == destination.format.colorSpace) {
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
