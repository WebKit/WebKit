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
#include "GCGLSpan.h"
#include "GraphicsContext.h"
#include "HostWindow.h"
#include "Image.h"
#include "ImageObserver.h"
#include "PixelBuffer.h"
#include "VideoFrame.h"

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

std::tuple<GCGLenum, GCGLenum> GraphicsContextGL::externalImageTextureBindingPoint()
{
    return std::make_tuple(GraphicsContextGL::TEXTURE_2D, GraphicsContextGL::TEXTURE_BINDING_2D);
}

unsigned GraphicsContextGL::computeBytesPerGroup(GCGLenum format, GCGLenum type)
{
    unsigned componentsPerGroup = 0;
    switch (format) {
    case GraphicsContextGL::ALPHA:
    case GraphicsContextGL::LUMINANCE:
    case GraphicsContextGL::RED:
    case GraphicsContextGL::RED_INTEGER:
    case GraphicsContextGL::DEPTH_COMPONENT:
    case GraphicsContextGL::DEPTH_STENCIL: // Treat it as one component.
        componentsPerGroup = 1;
        break;
    case GraphicsContextGL::LUMINANCE_ALPHA:
    case GraphicsContextGL::RG:
    case GraphicsContextGL::RG_INTEGER:
        componentsPerGroup = 2;
        break;
    case GraphicsContextGL::RGB:
    case GraphicsContextGL::RGB_INTEGER:
    case GraphicsContextGL::SRGB_EXT:
        componentsPerGroup = 3;
        break;
    case GraphicsContextGL::RGBA:
    case GraphicsContextGL::RGBA_INTEGER:
    case GraphicsContextGL::BGRA_EXT: // GL_EXT_texture_format_BGRA8888
    case GraphicsContextGL::SRGB_ALPHA_EXT:
        componentsPerGroup = 4;
        break;
    default:
        return 0;
    }

    switch (type) {
    case GraphicsContextGL::UNSIGNED_SHORT_5_6_5:
    case GraphicsContextGL::UNSIGNED_SHORT_4_4_4_4:
    case GraphicsContextGL::UNSIGNED_SHORT_5_5_5_1:
        return 2;
    case GraphicsContextGL::UNSIGNED_INT_24_8:
    case GraphicsContextGL::UNSIGNED_INT_10F_11F_11F_REV:
    case GraphicsContextGL::UNSIGNED_INT_5_9_9_9_REV:
    case GraphicsContextGL::UNSIGNED_INT_2_10_10_10_REV:
        return 4;
    case GraphicsContextGL::BYTE:
    case GraphicsContextGL::UNSIGNED_BYTE:
        return componentsPerGroup;
    case GraphicsContextGL::SHORT:
    case GraphicsContextGL::UNSIGNED_SHORT:
    case GraphicsContextGL::HALF_FLOAT:
    case GraphicsContextGL::HALF_FLOAT_OES: // OES_texture_half_float
        return componentsPerGroup * 2;
    case GraphicsContextGL::INT:
    case GraphicsContextGL::UNSIGNED_INT:
    case GraphicsContextGL::FLOAT: // OES_texture_float
        return componentsPerGroup * 4;
    case GraphicsContextGL::FLOAT_32_UNSIGNED_INT_24_8_REV:
        return componentsPerGroup * 8;
    default:
        return 0;
    }
}

std::optional<GraphicsContextGL::PixelRectangleSizes> GraphicsContextGL::computeImageSize(GCGLenum format, GCGLenum type, IntSize size, GCGLsizei inDepth, const PixelStoreParameters& parameters)
{
    ASSERT(parameters.alignment == 1 || parameters.alignment == 2 || parameters.alignment == 4 || parameters.alignment == 8);
    ASSERT(parameters.rowLength >= 0);
    ASSERT(parameters.imageHeight >= 0);
    ASSERT(parameters.skipPixels >= 0);
    ASSERT(parameters.skipRows >= 0);
    ASSERT(parameters.skipImages >= 0);

    CheckedUint32 bytesPerGroup = computeBytesPerGroup(format, type);
    if (!bytesPerGroup)
        return std::nullopt;
    if (!size.width() || !size.height() || !inDepth)
        return PixelRectangleSizes { };

    // Inputs are assigned to CheckedUint32 to ensure nothing is computed with negative inputs.
    CheckedUint32 width = size.width();
    CheckedUint32 height = size.height();
    CheckedUint32 depth = inDepth;
    CheckedUint32 rowLength = parameters.rowLength > 0 ? parameters.rowLength : size.width();
    CheckedUint32 imageHeight = parameters.imageHeight > 0 ? parameters.imageHeight : size.height();
    CheckedUint32 alignment = parameters.alignment;

    CheckedUint32 paddedRowBytes = rowLength * bytesPerGroup;
    CheckedUint32 alignedRowBytes = roundUpToMultipleOfNonPowerOfTwo(alignment, paddedRowBytes);

    // Last image is not affected by `parameters.imageHeight`.
    CheckedUint32 rows = imageHeight * (depth - 1) + height;

    // Last row is not affected by `parameters.alignment`, `parameters.rowLength`.
    CheckedUint32 lastRowBytes = width * bytesPerGroup;
    CheckedUint32 imageBytes = alignedRowBytes * (rows - 1) + lastRowBytes;

    CheckedUint32 skipImages = parameters.skipImages;
    CheckedUint32 skipRows = parameters.skipRows;
    CheckedUint32 skipPixels = parameters.skipPixels;
    CheckedUint32 initialSkipBytes = alignedRowBytes * (imageHeight * skipImages + skipRows) + bytesPerGroup * skipPixels;

    CheckedUint32 totalBytes = initialSkipBytes + imageBytes; // Validated only.
    if (imageBytes.hasOverflowed() || initialSkipBytes.hasOverflowed() || totalBytes.hasOverflowed())
        return std::nullopt;
    return PixelRectangleSizes { initialSkipBytes.value(), imageBytes.value(), alignedRowBytes.value(), lastRowBytes.value() };
}

bool GraphicsContextGL::packImageData(Image* image, const void* pixels, GCGLenum format, GCGLenum type, bool flipY, AlphaOp alphaOp, DataFormat sourceFormat, unsigned sourceImageWidth, unsigned sourceImageHeight, const IntRect& sourceImageSubRectangle, int depth, unsigned sourceUnpackAlignment, int unpackImageHeight, Vector<uint8_t>& data)
{
    if (!image || !pixels)
        return false;

    // Output data is tightly packed (alignment == 1).
    PixelStoreParameters params;
    params.alignment = 1;
    auto packSizes = computeImageSize(format, type, sourceImageSubRectangle.size(), depth, params);
    if (!packSizes)
        return false;
    data.resize(packSizes->imageBytes);

    if (!packPixels(static_cast<const uint8_t*>(pixels), sourceFormat, sourceImageWidth, sourceImageHeight, sourceImageSubRectangle, depth, sourceUnpackAlignment, unpackImageHeight, format, type, alphaOp, data.data(), flipY))
        return false;
    if (auto observer = image->imageObserver())
        observer->didDraw(*image);
    return true;
}

bool GraphicsContextGL::extractPixelBuffer(const PixelBuffer& pixelBuffer, DataFormat sourceDataFormat, const IntRect& sourceImageSubRectangle, int depth, int unpackImageHeight, GCGLenum format, GCGLenum type, bool flipY, bool premultiplyAlpha, Vector<uint8_t>& data)
{
    int width = pixelBuffer.size().width();
    int height = pixelBuffer.size().height();

    // Output data is tightly packed (alignment == 1).
    PixelStoreParameters params;
    params.alignment = 1;
    auto packSizes = computeImageSize(format, type, sourceImageSubRectangle.size(), depth, params);
    if (!packSizes)
        return false;
    data.resize(packSizes->imageBytes);

    if (!packPixels(pixelBuffer.bytes(), sourceDataFormat, width, height, sourceImageSubRectangle, depth, 0, unpackImageHeight, format, type, premultiplyAlpha ? AlphaOp::DoPremultiply : AlphaOp::DoNothing, data.data(), flipY))
        return false;

    return true;
}

bool GraphicsContextGL::extractTextureData(unsigned width, unsigned height, GCGLenum format, GCGLenum type, const PixelStoreParameters& unpackParams, bool flipY, bool premultiplyAlpha, std::span<const uint8_t> pixels, Vector<uint8_t>& data)
{
    // Assumes format, type, etc. have already been validated.
    DataFormat sourceDataFormat = getDataFormat(format, type);
    if (sourceDataFormat == GraphicsContextGL::DataFormat::Invalid)
        return false;
    unsigned bytesPerPixel = computeBytesPerGroup(format, type);
    if (!bytesPerPixel)
        return false;
    auto packSizes = computeImageSize(format, type, { static_cast<int>(width), static_cast<int>(height) }, 1, unpackParams);
    if (!packSizes)
        return false;
    data.resize(width * height * bytesPerPixel);
    const uint8_t* srcData = static_cast<const uint8_t*>(pixels.data());
    srcData += packSizes->initialSkipBytes;
    if (!packPixels(srcData, sourceDataFormat, unpackParams.rowLength ? unpackParams.rowLength : width, height, IntRect(0, 0, width, height), 1, unpackParams.alignment, 0, format, type, (premultiplyAlpha ? AlphaOp::DoPremultiply : AlphaOp::DoNothing), data.data(), flipY))
        return false;
    return true;
}

GCGLfloat GraphicsContextGL::getFloat(GCGLenum pname)
{
    GCGLfloat value[1] { };
    getFloatv(pname, value);
    return value[0];
}

GCGLboolean GraphicsContextGL::getBoolean(GCGLenum pname)
{
    GCGLboolean value[1] { };
    getBooleanv(pname, value);
    return value[0];
}

GCGLint GraphicsContextGL::getInteger(GCGLenum pname)
{
    GCGLint value[1] { };
    getIntegerv(pname, value);
    return value[0];
}

GCGLint GraphicsContextGL::getIntegeri(GCGLenum pname, GCGLuint index)
{
    GCGLint value[4] { };
    getIntegeri_v(pname, index, value);
    return value[0];
}

GCGLint GraphicsContextGL::getActiveUniformBlocki(GCGLuint program, GCGLuint uniformBlockIndex, GCGLenum pname)
{
    GCGLint value[1] { };
    getActiveUniformBlockiv(program, uniformBlockIndex, pname, value);
    return value[0];
}

GCGLint GraphicsContextGL::getInternalformati(GCGLenum target, GCGLenum internalformat, GCGLenum pname)
{
    GCGLint value[1] { };
    getInternalformativ(target, internalformat, pname, value);
    return value[0];
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
}

void GraphicsContextGL::paintToCanvas(NativeImage& image, const IntSize& canvasSize, GraphicsContext& context)
{
    if (canvasSize.isEmpty())
        return;

    auto imageSize = image.size();

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
    context.drawNativeImage(image, imageSize, canvasRect, FloatRect(FloatPoint(), imageSize), { CompositeOperator::Copy });
}

void GraphicsContextGL::paintToCanvas(const GraphicsContextGLAttributes& sourceContextAttributes, Ref<PixelBuffer>&& pixelBuffer, const IntSize& canvasSize, GraphicsContext& context)
{
    if (canvasSize.isEmpty())
        return;

    auto image = createNativeImageFromPixelBuffer(sourceContextAttributes, WTFMove(pixelBuffer));
    paintToCanvas(*image, canvasSize, context);
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

#if ENABLE(VIDEO)
RefPtr<Image> GraphicsContextGL::videoFrameToImage(VideoFrame& frame)
{
    IntSize size { static_cast<int>(frame.presentationSize().width()), static_cast<int>(frame.presentationSize().height()) };
    auto imageBuffer = ImageBuffer::create(size, RenderingPurpose::Unspecified, 1, DestinationColorSpace::SRGB(), PixelFormat::BGRA8);
    if (!imageBuffer)
        return { };

    imageBuffer->context().paintVideoFrame(frame, { { }, size }, true);
    return imageBuffer->copyImage(DontCopyBackingStore);
}
#endif

} // namespace WebCore

#endif // ENABLE(WEBGL)
