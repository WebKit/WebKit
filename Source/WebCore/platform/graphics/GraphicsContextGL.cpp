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
#include "GraphicsContextGLOpenGL.h"

#if ENABLE(WEBGL)

#include "ExtensionsGL.h"
#include "FormatConverter.h"
#include "HostWindow.h"
#include "Image.h"
#include "ImageData.h"
#include "ImageObserver.h"

namespace WebCore {

static GraphicsContextGL::DataFormat getDataFormat(GCGLenum destinationFormat, GCGLenum destinationType)
{
    GraphicsContextGL::DataFormat dstFormat = GraphicsContextGL::DataFormat::RGBA8;
    switch (destinationType) {
    case GraphicsContextGL::BYTE:
        switch (destinationFormat) {
        case GraphicsContextGL::RED:
        case GraphicsContextGL::RED_INTEGER:
            dstFormat = GraphicsContextGL::DataFormat::R8_S;
            break;
        case GraphicsContextGL::RG:
        case GraphicsContextGL::RG_INTEGER:
            dstFormat = GraphicsContextGL::DataFormat::RG8_S;
            break;
        case GraphicsContextGL::RGB:
        case GraphicsContextGL::RGB_INTEGER:
            dstFormat = GraphicsContextGL::DataFormat::RGB8_S;
            break;
        case GraphicsContextGL::RGBA:
        case GraphicsContextGL::RGBA_INTEGER:
            dstFormat = GraphicsContextGL::DataFormat::RGBA8_S;
            break;
        default:
            ASSERT_NOT_REACHED();
        }
        break;
    case GraphicsContextGL::UNSIGNED_BYTE:
        switch (destinationFormat) {
        case GraphicsContextGL::RGB:
        case GraphicsContextGL::RGB_INTEGER:
        case GraphicsContextGL::SRGB:
            dstFormat = GraphicsContextGL::GraphicsContextGL::DataFormat::RGB8;
            break;
        case GraphicsContextGL::RGBA:
        case GraphicsContextGL::RGBA_INTEGER:
        case GraphicsContextGL::SRGB_ALPHA:
            dstFormat = GraphicsContextGL::DataFormat::RGBA8;
            break;
        case GraphicsContextGL::ALPHA:
            dstFormat = GraphicsContextGL::DataFormat::A8;
            break;
        case GraphicsContextGL::LUMINANCE:
        case GraphicsContextGL::RED:
        case GraphicsContextGL::RED_INTEGER:
            dstFormat = GraphicsContextGL::DataFormat::R8;
            break;
        case GraphicsContextGL::RG:
        case GraphicsContextGL::RG_INTEGER:
            dstFormat = GraphicsContextGL::DataFormat::RG8;
            break;
        case GraphicsContextGL::LUMINANCE_ALPHA:
            dstFormat = GraphicsContextGL::DataFormat::RA8;
            break;
        default:
            ASSERT_NOT_REACHED();
        }
        break;
    case GraphicsContextGL::SHORT:
        switch (destinationFormat) {
        case GraphicsContextGL::RED_INTEGER:
            dstFormat = GraphicsContextGL::DataFormat::R16_S;
            break;
        case GraphicsContextGL::RG_INTEGER:
            dstFormat = GraphicsContextGL::DataFormat::RG16_S;
            break;
        case GraphicsContextGL::RGB_INTEGER:
            dstFormat = GraphicsContextGL::DataFormat::RGB16_S;
            break;
        case GraphicsContextGL::RGBA_INTEGER:
            dstFormat = GraphicsContextGL::DataFormat::RGBA16_S;
            break;
        default:
            ASSERT_NOT_REACHED();
        }
        break;
    case GraphicsContextGL::UNSIGNED_SHORT:
        switch (destinationFormat) {
        case GraphicsContextGL::RED_INTEGER:
            dstFormat = GraphicsContextGL::DataFormat::R16;
            break;
        case GraphicsContextGL::DEPTH_COMPONENT:
            dstFormat = GraphicsContextGL::DataFormat::D16;
            break;
        case GraphicsContextGL::RG_INTEGER:
            dstFormat = GraphicsContextGL::DataFormat::RG16;
            break;
        case GraphicsContextGL::RGB_INTEGER:
            dstFormat = GraphicsContextGL::DataFormat::RGB16;
            break;
        case GraphicsContextGL::RGBA_INTEGER:
            dstFormat = GraphicsContextGL::DataFormat::RGBA16;
            break;
        default:
            ASSERT_NOT_REACHED();
        }
        break;
    case GraphicsContextGL::INT:
        switch (destinationFormat) {
        case GraphicsContextGL::RED_INTEGER:
            dstFormat = GraphicsContextGL::DataFormat::R32_S;
            break;
        case GraphicsContextGL::RG_INTEGER:
            dstFormat = GraphicsContextGL::DataFormat::RG32_S;
            break;
        case GraphicsContextGL::RGB_INTEGER:
            dstFormat = GraphicsContextGL::DataFormat::RGB32_S;
            break;
        case GraphicsContextGL::RGBA_INTEGER:
            dstFormat = GraphicsContextGL::DataFormat::RGBA32_S;
            break;
        default:
            ASSERT_NOT_REACHED();
        }
        break;
    case GraphicsContextGL::UNSIGNED_INT:
        switch (destinationFormat) {
        case GraphicsContextGL::RED_INTEGER:
            dstFormat = GraphicsContextGL::DataFormat::R32;
            break;
        case GraphicsContextGL::DEPTH_COMPONENT:
            dstFormat = GraphicsContextGL::DataFormat::D32;
            break;
        case GraphicsContextGL::RG_INTEGER:
            dstFormat = GraphicsContextGL::DataFormat::RG32;
            break;
        case GraphicsContextGL::RGB_INTEGER:
            dstFormat = GraphicsContextGL::DataFormat::RGB32;
            break;
        case GraphicsContextGL::RGBA_INTEGER:
            dstFormat = GraphicsContextGL::DataFormat::RGBA32;
            break;
        default:
            ASSERT_NOT_REACHED();
        }
        break;
    case GraphicsContextGL::HALF_FLOAT_OES: // OES_texture_half_float
    case GraphicsContextGL::HALF_FLOAT:
        switch (destinationFormat) {
        case GraphicsContextGL::RGBA:
            dstFormat = GraphicsContextGL::DataFormat::RGBA16F;
            break;
        case GraphicsContextGL::RGB:
            dstFormat = GraphicsContextGL::DataFormat::RGB16F;
            break;
        case GraphicsContextGL::RG:
            dstFormat = GraphicsContextGL::DataFormat::RG16F;
            break;
        case GraphicsContextGL::ALPHA:
            dstFormat = GraphicsContextGL::DataFormat::A16F;
            break;
        case GraphicsContextGL::LUMINANCE:
        case GraphicsContextGL::RED:
            dstFormat = GraphicsContextGL::DataFormat::R16F;
            break;
        case GraphicsContextGL::LUMINANCE_ALPHA:
            dstFormat = GraphicsContextGL::DataFormat::RA16F;
            break;
        case GraphicsContextGL::SRGB:
            dstFormat = GraphicsContextGL::DataFormat::RGB16F;
            break;
        case GraphicsContextGL::SRGB_ALPHA:
            dstFormat = GraphicsContextGL::DataFormat::RGBA16F;
            break;
        default:
            ASSERT_NOT_REACHED();
        }
        break;
    case GraphicsContextGL::FLOAT: // OES_texture_float
        switch (destinationFormat) {
        case GraphicsContextGL::RGBA:
            dstFormat = GraphicsContextGL::DataFormat::RGBA32F;
            break;
        case GraphicsContextGL::RGB:
            dstFormat = GraphicsContextGL::DataFormat::RGB32F;
            break;
        case GraphicsContextGL::RG:
            dstFormat = GraphicsContextGL::DataFormat::RG32F;
            break;
        case GraphicsContextGL::ALPHA:
            dstFormat = GraphicsContextGL::DataFormat::A32F;
            break;
        case GraphicsContextGL::LUMINANCE:
        case GraphicsContextGL::RED:
            dstFormat = GraphicsContextGL::DataFormat::R32F;
            break;
        case GraphicsContextGL::DEPTH_COMPONENT:
            dstFormat = GraphicsContextGL::DataFormat::D32F;
            break;
        case GraphicsContextGL::LUMINANCE_ALPHA:
            dstFormat = GraphicsContextGL::DataFormat::RA32F;
            break;
        case GraphicsContextGL::SRGB:
            dstFormat = GraphicsContextGL::DataFormat::RGB32F;
            break;
        case GraphicsContextGL::SRGB_ALPHA:
            dstFormat = GraphicsContextGL::DataFormat::RGBA32F;
            break;
        default:
            ASSERT_NOT_REACHED();
        }
        break;
    case GraphicsContextGL::UNSIGNED_SHORT_4_4_4_4:
        dstFormat = GraphicsContextGL::DataFormat::RGBA4444;
        break;
    case GraphicsContextGL::UNSIGNED_SHORT_5_5_5_1:
        dstFormat = GraphicsContextGL::DataFormat::RGBA5551;
        break;
    case GraphicsContextGL::UNSIGNED_SHORT_5_6_5:
        dstFormat = GraphicsContextGL::DataFormat::RGB565;
        break;
    case GraphicsContextGL::UNSIGNED_INT_5_9_9_9_REV:
        dstFormat = GraphicsContextGL::DataFormat::RGB5999;
        break;
    case GraphicsContextGL::UNSIGNED_INT_24_8:
        dstFormat = GraphicsContextGL::DataFormat::DS24_8;
        break;
    case GraphicsContextGL::UNSIGNED_INT_10F_11F_11F_REV:
        dstFormat = GraphicsContextGL::DataFormat::RGB10F11F11F;
        break;
    case GraphicsContextGL::UNSIGNED_INT_2_10_10_10_REV:
        dstFormat = GraphicsContextGL::DataFormat::RGBA2_10_10_10;
        break;
    default:
        ASSERT_NOT_REACHED();
    }
    return dstFormat;
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

RefPtr<GraphicsContextGL> GraphicsContextGL::create(const GraphicsContextGLAttributes& attributes, HostWindow* hostWindow)
{
    RefPtr<GraphicsContextGL> result;
    if (hostWindow)
        result = hostWindow->createGraphicsContextGL(attributes);
    if (!result)
        result = GraphicsContextGLOpenGL::create(attributes, hostWindow);
    return result;
}

GraphicsContextGL::GraphicsContextGL(GraphicsContextGLAttributes attrs, Destination destination, GraphicsContextGL*)
    : m_attrs(attrs)
    , m_destination(destination)
{
}

void GraphicsContextGL::enablePreserveDrawingBuffer()
{
    // Canvas capture should not call this unless necessary.
    ASSERT(!m_attrs.preserveDrawingBuffer);
    m_attrs.preserveDrawingBuffer = true;
}

unsigned GraphicsContextGL::getClearBitsByAttachmentType(GCGLenum attachment)
{
    switch (attachment) {
    case GraphicsContextGL::COLOR_ATTACHMENT0:
    case ExtensionsGL::COLOR_ATTACHMENT1_EXT:
    case ExtensionsGL::COLOR_ATTACHMENT2_EXT:
    case ExtensionsGL::COLOR_ATTACHMENT3_EXT:
    case ExtensionsGL::COLOR_ATTACHMENT4_EXT:
    case ExtensionsGL::COLOR_ATTACHMENT5_EXT:
    case ExtensionsGL::COLOR_ATTACHMENT6_EXT:
    case ExtensionsGL::COLOR_ATTACHMENT7_EXT:
    case ExtensionsGL::COLOR_ATTACHMENT8_EXT:
    case ExtensionsGL::COLOR_ATTACHMENT9_EXT:
    case ExtensionsGL::COLOR_ATTACHMENT10_EXT:
    case ExtensionsGL::COLOR_ATTACHMENT11_EXT:
    case ExtensionsGL::COLOR_ATTACHMENT12_EXT:
    case ExtensionsGL::COLOR_ATTACHMENT13_EXT:
    case ExtensionsGL::COLOR_ATTACHMENT14_EXT:
    case ExtensionsGL::COLOR_ATTACHMENT15_EXT:
        return GraphicsContextGL::COLOR_BUFFER_BIT;
    case GraphicsContextGL::DEPTH_ATTACHMENT:
        return GraphicsContextGL::DEPTH_BUFFER_BIT;
    case GraphicsContextGL::STENCIL_ATTACHMENT:
        return GraphicsContextGL::STENCIL_BUFFER_BIT;
    case GraphicsContextGL::DEPTH_STENCIL_ATTACHMENT:
        return GraphicsContextGL::DEPTH_BUFFER_BIT | GraphicsContextGL::STENCIL_BUFFER_BIT;
    default:
        return 0;
    }
}

unsigned GraphicsContextGL::getClearBitsByFormat(GCGLenum format)
{
    switch (format) {
    case GraphicsContextGL::RGB:
    case GraphicsContextGL::RGBA:
    case GraphicsContextGL::LUMINANCE_ALPHA:
    case GraphicsContextGL::LUMINANCE:
    case GraphicsContextGL::ALPHA:
    case GraphicsContextGL::R8:
    case GraphicsContextGL::R8_SNORM:
    case GraphicsContextGL::R16F:
    case GraphicsContextGL::R32F:
    case GraphicsContextGL::R8UI:
    case GraphicsContextGL::R8I:
    case GraphicsContextGL::R16UI:
    case GraphicsContextGL::R16I:
    case GraphicsContextGL::R32UI:
    case GraphicsContextGL::R32I:
    case GraphicsContextGL::RG8:
    case GraphicsContextGL::RG8_SNORM:
    case GraphicsContextGL::RG16F:
    case GraphicsContextGL::RG32F:
    case GraphicsContextGL::RG8UI:
    case GraphicsContextGL::RG8I:
    case GraphicsContextGL::RG16UI:
    case GraphicsContextGL::RG16I:
    case GraphicsContextGL::RG32UI:
    case GraphicsContextGL::RG32I:
    case GraphicsContextGL::RGB8:
    case GraphicsContextGL::SRGB8:
    case GraphicsContextGL::RGB565:
    case GraphicsContextGL::RGB8_SNORM:
    case GraphicsContextGL::R11F_G11F_B10F:
    case GraphicsContextGL::RGB9_E5:
    case GraphicsContextGL::RGB16F:
    case GraphicsContextGL::RGB32F:
    case GraphicsContextGL::RGB8UI:
    case GraphicsContextGL::RGB8I:
    case GraphicsContextGL::RGB16UI:
    case GraphicsContextGL::RGB16I:
    case GraphicsContextGL::RGB32UI:
    case GraphicsContextGL::RGB32I:
    case GraphicsContextGL::RGBA8:
    case GraphicsContextGL::SRGB8_ALPHA8:
    case GraphicsContextGL::RGBA8_SNORM:
    case GraphicsContextGL::RGB5_A1:
    case GraphicsContextGL::RGBA4:
    case GraphicsContextGL::RGB10_A2:
    case GraphicsContextGL::RGBA16F:
    case GraphicsContextGL::RGBA32F:
    case GraphicsContextGL::RGBA8UI:
    case GraphicsContextGL::RGBA8I:
    case GraphicsContextGL::RGB10_A2UI:
    case GraphicsContextGL::RGBA16UI:
    case GraphicsContextGL::RGBA16I:
    case GraphicsContextGL::RGBA32I:
    case GraphicsContextGL::RGBA32UI:
    case ExtensionsGL::SRGB_EXT:
    case ExtensionsGL::SRGB_ALPHA_EXT:
        return GraphicsContextGL::COLOR_BUFFER_BIT;
    case GraphicsContextGL::DEPTH_COMPONENT16:
    case GraphicsContextGL::DEPTH_COMPONENT24:
    case GraphicsContextGL::DEPTH_COMPONENT32F:
    case GraphicsContextGL::DEPTH_COMPONENT:
        return GraphicsContextGL::DEPTH_BUFFER_BIT;
    case GraphicsContextGL::STENCIL_INDEX8:
        return GraphicsContextGL::STENCIL_BUFFER_BIT;
    case GraphicsContextGL::DEPTH_STENCIL:
    case GraphicsContextGL::DEPTH24_STENCIL8:
    case GraphicsContextGL::DEPTH32F_STENCIL8:
        return GraphicsContextGL::DEPTH_BUFFER_BIT | GraphicsContextGL::STENCIL_BUFFER_BIT;
    default:
        return 0;
    }
}

uint8_t GraphicsContextGL::getChannelBitsByFormat(GCGLenum format)
{
    switch (format) {
    case GraphicsContextGL::ALPHA:
        return static_cast<uint8_t>(ChannelBits::Alpha);
    case GraphicsContextGL::LUMINANCE:
        return static_cast<uint8_t>(ChannelBits::RGB);
    case GraphicsContextGL::LUMINANCE_ALPHA:
        return static_cast<uint8_t>(ChannelBits::RGBA);
    case GraphicsContextGL::RGB:
    case GraphicsContextGL::RGB565:
    case ExtensionsGL::SRGB_EXT:
        return static_cast<uint8_t>(ChannelBits::RGB);
    case GraphicsContextGL::RGBA:
    case GraphicsContextGL::RGBA4:
    case GraphicsContextGL::RGB5_A1:
    case ExtensionsGL::SRGB_ALPHA_EXT:
        return static_cast<uint8_t>(ChannelBits::RGBA);
    case GraphicsContextGL::DEPTH_COMPONENT16:
    case GraphicsContextGL::DEPTH_COMPONENT:
        return static_cast<uint8_t>(ChannelBits::Depth);
    case GraphicsContextGL::STENCIL_INDEX8:
        return static_cast<uint8_t>(ChannelBits::Stencil);
    case GraphicsContextGL::DEPTH_STENCIL:
        return static_cast<uint8_t>(ChannelBits::DepthStencil);
    default:
        return 0;
    }
}

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
    case ExtensionsGL::SRGB_EXT:
        *componentsPerPixel = 3;
        break;
    case GraphicsContextGL::RGBA:
    case GraphicsContextGL::RGBA_INTEGER:
    case ExtensionsGL::BGRA_EXT: // GL_EXT_texture_format_BGRA8888
    case ExtensionsGL::SRGB_ALPHA_EXT:
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
    Checked<uint32_t, RecordOverflow> checkedValue = static_cast<uint32_t>(rowLength);
    checkedValue *= bytesPerGroup;
    if (checkedValue.hasOverflowed())
        return GraphicsContextGL::INVALID_VALUE;

    unsigned lastRowSize;
    if (params.rowLength > 0 && params.rowLength != width) {
        Checked<uint32_t, RecordOverflow> tmp = width;
        tmp *= bytesPerGroup;
        if (tmp.hasOverflowed())
            return GraphicsContextGL::INVALID_VALUE;
        lastRowSize = tmp.unsafeGet();
    } else
        lastRowSize = checkedValue.unsafeGet();

    unsigned padding = 0;
    unsigned residual = checkedValue.unsafeGet() % params.alignment;
    if (residual) {
        padding = params.alignment - residual;
        checkedValue += padding;
    }
    if (checkedValue.hasOverflowed())
        return GraphicsContextGL::INVALID_VALUE;
    unsigned paddedRowSize = checkedValue.unsafeGet();

    Checked<uint32_t, RecordOverflow> rows = imageHeight;
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
    *imageSizeInBytes = checkedValue.unsafeGet();
    if (paddingInBytes)
        *paddingInBytes = padding;

    Checked<uint32_t, RecordOverflow> skipSize = 0;
    if (params.skipImages > 0) {
        Checked<uint32_t, RecordOverflow> tmp = paddedRowSize;
        tmp *= imageHeight;
        tmp *= params.skipImages;
        if (tmp.hasOverflowed())
            return GraphicsContextGL::INVALID_VALUE;
        skipSize += tmp.unsafeGet();
    }
    if (params.skipRows > 0) {
        Checked<uint32_t, RecordOverflow> tmp = paddedRowSize;
        tmp *= params.skipRows;
        if (tmp.hasOverflowed())
            return GraphicsContextGL::INVALID_VALUE;
        skipSize += tmp.unsafeGet();
    }
    if (params.skipPixels > 0) {
        Checked<uint32_t, RecordOverflow> tmp = bytesPerGroup;
        tmp *= params.skipPixels;
        if (tmp.hasOverflowed())
            return GraphicsContextGL::INVALID_VALUE;
        skipSize += tmp.unsafeGet();
    }
    if (skipSize.hasOverflowed())
        return GraphicsContextGL::INVALID_VALUE;
    if (skipSizeInBytes)
        *skipSizeInBytes = skipSize.unsafeGet();

    checkedValue += skipSize.unsafeGet();
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

    if (!packPixels(reinterpret_cast<const uint8_t*>(pixels), sourceFormat, sourceImageWidth, sourceImageHeight, sourceImageSubRectangle, depth, sourceUnpackAlignment, unpackImageHeight, format, type, alphaOp, data.data(), flipY))
        return false;
    if (ImageObserver* observer = image->imageObserver())
        observer->didDraw(*image);
    return true;
}

bool GraphicsContextGL::extractImageData(ImageData* imageData, DataFormat sourceDataFormat, const IntRect& sourceImageSubRectangle, int depth, int unpackImageHeight, GCGLenum format, GCGLenum type, bool flipY, bool premultiplyAlpha, Vector<uint8_t>& data)
{
    if (!imageData)
        return false;
    int width = imageData->width();
    int height = imageData->height();

    unsigned packedSize;
    // Output data is tightly packed (alignment == 1).
    PixelStoreParams params;
    params.alignment = 1;
    if (computeImageSizeInBytes(format, type, sourceImageSubRectangle.width(), sourceImageSubRectangle.height(), depth, params, &packedSize, nullptr, nullptr) != GraphicsContextGL::NO_ERROR)
        return false;
    data.resize(packedSize);

    if (!packPixels(imageData->data()->data(), sourceDataFormat, width, height, sourceImageSubRectangle, depth, 0, unpackImageHeight, format, type, premultiplyAlpha ? AlphaOp::DoPremultiply : AlphaOp::DoNothing, data.data(), flipY))
        return false;

    return true;
}

bool GraphicsContextGL::extractTextureData(unsigned width, unsigned height, GCGLenum format, GCGLenum type, const PixelStoreParams& unpackParams, bool flipY, bool premultiplyAlpha, const void* pixels, Vector<uint8_t>& data)
{
    // Assumes format, type, etc. have already been validated.
    DataFormat sourceDataFormat = getDataFormat(format, type);

    // Resize the output buffer.
    unsigned componentsPerPixel, bytesPerComponent;
    if (!computeFormatAndTypeParameters(format, type, &componentsPerPixel, &bytesPerComponent))
        return false;
    unsigned bytesPerPixel = componentsPerPixel * bytesPerComponent;
    data.resize(width * height * bytesPerPixel);

    unsigned imageSizeInBytes, skipSizeInBytes;
    computeImageSizeInBytes(format, type, width, height, 1, unpackParams, &imageSizeInBytes, nullptr, &skipSizeInBytes);
    const uint8_t* srcData = static_cast<const uint8_t*>(pixels);
    if (skipSizeInBytes)
        srcData += skipSizeInBytes;

    if (!packPixels(srcData, sourceDataFormat, unpackParams.rowLength ? unpackParams.rowLength : width, height, IntRect(0, 0, width, height), 1, unpackParams.alignment, 0, format, type, (premultiplyAlpha ? AlphaOp::DoPremultiply : AlphaOp::DoNothing), data.data(), flipY))
        return false;

    return true;
}

} // namespace WebCore

#endif // ENABLE(WEBGL)
