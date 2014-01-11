/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2010 Mozilla Corporation. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"

#if USE(3D_GRAPHICS)

#include "GraphicsContext3D.h"

#include "Extensions3D.h"
#include "FormatConverter.h"
#include "Image.h"
#include "ImageData.h"
#include "ImageObserver.h"

namespace WebCore {

namespace {

GraphicsContext3D::DataFormat getDataFormat(GC3Denum destinationFormat, GC3Denum destinationType)
{
    GraphicsContext3D::DataFormat dstFormat = GraphicsContext3D::DataFormatRGBA8;
    switch (destinationType) {
    case GraphicsContext3D::UNSIGNED_BYTE:
        switch (destinationFormat) {
        case GraphicsContext3D::RGB:
            dstFormat = GraphicsContext3D::DataFormatRGB8;
            break;
        case GraphicsContext3D::RGBA:
            dstFormat = GraphicsContext3D::DataFormatRGBA8;
            break;
        case GraphicsContext3D::ALPHA:
            dstFormat = GraphicsContext3D::DataFormatA8;
            break;
        case GraphicsContext3D::LUMINANCE:
            dstFormat = GraphicsContext3D::DataFormatR8;
            break;
        case GraphicsContext3D::LUMINANCE_ALPHA:
            dstFormat = GraphicsContext3D::DataFormatRA8;
            break;
        default:
            ASSERT_NOT_REACHED();
        }
        break;
    case GraphicsContext3D::UNSIGNED_SHORT_4_4_4_4:
        dstFormat = GraphicsContext3D::DataFormatRGBA4444;
        break;
    case GraphicsContext3D::UNSIGNED_SHORT_5_5_5_1:
        dstFormat = GraphicsContext3D::DataFormatRGBA5551;
        break;
    case GraphicsContext3D::UNSIGNED_SHORT_5_6_5:
        dstFormat = GraphicsContext3D::DataFormatRGB565;
        break;
    case GraphicsContext3D::HALF_FLOAT_OES: // OES_texture_half_float
        switch (destinationFormat) {
        case GraphicsContext3D::RGB:
            dstFormat = GraphicsContext3D::DataFormatRGB16F;
            break;
        case GraphicsContext3D::RGBA:
            dstFormat = GraphicsContext3D::DataFormatRGBA16F;
            break;
        case GraphicsContext3D::ALPHA:
            dstFormat = GraphicsContext3D::DataFormatA16F;
            break;
        case GraphicsContext3D::LUMINANCE:
            dstFormat = GraphicsContext3D::DataFormatR16F;
            break;
        case GraphicsContext3D::LUMINANCE_ALPHA:
            dstFormat = GraphicsContext3D::DataFormatRA16F;
            break;
        default:
            ASSERT_NOT_REACHED();
        }
        break;
    case GraphicsContext3D::FLOAT: // OES_texture_float
        switch (destinationFormat) {
        case GraphicsContext3D::RGB:
            dstFormat = GraphicsContext3D::DataFormatRGB32F;
            break;
        case GraphicsContext3D::RGBA:
            dstFormat = GraphicsContext3D::DataFormatRGBA32F;
            break;
        case GraphicsContext3D::ALPHA:
            dstFormat = GraphicsContext3D::DataFormatA32F;
            break;
        case GraphicsContext3D::LUMINANCE:
            dstFormat = GraphicsContext3D::DataFormatR32F;
            break;
        case GraphicsContext3D::LUMINANCE_ALPHA:
            dstFormat = GraphicsContext3D::DataFormatRA32F;
            break;
        default:
            ASSERT_NOT_REACHED();
        }
        break;
    default:
        ASSERT_NOT_REACHED();
    }
    return dstFormat;
}

} // anonymous namespace

bool GraphicsContext3D::texImage2DResourceSafe(GC3Denum target, GC3Dint level, GC3Denum internalformat, GC3Dsizei width, GC3Dsizei height, GC3Dint border, GC3Denum format, GC3Denum type, GC3Dint unpackAlignment)
{
    ASSERT(unpackAlignment == 1 || unpackAlignment == 2 || unpackAlignment == 4 || unpackAlignment == 8);
    std::unique_ptr<unsigned char[]> zero;
    if (!isResourceSafe() && width > 0 && height > 0) {
        unsigned int size;
        GC3Denum error = computeImageSizeInBytes(format, type, width, height, unpackAlignment, &size, 0);
        if (error != GraphicsContext3D::NO_ERROR) {
            synthesizeGLError(error);
            return false;
        }
        zero = std::make_unique<unsigned char[]>(size);
        if (!zero) {
            synthesizeGLError(GraphicsContext3D::INVALID_VALUE);
            return false;
        }
        memset(zero.get(), 0, size);
    }
    return texImage2D(target, level, internalformat, width, height, border, format, type, zero.get());
}

bool GraphicsContext3D::computeFormatAndTypeParameters(GC3Denum format, GC3Denum type, unsigned int* componentsPerPixel, unsigned int* bytesPerComponent)
{
    switch (format) {
    case GraphicsContext3D::ALPHA:
    case GraphicsContext3D::LUMINANCE:
    case GraphicsContext3D::DEPTH_COMPONENT:
    case GraphicsContext3D::DEPTH_STENCIL:
        *componentsPerPixel = 1;
        break;
    case GraphicsContext3D::LUMINANCE_ALPHA:
        *componentsPerPixel = 2;
        break;
    case GraphicsContext3D::RGB:
        *componentsPerPixel = 3;
        break;
    case GraphicsContext3D::RGBA:
    case Extensions3D::BGRA_EXT: // GL_EXT_texture_format_BGRA8888
        *componentsPerPixel = 4;
        break;
    default:
        return false;
    }
    switch (type) {
    case GraphicsContext3D::UNSIGNED_BYTE:
        *bytesPerComponent = sizeof(GC3Dubyte);
        break;
    case GraphicsContext3D::UNSIGNED_SHORT:
        *bytesPerComponent = sizeof(GC3Dushort);
        break;
    case GraphicsContext3D::UNSIGNED_SHORT_5_6_5:
    case GraphicsContext3D::UNSIGNED_SHORT_4_4_4_4:
    case GraphicsContext3D::UNSIGNED_SHORT_5_5_5_1:
        *componentsPerPixel = 1;
        *bytesPerComponent = sizeof(GC3Dushort);
        break;
    case GraphicsContext3D::UNSIGNED_INT_24_8:
    case GraphicsContext3D::UNSIGNED_INT:
        *bytesPerComponent = sizeof(GC3Duint);
        break;
    case GraphicsContext3D::FLOAT: // OES_texture_float
        *bytesPerComponent = sizeof(GC3Dfloat);
        break;
    case GraphicsContext3D::HALF_FLOAT_OES: // OES_texture_half_float
        *bytesPerComponent = sizeof(GC3Dhalffloat);
        break;
    default:
        return false;
    }
    return true;
}

GC3Denum GraphicsContext3D::computeImageSizeInBytes(GC3Denum format, GC3Denum type, GC3Dsizei width, GC3Dsizei height, GC3Dint alignment, unsigned int* imageSizeInBytes, unsigned int* paddingInBytes)
{
    ASSERT(imageSizeInBytes);
    ASSERT(alignment == 1 || alignment == 2 || alignment == 4 || alignment == 8);
    if (width < 0 || height < 0)
        return GraphicsContext3D::INVALID_VALUE;
    unsigned int bytesPerComponent, componentsPerPixel;
    if (!computeFormatAndTypeParameters(format, type, &bytesPerComponent, &componentsPerPixel))
        return GraphicsContext3D::INVALID_ENUM;
    if (!width || !height) {
        *imageSizeInBytes = 0;
        if (paddingInBytes)
            *paddingInBytes = 0;
        return GraphicsContext3D::NO_ERROR;
    }
    Checked<uint32_t, RecordOverflow> checkedValue = bytesPerComponent * componentsPerPixel;
    checkedValue *=  width;
    if (checkedValue.hasOverflowed())
        return GraphicsContext3D::INVALID_VALUE;
    unsigned int validRowSize = checkedValue.unsafeGet();
    unsigned int padding = 0;
    unsigned int residual = validRowSize % alignment;
    if (residual) {
        padding = alignment - residual;
        checkedValue += padding;
    }
    // Last row needs no padding.
    checkedValue *= (height - 1);
    checkedValue += validRowSize;
    if (checkedValue.hasOverflowed())
        return GraphicsContext3D::INVALID_VALUE;
    *imageSizeInBytes = checkedValue.unsafeGet();
    if (paddingInBytes)
        *paddingInBytes = padding;
    return GraphicsContext3D::NO_ERROR;
}

GraphicsContext3D::ImageExtractor::ImageExtractor(Image* image, ImageHtmlDomSource imageHtmlDomSource, bool premultiplyAlpha, bool ignoreGammaAndColorProfile)
{
    m_image = image;
    m_imageHtmlDomSource = imageHtmlDomSource;
    m_extractSucceeded = extractImage(premultiplyAlpha, ignoreGammaAndColorProfile);
}

bool GraphicsContext3D::packImageData( Image* image, const void* pixels, GC3Denum format, GC3Denum type, bool flipY, AlphaOp alphaOp, DataFormat sourceFormat, unsigned width, unsigned height, unsigned sourceUnpackAlignment, Vector<uint8_t>& data)
{
    if (!pixels)
        return false;

    unsigned packedSize;
    // Output data is tightly packed (alignment == 1).
    if (computeImageSizeInBytes(format, type, width, height, 1, &packedSize, 0) != GraphicsContext3D::NO_ERROR)
        return false;
    data.resize(packedSize);

    if (!packPixels(reinterpret_cast<const uint8_t*>(pixels), sourceFormat, width, height, sourceUnpackAlignment, format, type, alphaOp, data.data(), flipY))
        return false;
    if (ImageObserver* observer = image->imageObserver())
        observer->didDraw(image);
    return true;
}

bool GraphicsContext3D::extractImageData(ImageData* imageData, GC3Denum format, GC3Denum type, bool flipY, bool premultiplyAlpha, Vector<uint8_t>& data)
{
    if (!imageData)
        return false;
    int width = imageData->width();
    int height = imageData->height();

    unsigned int packedSize;
    // Output data is tightly packed (alignment == 1).
    if (computeImageSizeInBytes(format, type, width, height, 1, &packedSize, 0) != GraphicsContext3D::NO_ERROR)
        return false;
    data.resize(packedSize);

    if (!packPixels(imageData->data()->data(), DataFormatRGBA8, width, height, 0, format, type, premultiplyAlpha ? AlphaDoPremultiply : AlphaDoNothing, data.data(), flipY))
        return false;

    return true;
}

bool GraphicsContext3D::extractTextureData(unsigned int width, unsigned int height, GC3Denum format, GC3Denum type, unsigned int unpackAlignment, bool flipY, bool premultiplyAlpha, const void* pixels, Vector<uint8_t>& data)
{
    // Assumes format, type, etc. have already been validated.
    DataFormat sourceDataFormat = getDataFormat(format, type);

    // Resize the output buffer.
    unsigned int componentsPerPixel, bytesPerComponent;
    if (!computeFormatAndTypeParameters(format, type, &componentsPerPixel, &bytesPerComponent))
        return false;
    unsigned int bytesPerPixel = componentsPerPixel * bytesPerComponent;
    data.resize(width * height * bytesPerPixel);

    if (!packPixels(static_cast<const uint8_t*>(pixels), sourceDataFormat, width, height, unpackAlignment, format, type, (premultiplyAlpha ? AlphaDoPremultiply : AlphaDoNothing), data.data(), flipY))
        return false;

    return true;
}

ALWAYS_INLINE unsigned TexelBytesForFormat(GraphicsContext3D::DataFormat format)
{
    switch (format) {
    case GraphicsContext3D::DataFormatR8:
    case GraphicsContext3D::DataFormatA8:
        return 1;
    case GraphicsContext3D::DataFormatRA8:
    case GraphicsContext3D::DataFormatAR8:
    case GraphicsContext3D::DataFormatRGBA5551:
    case GraphicsContext3D::DataFormatRGBA4444:
    case GraphicsContext3D::DataFormatRGB565:
    case GraphicsContext3D::DataFormatA16F:
    case GraphicsContext3D::DataFormatR16F:
        return 2;
    case GraphicsContext3D::DataFormatRGB8:
    case GraphicsContext3D::DataFormatBGR8:
        return 3;
    case GraphicsContext3D::DataFormatRGBA8:
    case GraphicsContext3D::DataFormatARGB8:
    case GraphicsContext3D::DataFormatABGR8:
    case GraphicsContext3D::DataFormatBGRA8:
    case GraphicsContext3D::DataFormatR32F:
    case GraphicsContext3D::DataFormatA32F:
    case GraphicsContext3D::DataFormatRA16F:
        return 4;
    case GraphicsContext3D::DataFormatRGB16F:
        return 6;
    case GraphicsContext3D::DataFormatRA32F:
    case GraphicsContext3D::DataFormatRGBA16F:
        return 8;
    case GraphicsContext3D::DataFormatRGB32F:
        return 12;
    case GraphicsContext3D::DataFormatRGBA32F:
        return 16;
    default:
        return 0;
    }
}

bool GraphicsContext3D::packPixels(const uint8_t* sourceData, DataFormat sourceDataFormat, unsigned width, unsigned height, unsigned sourceUnpackAlignment, unsigned destinationFormat, unsigned destinationType, AlphaOp alphaOp, void* destinationData, bool flipY)
{
    int validSrc = width * TexelBytesForFormat(sourceDataFormat);
    int remainder = sourceUnpackAlignment ? (validSrc % sourceUnpackAlignment) : 0;
    int srcStride = remainder ? (validSrc + sourceUnpackAlignment - remainder) : validSrc;

    DataFormat dstDataFormat = getDataFormat(destinationFormat, destinationType);
    int dstStride = width * TexelBytesForFormat(dstDataFormat);
    if (flipY) {
        destinationData = static_cast<uint8_t*>(destinationData) + dstStride*(height - 1);
        dstStride = -dstStride;
    }
    if (!hasAlpha(sourceDataFormat) || !hasColor(sourceDataFormat) || !hasColor(dstDataFormat))
        alphaOp = AlphaDoNothing;

    if (sourceDataFormat == dstDataFormat && alphaOp == AlphaDoNothing) {
        const uint8_t* ptr = sourceData;
        const uint8_t* ptrEnd = sourceData + srcStride * height;
        unsigned rowSize = (dstStride > 0) ? dstStride: -dstStride;
        uint8_t* dst = static_cast<uint8_t*>(destinationData);
        while (ptr < ptrEnd) {
            memcpy(dst, ptr, rowSize);
            ptr += srcStride;
            dst += dstStride;
        }
        return true;
    }

    FormatConverter converter(width, height, sourceData, destinationData, srcStride, dstStride);
    converter.convert(sourceDataFormat, dstDataFormat, alphaOp);
    if (!converter.success())
        return false;
    return true;
}

unsigned GraphicsContext3D::getClearBitsByAttachmentType(GC3Denum attachment)
{
    switch (attachment) {
    case GraphicsContext3D::COLOR_ATTACHMENT0:
    case Extensions3D::COLOR_ATTACHMENT1_EXT:
    case Extensions3D::COLOR_ATTACHMENT2_EXT:
    case Extensions3D::COLOR_ATTACHMENT3_EXT:
    case Extensions3D::COLOR_ATTACHMENT4_EXT:
    case Extensions3D::COLOR_ATTACHMENT5_EXT:
    case Extensions3D::COLOR_ATTACHMENT6_EXT:
    case Extensions3D::COLOR_ATTACHMENT7_EXT:
    case Extensions3D::COLOR_ATTACHMENT8_EXT:
    case Extensions3D::COLOR_ATTACHMENT9_EXT:
    case Extensions3D::COLOR_ATTACHMENT10_EXT:
    case Extensions3D::COLOR_ATTACHMENT11_EXT:
    case Extensions3D::COLOR_ATTACHMENT12_EXT:
    case Extensions3D::COLOR_ATTACHMENT13_EXT:
    case Extensions3D::COLOR_ATTACHMENT14_EXT:
    case Extensions3D::COLOR_ATTACHMENT15_EXT:
        return GraphicsContext3D::COLOR_BUFFER_BIT;
    case GraphicsContext3D::DEPTH_ATTACHMENT:
        return GraphicsContext3D::DEPTH_BUFFER_BIT;
    case GraphicsContext3D::STENCIL_ATTACHMENT:
        return GraphicsContext3D::STENCIL_BUFFER_BIT;
    case GraphicsContext3D::DEPTH_STENCIL_ATTACHMENT:
        return GraphicsContext3D::DEPTH_BUFFER_BIT | GraphicsContext3D::STENCIL_BUFFER_BIT;
    default:
        return 0;
    }
}

unsigned GraphicsContext3D::getClearBitsByFormat(GC3Denum format)
{
    switch (format) {
    case GraphicsContext3D::ALPHA:
    case GraphicsContext3D::LUMINANCE:
    case GraphicsContext3D::LUMINANCE_ALPHA:
    case GraphicsContext3D::RGB:
    case GraphicsContext3D::RGB565:
    case GraphicsContext3D::RGBA:
    case GraphicsContext3D::RGBA4:
    case GraphicsContext3D::RGB5_A1:
        return GraphicsContext3D::COLOR_BUFFER_BIT;
    case GraphicsContext3D::DEPTH_COMPONENT16:
    case GraphicsContext3D::DEPTH_COMPONENT:
        return GraphicsContext3D::DEPTH_BUFFER_BIT;
    case GraphicsContext3D::STENCIL_INDEX8:
        return GraphicsContext3D::STENCIL_BUFFER_BIT;
    case GraphicsContext3D::DEPTH_STENCIL:
        return GraphicsContext3D::DEPTH_BUFFER_BIT | GraphicsContext3D::STENCIL_BUFFER_BIT;
    default:
        return 0;
    }
}

unsigned GraphicsContext3D::getChannelBitsByFormat(GC3Denum format)
{
    switch (format) {
    case GraphicsContext3D::ALPHA:
        return ChannelAlpha;
    case GraphicsContext3D::LUMINANCE:
        return ChannelRGB;
    case GraphicsContext3D::LUMINANCE_ALPHA:
        return ChannelRGBA;
    case GraphicsContext3D::RGB:
    case GraphicsContext3D::RGB565:
        return ChannelRGB;
    case GraphicsContext3D::RGBA:
    case GraphicsContext3D::RGBA4:
    case GraphicsContext3D::RGB5_A1:
        return ChannelRGBA;
    case GraphicsContext3D::DEPTH_COMPONENT16:
    case GraphicsContext3D::DEPTH_COMPONENT:
        return ChannelDepth;
    case GraphicsContext3D::STENCIL_INDEX8:
        return ChannelStencil;
    case GraphicsContext3D::DEPTH_STENCIL:
        return ChannelDepth | ChannelStencil;
    default:
        return 0;
    }
}

} // namespace WebCore

#endif // USE(3D_GRAPHICS)
