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

#if ENABLE(GRAPHICS_CONTEXT_GL)

#include "BitmapImage.h"
#include "GraphicsContextCG.h"
#include "GraphicsContextGLOpenGL.h"
#include "Image.h"
#include "ImageBufferUtilitiesCG.h"

#if HAVE(ARM_NEON_INTRINSICS)
#include "GraphicsContextGLNEON.h"
#endif

#include <CoreGraphics/CGBitmapContext.h>
#include <CoreGraphics/CGContext.h>
#include <CoreGraphics/CGDataProvider.h>
#include <CoreGraphics/CGImage.h>

#include <wtf/RetainPtr.h>
#include <wtf/StdLibExtras.h>

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
static GraphicsContextGL::DataFormat getSourceDataFormat(unsigned componentsPerPixel, AlphaFormat alphaFormat, bool is16BitFormat, bool bigEndian)
{
    const static SourceDataFormatBase formatTableBase[4][AlphaFormatNumFormats] = { // componentsPerPixel x AlphaFormat
        // AlphaFormatNone            AlphaFormatFirst            AlphaFormatLast
        { SourceFormatBaseR,          SourceFormatBaseA,          SourceFormatBaseA          }, // 1 componentsPerPixel
        { SourceFormatBaseNumFormats, SourceFormatBaseAR,         SourceFormatBaseRA         }, // 2 componentsPerPixel
        { SourceFormatBaseRGB,        SourceFormatBaseNumFormats, SourceFormatBaseNumFormats }, // 3 componentsPerPixel
        { SourceFormatBaseNumFormats, SourceFormatBaseARGB,       SourceFormatBaseRGBA        } // 4 componentsPerPixel
    };
    const static GraphicsContextGL::DataFormat formatTable[SourceFormatBaseNumFormats][4] = { // SourceDataFormat::Base x bitsPerComponent x endian
        // 8bits, little endian                 8bits, big endian                     16bits, little endian                        16bits, big endian
        { GraphicsContextGL::DataFormat::R8,    GraphicsContextGL::DataFormat::R8,    GraphicsContextGL::DataFormat::R16Little,    GraphicsContextGL::DataFormat::R16Big },
        { GraphicsContextGL::DataFormat::A8,    GraphicsContextGL::DataFormat::A8,    GraphicsContextGL::DataFormat::A16Little,    GraphicsContextGL::DataFormat::A16Big },
        { GraphicsContextGL::DataFormat::AR8,   GraphicsContextGL::DataFormat::RA8,   GraphicsContextGL::DataFormat::RA16Little,   GraphicsContextGL::DataFormat::RA16Big },
        { GraphicsContextGL::DataFormat::RA8,   GraphicsContextGL::DataFormat::AR8,   GraphicsContextGL::DataFormat::AR16Little,   GraphicsContextGL::DataFormat::AR16Big },
        { GraphicsContextGL::DataFormat::BGR8,  GraphicsContextGL::DataFormat::RGB8,  GraphicsContextGL::DataFormat::RGB16Little,  GraphicsContextGL::DataFormat::RGB16Big },
        { GraphicsContextGL::DataFormat::ABGR8, GraphicsContextGL::DataFormat::RGBA8, GraphicsContextGL::DataFormat::RGBA16Little, GraphicsContextGL::DataFormat::RGBA16Big },
        { GraphicsContextGL::DataFormat::BGRA8, GraphicsContextGL::DataFormat::ARGB8, GraphicsContextGL::DataFormat::ARGB16Little, GraphicsContextGL::DataFormat::ARGB16Big }
    };

    ASSERT(componentsPerPixel <= 4 && componentsPerPixel > 0);
    SourceDataFormatBase formatBase = formatTableBase[componentsPerPixel - 1][alphaFormat];
    if (formatBase == SourceFormatBaseNumFormats)
        return GraphicsContextGL::DataFormat::NumFormats;
    return formatTable[formatBase][(is16BitFormat ? 2 : 0) + (bigEndian ? 1 : 0)];
}

namespace {
uint8_t convertColor16LittleTo8(uint16_t value)
{
    return value >> 8;
}

uint8_t convertColor16BigTo8(uint16_t value)
{
    return static_cast<uint8_t>(value & 0x00FF);
}

template<GraphicsContextGL::DataFormat format, typename SourceType, typename DstType>
ALWAYS_INLINE void convert16BitFormatToRGBA8(const SourceType*, DstType*, unsigned)
{
    ASSERT_NOT_REACHED();
}

template<> ALWAYS_INLINE void convert16BitFormatToRGBA8<GraphicsContextGL::DataFormat::RGBA16Little, uint16_t, uint8_t>(const uint16_t* source, uint8_t* destination, unsigned pixelsPerRow)
{
#if HAVE(ARM_NEON_INTRINSICS)
    SIMD::unpackOneRowOfRGBA16LittleToRGBA8(source, destination, pixelsPerRow);
#endif
    for (unsigned i = 0; i < pixelsPerRow; ++i) {
        destination[0] = convertColor16LittleTo8(source[0]);
        destination[1] = convertColor16LittleTo8(source[1]);
        destination[2] = convertColor16LittleTo8(source[2]);
        destination[3] = convertColor16LittleTo8(source[3]);
        source += 4;
        destination += 4;
    }
}

template<> ALWAYS_INLINE void convert16BitFormatToRGBA8<GraphicsContextGL::DataFormat::RGBA16Big, uint16_t, uint8_t>(const uint16_t* source, uint8_t* destination, unsigned pixelsPerRow)
{
    for (unsigned i = 0; i < pixelsPerRow; ++i) {
        destination[0] = convertColor16BigTo8(source[0]);
        destination[1] = convertColor16BigTo8(source[1]);
        destination[2] = convertColor16BigTo8(source[2]);
        destination[3] = convertColor16BigTo8(source[3]);
        source += 4;
        destination += 4;
    }
}

template<> ALWAYS_INLINE void convert16BitFormatToRGBA8<GraphicsContextGL::DataFormat::RGB16Little, uint16_t, uint8_t>(const uint16_t* source, uint8_t* destination, unsigned pixelsPerRow)
{
#if HAVE(ARM_NEON_INTRINSICS)
    SIMD::unpackOneRowOfRGB16LittleToRGBA8(source, destination, pixelsPerRow);
#endif
    for (unsigned i = 0; i < pixelsPerRow; ++i) {
        destination[0] = convertColor16LittleTo8(source[0]);
        destination[1] = convertColor16LittleTo8(source[1]);
        destination[2] = convertColor16LittleTo8(source[2]);
        destination[3] = 0xFF;
        source += 3;
        destination += 4;
    }
}

template<> ALWAYS_INLINE void convert16BitFormatToRGBA8<GraphicsContextGL::DataFormat::RGB16Big, uint16_t, uint8_t>(const uint16_t* source, uint8_t* destination, unsigned pixelsPerRow)
{
    for (unsigned i = 0; i < pixelsPerRow; ++i) {
        destination[0] = convertColor16BigTo8(source[0]);
        destination[1] = convertColor16BigTo8(source[1]);
        destination[2] = convertColor16BigTo8(source[2]);
        destination[3] = 0xFF;
        source += 3;
        destination += 4;
    }
}

template<> ALWAYS_INLINE void convert16BitFormatToRGBA8<GraphicsContextGL::DataFormat::ARGB16Little, uint16_t, uint8_t>(const uint16_t* source, uint8_t* destination, unsigned pixelsPerRow)
{
#if HAVE(ARM_NEON_INTRINSICS)
    SIMD::unpackOneRowOfARGB16LittleToRGBA8(source, destination, pixelsPerRow);
#endif
    for (unsigned i = 0; i < pixelsPerRow; ++i) {
        destination[0] = convertColor16LittleTo8(source[1]);
        destination[1] = convertColor16LittleTo8(source[2]);
        destination[2] = convertColor16LittleTo8(source[3]);
        destination[3] = convertColor16LittleTo8(source[0]);
        source += 4;
        destination += 4;
    }
}

template<> ALWAYS_INLINE void convert16BitFormatToRGBA8<GraphicsContextGL::DataFormat::ARGB16Big, uint16_t, uint8_t>(const uint16_t* source, uint8_t* destination, unsigned pixelsPerRow)
{
    for (unsigned i = 0; i < pixelsPerRow; ++i) {
        destination[0] = convertColor16BigTo8(source[1]);
        destination[1] = convertColor16BigTo8(source[2]);
        destination[2] = convertColor16BigTo8(source[3]);
        destination[3] = convertColor16BigTo8(source[0]);
        source += 4;
        destination += 4;
    }
}

template<> ALWAYS_INLINE void convert16BitFormatToRGBA8<GraphicsContextGL::DataFormat::R16Little, uint16_t, uint8_t>(const uint16_t* source, uint8_t* destination, unsigned pixelsPerRow)
{
    for (unsigned i = 0; i < pixelsPerRow; ++i) {
        destination[0] = convertColor16LittleTo8(source[0]);
        destination[1] = convertColor16LittleTo8(source[0]);
        destination[2] = convertColor16LittleTo8(source[0]);
        destination[3] = 0xFF;
        source += 1;
        destination += 4;
    }
}

template<> ALWAYS_INLINE void convert16BitFormatToRGBA8<GraphicsContextGL::DataFormat::R16Big, uint16_t, uint8_t>(const uint16_t* source, uint8_t* destination, unsigned pixelsPerRow)
{
    for (unsigned i = 0; i < pixelsPerRow; ++i) {
        destination[0] = convertColor16BigTo8(source[0]);
        destination[1] = convertColor16BigTo8(source[0]);
        destination[2] = convertColor16BigTo8(source[0]);
        destination[3] = 0xFF;
        source += 1;
        destination += 4;
    }
}

template<> ALWAYS_INLINE void convert16BitFormatToRGBA8<GraphicsContextGL::DataFormat::RA16Little, uint16_t, uint8_t>(const uint16_t* source, uint8_t* destination, unsigned pixelsPerRow)
{
    for (unsigned i = 0; i < pixelsPerRow; ++i) {
        destination[0] = convertColor16LittleTo8(source[0]);
        destination[1] = convertColor16LittleTo8(source[0]);
        destination[2] = convertColor16LittleTo8(source[0]);
        destination[3] = convertColor16LittleTo8(source[1]);
        source += 2;
        destination += 4;
    }
}

template<> ALWAYS_INLINE void convert16BitFormatToRGBA8<GraphicsContextGL::DataFormat::RA16Big, uint16_t, uint8_t>(const uint16_t* source, uint8_t* destination, unsigned pixelsPerRow)
{
    for (unsigned i = 0; i < pixelsPerRow; ++i) {
        destination[0] = convertColor16BigTo8(source[0]);
        destination[1] = convertColor16BigTo8(source[0]);
        destination[2] = convertColor16BigTo8(source[0]);
        destination[3] = convertColor16BigTo8(source[1]);
        source += 2;
        destination += 4;
    }
}

template<> ALWAYS_INLINE void convert16BitFormatToRGBA8<GraphicsContextGL::DataFormat::AR16Little, uint16_t, uint8_t>(const uint16_t* source, uint8_t* destination, unsigned pixelsPerRow)
{
    for (unsigned i = 0; i < pixelsPerRow; ++i) {
        destination[0] = convertColor16LittleTo8(source[1]);
        destination[1] = convertColor16LittleTo8(source[1]);
        destination[2] = convertColor16LittleTo8(source[1]);
        destination[3] = convertColor16LittleTo8(source[0]);
        source += 2;
        destination += 4;
    }
}

template<> ALWAYS_INLINE void convert16BitFormatToRGBA8<GraphicsContextGL::DataFormat::AR16Big, uint16_t, uint8_t>(const uint16_t* source, uint8_t* destination, unsigned pixelsPerRow)
{
    for (unsigned i = 0; i < pixelsPerRow; ++i) {
        destination[0] = convertColor16BigTo8(source[1]);
        destination[1] = convertColor16BigTo8(source[1]);
        destination[2] = convertColor16BigTo8(source[1]);
        destination[3] = convertColor16BigTo8(source[0]);
        source += 2;
        destination += 4;
    }
}

template<> ALWAYS_INLINE void convert16BitFormatToRGBA8<GraphicsContextGL::DataFormat::A16Little, uint16_t, uint8_t>(const uint16_t* source, uint8_t* destination, unsigned pixelsPerRow)
{
    for (unsigned i = 0; i < pixelsPerRow; ++i) {
        destination[0] = 0x0;
        destination[1] = 0x0;
        destination[2] = 0x0;
        destination[3] = convertColor16LittleTo8(source[0]);
        source += 1;
        destination += 4;
    }
}

template<> ALWAYS_INLINE void convert16BitFormatToRGBA8<GraphicsContextGL::DataFormat::A16Big, uint16_t, uint8_t>(const uint16_t* source, uint8_t* destination, unsigned pixelsPerRow)
{
    for (unsigned i = 0; i < pixelsPerRow; ++i) {
        destination[0] = 0x0;
        destination[1] = 0x0;
        destination[2] = 0x0;
        destination[3] = convertColor16BigTo8(source[0]);
        source += 1;
        destination += 4;
    }
}

void convert16BitFormatToRGBA8(GraphicsContextGL::DataFormat srcFormat, const uint16_t* source, uint8_t* destination, unsigned pixelsPerRow)
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

GraphicsContextGLOpenGL::ImageExtractor::~ImageExtractor() = default;

bool GraphicsContextGLOpenGL::ImageExtractor::extractImage(bool premultiplyAlpha, bool ignoreGammaAndColorProfile, bool ignoreNativeImageAlphaPremultiplication)
{
    if (!m_image)
        return false;
    bool hasAlpha = !m_image->currentFrameKnownToBeOpaque();
    if ((ignoreGammaAndColorProfile || (hasAlpha && !premultiplyAlpha)) && m_image->data()) {
        auto source = ImageSource::create(nullptr, AlphaOption::NotPremultiplied, ignoreGammaAndColorProfile ? GammaAndColorProfileOption::Ignored : GammaAndColorProfileOption::Applied);
        source->setData(m_image->data(), true);
        if (!source->frameCount())
            return false;

        m_decodedImage = source->createFrameImageAtIndex(0);
        m_cgImage = m_decodedImage;
    } else
        m_cgImage = m_image->nativeImageForCurrentFrame();
    if (!m_cgImage)
        return false;

    m_imageWidth = CGImageGetWidth(m_cgImage.get());
    m_imageHeight = CGImageGetHeight(m_cgImage.get());
    if (!m_imageWidth || !m_imageHeight)
        return false;

    // See whether the image is using an indexed color space, and if
    // so, re-render it into an RGB color space. The image re-packing
    // code requires color data, not color table indices, for the
    // image data.
    CGColorSpaceRef colorSpace = CGImageGetColorSpace(m_cgImage.get());
    CGColorSpaceModel model = CGColorSpaceGetModel(colorSpace);
    if (model == kCGColorSpaceModelIndexed) {
        RetainPtr<CGContextRef> bitmapContext;
        // FIXME: we should probably manually convert the image by indexing into
        // the color table, which would allow us to avoid premultiplying the
        // alpha channel. Creation of a bitmap context with an alpha channel
        // doesn't seem to work unless it's premultiplied.
        bitmapContext = adoptCF(CGBitmapContextCreate(0, m_imageWidth, m_imageHeight, 8, m_imageWidth * 4,
            sRGBColorSpaceRef(), kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Host));
        if (!bitmapContext)
            return false;

        CGContextSetBlendMode(bitmapContext.get(), kCGBlendModeCopy);
        CGContextSetInterpolationQuality(bitmapContext.get(), kCGInterpolationNone);
        CGContextDrawImage(bitmapContext.get(), CGRectMake(0, 0, m_imageWidth, m_imageHeight), m_cgImage.get());

        // Now discard the original CG image and replace it with a copy from the bitmap context.
        m_decodedImage = adoptCF(CGBitmapContextCreateImage(bitmapContext.get()));
        m_cgImage = m_decodedImage.get();
    }

    size_t bitsPerComponent = CGImageGetBitsPerComponent(m_cgImage.get());
    size_t bitsPerPixel = CGImageGetBitsPerPixel(m_cgImage.get());
    if (bitsPerComponent != 8 && bitsPerComponent != 16)
        return false;
    if (bitsPerPixel % bitsPerComponent)
        return false;
    size_t componentsPerPixel = bitsPerPixel / bitsPerComponent;

    CGBitmapInfo bitInfo = CGImageGetBitmapInfo(m_cgImage.get());
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

    m_alphaOp = AlphaOp::DoNothing;
    AlphaFormat alphaFormat = AlphaFormatNone;
    switch (CGImageGetAlphaInfo(m_cgImage.get())) {
    case kCGImageAlphaPremultipliedFirst:
        if (!premultiplyAlpha)
            m_alphaOp = AlphaOp::DoUnmultiply;
        else if (ignoreNativeImageAlphaPremultiplication)
            m_alphaOp = AlphaOp::DoPremultiply;
        alphaFormat = AlphaFormatFirst;
        break;
    case kCGImageAlphaFirst:
        // This path is only accessible for MacOS earlier than 10.6.4.
        if (premultiplyAlpha)
            m_alphaOp = AlphaOp::DoPremultiply;
        alphaFormat = AlphaFormatFirst;
        break;
    case kCGImageAlphaNoneSkipFirst:
        // This path is only accessible for MacOS earlier than 10.6.4.
        alphaFormat = AlphaFormatFirst;
        break;
    case kCGImageAlphaPremultipliedLast:
        if (!premultiplyAlpha)
            m_alphaOp = AlphaOp::DoUnmultiply;
        else if (ignoreNativeImageAlphaPremultiplication)
            m_alphaOp = AlphaOp::DoPremultiply;
        alphaFormat = AlphaFormatLast;
        break;
    case kCGImageAlphaLast:
        if (premultiplyAlpha)
            m_alphaOp = AlphaOp::DoPremultiply;
        alphaFormat = AlphaFormatLast;
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

    m_imageSourceFormat = getSourceDataFormat(componentsPerPixel, alphaFormat, bitsPerComponent == 16, bigEndianSource);
    if (m_imageSourceFormat == DataFormat::NumFormats)
        return false;

    m_pixelData = adoptCF(CGDataProviderCopyData(CGImageGetDataProvider(m_cgImage.get())));
    if (!m_pixelData)
        return false;

    m_imagePixelData = reinterpret_cast<const void*>(CFDataGetBytePtr(m_pixelData.get()));

    unsigned srcUnpackAlignment = 0;
    size_t bytesPerRow = CGImageGetBytesPerRow(m_cgImage.get());
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
        m_formalizedRGBA8Data = makeUniqueArray<uint8_t>((Checked<size_t>(m_imageWidth) * m_imageHeight * 4U).unsafeGet());
        const uint16_t* source = reinterpret_cast<const uint16_t*>(m_imagePixelData);
        uint8_t* destination = m_formalizedRGBA8Data.get();
        const ptrdiff_t srcStrideInElements = bytesPerRow / sizeof(uint16_t);
        const ptrdiff_t dstStrideInElements = 4 * m_imageWidth;
        for (unsigned i =0; i < m_imageHeight; i++) {
            convert16BitFormatToRGBA8(m_imageSourceFormat, source, destination, m_imageWidth);
            source += srcStrideInElements;
            destination += dstStrideInElements;
        }
        m_imagePixelData = reinterpret_cast<const void*>(m_formalizedRGBA8Data.get());
        m_imageSourceFormat = DataFormat::RGBA8;
        m_imageSourceUnpackAlignment = 1;
    }
    return true;
}

static void releaseImageData(void*, const void* data, size_t)
{
    fastFree(const_cast<void*>(data));
}

void GraphicsContextGLOpenGL::paintToCanvas(const unsigned char* imagePixels, const IntSize& imageSize, const IntSize& canvasSize, GraphicsContext& context)
{
    if (!imagePixels || imageSize.isEmpty() || canvasSize.isEmpty())
        return;
    int rowBytes = imageSize.width() * 4;
    RetainPtr<CGDataProviderRef> dataProvider;

    if (context.isAcceleratedContext()) {
        unsigned char* copiedPixels;

        if (!tryFastCalloc(imageSize.height(), rowBytes).getValue(copiedPixels))
            return;

        memcpy(copiedPixels, imagePixels, rowBytes * imageSize.height());

        size_t dataSize = rowBytes * imageSize.height();
        verifyImageBufferIsBigEnough(copiedPixels, dataSize);
        dataProvider = adoptCF(CGDataProviderCreateWithData(0, copiedPixels, dataSize, releaseImageData));
    } else {
        size_t dataSize = rowBytes * imageSize.height();
        verifyImageBufferIsBigEnough(imagePixels, dataSize);
        dataProvider = adoptCF(CGDataProviderCreateWithData(0, imagePixels, dataSize, 0));
    }

    RetainPtr<CGImageRef> cgImage = adoptCF(CGImageCreate(imageSize.width(), imageSize.height(), 8, 32, rowBytes, sRGBColorSpaceRef(), kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Host,
        dataProvider.get(), 0, false, kCGRenderingIntentDefault));

    // CSS styling may cause the canvas's content to be resized on
    // the page. Go back to the Canvas to figure out the correct
    // width and height to draw.
    FloatRect canvasRect(FloatPoint(), canvasSize);
    // We want to completely overwrite the previous frame's
    // rendering results.

    GraphicsContextStateSaver stateSaver(context);
    context.scale(FloatSize(1, -1));
    context.translate(0, -imageSize.height());
    context.setImageInterpolationQuality(InterpolationQuality::DoNotInterpolate);
    context.drawNativeImage(cgImage, imageSize, canvasRect, FloatRect(FloatPoint(), imageSize), { CompositeOperator::Copy });
}

} // namespace WebCore

#endif // ENABLE(GRAPHICS_CONTEXT_GL)
