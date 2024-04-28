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
#include "PixelFormat.h"

#if USE(ACCELERATE) && USE(CG)
#include <Accelerate/Accelerate.h>
#elif USE(SKIA)
IGNORE_CLANG_WARNINGS_BEGIN("cast-align")
#include <skia/core/SkPixmap.h>
IGNORE_CLANG_WARNINGS_END
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
        case PixelFormat::RGB10:
        case PixelFormat::RGB10A8:
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
    result.data = const_cast<uint8_t*>(view.rows);

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

static void convertImagePixelsSkia(const ConstPixelBufferConversionView& source, const PixelBufferConversionView& destination, const IntSize& destinationSize)
{
    auto toSkiaColorType = [](const PixelFormat& pixelFormat) {
        switch (pixelFormat) {
        case PixelFormat::RGBA8:
            return SkColorType::kRGBA_8888_SkColorType;
        case PixelFormat::BGRA8:
            return SkColorType::kBGRA_8888_SkColorType;
        case PixelFormat::BGRX8:
        case PixelFormat::RGB10:
        case PixelFormat::RGB10A8:
            break;
        }
        ASSERT_NOT_REACHED();
        return SkColorType::kUnknown_SkColorType;
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
    SkImageInfo sourceImageInfo = SkImageInfo::Make(
        destinationSize.width(),
        destinationSize.height(),
        toSkiaColorType(source.format.pixelFormat),
        toSkiaAlphaType(source.format.alphaFormat),
        source.format.colorSpace.platformColorSpace()
    );
    // Utilize SkPixmap which is a raw bytes wrapper capable of performing conversions.
    SkPixmap sourcePixmap(sourceImageInfo, source.rows, source.bytesPerRow);
    SkImageInfo destinationImageInfo = SkImageInfo::Make(
        destinationSize.width(),
        destinationSize.height(),
        toSkiaColorType(destination.format.pixelFormat),
        toSkiaAlphaType(destination.format.alphaFormat),
        destination.format.colorSpace.platformColorSpace()
    );
    // Read pixels from source to destination and convert pixels if necessary.
    sourcePixmap.readPixels(destinationImageInfo, destination.rows, destination.bytesPerRow);
}

#endif

enum class PixelFormatConversion { None, Permute };

template<PixelFormatConversion pixelFormatConversion>
static void convertSinglePixelPremultipliedToPremultiplied(const uint8_t* sourcePixel, uint8_t* destinationPixel)
{
    uint8_t alpha = sourcePixel[3];
    if (!alpha) {
        reinterpret_cast<uint32_t*>(destinationPixel)[0] = 0;
        return;
    }

    if constexpr (pixelFormatConversion == PixelFormatConversion::None)
        reinterpret_cast<uint32_t*>(destinationPixel)[0] = reinterpret_cast<const uint32_t*>(sourcePixel)[0];
    else {
        // Swap pixel channels BGRA <-> RGBA.
        destinationPixel[0] = sourcePixel[2];
        destinationPixel[1] = sourcePixel[1];
        destinationPixel[2] = sourcePixel[0];
        destinationPixel[3] = sourcePixel[3];
    }
}

template<PixelFormatConversion pixelFormatConversion>
static void convertSinglePixelPremultipliedToUnpremultiplied(const uint8_t* sourcePixel, uint8_t* destinationPixel)
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
static void convertSinglePixelUnpremultipliedToPremultiplied(const uint8_t* sourcePixel, uint8_t* destinationPixel)
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
static void convertSinglePixelUnpremultipliedToUnpremultiplied(const uint8_t* sourcePixel, uint8_t* destinationPixel)
{
    if constexpr (pixelFormatConversion == PixelFormatConversion::None)
        reinterpret_cast<uint32_t*>(destinationPixel)[0] = reinterpret_cast<const uint32_t*>(sourcePixel)[0];
    else {
        // Swap pixel channels BGRA <-> RGBA.
        destinationPixel[0] = sourcePixel[2];
        destinationPixel[1] = sourcePixel[1];
        destinationPixel[2] = sourcePixel[0];
        destinationPixel[3] = sourcePixel[3];
    }
}

template<void (*convertFunctor)(const uint8_t*, uint8_t*)>
static void convertImagePixelsUnaccelerated(const ConstPixelBufferConversionView& source, const PixelBufferConversionView& destination, const IntSize& destinationSize)
{
    const uint8_t* sourceRows = source.rows;
    uint8_t* destinationRows = destination.rows;

    size_t bytesPerRow = destinationSize.width() * 4;
    for (int y = 0; y < destinationSize.height(); ++y) {
        for (size_t x = 0; x < bytesPerRow; x += 4)
            convertFunctor(&sourceRows[x], &destinationRows[x]);
        sourceRows += source.bytesPerRow;
        destinationRows += destination.bytesPerRow;
    }
}

void convertImagePixels(const ConstPixelBufferConversionView& source, const PixelBufferConversionView& destination, const IntSize& destinationSize)
{
    // We currently only support converting between RGBA8, BGRA8, and BGRX8.
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
#elif USE(SKIA)
    if (source.format.alphaFormat == destination.format.alphaFormat && source.format.pixelFormat == destination.format.pixelFormat && source.format.colorSpace == destination.format.colorSpace)
        memcpy(destination.rows, source.rows, source.bytesPerRow * destinationSize.height());
    else
        convertImagePixelsSkia(source, destination, destinationSize);
#else
    // FIXME: We don't currently support converting pixel data between different color spaces in the non-accelerated path.
    // This could be added using conversion functions from ColorConversion.h.
    ASSERT(source.format.colorSpace == destination.format.colorSpace);

    if (source.format.alphaFormat == destination.format.alphaFormat && source.format.pixelFormat == destination.format.pixelFormat) {
        memcpy(destination.rows, source.rows, source.bytesPerRow * destinationSize.height());
        return;
    }

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

void copyRows(unsigned sourceBytesPerRow, const uint8_t* source, unsigned destinationBytesPerRow, uint8_t* destination, unsigned rows, unsigned copyBytesPerRow)
{
    if (sourceBytesPerRow == destinationBytesPerRow && copyBytesPerRow == sourceBytesPerRow)
        std::copy(source, source + copyBytesPerRow * rows, destination);
    else {
        for (unsigned row = 0; row < rows; ++row) {
            std::copy(source, source + copyBytesPerRow, destination);
            source += sourceBytesPerRow;
            destination += destinationBytesPerRow;
        }
    }
}

}
