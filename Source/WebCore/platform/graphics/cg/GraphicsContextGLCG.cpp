/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

#if ENABLE(WEBGL)

#include "GraphicsContextCG.h"
#include "GraphicsContextGLImageExtractor.h"
#include "Image.h"
#include "NativeImage.h"

#if HAVE(ARM_NEON_INTRINSICS)
#include "GraphicsContextGLNEON.h"
#endif

#include <CoreGraphics/CGBitmapContext.h>
#include <CoreGraphics/CGContext.h>
#include <CoreGraphics/CGDataProvider.h>
#include <CoreGraphics/CGImage.h>

#include <wtf/RetainPtr.h>
#include <wtf/StdLibExtras.h>
#include <wtf/cf/VectorCF.h>

namespace WebCore {

enum SourceDataFormatBase {
    SourceFormatBaseR = 0,
    SourceFormatBaseA,
    SourceFormatBaseRA,
    SourceFormatBaseAR,
    SourceFormatBaseRGB,
    SourceFormatBaseRGBA,
    SourceFormatBaseARGB,
    SourceFormatBaseNumFormats
};

enum AlphaFormat {
    AlphaFormatNone = 0,
    AlphaFormatFirst,
    AlphaFormatLast,
    AlphaFormatNumFormats
};

// This returns SourceFormatNumFormats if the combination of input parameters is unsupported.
static GraphicsContextGL::DataFormat NODELETE getSourceDataFormat(unsigned componentsPerPixel, AlphaFormat alphaFormat, bool is16BitFormat, bool bigEndian)
{
    static const std::array formatTableBase = { // componentsPerPixel x AlphaFormat
        //           AlphaFormatNone             AlphaFormatFirst            AlphaFormatLast
        std::array { SourceFormatBaseR,          SourceFormatBaseA,          SourceFormatBaseA          }, // 1 componentsPerPixel
        std::array { SourceFormatBaseNumFormats, SourceFormatBaseAR,         SourceFormatBaseRA         }, // 2 componentsPerPixel
        std::array { SourceFormatBaseRGB,        SourceFormatBaseNumFormats, SourceFormatBaseNumFormats }, // 3 componentsPerPixel
        std::array { SourceFormatBaseNumFormats, SourceFormatBaseARGB,       SourceFormatBaseRGBA       }, // 4 componentsPerPixel
    };
    static const std::array formatTable = { // SourceDataFormat::Base x bitsPerComponent x endian
        //           8bits, little endian                  8bits, big endian                     16bits, little endian                        16bits, big endian
        std::array { GraphicsContextGL::DataFormat::R8,    GraphicsContextGL::DataFormat::R8,    GraphicsContextGL::DataFormat::R16Little,    GraphicsContextGL::DataFormat::R16Big },
        std::array { GraphicsContextGL::DataFormat::A8,    GraphicsContextGL::DataFormat::A8,    GraphicsContextGL::DataFormat::A16Little,    GraphicsContextGL::DataFormat::A16Big },
        std::array { GraphicsContextGL::DataFormat::AR8,   GraphicsContextGL::DataFormat::RA8,   GraphicsContextGL::DataFormat::RA16Little,   GraphicsContextGL::DataFormat::RA16Big },
        std::array { GraphicsContextGL::DataFormat::RA8,   GraphicsContextGL::DataFormat::AR8,   GraphicsContextGL::DataFormat::AR16Little,   GraphicsContextGL::DataFormat::AR16Big },
        std::array { GraphicsContextGL::DataFormat::BGR8,  GraphicsContextGL::DataFormat::RGB8,  GraphicsContextGL::DataFormat::RGB16Little,  GraphicsContextGL::DataFormat::RGB16Big },
        std::array { GraphicsContextGL::DataFormat::ABGR8, GraphicsContextGL::DataFormat::RGBA8, GraphicsContextGL::DataFormat::RGBA16Little, GraphicsContextGL::DataFormat::RGBA16Big },
        std::array { GraphicsContextGL::DataFormat::BGRA8, GraphicsContextGL::DataFormat::ARGB8, GraphicsContextGL::DataFormat::ARGB16Little, GraphicsContextGL::DataFormat::ARGB16Big },
    };

    ASSERT(componentsPerPixel <= 4 && componentsPerPixel > 0);
    SourceDataFormatBase formatBase = formatTableBase[componentsPerPixel - 1][alphaFormat];
    if (formatBase == SourceFormatBaseNumFormats)
        return GraphicsContextGL::DataFormat::NumFormats;
    return formatTable[formatBase][(is16BitFormat ? 2 : 0) + (bigEndian ? 1 : 0)];
}

namespace {
uint8_t NODELETE convertColor16LittleTo8(uint16_t value)
{
    return value >> 8;
}

uint8_t NODELETE convertColor16BigTo8(uint16_t value)
{
    return static_cast<uint8_t>(value & 0x00FF);
}

template<GraphicsContextGL::DataFormat format, typename SourceType, typename DstType>
ALWAYS_INLINE void convert16BitFormatToRGBA8(std::span<const SourceType>, std::span<DstType>, unsigned)
{
    ASSERT_NOT_REACHED();
}

template<> ALWAYS_INLINE void NODELETE convert16BitFormatToRGBA8<GraphicsContextGL::DataFormat::RGBA16Little, uint16_t, uint8_t>(std::span<const uint16_t> source, std::span<uint8_t> destination, unsigned pixelsPerRow)
{
#if HAVE(ARM_NEON_INTRINSICS)
    SIMD::unpackOneRowOfRGBA16LittleToRGBA8(source.data(), destination.data(), pixelsPerRow);
#endif
    for (unsigned i = 0; i < pixelsPerRow; ++i) {
        destination[0] = convertColor16LittleTo8(source[0]);
        destination[1] = convertColor16LittleTo8(source[1]);
        destination[2] = convertColor16LittleTo8(source[2]);
        destination[3] = convertColor16LittleTo8(source[3]);
        skip(source, 4);
        skip(destination, 4);
    }
}

template<> ALWAYS_INLINE void NODELETE convert16BitFormatToRGBA8<GraphicsContextGL::DataFormat::RGBA16Big, uint16_t, uint8_t>(std::span<const uint16_t> source, std::span<uint8_t> destination, unsigned pixelsPerRow)
{
    for (unsigned i = 0; i < pixelsPerRow; ++i) {
        destination[0] = convertColor16BigTo8(source[0]);
        destination[1] = convertColor16BigTo8(source[1]);
        destination[2] = convertColor16BigTo8(source[2]);
        destination[3] = convertColor16BigTo8(source[3]);
        skip(source, 4);
        skip(destination, 4);
    }
}

template<> ALWAYS_INLINE void NODELETE convert16BitFormatToRGBA8<GraphicsContextGL::DataFormat::RGB16Little, uint16_t, uint8_t>(std::span<const uint16_t> source, std::span<uint8_t> destination, unsigned pixelsPerRow)
{
#if HAVE(ARM_NEON_INTRINSICS)
    SIMD::unpackOneRowOfRGB16LittleToRGBA8(source.data(), destination.data(), pixelsPerRow);
#endif
    for (unsigned i = 0; i < pixelsPerRow; ++i) {
        destination[0] = convertColor16LittleTo8(source[0]);
        destination[1] = convertColor16LittleTo8(source[1]);
        destination[2] = convertColor16LittleTo8(source[2]);
        destination[3] = 0xFF;
        skip(source, 3);
        skip(destination, 4);
    }
}

template<> ALWAYS_INLINE void NODELETE convert16BitFormatToRGBA8<GraphicsContextGL::DataFormat::RGB16Big, uint16_t, uint8_t>(std::span<const uint16_t> source, std::span<uint8_t> destination, unsigned pixelsPerRow)
{
    for (unsigned i = 0; i < pixelsPerRow; ++i) {
        destination[0] = convertColor16BigTo8(source[0]);
        destination[1] = convertColor16BigTo8(source[1]);
        destination[2] = convertColor16BigTo8(source[2]);
        destination[3] = 0xFF;
        skip(source, 3);
        skip(destination, 4);
    }
}

template<> ALWAYS_INLINE void NODELETE convert16BitFormatToRGBA8<GraphicsContextGL::DataFormat::ARGB16Little, uint16_t, uint8_t>(std::span<const uint16_t> source, std::span<uint8_t> destination, unsigned pixelsPerRow)
{
#if HAVE(ARM_NEON_INTRINSICS)
    SIMD::unpackOneRowOfARGB16LittleToRGBA8(source.data(), destination.data(), pixelsPerRow);
#endif
    for (unsigned i = 0; i < pixelsPerRow; ++i) {
        destination[0] = convertColor16LittleTo8(source[1]);
        destination[1] = convertColor16LittleTo8(source[2]);
        destination[2] = convertColor16LittleTo8(source[3]);
        destination[3] = convertColor16LittleTo8(source[0]);
        skip(source, 4);
        skip(destination, 4);
    }
}

template<> ALWAYS_INLINE void NODELETE convert16BitFormatToRGBA8<GraphicsContextGL::DataFormat::ARGB16Big, uint16_t, uint8_t>(std::span<const uint16_t> source, std::span<uint8_t> destination, unsigned pixelsPerRow)
{
    for (unsigned i = 0; i < pixelsPerRow; ++i) {
        destination[0] = convertColor16BigTo8(source[1]);
        destination[1] = convertColor16BigTo8(source[2]);
        destination[2] = convertColor16BigTo8(source[3]);
        destination[3] = convertColor16BigTo8(source[0]);
        skip(source, 4);
        skip(destination, 4);
    }
}

template<> ALWAYS_INLINE void NODELETE convert16BitFormatToRGBA8<GraphicsContextGL::DataFormat::R16Little, uint16_t, uint8_t>(std::span<const uint16_t> source, std::span<uint8_t> destination, unsigned pixelsPerRow)
{
    for (unsigned i = 0; i < pixelsPerRow; ++i) {
        destination[0] = convertColor16LittleTo8(source[0]);
        destination[1] = convertColor16LittleTo8(source[0]);
        destination[2] = convertColor16LittleTo8(source[0]);
        destination[3] = 0xFF;
        skip(source, 1);
        skip(destination, 4);
    }
}

template<> ALWAYS_INLINE void NODELETE convert16BitFormatToRGBA8<GraphicsContextGL::DataFormat::R16Big, uint16_t, uint8_t>(std::span<const uint16_t> source, std::span<uint8_t> destination, unsigned pixelsPerRow)
{
    for (unsigned i = 0; i < pixelsPerRow; ++i) {
        destination[0] = convertColor16BigTo8(source[0]);
        destination[1] = convertColor16BigTo8(source[0]);
        destination[2] = convertColor16BigTo8(source[0]);
        destination[3] = 0xFF;
        skip(source, 1);
        skip(destination, 4);
    }
}

template<> ALWAYS_INLINE void NODELETE convert16BitFormatToRGBA8<GraphicsContextGL::DataFormat::RA16Little, uint16_t, uint8_t>(std::span<const uint16_t> source, std::span<uint8_t> destination, unsigned pixelsPerRow)
{
    for (unsigned i = 0; i < pixelsPerRow; ++i) {
        destination[0] = convertColor16LittleTo8(source[0]);
        destination[1] = convertColor16LittleTo8(source[0]);
        destination[2] = convertColor16LittleTo8(source[0]);
        destination[3] = convertColor16LittleTo8(source[1]);
        skip(source, 2);
        skip(destination, 4);
    }
}

template<> ALWAYS_INLINE void NODELETE convert16BitFormatToRGBA8<GraphicsContextGL::DataFormat::RA16Big, uint16_t, uint8_t>(std::span<const uint16_t> source, std::span<uint8_t> destination, unsigned pixelsPerRow)
{
    for (unsigned i = 0; i < pixelsPerRow; ++i) {
        destination[0] = convertColor16BigTo8(source[0]);
        destination[1] = convertColor16BigTo8(source[0]);
        destination[2] = convertColor16BigTo8(source[0]);
        destination[3] = convertColor16BigTo8(source[1]);
        skip(source, 2);
        skip(destination, 4);
    }
}

template<> ALWAYS_INLINE void NODELETE convert16BitFormatToRGBA8<GraphicsContextGL::DataFormat::AR16Little, uint16_t, uint8_t>(std::span<const uint16_t> source, std::span<uint8_t> destination, unsigned pixelsPerRow)
{
    for (unsigned i = 0; i < pixelsPerRow; ++i) {
        destination[0] = convertColor16LittleTo8(source[1]);
        destination[1] = convertColor16LittleTo8(source[1]);
        destination[2] = convertColor16LittleTo8(source[1]);
        destination[3] = convertColor16LittleTo8(source[0]);
        skip(source, 2);
        skip(destination, 4);
    }
}

template<> ALWAYS_INLINE void NODELETE convert16BitFormatToRGBA8<GraphicsContextGL::DataFormat::AR16Big, uint16_t, uint8_t>(std::span<const uint16_t> source, std::span<uint8_t> destination, unsigned pixelsPerRow)
{
    for (unsigned i = 0; i < pixelsPerRow; ++i) {
        destination[0] = convertColor16BigTo8(source[1]);
        destination[1] = convertColor16BigTo8(source[1]);
        destination[2] = convertColor16BigTo8(source[1]);
        destination[3] = convertColor16BigTo8(source[0]);
        skip(source, 2);
        skip(destination, 4);
    }
}

template<> ALWAYS_INLINE void NODELETE convert16BitFormatToRGBA8<GraphicsContextGL::DataFormat::A16Little, uint16_t, uint8_t>(std::span<const uint16_t> source, std::span<uint8_t> destination, unsigned pixelsPerRow)
{
    for (unsigned i = 0; i < pixelsPerRow; ++i) {
        destination[0] = 0x0;
        destination[1] = 0x0;
        destination[2] = 0x0;
        destination[3] = convertColor16LittleTo8(source[0]);
        skip(source, 1);
        skip(destination, 4);
    }
}

template<> ALWAYS_INLINE void NODELETE convert16BitFormatToRGBA8<GraphicsContextGL::DataFormat::A16Big, uint16_t, uint8_t>(std::span<const uint16_t> source, std::span<uint8_t> destination, unsigned pixelsPerRow)
{
    for (unsigned i = 0; i < pixelsPerRow; ++i) {
        destination[0] = 0x0;
        destination[1] = 0x0;
        destination[2] = 0x0;
        destination[3] = convertColor16BigTo8(source[0]);
        skip(source, 1);
        skip(destination, 4);
    }
}

void NODELETE convert16BitFormatToRGBA8(GraphicsContextGL::DataFormat srcFormat, std::span<const uint16_t> source, std::span<uint8_t> destination, unsigned pixelsPerRow)
{
#define CONVERT16BITFORMATTORGBA8(SrcFormat) \
    case SrcFormat: \
        return convert16BitFormatToRGBA8<SrcFormat>(source, destination, pixelsPerRow);

    switch (srcFormat) {
        CONVERT16BITFORMATTORGBA8(GraphicsContextGL::DataFormat::R16Little)
        CONVERT16BITFORMATTORGBA8(GraphicsContextGL::DataFormat::R16Big)
        CONVERT16BITFORMATTORGBA8(GraphicsContextGL::DataFormat::A16Little)
        CONVERT16BITFORMATTORGBA8(GraphicsContextGL::DataFormat::A16Big)
        CONVERT16BITFORMATTORGBA8(GraphicsContextGL::DataFormat::RA16Little)
        CONVERT16BITFORMATTORGBA8(GraphicsContextGL::DataFormat::RA16Big)
        CONVERT16BITFORMATTORGBA8(GraphicsContextGL::DataFormat::AR16Little)
        CONVERT16BITFORMATTORGBA8(GraphicsContextGL::DataFormat::AR16Big)
        CONVERT16BITFORMATTORGBA8(GraphicsContextGL::DataFormat::RGB16Little)
        CONVERT16BITFORMATTORGBA8(GraphicsContextGL::DataFormat::RGB16Big)
        CONVERT16BITFORMATTORGBA8(GraphicsContextGL::DataFormat::RGBA16Little)
        CONVERT16BITFORMATTORGBA8(GraphicsContextGL::DataFormat::RGBA16Big)
        CONVERT16BITFORMATTORGBA8(GraphicsContextGL::DataFormat::ARGB16Little)
        CONVERT16BITFORMATTORGBA8(GraphicsContextGL::DataFormat::ARGB16Big)
    default:
        ASSERT_NOT_REACHED();
    }
#undef CONVERT16BITFORMATTORGBA8
}

}

GraphicsContextGLImageExtractor::~GraphicsContextGLImageExtractor() = default;

bool GraphicsContextGLImageExtractor::extractImage(AlphaPremultiplication sourceAlphaPremultiplication, bool premultiplyAlpha)
{
    RefPtr decodedImage = m_image.ptr();
    m_imageWidth = CGImageGetWidth(decodedImage->platformImage().get());
    m_imageHeight = CGImageGetHeight(decodedImage->platformImage().get());
    if (!m_imageWidth || !m_imageHeight)
        return false;

    // See whether the image is using an indexed color space, and if
    // so, re-render it into an RGB color space. The image re-packing
    // code requires color data, not color table indices, for the
    // image data.
    CGColorSpaceRef colorSpace = CGImageGetColorSpace(decodedImage->platformImage().get());
    CGColorSpaceModel model = CGColorSpaceGetModel(colorSpace);
    if (model == kCGColorSpaceModelIndexed) {
        RetainPtr<CGContextRef> bitmapContext;
        // FIXME: we should probably manually convert the image by indexing into
        // the color table, which would allow us to avoid premultiplying the
        // alpha channel. Creation of a bitmap context with an alpha channel
        // doesn't seem to work unless it's premultiplied.
        bitmapContext = adoptCF(CGBitmapContextCreate(0, m_imageWidth, m_imageHeight, 8, m_imageWidth * 4,
            sRGBColorSpaceSingleton(), static_cast<uint32_t>(kCGImageAlphaPremultipliedFirst) | static_cast<uint32_t>(kCGBitmapByteOrder32Host)));
        if (!bitmapContext)
            return false;

        CGContextSetBlendMode(bitmapContext.get(), kCGBlendModeCopy);
        CGContextSetInterpolationQuality(bitmapContext.get(), kCGInterpolationNone);
        CGContextDrawImage(bitmapContext.get(), CGRectMake(0, 0, m_imageWidth, m_imageHeight), decodedImage->platformImage().get());

        // Now discard the original CG image and replace it with a copy from the bitmap context.
        // The bitmap context premultiplied the alpha, whatever the original contents held.
        sourceAlphaPremultiplication = AlphaPremultiplication::Premultiplied;
        decodedImage = NativeImage::create(adoptCF(CGBitmapContextCreateImage(bitmapContext.get())));
    }

    if (!decodedImage)
        return false;

    size_t bitsPerComponent = CGImageGetBitsPerComponent(decodedImage->platformImage().get());
    size_t bitsPerPixel = CGImageGetBitsPerPixel(decodedImage->platformImage().get());
    if (bitsPerComponent != 8 && bitsPerComponent != 16)
        return false;
    if (bitsPerPixel % bitsPerComponent)
        return false;
    size_t componentsPerPixel = bitsPerPixel / bitsPerComponent;

    CGBitmapInfo bitInfo = CGImageGetBitmapInfo(decodedImage->platformImage().get());
    bool bigEndianSource = false;
    // These could technically be combined into one large switch
    // statement, but we prefer not to so that we fail fast if we
    // encounter an unexpected image configuration.
    if (bitsPerComponent == 16) {
        switch (bitInfo & kCGBitmapByteOrderMask) {
        case kCGBitmapByteOrder16Big:
            bigEndianSource = true;
            break;
        case kCGBitmapByteOrder16Little:
            bigEndianSource = false;
            break;
        case kCGBitmapByteOrderDefault:
            // This is a bug in earlier version of cg where the default endian
            // is little whereas the decoded 16-bit png image data is actually
            // Big. Later version (10.6.4) no longer returns ByteOrderDefault.
            bigEndianSource = true;
            break;
        default:
            return false;
        }
    } else {
        switch (bitInfo & kCGBitmapByteOrderMask) {
        case kCGBitmapByteOrder32Big:
            bigEndianSource = true;
            break;
        case kCGBitmapByteOrder32Little:
            bigEndianSource = false;
            break;
        case kCGBitmapByteOrderDefault:
            // It appears that the default byte order is actually big
            // endian even on little endian architectures.
            bigEndianSource = true;
            break;
        default:
            return false;
        }
    }

    // The CGImage records the premultiplication of its contents, but it may record it incorrectly,
    // so only the alpha channel position is taken from it and the premultiplication from the caller.
    AlphaFormat alphaFormat = AlphaFormatNone;
    bool hasAlpha = false;
    switch (CGImageGetAlphaInfo(decodedImage->platformImage().get())) {
    case kCGImageAlphaPremultipliedFirst:
    case kCGImageAlphaFirst:
        alphaFormat = AlphaFormatFirst;
        hasAlpha = true;
        break;
    case kCGImageAlphaPremultipliedLast:
    case kCGImageAlphaLast:
        alphaFormat = AlphaFormatLast;
        hasAlpha = true;
        break;
    case kCGImageAlphaNoneSkipFirst:
        // The skipped channel holds undefined values, so it must not take part in an alpha op.
        alphaFormat = AlphaFormatFirst;
        break;
    case kCGImageAlphaNoneSkipLast:
        alphaFormat = AlphaFormatLast;
        break;
    case kCGImageAlphaNone:
        alphaFormat = AlphaFormatNone;
        break;
    default:
        return false;
    }
    m_alphaOp = hasAlpha ? alphaOpForPremultiplication(sourceAlphaPremultiplication, premultiplyAlpha) : AlphaOp::DoNothing;

    m_imageSourceFormat = getSourceDataFormat(componentsPerPixel, alphaFormat, bitsPerComponent == 16, bigEndianSource);
    if (m_imageSourceFormat == DataFormat::NumFormats)
        return false;

    m_pixelData = adoptCF(CGDataProviderCopyData(CGImageGetDataProvider(decodedImage->platformImage().get())));
    if (!m_pixelData)
        return false;

    m_imagePixelData = span(m_pixelData.get());

    unsigned srcUnpackAlignment = 0;
    size_t bytesPerRow = CGImageGetBytesPerRow(decodedImage->platformImage().get());
    unsigned padding = bytesPerRow - bitsPerPixel / 8 * m_imageWidth;
    if (padding) {
        srcUnpackAlignment = padding + 1;
        while (bytesPerRow % srcUnpackAlignment)
            ++srcUnpackAlignment;
    }
    m_imageSourceUnpackAlignment = srcUnpackAlignment;

    // Using a bitmap context created according to destination format and drawing the CGImage to the bitmap context can also do the format conversion,
    // but it would premultiply the alpha channel as a side effect.
    // Prefer to mannually Convert 16bit per-component formats to RGBA8 formats instead.
    if (bitsPerComponent == 16) {
        m_formalizedRGBA8Data = MallocSpan<uint8_t>::malloc(Checked<size_t>(m_imageWidth) * m_imageHeight * 4U);
        auto source = spanReinterpretCast<const uint16_t>(m_imagePixelData);
        auto destination = m_formalizedRGBA8Data.mutableSpan();
        const ptrdiff_t srcStrideInElements = bytesPerRow / sizeof(uint16_t);
        const ptrdiff_t dstStrideInElements = 4 * m_imageWidth;
        for (unsigned i = 0; i < m_imageHeight; i++) {
            convert16BitFormatToRGBA8(m_imageSourceFormat, source, destination, m_imageWidth);
            skip(source, srcStrideInElements);
            skip(destination, dstStrideInElements);
        }
        m_imagePixelData = m_formalizedRGBA8Data.span();
        m_imageSourceFormat = DataFormat::RGBA8;
        m_imageSourceUnpackAlignment = 1;
    }
    return true;
}

} // namespace WebCore

#endif // ENABLE(WEBGL)
