/*
 * Copyright (C) 2010, 2016 Apple Inc. All rights reserved.
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
#include "GraphicsContextGL.h"

#if ENABLE(WEBGL)

#include "FormatConverter.h"
#include "HostWindow.h"
#include "Image.h"
#include "ImageObserver.h"
#include "PixelBuffer.h"

namespace WebCore {

static GraphicsContextGL::DataFormat getDataFormat(GCGLenum destinationFormat, GCGLenum destinationType)
{
    switch (destinationType) {
    case GraphicsContextGL::BYTE:
        switch (destinationFormat) {
        case GraphicsContextGL::RED:
        case GraphicsContextGL::RED_INTEGER:
            return GraphicsContextGL::DataFormat::R8_S;
        case GraphicsContextGL::RG:
        case GraphicsContextGL::RG_INTEGER:
            return GraphicsContextGL::DataFormat::RG8_S;
        case GraphicsContextGL::RGB:
        case GraphicsContextGL::RGB_INTEGER:
            return GraphicsContextGL::DataFormat::RGB8_S;
        case GraphicsContextGL::RGBA:
        case GraphicsContextGL::RGBA_INTEGER:
            return GraphicsContextGL::DataFormat::RGBA8_S;
        default:
            return GraphicsContextGL::DataFormat::Invalid;
        }
    case GraphicsContextGL::UNSIGNED_BYTE:
        switch (destinationFormat) {
        case GraphicsContextGL::RGB:
        case GraphicsContextGL::RGB_INTEGER:
        case GraphicsContextGL::SRGB:
            return GraphicsContextGL::GraphicsContextGL::DataFormat::RGB8;
        case GraphicsContextGL::RGBA:
        case GraphicsContextGL::RGBA_INTEGER:
        case GraphicsContextGL::SRGB_ALPHA:
            return GraphicsContextGL::DataFormat::RGBA8;
        case GraphicsContextGL::ALPHA:
            return GraphicsContextGL::DataFormat::A8;
        case GraphicsContextGL::LUMINANCE:
        case GraphicsContextGL::RED:
        case GraphicsContextGL::RED_INTEGER:
            return GraphicsContextGL::DataFormat::R8;
        case GraphicsContextGL::RG:
        case GraphicsContextGL::RG_INTEGER:
            return GraphicsContextGL::DataFormat::RG8;
        case GraphicsContextGL::LUMINANCE_ALPHA:
            return GraphicsContextGL::DataFormat::RA8;
        default:
            return GraphicsContextGL::DataFormat::Invalid;
        }
    case GraphicsContextGL::SHORT:
        switch (destinationFormat) {
        case GraphicsContextGL::RED_INTEGER:
            return GraphicsContextGL::DataFormat::R16_S;
        case GraphicsContextGL::RG_INTEGER:
            return GraphicsContextGL::DataFormat::RG16_S;
        case GraphicsContextGL::RGB_INTEGER:
            return GraphicsContextGL::DataFormat::RGB16_S;
        case GraphicsContextGL::RGBA_INTEGER:
            return GraphicsContextGL::DataFormat::RGBA16_S;
        default:
            return GraphicsContextGL::DataFormat::Invalid;
        }
    case GraphicsContextGL::UNSIGNED_SHORT:
        switch (destinationFormat) {
        case GraphicsContextGL::RED_INTEGER:
            return GraphicsContextGL::DataFormat::R16;
        case GraphicsContextGL::DEPTH_COMPONENT:
            return GraphicsContextGL::DataFormat::D16;
        case GraphicsContextGL::RG_INTEGER:
            return GraphicsContextGL::DataFormat::RG16;
        case GraphicsContextGL::RGB_INTEGER:
            return GraphicsContextGL::DataFormat::RGB16;
        case GraphicsContextGL::RGBA_INTEGER:
            return GraphicsContextGL::DataFormat::RGBA16;
        default:
            return GraphicsContextGL::DataFormat::Invalid;
        }
    case GraphicsContextGL::INT:
        switch (destinationFormat) {
        case GraphicsContextGL::RED_INTEGER:
            return GraphicsContextGL::DataFormat::R32_S;
        case GraphicsContextGL::RG_INTEGER:
            return GraphicsContextGL::DataFormat::RG32_S;
        case GraphicsContextGL::RGB_INTEGER:
            return GraphicsContextGL::DataFormat::RGB32_S;
        case GraphicsContextGL::RGBA_INTEGER:
            return GraphicsContextGL::DataFormat::RGBA32_S;
        default:
            return GraphicsContextGL::DataFormat::Invalid;
        }
    case GraphicsContextGL::UNSIGNED_INT:
        switch (destinationFormat) {
        case GraphicsContextGL::RED_INTEGER:
            return GraphicsContextGL::DataFormat::R32;
        case GraphicsContextGL::DEPTH_COMPONENT:
            return GraphicsContextGL::DataFormat::D32;
        case GraphicsContextGL::RG_INTEGER:
            return GraphicsContextGL::DataFormat::RG32;
        case GraphicsContextGL::RGB_INTEGER:
            return GraphicsContextGL::DataFormat::RGB32;
        case GraphicsContextGL::RGBA_INTEGER:
            return GraphicsContextGL::DataFormat::RGBA32;
        default:
            return GraphicsContextGL::DataFormat::Invalid;
        }
    case GraphicsContextGL::HALF_FLOAT_OES: // OES_texture_half_float
    case GraphicsContextGL::HALF_FLOAT:
        switch (destinationFormat) {
        case GraphicsContextGL::RGBA:
            return GraphicsContextGL::DataFormat::RGBA16F;
        case GraphicsContextGL::RGB:
            return GraphicsContextGL::DataFormat::RGB16F;
        case GraphicsContextGL::RG:
            return GraphicsContextGL::DataFormat::RG16F;
        case GraphicsContextGL::ALPHA:
            return GraphicsContextGL::DataFormat::A16F;
        case GraphicsContextGL::LUMINANCE:
        case GraphicsContextGL::RED:
            return GraphicsContextGL::DataFormat::R16F;
        case GraphicsContextGL::LUMINANCE_ALPHA:
            return GraphicsContextGL::DataFormat::RA16F;
        case GraphicsContextGL::SRGB:
            return GraphicsContextGL::DataFormat::RGB16F;
        case GraphicsContextGL::SRGB_ALPHA:
            return GraphicsContextGL::DataFormat::RGBA16F;
        default:
            return GraphicsContextGL::DataFormat::Invalid;
        }
    case GraphicsContextGL::FLOAT: // OES_texture_float
        switch (destinationFormat) {
        case GraphicsContextGL::RGBA:
            return GraphicsContextGL::DataFormat::RGBA32F;
        case GraphicsContextGL::RGB:
            return GraphicsContextGL::DataFormat::RGB32F;
        case GraphicsContextGL::RG:
            return GraphicsContextGL::DataFormat::RG32F;
        case GraphicsContextGL::ALPHA:
            return GraphicsContextGL::DataFormat::A32F;
        case GraphicsContextGL::LUMINANCE:
        case GraphicsContextGL::RED:
            return GraphicsContextGL::DataFormat::R32F;
        case GraphicsContextGL::DEPTH_COMPONENT:
            return GraphicsContextGL::DataFormat::D32F;
        case GraphicsContextGL::LUMINANCE_ALPHA:
            return GraphicsContextGL::DataFormat::RA32F;
        case GraphicsContextGL::SRGB:
            return GraphicsContextGL::DataFormat::RGB32F;
        case GraphicsContextGL::SRGB_ALPHA:
            return GraphicsContextGL::DataFormat::RGBA32F;
        default:
            return GraphicsContextGL::DataFormat::Invalid;
        }
    case GraphicsContextGL::UNSIGNED_SHORT_4_4_4_4:
        return GraphicsContextGL::DataFormat::RGBA4444;
    case GraphicsContextGL::UNSIGNED_SHORT_5_5_5_1:
        return GraphicsContextGL::DataFormat::RGBA5551;
    case GraphicsContextGL::UNSIGNED_SHORT_5_6_5:
        return GraphicsContextGL::DataFormat::RGB565;
    case GraphicsContextGL::UNSIGNED_INT_5_9_9_9_REV:
        return GraphicsContextGL::DataFormat::RGB5999;
    case GraphicsContextGL::UNSIGNED_INT_24_8:
        return GraphicsContextGL::DataFormat::DS24_8;
    case GraphicsContextGL::UNSIGNED_INT_10F_11F_11F_REV:
        return GraphicsContextGL::DataFormat::RGB10F11F11F;
    case GraphicsContextGL::UNSIGNED_INT_2_10_10_10_REV:
        return GraphicsContextGL::DataFormat::RGBA2_10_10_10;
    default:
        return GraphicsContextGL::DataFormat::Invalid;
    }
    ASSERT_NOT_REACHED();
    return GraphicsContextGL::DataFormat::Invalid;
}

ALWAYS_INLINE static unsigned texelBytesForFormat(GraphicsContextGL::DataFormat format)
{
    switch (format) {
    case GraphicsContextGL::DataFormat::R8:
    case GraphicsContextGL::DataFormat::R8_S:
    case GraphicsContextGL::DataFormat::A8:
        return 1;
    case GraphicsContextGL::DataFormat::RG8:
    case GraphicsContextGL::DataFormat::RG8_S:
    case GraphicsContextGL::DataFormat::RA8:
    case GraphicsContextGL::DataFormat::AR8:
    case GraphicsContextGL::DataFormat::RGBA5551:
    case GraphicsContextGL::DataFormat::RGBA4444:
    case GraphicsContextGL::DataFormat::RGB565:
    case GraphicsContextGL::DataFormat::A16F:
    case GraphicsContextGL::DataFormat::R16:
    case GraphicsContextGL::DataFormat::R16_S:
    case GraphicsContextGL::DataFormat::R16F:
    case GraphicsContextGL::DataFormat::D16:
        return 2;
    case GraphicsContextGL::DataFormat::RGB8:
    case GraphicsContextGL::DataFormat::RGB8_S:
    case GraphicsContextGL::DataFormat::BGR8:
        return 3;
    case GraphicsContextGL::DataFormat::RGBA8:
    case GraphicsContextGL::DataFormat::RGBA8_S:
    case GraphicsContextGL::DataFormat::ARGB8:
    case GraphicsContextGL::DataFormat::ABGR8:
    case GraphicsContextGL::DataFormat::BGRA8:
    case GraphicsContextGL::DataFormat::R32:
    case GraphicsContextGL::DataFormat::R32_S:
    case GraphicsContextGL::DataFormat::R32F:
    case GraphicsContextGL::DataFormat::A32F:
    case GraphicsContextGL::DataFormat::RA16F:
    case GraphicsContextGL::DataFormat::RGBA2_10_10_10:
    case GraphicsContextGL::DataFormat::RGB10F11F11F:
    case GraphicsContextGL::DataFormat::RGB5999:
    case GraphicsContextGL::DataFormat::RG16:
    case GraphicsContextGL::DataFormat::RG16_S:
    case GraphicsContextGL::DataFormat::RG16F:
    case GraphicsContextGL::DataFormat::D32:
    case GraphicsContextGL::DataFormat::D32F:
    case GraphicsContextGL::DataFormat::DS24_8:
        return 4;
    case GraphicsContextGL::DataFormat::RGB16:
    case GraphicsContextGL::DataFormat::RGB16_S:
    case GraphicsContextGL::DataFormat::RGB16F:
        return 6;
    case GraphicsContextGL::DataFormat::RGBA16:
    case GraphicsContextGL::DataFormat::RGBA16_S:
    case GraphicsContextGL::DataFormat::RA32F:
    case GraphicsContextGL::DataFormat::RGBA16F:
    case GraphicsContextGL::DataFormat::RG32:
    case GraphicsContextGL::DataFormat::RG32_S:
    case GraphicsContextGL::DataFormat::RG32F:
        return 8;
    case GraphicsContextGL::DataFormat::RGB32:
    case GraphicsContextGL::DataFormat::RGB32_S:
    case GraphicsContextGL::DataFormat::RGB32F:
        return 12;
    case GraphicsContextGL::DataFormat::RGBA32:
    case GraphicsContextGL::DataFormat::RGBA32_S:
    case GraphicsContextGL::DataFormat::RGBA32F:
        return 16;
    default:
        return 0;
    }
}


// Helper for packImageData/extractImageData/extractTextureData which implement packing of pixel
// data into the specified OpenGL destination format and type.
// A sourceUnpackAlignment of zero indicates that the source
// data is tightly packed. Non-zero values may take a slow path.
// Destination data will have no gaps between rows.
static bool packPixels(const uint8_t* sourceData, GraphicsContextGL::DataFormat sourceDataFormat, unsigned sourceDataWidth, unsigned sourceDataHeight, const IntRect& sourceDataSubRectangle, int depth, unsigned sourceUnpackAlignment, int unpackImageHeight, unsigned destinationFormat, unsigned destinationType, GraphicsContextGL::AlphaOp alphaOp, void* destinationData, bool flipY)
{
    ASSERT(depth >= 1);
    UNUSED_PARAM(sourceDataHeight); // Derived from sourceDataSubRectangle.height().
    if (!unpackImageHeight)
        unpackImageHeight = sourceDataSubRectangle.height();
    int validSrc = sourceDataWidth * texelBytesForFormat(sourceDataFormat);
    int remainder = sourceUnpackAlignment ? (validSrc % sourceUnpackAlignment) : 0;
    int srcStride = remainder ? (validSrc + sourceUnpackAlignment - remainder) : validSrc;
    int srcRowOffset = sourceDataSubRectangle.x() * texelBytesForFormat(sourceDataFormat);

    GraphicsContextGL::DataFormat dstDataFormat = getDataFormat(destinationFormat, destinationType);
    if (dstDataFormat == GraphicsContextGL::DataFormat::Invalid)
        return false;
    int dstStride = sourceDataSubRectangle.width() * texelBytesForFormat(dstDataFormat);
    if (flipY) {
        destinationData = static_cast<uint8_t*>(destinationData) + dstStride * ((depth * sourceDataSubRectangle.height()) - 1);
        dstStride = -dstStride;
    }
    if (!GraphicsContextGL::hasAlpha(sourceDataFormat) || !GraphicsContextGL::hasColor(sourceDataFormat) || !GraphicsContextGL::hasColor(dstDataFormat))
        alphaOp = GraphicsContextGL::AlphaOp::DoNothing;

    if (sourceDataFormat == dstDataFormat && alphaOp == GraphicsContextGL::AlphaOp::DoNothing) {
        const uint8_t* basePtr = sourceData + srcStride * sourceDataSubRectangle.y();
        const uint8_t* baseEnd = sourceData + srcStride * sourceDataSubRectangle.maxY();

        // If packing multiple images into a 3D texture, and flipY is true,
        // then the sub-rectangle is pointing at the start of the
        // "bottommost" of those images. Since the source pointer strides in
        // the positive direction, we need to back it up to point at the
        // last, or "topmost", of these images.
        if (flipY && depth > 1) {
            const ptrdiff_t distanceToTopImage = (depth - 1) * srcStride * unpackImageHeight;
            basePtr -= distanceToTopImage;
            baseEnd -= distanceToTopImage;
        }

        unsigned rowSize = (dstStride > 0) ? dstStride: -dstStride;
        uint8_t* dst = static_cast<uint8_t*>(destinationData);

        for (int i = 0; i < depth; ++i) {
            const uint8_t* ptr = basePtr;
            const uint8_t* ptrEnd = baseEnd;
            while (ptr < ptrEnd) {
                memcpy(dst, ptr + srcRowOffset, rowSize);
                ptr += srcStride;
                dst += dstStride;
            }
            basePtr += unpackImageHeight * srcStride;
            baseEnd += unpackImageHeight * srcStride;
        }
        return true;
    }

    FormatConverter converter(
        sourceDataSubRectangle, depth,
        unpackImageHeight, sourceData, destinationData,
        srcStride, srcRowOffset, dstStride);
    converter.convert(sourceDataFormat, dstDataFormat, alphaOp);
    if (!converter.success())
        return false;
    return true;
}

GraphicsContextGL::Client::Client() = default;

GraphicsContextGL::Client::~Client() = default;

GraphicsContextGL::GraphicsContextGL(GraphicsContextGLAttributes attrs)
    : m_attrs(attrs)
{
}

GraphicsContextGL::~GraphicsContextGL() = default;

bool GraphicsContextGL::computeFormatAndTypeParameters(GCGLenum format, GCGLenum type, unsigned* componentsPerPixel, unsigned* bytesPerComponent)
{
    switch (format) {
    case GraphicsContextGL::ALPHA:
    case GraphicsContextGL::LUMINANCE:
    case GraphicsContextGL::RED:
    case GraphicsContextGL::RED_INTEGER:
    case GraphicsContextGL::DEPTH_COMPONENT:
    case GraphicsContextGL::DEPTH_STENCIL: // Treat it as one component.
        *componentsPerPixel = 1;
        break;
    case GraphicsContextGL::LUMINANCE_ALPHA:
    case GraphicsContextGL::RG:
    case GraphicsContextGL::RG_INTEGER:
        *componentsPerPixel = 2;
        break;
    case GraphicsContextGL::RGB:
    case GraphicsContextGL::RGB_INTEGER:
    case GraphicsContextGL::SRGB_EXT:
        *componentsPerPixel = 3;
        break;
    case GraphicsContextGL::RGBA:
    case GraphicsContextGL::RGBA_INTEGER:
    case GraphicsContextGL::BGRA_EXT: // GL_EXT_texture_format_BGRA8888
    case GraphicsContextGL::SRGB_ALPHA_EXT:
        *componentsPerPixel = 4;
        break;
    default:
        return false;
    }

    switch (type) {
    case GraphicsContextGL::BYTE:
        *bytesPerComponent = sizeof(GCGLbyte);
        break;
    case GraphicsContextGL::UNSIGNED_BYTE:
        *bytesPerComponent = sizeof(GCGLubyte);
        break;
    case GraphicsContextGL::SHORT:
        *bytesPerComponent = sizeof(GCGLshort);
        break;
    case GraphicsContextGL::UNSIGNED_SHORT:
        *bytesPerComponent = sizeof(GCGLushort);
        break;
    case GraphicsContextGL::UNSIGNED_SHORT_5_6_5:
    case GraphicsContextGL::UNSIGNED_SHORT_4_4_4_4:
    case GraphicsContextGL::UNSIGNED_SHORT_5_5_5_1:
        *componentsPerPixel = 1;
        *bytesPerComponent = sizeof(GCGLushort);
        break;
    case GraphicsContextGL::INT:
        *bytesPerComponent = sizeof(GCGLint);
        break;
    case GraphicsContextGL::UNSIGNED_INT:
        *bytesPerComponent = sizeof(GCGLuint);
        break;
    case GraphicsContextGL::UNSIGNED_INT_24_8:
    case GraphicsContextGL::UNSIGNED_INT_10F_11F_11F_REV:
    case GraphicsContextGL::UNSIGNED_INT_5_9_9_9_REV:
    case GraphicsContextGL::UNSIGNED_INT_2_10_10_10_REV:
        *componentsPerPixel = 1;
        *bytesPerComponent = sizeof(GCGLuint);
        break;
    case GraphicsContextGL::FLOAT: // OES_texture_float
        *bytesPerComponent = sizeof(GCGLfloat);
        break;
    case GraphicsContextGL::HALF_FLOAT:
    case GraphicsContextGL::HALF_FLOAT_OES: // OES_texture_half_float
        *bytesPerComponent = sizeof(GCGLhalffloat);
        break;
    case GraphicsContextGL::FLOAT_32_UNSIGNED_INT_24_8_REV:
        *bytesPerComponent = sizeof(GCGLfloat) + sizeof(GCGLuint);
        break;
    default:
        return false;
    }
    return true;
}

GCGLenum GraphicsContextGL::computeImageSizeInBytes(GCGLenum format, GCGLenum type, GCGLsizei width, GCGLsizei height, GCGLsizei depth, const PixelStoreParams& params, unsigned* imageSizeInBytes, unsigned* paddingInBytes, unsigned* skipSizeInBytes)
{
    ASSERT(imageSizeInBytes);
    ASSERT(params.alignment == 1 || params.alignment == 2 || params.alignment == 4 || params.alignment == 8);
    ASSERT(params.rowLength >= 0);
    ASSERT(params.imageHeight >= 0);
    ASSERT(params.skipPixels >= 0);
    ASSERT(params.skipRows >= 0);
    ASSERT(params.skipImages >= 0);
    if (width < 0 || height < 0 || depth < 0)
        return GraphicsContextGL::INVALID_VALUE;
    if (!width || !height || !depth) {
        *imageSizeInBytes = 0;
        if (paddingInBytes)
            *paddingInBytes = 0;
        if (skipSizeInBytes)
            *skipSizeInBytes = 0;
        return GraphicsContextGL::NO_ERROR;
    }

    int rowLength = params.rowLength > 0 ? params.rowLength : width;
    int imageHeight = params.imageHeight > 0 ? params.imageHeight : height;

    unsigned bytesPerComponent, componentsPerPixel;
    if (!computeFormatAndTypeParameters(format, type, &componentsPerPixel, &bytesPerComponent))
        return GraphicsContextGL::INVALID_ENUM;
    unsigned bytesPerGroup = bytesPerComponent * componentsPerPixel;
    CheckedUint32 checkedValue = static_cast<uint32_t>(rowLength);
    checkedValue *= bytesPerGroup;
    if (checkedValue.hasOverflowed())
        return GraphicsContextGL::INVALID_VALUE;

    unsigned lastRowSize;
    if (params.rowLength > 0 && params.rowLength != width) {
        CheckedUint32 tmp = width;
        tmp *= bytesPerGroup;
        if (tmp.hasOverflowed())
            return GraphicsContextGL::INVALID_VALUE;
        lastRowSize = tmp;
    } else
        lastRowSize = checkedValue;

    unsigned padding = 0;
    unsigned residual = checkedValue.value() % params.alignment;
    if (residual) {
        padding = params.alignment - residual;
        checkedValue += padding;
    }
    if (checkedValue.hasOverflowed())
        return GraphicsContextGL::INVALID_VALUE;
    unsigned paddedRowSize = checkedValue;

    CheckedUint32 rows = imageHeight;
    rows *= (depth - 1);
    // Last image is not affected by IMAGE_HEIGHT parameter.
    rows += height;
    if (rows.hasOverflowed())
        return GraphicsContextGL::INVALID_VALUE;
    checkedValue *= (rows - 1);
    // Last row is not affected by ROW_LENGTH parameter.
    checkedValue += lastRowSize;
    if (checkedValue.hasOverflowed())
        return GraphicsContextGL::INVALID_VALUE;
    *imageSizeInBytes = checkedValue;
    if (paddingInBytes)
        *paddingInBytes = padding;

    CheckedUint32 skipSize = 0;
    if (params.skipImages > 0) {
        CheckedUint32 tmp = paddedRowSize;
        tmp *= imageHeight;
        tmp *= params.skipImages;
        if (tmp.hasOverflowed())
            return GraphicsContextGL::INVALID_VALUE;
        skipSize += tmp;
    }
    if (params.skipRows > 0) {
        CheckedUint32 tmp = paddedRowSize;
        tmp *= params.skipRows;
        if (tmp.hasOverflowed())
            return GraphicsContextGL::INVALID_VALUE;
        skipSize += tmp;
    }
    if (params.skipPixels > 0) {
        CheckedUint32 tmp = bytesPerGroup;
        tmp *= params.skipPixels;
        if (tmp.hasOverflowed())
            return GraphicsContextGL::INVALID_VALUE;
        skipSize += tmp;
    }
    if (skipSize.hasOverflowed())
        return GraphicsContextGL::INVALID_VALUE;
    if (skipSizeInBytes)
        *skipSizeInBytes = skipSize;

    checkedValue += skipSize;
    if (checkedValue.hasOverflowed())
        return GraphicsContextGL::INVALID_VALUE;
    return GraphicsContextGL::NO_ERROR;
}

bool GraphicsContextGL::packImageData(Image* image, const void* pixels, GCGLenum format, GCGLenum type, bool flipY, AlphaOp alphaOp, DataFormat sourceFormat, unsigned sourceImageWidth, unsigned sourceImageHeight, const IntRect& sourceImageSubRectangle, int depth, unsigned sourceUnpackAlignment, int unpackImageHeight, Vector<uint8_t>& data)
{
    if (!image || !pixels)
        return false;

    unsigned packedSize;
    // Output data is tightly packed (alignment == 1).
    PixelStoreParams params;
    params.alignment = 1;
    if (computeImageSizeInBytes(format, type, sourceImageSubRectangle.width(), sourceImageSubRectangle.height(), depth, params, &packedSize, nullptr, nullptr) != GraphicsContextGL::NO_ERROR)
        return false;
    data.resize(packedSize);

    if (!packPixels(static_cast<const uint8_t*>(pixels), sourceFormat, sourceImageWidth, sourceImageHeight, sourceImageSubRectangle, depth, sourceUnpackAlignment, unpackImageHeight, format, type, alphaOp, data.data(), flipY))
        return false;
    if (ImageObserver* observer = image->imageObserver())
        observer->didDraw(*image);
    return true;
}

bool GraphicsContextGL::extractPixelBuffer(const PixelBuffer& pixelBuffer, DataFormat sourceDataFormat, const IntRect& sourceImageSubRectangle, int depth, int unpackImageHeight, GCGLenum format, GCGLenum type, bool flipY, bool premultiplyAlpha, Vector<uint8_t>& data)
{
    int width = pixelBuffer.size().width();
    int height = pixelBuffer.size().height();

    unsigned packedSize;
    // Output data is tightly packed (alignment == 1).
    PixelStoreParams params;
    params.alignment = 1;
    if (computeImageSizeInBytes(format, type, sourceImageSubRectangle.width(), sourceImageSubRectangle.height(), depth, params, &packedSize, nullptr, nullptr) != GraphicsContextGL::NO_ERROR)
        return false;
    data.resize(packedSize);

    if (!packPixels(pixelBuffer.bytes(), sourceDataFormat, width, height, sourceImageSubRectangle, depth, 0, unpackImageHeight, format, type, premultiplyAlpha ? AlphaOp::DoPremultiply : AlphaOp::DoNothing, data.data(), flipY))
        return false;

    return true;
}

bool GraphicsContextGL::extractTextureData(unsigned width, unsigned height, GCGLenum format, GCGLenum type, const PixelStoreParams& unpackParams, bool flipY, bool premultiplyAlpha, GCGLSpan<const GCGLvoid> pixels, Vector<uint8_t>& data)
{
    // Assumes format, type, etc. have already been validated.
    DataFormat sourceDataFormat = getDataFormat(format, type);
    if (sourceDataFormat == GraphicsContextGL::DataFormat::Invalid)
        return false;
    // Resize the output buffer.
    unsigned componentsPerPixel, bytesPerComponent;
    if (!computeFormatAndTypeParameters(format, type, &componentsPerPixel, &bytesPerComponent))
        return false;
    unsigned bytesPerPixel = componentsPerPixel * bytesPerComponent;
    data.resize(width * height * bytesPerPixel);

    unsigned imageSizeInBytes, skipSizeInBytes;
    computeImageSizeInBytes(format, type, width, height, 1, unpackParams, &imageSizeInBytes, nullptr, &skipSizeInBytes);
    const uint8_t* srcData = static_cast<const uint8_t*>(pixels.data());
    if (skipSizeInBytes)
        srcData += skipSizeInBytes;

    if (!packPixels(srcData, sourceDataFormat, unpackParams.rowLength ? unpackParams.rowLength : width, height, IntRect(0, 0, width, height), 1, unpackParams.alignment, 0, format, type, (premultiplyAlpha ? AlphaOp::DoPremultiply : AlphaOp::DoNothing), data.data(), flipY))
        return false;

    return true;
}

void GraphicsContextGL::setDrawingBufferColorSpace(const DestinationColorSpace&)
{
}

void GraphicsContextGL::markContextChanged()
{
    m_layerComposited = false;
}

bool GraphicsContextGL::layerComposited() const
{
    return m_layerComposited;
}

void GraphicsContextGL::setBuffersToAutoClear(GCGLbitfield buffers)
{
    if (!contextAttributes().preserveDrawingBuffer)
        m_buffersToAutoClear = buffers;
}

GCGLbitfield GraphicsContextGL::getBuffersToAutoClear() const
{
    return m_buffersToAutoClear;
}

void GraphicsContextGL::markLayerComposited()
{
    m_layerComposited = true;
    auto attrs = contextAttributes();
    if (!attrs.preserveDrawingBuffer) {
        m_buffersToAutoClear = GraphicsContextGL::COLOR_BUFFER_BIT;
        if (attrs.depth)
            m_buffersToAutoClear |= GraphicsContextGL::DEPTH_BUFFER_BIT;
        if (attrs.stencil)
            m_buffersToAutoClear |= GraphicsContextGL::STENCIL_BUFFER_BIT;
    }
    if (m_client)
        m_client->didComposite();
}


void GraphicsContextGL::forceContextLost()
{
    if (m_client)
        m_client->forceContextLost();
}

void GraphicsContextGL::dispatchContextChangedNotification()
{
    if (m_client)
        m_client->dispatchContextChangedNotification();
}

} // namespace WebCore

#endif // ENABLE(WEBGL)
