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

template <auto... values>
struct CountPackedEnums {
    static constexpr unsigned count = sizeof...(values);

    static_assert(count, "There must be at least one enum value.");
    static_assert(std::ranges::equal(std::views::iota(0U, count), std::array { static_cast<unsigned>(values)... }), "Enum values must be 0..(count-1).");
};

using ConvertImagePixelsFunction = bool (*)(const ConstPixelBufferConversionView&, const PixelBufferConversionView&, const IntSize&);

template <typename ConvertImagePixelsFunctionGetter>
class ConvertImagePixelsFunctionTable {
public:
    consteval ConvertImagePixelsFunctionTable() : m_table(createTable()) { }

    constexpr bool invoke(const ConstPixelBufferConversionView& source, const PixelBufferConversionView& destination, const IntSize& destinationSize) const
    {
        unsigned sourceAlpha = static_cast<unsigned>(source.format.alphaFormat);
        if (sourceAlpha >= alphaFormatCount)
            return false;
        unsigned sourcePixelFormat = static_cast<unsigned>(source.format.pixelFormat);
        if (sourcePixelFormat >= pixelFormatCount)
            return false;
        unsigned destinationAlpha = static_cast<unsigned>(destination.format.alphaFormat);
        if (destinationAlpha >= alphaFormatCount)
            return false;
        unsigned destinationPixelFormat = static_cast<unsigned>(destination.format.pixelFormat);
        if (destinationPixelFormat >= pixelFormatCount)
            return false;

        return m_table[sourceAlpha][sourcePixelFormat][destinationAlpha][destinationPixelFormat](source, destination, destinationSize);
    }

private:
    static constexpr unsigned alphaFormatCount = ConvertImagePixelsFunctionGetter::alphaFormatCount;
    static constexpr unsigned pixelFormatCount = ConvertImagePixelsFunctionGetter::pixelFormatCount;

    using TableDepth4 = std::array<ConvertImagePixelsFunction, pixelFormatCount>;
    using TableDepth3 = std::array<TableDepth4, alphaFormatCount>;
    using TableDepth2 = std::array<TableDepth3, pixelFormatCount>;
    using Table = std::array<TableDepth2, alphaFormatCount>;

    template <AlphaPremultiplication sourceAlphaPremultiplication, PixelFormat sourcePixelFormat, AlphaPremultiplication destinationAlphaPremultiplication, PixelFormat destinationPixelFormat>
    static consteval ConvertImagePixelsFunction selectTableFunction()
    {
        // gcc error on the static_assert: "the address of
        // ‘bool WebCore::convertImagePixelsUnacceleratedFunction(const ConstPixelBufferConversionView&, const PixelBufferConversionView&, const IntSize&) [with AlphaPremultiplication sourceAlphaPremultiplication = AlphaPremultiplication::Unpremultiplied; PixelFormat sourcePixelFormat = PixelFormat::BGRX8; AlphaPremultiplication destinationAlphaPremultiplication = AlphaPremultiplication::Unpremultiplied; PixelFormat destinationPixelFormat = PixelFormat::RGBA8]’
        // will never be null [-Werror=address]"
        // But that's just looking at a single instantiation which obviously returns a non-null function pointer in that case.
        // We still want to make sure that stays true for all instantiations if the external ConvertImagePixelsFunctionGetter ever changes.
IGNORE_GCC_WARNINGS_BEGIN("address")
        static_assert(ConvertImagePixelsFunctionGetter::template selectFunction<sourceAlphaPremultiplication, sourcePixelFormat, destinationAlphaPremultiplication, destinationPixelFormat>(), "A non-null ConvertImagePixelsFunction must be provided for all combinations of template arguments");
IGNORE_GCC_WARNINGS_END
        return ConvertImagePixelsFunctionGetter::template selectFunction<sourceAlphaPremultiplication, sourcePixelFormat, destinationAlphaPremultiplication, destinationPixelFormat>();
    }

    template <AlphaPremultiplication sourceAlphaPremultiplication, PixelFormat sourcePixelFormat, AlphaPremultiplication destinationAlphaPremultiplication, uint8_t... is>
    static consteval TableDepth4 createTableDepth4(std::integer_sequence<uint8_t, is...>)
    {
        TableDepth4 table;
        ((table[is] = selectTableFunction<sourceAlphaPremultiplication, sourcePixelFormat, destinationAlphaPremultiplication, static_cast<PixelFormat>(is)>()), ...);
        return table;
    }

    template <AlphaPremultiplication sourceAlphaPremultiplication, PixelFormat sourcePixelFormat, uint8_t... is>
    static consteval TableDepth3 createTableDepth3(std::integer_sequence<uint8_t, is...>)
    {
        TableDepth3 table;
        ((table[is] = createTableDepth4<sourceAlphaPremultiplication, sourcePixelFormat, static_cast<AlphaPremultiplication>(is) >(std::make_integer_sequence<uint8_t, pixelFormatCount>())), ...);
        return table;
    }

    template <AlphaPremultiplication sourceAlphaPremultiplication, uint8_t... is>
    static consteval TableDepth2 createTableDepth2(std::integer_sequence<uint8_t, is...>)
    {
        TableDepth2 table;
        ((table[is] = createTableDepth3<sourceAlphaPremultiplication, static_cast<PixelFormat>(is)>(std::make_integer_sequence<uint8_t, alphaFormatCount>())), ...);
        return table;
    }

    template <uint8_t... is>
    static consteval Table createTableDepth1(std::integer_sequence<uint8_t, is...>)
    {
        Table table;
        ((table[is] = createTableDepth2<static_cast<AlphaPremultiplication>(is)>(std::make_integer_sequence<uint8_t, pixelFormatCount>())), ...);
        return table;
    }

    static consteval Table createTable()
    {
        return createTableDepth1(std::make_integer_sequence<uint8_t, alphaFormatCount>());
    }

    Table m_table;
};

static bool NODELETE copyImagePixels(const ConstPixelBufferConversionView& source, const PixelBufferConversionView& destination, const IntSize& destinationSize)
{
    ASSERT(source.format.pixelFormat == destination.format.pixelFormat || (source.format.pixelFormat == PixelFormat::BGRA8 && destination.format.pixelFormat == PixelFormat::BGRX8));
    size_t bytesPerRow = static_cast<size_t>(destinationSize.width()) * PixelBuffer::bytesPerPixel(destination.format.pixelFormat);

    if (bytesPerRow == source.bytesPerRow && bytesPerRow == destination.bytesPerRow) {
        memcpySpan(destination.rows, source.rows.first(bytesPerRow * destinationSize.height()));
        return true;
    }

    for (int y = 0; y < destinationSize.height(); ++y) {
        auto sourceRow = source.rows.subspan(source.bytesPerRow * y);
        auto destinationRow = destination.rows.subspan(destination.bytesPerRow * y);
        memcpySpan(destinationRow, sourceRow.first(bytesPerRow));
    }
    return true;
}

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
            return std::make_tuple(8u, 32u, static_cast<CGBitmapInfo>(kCGBitmapByteOrder32Little) | static_cast<CGBitmapInfo>(kCGImageAlphaNoneSkipFirst));

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

template <PixelFormat sourcePixelFormat, AlphaPremultiplication sourceAlphaPremultiplication, PixelFormat destinationPixelFormat, AlphaPremultiplication destinationAlphaPremultiplication>
static bool convertImagePixelsAcceleratedDifferentFormats(const ConstPixelBufferConversionView& source, const PixelBufferConversionView& destination, const IntSize& destinationSize)
{
    static_assert(PixelBuffer::bytesPerPixelComponent(sourcePixelFormat) == PixelBuffer::bytesPerPixelComponent(destinationPixelFormat));
    static_assert(!(pixelFormatIsOpaque(sourcePixelFormat) && sourceAlphaPremultiplication != AlphaPremultiplication::Premultiplied));
    static_assert(!(sourcePixelFormat == PixelFormat::BGRX8 && destinationPixelFormat != PixelFormat::BGRX8));
    static_assert(sourcePixelFormat != destinationPixelFormat || sourceAlphaPremultiplication != destinationAlphaPremultiplication);
    static_assert(!(sourcePixelFormat == PixelFormat::BGRA8 && destinationPixelFormat == PixelFormat::BGRX8 && sourceAlphaPremultiplication == destinationAlphaPremultiplication));

    auto sourceVImageBuffer = makeVImageBuffer(source, destinationSize);
    auto destinationVImageBuffer = makeVImageBuffer(destination, destinationSize);

    if constexpr (sourceAlphaPremultiplication != destinationAlphaPremultiplication) {
#if ENABLE(PIXEL_FORMAT_RGBA16F)
        if constexpr (sourcePixelFormat == PixelFormat::RGBA16F) {
            if constexpr (destinationAlphaPremultiplication == AlphaPremultiplication::Unpremultiplied)
                vImageUnpremultiplyData_RGBA16F(&sourceVImageBuffer, &destinationVImageBuffer, kvImageNoFlags);
            else
                vImagePremultiplyData_RGBA16F(&sourceVImageBuffer, &destinationVImageBuffer, kvImageNoFlags);
        } else
#endif
        if constexpr (destinationAlphaPremultiplication == AlphaPremultiplication::Unpremultiplied) {
            if constexpr (sourcePixelFormat == PixelFormat::RGBA8)
                vImageUnpremultiplyData_RGBA8888(&sourceVImageBuffer, &destinationVImageBuffer, kvImageNoFlags);
            else
                vImageUnpremultiplyData_BGRA8888(&sourceVImageBuffer, &destinationVImageBuffer, kvImageNoFlags);
        } else {
            if constexpr (sourcePixelFormat == PixelFormat::RGBA8)
                vImagePremultiplyData_RGBA8888(&sourceVImageBuffer, &destinationVImageBuffer, kvImageNoFlags);
            else
                vImagePremultiplyData_BGRA8888(&sourceVImageBuffer, &destinationVImageBuffer, kvImageNoFlags);
        }

        sourceVImageBuffer = destinationVImageBuffer;
    }

    if constexpr (pixelComponentOrder(sourcePixelFormat) != pixelComponentOrder(destinationPixelFormat)) {
        constexpr std::array<uint8_t, 4> map { 2, 1, 0, 3 };
        vImagePermuteChannels_ARGB8888(&sourceVImageBuffer, &destinationVImageBuffer, map.data(), kvImageNoFlags);
    }

    return true;
}

struct ConvertImagePixelsAcceleratedFunctions {
    static constexpr unsigned alphaFormatCount = CountPackedEnums<AlphaPremultiplication::Premultiplied, AlphaPremultiplication::Unpremultiplied>::count;

    static constexpr unsigned pixelFormatCount = CountPackedEnums<
        PixelFormat::RGBA8, PixelFormat::BGRX8, PixelFormat::BGRA8
#if ENABLE(PIXEL_FORMAT_RGBA16F)
    , PixelFormat::RGBA16F
#endif
    >::count;

    template <AlphaPremultiplication sourceAlphaPremultiplication, PixelFormat sourcePixelFormat, AlphaPremultiplication destinationAlphaPremultiplication, PixelFormat destinationPixelFormat>
    static consteval ConvertImagePixelsFunction selectFunction()
    {
        if constexpr (sourcePixelFormat == destinationPixelFormat && sourceAlphaPremultiplication == destinationAlphaPremultiplication)
            return copyImagePixels;
        else if constexpr(sourcePixelFormat == destinationPixelFormat && pixelFormatIsOpaque(sourcePixelFormat) && pixelFormatIsOpaque(destinationPixelFormat))
            return copyImagePixels;
        else if constexpr (PixelBuffer::bytesPerPixelComponent(sourcePixelFormat) != PixelBuffer::bytesPerPixelComponent(destinationPixelFormat))
            return convertImagePixelsAcceleratedAnyToAny;
        else if constexpr (pixelFormatIsOpaque(destinationPixelFormat) && destinationAlphaPremultiplication == AlphaPremultiplication::Unpremultiplied)
            return selectFunction<sourceAlphaPremultiplication, sourcePixelFormat, AlphaPremultiplication::Premultiplied, destinationPixelFormat>(); // If the destination is opaque, consider it premultiplied so that the source will be premultiplied as well to apply its alpha.
        else if constexpr (pixelFormatIsOpaque(sourcePixelFormat) && sourceAlphaPremultiplication != destinationAlphaPremultiplication)
            return selectFunction<destinationAlphaPremultiplication, sourcePixelFormat, destinationAlphaPremultiplication, destinationPixelFormat>(); // If the source is opaque (its alpha is effectively 255), just pretend that it's the same alpha format as the destination, as premultiplying or unpremultiplying wouldn't have any actual effect on color components.
        else if constexpr (sourcePixelFormat == PixelFormat::BGRX8 && destinationPixelFormat != PixelFormat::BGRX8)
            return convertImagePixelsAcceleratedAnyToAny;
        else if constexpr (sourcePixelFormat == PixelFormat::BGRA8 && destinationPixelFormat == PixelFormat::BGRX8 && sourceAlphaPremultiplication == destinationAlphaPremultiplication)
            return copyImagePixels;
        else
            return convertImagePixelsAcceleratedDifferentFormats<sourcePixelFormat, sourceAlphaPremultiplication, destinationPixelFormat, destinationAlphaPremultiplication>;
    }
};

static constexpr ConvertImagePixelsFunctionTable<ConvertImagePixelsAcceleratedFunctions> convertImagePixelsFunctionTable;

static bool convertImagePixelsAccelerated(const ConstPixelBufferConversionView& source, const PixelBufferConversionView& destination, const IntSize& destinationSize)
{
    if (source.format.colorSpace != destination.format.colorSpace)
        return convertImagePixelsAcceleratedAnyToAny(source, destination, destinationSize);

    return convertImagePixelsFunctionTable.invoke(source, destination, destinationSize);
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

template <AlphaPremultiplication sourceAlphaPremultiplication, PixelFormat sourcePixelFormat, AlphaPremultiplication destinationAlphaPremultiplication, PixelFormat destinationPixelFormat>
static bool NODELETE convertImagePixelsUnacceleratedFunction(const ConstPixelBufferConversionView& source, const PixelBufferConversionView& destination, const IntSize& destinationSize)
{
    static_assert(PixelBuffer::bytesPerPixelComponent(sourcePixelFormat) == 1);
    static_assert(PixelBuffer::bytesPerPixel(sourcePixelFormat) == 4);
    static_assert(PixelBuffer::bytesPerPixelComponent(destinationPixelFormat) == 1);
    static_assert(PixelBuffer::bytesPerPixel(destinationPixelFormat) == 4);

    size_t bytesPerRow = destinationSize.width() * 4;
    for (int y = 0; y < destinationSize.height(); ++y) {
        auto sourceRow = source.rows.subspan(source.bytesPerRow * y);
        auto destinationRow = destination.rows.subspan(destination.bytesPerRow * y);
        for (size_t x = 0; x < bytesPerRow; x += 4) {
            uint8_t alpha;
            if constexpr (pixelFormatIsOpaque(sourcePixelFormat))
                alpha = 255;
            else
                alpha = sourceRow[x + 3];

            uint8_t c0, c1, c2;
            if constexpr (sourceAlphaPremultiplication == AlphaPremultiplication::Unpremultiplied && destinationAlphaPremultiplication == AlphaPremultiplication::Unpremultiplied) {
                c0 = sourceRow[x + 0];
                c1 = sourceRow[x + 1];
                c2 = sourceRow[x + 2];
            } else if constexpr (sourceAlphaPremultiplication == AlphaPremultiplication::Premultiplied && destinationAlphaPremultiplication == AlphaPremultiplication::Premultiplied) {
                if (!alpha) {
                    c0 = 0;
                    c1 = 0;
                    c2 = 0;
                } else {
                    c0 = sourceRow[x + 0];
                    c1 = sourceRow[x + 1];
                    c2 = sourceRow[x + 2];
                }
            } else if constexpr (sourceAlphaPremultiplication == AlphaPremultiplication::Premultiplied && destinationAlphaPremultiplication == AlphaPremultiplication::Unpremultiplied) {
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
                static_assert(sourceAlphaPremultiplication == AlphaPremultiplication::Unpremultiplied && destinationAlphaPremultiplication == AlphaPremultiplication::Premultiplied);
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

            if constexpr (pixelComponentOrder(sourcePixelFormat) == pixelComponentOrder(destinationPixelFormat)) {
                destinationRow[x + 0] = c0;
                destinationRow[x + 1] = c1;
                destinationRow[x + 2] = c2;
            } else {
                destinationRow[x + 0] = c2;
                destinationRow[x + 1] = c1;
                destinationRow[x + 2] = c0;
            }

            if constexpr (!pixelFormatIsOpaque(destinationPixelFormat))
                destinationRow[x + 3] = alpha;
            else
                destinationRow[x + 3] = 255; // Not strictly necessary, but this prevent exposing uninitialized memory.
        }
    }

    return true;
}

struct ConvertImagePixelsUnacceleratedFunctions {
    static constexpr unsigned alphaFormatCount = CountPackedEnums<AlphaPremultiplication::Premultiplied, AlphaPremultiplication::Unpremultiplied>::count;

    static constexpr unsigned pixelFormatCount = CountPackedEnums<PixelFormat::RGBA8, PixelFormat::BGRX8, PixelFormat::BGRA8>::count;

    template <AlphaPremultiplication sourceAlphaPremultiplication, PixelFormat sourcePixelFormat, AlphaPremultiplication destinationAlphaPremultiplication, PixelFormat destinationPixelFormat>
    static consteval ConvertImagePixelsFunction selectFunction()
    {
        static_assert(PixelBuffer::bytesPerPixelComponent(sourcePixelFormat) == PixelBuffer::bytesPerPixelComponent(destinationPixelFormat));

        if constexpr (sourcePixelFormat == destinationPixelFormat && sourceAlphaPremultiplication == destinationAlphaPremultiplication)
            return copyImagePixels;
        else if constexpr(sourcePixelFormat == destinationPixelFormat && pixelFormatIsOpaque(sourcePixelFormat) && pixelFormatIsOpaque(destinationPixelFormat))
            return copyImagePixels;
        else if constexpr (pixelFormatIsOpaque(destinationPixelFormat) && destinationAlphaPremultiplication == AlphaPremultiplication::Unpremultiplied)
            return selectFunction<sourceAlphaPremultiplication, sourcePixelFormat, AlphaPremultiplication::Premultiplied, destinationPixelFormat>(); // If the destination is opaque, consider it premultiplied so that the source will be premultiplied as well to apply its alpha.
        else if constexpr (pixelFormatIsOpaque(sourcePixelFormat) && sourceAlphaPremultiplication != destinationAlphaPremultiplication)
            return selectFunction<destinationAlphaPremultiplication, sourcePixelFormat, destinationAlphaPremultiplication, destinationPixelFormat>(); // If the source is opaque (its alpha is effectively 255), just pretend that it's the same alpha format as the destination, as premultiplying or unpremultiplying wouldn't have any actual effect on color components.
        else
            return convertImagePixelsUnacceleratedFunction<sourceAlphaPremultiplication, sourcePixelFormat, destinationAlphaPremultiplication, destinationPixelFormat>;
    }
};

static constexpr ConvertImagePixelsFunctionTable<ConvertImagePixelsUnacceleratedFunctions> convertImagePixelsFunctionTable;

static bool convertImagePixelsUnaccelerated(const ConstPixelBufferConversionView& source, const PixelBufferConversionView& destination, const IntSize& destinationSize)
{
    // FIXME: We don't currently support converting pixel data between different color spaces in the non-accelerated path.
    // This could be added using conversion functions from ColorConversion.h.
    ASSERT(source.format.colorSpace == destination.format.colorSpace);

    return convertImagePixelsFunctionTable.invoke(source, destination, destinationSize);
}
#endif // !(USE(ACCELERATE) && USE(CG))

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
template<typename View>
static bool NODELETE hasEnoughBytesForConversion(const View& view, const IntSize& destinationSize)
{
    if (destinationSize.width() <= 0 || destinationSize.height() <= 0)
        return true;

    CheckedSize requiredBytes = CheckedSize { static_cast<size_t>(destinationSize.height() - 1) } * view.bytesPerRow;
    requiredBytes += CheckedSize { static_cast<size_t>(destinationSize.width()) } * PixelBuffer::bytesPerPixel(view.format.pixelFormat);
    return !requiredBytes.hasOverflowed() && view.rows.size_bytes() >= requiredBytes.value();
}
#endif

static void zeroImagePixels(const PixelBufferConversionView& destination, const IntSize& destinationSize)
{
    size_t rowFillBytes = static_cast<size_t>(destinationSize.width()) * PixelBuffer::bytesPerPixel(destination.format.pixelFormat);
    for (int y = 0; y < destinationSize.height(); ++y)
        zeroSpan(destination.rows.subspan(static_cast<size_t>(y) * destination.bytesPerRow, rowFillBytes));
}

void convertImagePixels(const ConstPixelBufferConversionView& source, const PixelBufferConversionView& destination, const IntSize& destinationSize)
{
    // We currently only support converting between RGBA8, BGRA8, BGRX8, and (where enabled) RGBA16F.
    ASSERT(isSupportedConversionFormat(source.format.pixelFormat));
    ASSERT(isSupportedConversionFormat(destination.format.pixelFormat));

#if ENABLE(PIXEL_FORMAT_RGBA16F)
    if (source.format.pixelFormat == PixelFormat::RGBA16F)
        RELEASE_ASSERT(hasEnoughBytesForConversion(source, destinationSize), "Source buffer is too small for the requested conversion");
    if (destination.format.pixelFormat == PixelFormat::RGBA16F)
        RELEASE_ASSERT(hasEnoughBytesForConversion(destination, destinationSize), "Destination buffer is too small for the requested conversion");
#endif

#if USE(ACCELERATE) && USE(CG)
    if (!convertImagePixelsAccelerated(source, destination, destinationSize))
        zeroImagePixels(destination, destinationSize);
#else
    if (source.format.alphaFormat == destination.format.alphaFormat && source.format.pixelFormat == destination.format.pixelFormat && source.format.colorSpace == destination.format.colorSpace) {
        copyImagePixels(source, destination, destinationSize);
        return;
    }
#if USE(SKIA)
    if (convertImagePixelsSkia(source, destination, destinationSize))
        return;
#endif
    // FIXME: In Linux platform the following paths could be optimized with ORC.

    if (!convertImagePixelsUnaccelerated(source, destination, destinationSize))
        zeroImagePixels(destination, destinationSize);
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
