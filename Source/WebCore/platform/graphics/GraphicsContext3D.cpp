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

#if ENABLE(WEBGL)

#include "GraphicsContext3D.h"

#include "ArrayBufferView.h"
#include "CheckedInt.h"
#include "DrawingBuffer.h"
#include "Extensions3D.h"
#include "Image.h"
#include "ImageData.h"

#include <wtf/OwnArrayPtr.h>
#include <wtf/PassOwnArrayPtr.h>

namespace WebCore {

namespace {

    uint8_t convertColor16LittleTo8(uint16_t value)
    {
        return value >> 8;
    }

    uint8_t convertColor16BigTo8(uint16_t value)
    {
        return static_cast<uint8_t>(value & 0x00FF);
    }

} // anonymous namespace


PassRefPtr<DrawingBuffer> GraphicsContext3D::createDrawingBuffer(const IntSize& size)
{
    return DrawingBuffer::create(this, size);
}

bool GraphicsContext3D::texImage2DResourceSafe(GC3Denum target, GC3Dint level, GC3Denum internalformat, GC3Dsizei width, GC3Dsizei height, GC3Dint border, GC3Denum format, GC3Denum type, GC3Dint unpackAlignment)
{
    ASSERT(unpackAlignment == 1 || unpackAlignment == 2 || unpackAlignment == 4 || unpackAlignment == 8);
    OwnArrayPtr<unsigned char> zero;
    if (width > 0 && height > 0) {
        unsigned int size;
        GC3Denum error = computeImageSizeInBytes(format, type, width, height, unpackAlignment, &size, 0);
        if (error != GraphicsContext3D::NO_ERROR) {
            synthesizeGLError(error);
            return false;
        }
        zero = adoptArrayPtr(new unsigned char[size]);
        if (!zero) {
            synthesizeGLError(GraphicsContext3D::INVALID_VALUE);
            return false;
        }
        memset(zero.get(), 0, size);
    }
    return texImage2D(target, level, internalformat, width, height, border, format, type, zero.get());
}

bool GraphicsContext3D::computeFormatAndTypeParameters(GC3Denum format,
                                                       GC3Denum type,
                                                       unsigned int* componentsPerPixel,
                                                       unsigned int* bytesPerComponent)
{
    switch (format) {
    case GraphicsContext3D::ALPHA:
        *componentsPerPixel = 1;
        break;
    case GraphicsContext3D::LUMINANCE:
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
    case GraphicsContext3D::UNSIGNED_SHORT_5_6_5:
    case GraphicsContext3D::UNSIGNED_SHORT_4_4_4_4:
    case GraphicsContext3D::UNSIGNED_SHORT_5_5_5_1:
        *componentsPerPixel = 1;
        *bytesPerComponent = sizeof(GC3Dushort);
        break;
    case GraphicsContext3D::FLOAT: // OES_texture_float
        *bytesPerComponent = sizeof(GC3Dfloat);
        break;
    default:
        return false;
    }
    return true;
}

GC3Denum GraphicsContext3D::computeImageSizeInBytes(GC3Denum format, GC3Denum type, GC3Dsizei width, GC3Dsizei height, GC3Dint alignment,
                                                    unsigned int* imageSizeInBytes, unsigned int* paddingInBytes)
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
    CheckedInt<uint32_t> checkedValue(bytesPerComponent * componentsPerPixel);
    checkedValue *=  width;
    if (!checkedValue.valid())
        return GraphicsContext3D::INVALID_VALUE;
    unsigned int validRowSize = checkedValue.value();
    unsigned int padding = 0;
    unsigned int residual = validRowSize % alignment;
    if (residual) {
        padding = alignment - residual;
        checkedValue += padding;
    }
    // Last row needs no padding.
    checkedValue *= (height - 1);
    checkedValue += validRowSize;
    if (!checkedValue.valid())
        return GraphicsContext3D::INVALID_VALUE;
    *imageSizeInBytes = checkedValue.value();
    if (paddingInBytes)
        *paddingInBytes = padding;
    return GraphicsContext3D::NO_ERROR;
}

bool GraphicsContext3D::extractImageData(Image* image,
                                         GC3Denum format,
                                         GC3Denum type,
                                         bool flipY,
                                         bool premultiplyAlpha,
                                         bool ignoreGammaAndColorProfile,
                                         Vector<uint8_t>& data)
{
    if (!image)
        return false;
    if (!getImageData(image, format, type, premultiplyAlpha, ignoreGammaAndColorProfile, data))
        return false;
    if (flipY) {
        unsigned int componentsPerPixel, bytesPerComponent;
        if (!computeFormatAndTypeParameters(format, type,
                                            &componentsPerPixel,
                                            &bytesPerComponent))
            return false;
        // The image data is tightly packed, and we upload it as such.
        unsigned int unpackAlignment = 1;
        flipVertically(data.data(), image->width(), image->height(),
                       componentsPerPixel * bytesPerComponent,
                       unpackAlignment);
    }
    return true;
}

bool GraphicsContext3D::extractImageData(ImageData* imageData,
                                         GC3Denum format,
                                         GC3Denum type,
                                         bool flipY,
                                         bool premultiplyAlpha,
                                         Vector<uint8_t>& data)
{
    if (!imageData)
        return false;
    int width = imageData->width();
    int height = imageData->height();
    int dataBytes = width * height * 4;
    data.resize(dataBytes);
    if (!packPixels(imageData->data()->data()->data(),
                    SourceFormatRGBA8,
                    width,
                    height,
                    0,
                    format,
                    type,
                    premultiplyAlpha ? AlphaDoPremultiply : AlphaDoNothing,
                    data.data()))
        return false;
    if (flipY) {
        unsigned int componentsPerPixel, bytesPerComponent;
        if (!computeFormatAndTypeParameters(format, type,
                                            &componentsPerPixel,
                                            &bytesPerComponent))
            return false;
        // The image data is tightly packed, and we upload it as such.
        unsigned int unpackAlignment = 1;
        flipVertically(data.data(), width, height,
                       componentsPerPixel * bytesPerComponent,
                       unpackAlignment);
    }
    return true;
}

bool GraphicsContext3D::extractTextureData(unsigned int width, unsigned int height,
                                           GC3Denum format, GC3Denum type,
                                           unsigned int unpackAlignment,
                                           bool flipY, bool premultiplyAlpha,
                                           const void* pixels,
                                           Vector<uint8_t>& data)
{
    // Assumes format, type, etc. have already been validated.
    SourceDataFormat sourceDataFormat = SourceFormatRGBA8;
    switch (type) {
    case UNSIGNED_BYTE:
        switch (format) {
        case RGBA:
            sourceDataFormat = SourceFormatRGBA8;
            break;
        case RGB:
            sourceDataFormat = SourceFormatRGB8;
            break;
        case ALPHA:
            sourceDataFormat = SourceFormatA8;
            break;
        case LUMINANCE:
            sourceDataFormat = SourceFormatR8;
            break;
        case LUMINANCE_ALPHA:
            sourceDataFormat = SourceFormatRA8;
            break;
        default:
            ASSERT_NOT_REACHED();
        }
        break;
    case FLOAT: // OES_texture_float
        switch (format) {
        case RGBA:
            sourceDataFormat = SourceFormatRGBA32F;
            break;
        case RGB:
            sourceDataFormat = SourceFormatRGB32F;
            break;
        case ALPHA:
            sourceDataFormat = SourceFormatA32F;
            break;
        case LUMINANCE:
            sourceDataFormat = SourceFormatR32F;
            break;
        case LUMINANCE_ALPHA:
            sourceDataFormat = SourceFormatRA32F;
            break;
        default:
            ASSERT_NOT_REACHED();
        }
        break;
    case UNSIGNED_SHORT_5_5_5_1:
        sourceDataFormat = SourceFormatRGBA5551;
        break;
    case UNSIGNED_SHORT_4_4_4_4:
        sourceDataFormat = SourceFormatRGBA4444;
        break;
    case UNSIGNED_SHORT_5_6_5:
        sourceDataFormat = SourceFormatRGB565;
        break;
    default:
        ASSERT_NOT_REACHED();
    }

    // Resize the output buffer.
    unsigned int componentsPerPixel, bytesPerComponent;
    if (!computeFormatAndTypeParameters(format, type,
                                        &componentsPerPixel,
                                        &bytesPerComponent))
        return false;
    unsigned int bytesPerPixel = componentsPerPixel * bytesPerComponent;
    data.resize(width * height * bytesPerPixel);

    if (!packPixels(static_cast<const uint8_t*>(pixels),
                    sourceDataFormat,
                    width, height, unpackAlignment,
                    format, type,
                    (premultiplyAlpha ? AlphaDoPremultiply : AlphaDoNothing),
                    data.data()))
        return false;
    // The pixel data is now tightly packed.
    if (flipY)
        flipVertically(data.data(), width, height, bytesPerPixel, 1);
    return true;
}

void GraphicsContext3D::flipVertically(void* imageData,
                                       unsigned int width,
                                       unsigned int height,
                                       unsigned int bytesPerPixel,
                                       unsigned int unpackAlignment)
{
    if (!width || !height)
        return;
    unsigned int validRowBytes = width * bytesPerPixel;
    unsigned int totalRowBytes = validRowBytes;
    unsigned int remainder = validRowBytes % unpackAlignment;
    if (remainder)
        totalRowBytes += (unpackAlignment - remainder);
    uint8_t* tempRow = new uint8_t[validRowBytes];
    uint8_t* data = static_cast<uint8_t*>(imageData);
    for (unsigned i = 0; i < height / 2; i++) {
        uint8_t* lowRow = data + (totalRowBytes * i);
        uint8_t* highRow = data + (totalRowBytes * (height - i - 1));
        memcpy(tempRow, lowRow, validRowBytes);
        memcpy(lowRow, highRow, validRowBytes);
        memcpy(highRow, tempRow, validRowBytes);
    }
    delete[] tempRow;
}

// These functions can not be static, or gcc will not allow them to be
// used as template parameters. Use an anonymous namespace to prevent
// the need to declare prototypes for them.
namespace {

//----------------------------------------------------------------------
// Pixel unpacking routines.

void unpackRGBA8ToRGBA8(const uint8_t* source, uint8_t* destination)
{
    destination[0] = source[0];
    destination[1] = source[1];
    destination[2] = source[2];
    destination[3] = source[3];
}

void unpackRGBA16LittleToRGBA8(const uint16_t* source, uint8_t* destination)
{
    destination[0] = convertColor16LittleTo8(source[0]);
    destination[1] = convertColor16LittleTo8(source[1]);
    destination[2] = convertColor16LittleTo8(source[2]);
    destination[3] = convertColor16LittleTo8(source[3]);
}

void unpackRGBA16BigToRGBA8(const uint16_t* source, uint8_t* destination)
{
    destination[0] = convertColor16BigTo8(source[0]);
    destination[1] = convertColor16BigTo8(source[1]);
    destination[2] = convertColor16BigTo8(source[2]);
    destination[3] = convertColor16BigTo8(source[3]);
}

void unpackRGB8ToRGBA8(const uint8_t* source, uint8_t* destination)
{
    destination[0] = source[0];
    destination[1] = source[1];
    destination[2] = source[2];
    destination[3] = 0xFF;
}

void unpackRGB16LittleToRGBA8(const uint16_t* source, uint8_t* destination)
{
    destination[0] = convertColor16LittleTo8(source[0]);
    destination[1] = convertColor16LittleTo8(source[1]);
    destination[2] = convertColor16LittleTo8(source[2]);
    destination[3] = 0xFF;
}

void unpackRGB16BigToRGBA8(const uint16_t* source, uint8_t* destination)
{
    destination[0] = convertColor16BigTo8(source[0]);
    destination[1] = convertColor16BigTo8(source[1]);
    destination[2] = convertColor16BigTo8(source[2]);
    destination[3] = 0xFF;
}

void unpackBGR8ToRGBA8(const uint8_t* source, uint8_t* destination)
{
    destination[0] = source[2];
    destination[1] = source[1];
    destination[2] = source[0];
    destination[3] = 0xFF;
}

void unpackARGB8ToRGBA8(const uint8_t* source, uint8_t* destination)
{
    destination[0] = source[1];
    destination[1] = source[2];
    destination[2] = source[3];
    destination[3] = source[0];
}

void unpackARGB16LittleToRGBA8(const uint16_t* source, uint8_t* destination)
{
    destination[0] = convertColor16LittleTo8(source[1]);
    destination[1] = convertColor16LittleTo8(source[2]);
    destination[2] = convertColor16LittleTo8(source[3]);
    destination[3] = convertColor16LittleTo8(source[0]);
}

void unpackARGB16BigToRGBA8(const uint16_t* source, uint8_t* destination)
{
    destination[0] = convertColor16BigTo8(source[1]);
    destination[1] = convertColor16BigTo8(source[2]);
    destination[2] = convertColor16BigTo8(source[3]);
    destination[3] = convertColor16BigTo8(source[0]);
}

void unpackABGR8ToRGBA8(const uint8_t* source, uint8_t* destination)
{
    destination[0] = source[3];
    destination[1] = source[2];
    destination[2] = source[1];
    destination[3] = source[0];
}

void unpackBGRA8ToRGBA8(const uint8_t* source, uint8_t* destination)
{
    destination[0] = source[2];
    destination[1] = source[1];
    destination[2] = source[0];
    destination[3] = source[3];
}

void unpackBGRA16LittleToRGBA8(const uint16_t* source, uint8_t* destination)
{
    destination[0] = convertColor16LittleTo8(source[2]);
    destination[1] = convertColor16LittleTo8(source[1]);
    destination[2] = convertColor16LittleTo8(source[0]);
    destination[3] = convertColor16LittleTo8(source[3]);
}

void unpackBGRA16BigToRGBA8(const uint16_t* source, uint8_t* destination)
{
    destination[0] = convertColor16BigTo8(source[2]);
    destination[1] = convertColor16BigTo8(source[1]);
    destination[2] = convertColor16BigTo8(source[0]);
    destination[3] = convertColor16BigTo8(source[3]);
}

void unpackRGBA5551ToRGBA8(const uint16_t* source, uint8_t* destination)
{
    uint16_t packedValue = source[0];
    uint8_t r = packedValue >> 11;
    uint8_t g = (packedValue >> 6) & 0x1F;
    uint8_t b = (packedValue >> 1) & 0x1F;
    destination[0] = (r << 3) | (r & 0x7);
    destination[1] = (g << 3) | (g & 0x7);
    destination[2] = (b << 3) | (b & 0x7);
    destination[3] = (packedValue & 0x1) ? 0xFF : 0x0;
}

void unpackRGBA4444ToRGBA8(const uint16_t* source, uint8_t* destination)
{
    uint16_t packedValue = source[0];
    uint8_t r = packedValue >> 12;
    uint8_t g = (packedValue >> 8) & 0x0F;
    uint8_t b = (packedValue >> 4) & 0x0F;
    uint8_t a = packedValue & 0x0F;
    destination[0] = r << 4 | r;
    destination[1] = g << 4 | g;
    destination[2] = b << 4 | b;
    destination[3] = a << 4 | a;
}

void unpackRGB565ToRGBA8(const uint16_t* source, uint8_t* destination)
{
    uint16_t packedValue = source[0];
    uint8_t r = packedValue >> 11;
    uint8_t g = (packedValue >> 5) & 0x3F;
    uint8_t b = packedValue & 0x1F;
    destination[0] = (r << 3) | (r & 0x7);
    destination[1] = (g << 2) | (g & 0x3);
    destination[2] = (b << 3) | (b & 0x7);
    destination[3] = 0xFF;
}

void unpackR8ToRGBA8(const uint8_t* source, uint8_t* destination)
{
    destination[0] = source[0];
    destination[1] = source[0];
    destination[2] = source[0];
    destination[3] = 0xFF;
}

void unpackR16LittleToRGBA8(const uint16_t* source, uint8_t* destination)
{
    destination[0] = convertColor16LittleTo8(source[0]);
    destination[1] = convertColor16LittleTo8(source[0]);
    destination[2] = convertColor16LittleTo8(source[0]);
    destination[3] = 0xFF;
}

void unpackR16BigToRGBA8(const uint16_t* source, uint8_t* destination)
{
    destination[0] = convertColor16BigTo8(source[0]);
    destination[1] = convertColor16BigTo8(source[0]);
    destination[2] = convertColor16BigTo8(source[0]);
    destination[3] = 0xFF;
}

void unpackRA8ToRGBA8(const uint8_t* source, uint8_t* destination)
{
    destination[0] = source[0];
    destination[1] = source[0];
    destination[2] = source[0];
    destination[3] = source[1];
}

void unpackRA16LittleToRGBA8(const uint16_t* source, uint8_t* destination)
{
    destination[0] = convertColor16LittleTo8(source[0]);
    destination[1] = convertColor16LittleTo8(source[0]);
    destination[2] = convertColor16LittleTo8(source[0]);
    destination[3] = convertColor16LittleTo8(source[1]);
}

void unpackRA16BigToRGBA8(const uint16_t* source, uint8_t* destination)
{
    destination[0] = convertColor16BigTo8(source[0]);
    destination[1] = convertColor16BigTo8(source[0]);
    destination[2] = convertColor16BigTo8(source[0]);
    destination[3] = convertColor16BigTo8(source[1]);
}

void unpackAR8ToRGBA8(const uint8_t* source, uint8_t* destination)
{
    destination[0] = source[1];
    destination[1] = source[1];
    destination[2] = source[1];
    destination[3] = source[0];
}

void unpackAR16LittleToRGBA8(const uint16_t* source, uint8_t* destination)
{
    destination[0] = convertColor16LittleTo8(source[1]);
    destination[1] = convertColor16LittleTo8(source[1]);
    destination[2] = convertColor16LittleTo8(source[1]);
    destination[3] = convertColor16LittleTo8(source[0]);
}

void unpackAR16BigToRGBA8(const uint16_t* source, uint8_t* destination)
{
    destination[0] = convertColor16BigTo8(source[1]);
    destination[1] = convertColor16BigTo8(source[1]);
    destination[2] = convertColor16BigTo8(source[1]);
    destination[3] = convertColor16BigTo8(source[0]);
}

void unpackA8ToRGBA8(const uint8_t* source, uint8_t* destination)
{
    destination[0] = 0x0;
    destination[1] = 0x0;
    destination[2] = 0x0;
    destination[3] = source[0];
}

void unpackA16LittleToRGBA8(const uint16_t* source, uint8_t* destination)
{
    destination[0] = 0x0;
    destination[1] = 0x0;
    destination[2] = 0x0;
    destination[3] = convertColor16LittleTo8(source[0]);
}

void unpackA16BigToRGBA8(const uint16_t* source, uint8_t* destination)
{
    destination[0] = 0x0;
    destination[1] = 0x0;
    destination[2] = 0x0;
    destination[3] = convertColor16BigTo8(source[0]);
}

void unpackRGB32FToRGBA32F(const float* source, float* destination)
{
    destination[0] = source[0];
    destination[1] = source[1];
    destination[2] = source[2];
    destination[3] = 1;
}

void unpackR32FToRGBA32F(const float* source, float* destination)
{
    destination[0] = source[0];
    destination[1] = source[0];
    destination[2] = source[0];
    destination[3] = 1;
}

void unpackRA32FToRGBA32F(const float* source, float* destination)
{
    destination[0] = source[0];
    destination[1] = source[0];
    destination[2] = source[0];
    destination[3] = source[1];
}

void unpackA32FToRGBA32F(const float* source, float* destination)
{
    destination[0] = 0;
    destination[1] = 0;
    destination[2] = 0;
    destination[3] = source[0];
}

//----------------------------------------------------------------------
// Pixel packing routines.
//

void packRGBA8ToA8(const uint8_t* source, uint8_t* destination)
{
    destination[0] = source[3];
}

void packRGBA8ToR8(const uint8_t* source, uint8_t* destination)
{
    destination[0] = source[0];
}

void packRGBA8ToR8Premultiply(const uint8_t* source, uint8_t* destination)
{
    float scaleFactor = source[3] / 255.0f;
    uint8_t sourceR = static_cast<uint8_t>(static_cast<float>(source[0]) * scaleFactor);
    destination[0] = sourceR;
}

// FIXME: this routine is lossy and must be removed.
void packRGBA8ToR8Unmultiply(const uint8_t* source, uint8_t* destination)
{
    float scaleFactor = 1.0f / (source[3] ? source[3] / 255.0f : 1.0f);
    uint8_t sourceR = static_cast<uint8_t>(static_cast<float>(source[0]) * scaleFactor);
    destination[0] = sourceR;
}

void packRGBA8ToRA8(const uint8_t* source, uint8_t* destination)
{
    destination[0] = source[0];
    destination[1] = source[3];
}

void packRGBA8ToRA8Premultiply(const uint8_t* source, uint8_t* destination)
{
    float scaleFactor = source[3] / 255.0f;
    uint8_t sourceR = static_cast<uint8_t>(static_cast<float>(source[0]) * scaleFactor);
    destination[0] = sourceR;
    destination[1] = source[3];
}

// FIXME: this routine is lossy and must be removed.
void packRGBA8ToRA8Unmultiply(const uint8_t* source, uint8_t* destination)
{
    float scaleFactor = 1.0f / (source[3] ? source[3] / 255.0f : 1.0f);
    uint8_t sourceR = static_cast<uint8_t>(static_cast<float>(source[0]) * scaleFactor);
    destination[0] = sourceR;
    destination[1] = source[3];
}

void packRGBA8ToRGB8(const uint8_t* source, uint8_t* destination)
{
    destination[0] = source[0];
    destination[1] = source[1];
    destination[2] = source[2];
}

void packRGBA8ToRGB8Premultiply(const uint8_t* source, uint8_t* destination)
{
    float scaleFactor = source[3] / 255.0f;
    uint8_t sourceR = static_cast<uint8_t>(static_cast<float>(source[0]) * scaleFactor);
    uint8_t sourceG = static_cast<uint8_t>(static_cast<float>(source[1]) * scaleFactor);
    uint8_t sourceB = static_cast<uint8_t>(static_cast<float>(source[2]) * scaleFactor);
    destination[0] = sourceR;
    destination[1] = sourceG;
    destination[2] = sourceB;
}

// FIXME: this routine is lossy and must be removed.
void packRGBA8ToRGB8Unmultiply(const uint8_t* source, uint8_t* destination)
{
    float scaleFactor = 1.0f / (source[3] ? source[3] / 255.0f : 1.0f);
    uint8_t sourceR = static_cast<uint8_t>(static_cast<float>(source[0]) * scaleFactor);
    uint8_t sourceG = static_cast<uint8_t>(static_cast<float>(source[1]) * scaleFactor);
    uint8_t sourceB = static_cast<uint8_t>(static_cast<float>(source[2]) * scaleFactor);
    destination[0] = sourceR;
    destination[1] = sourceG;
    destination[2] = sourceB;
}

// This is only used when the source format is different than SourceFormatRGBA8.
void packRGBA8ToRGBA8(const uint8_t* source, uint8_t* destination)
{
    destination[0] = source[0];
    destination[1] = source[1];
    destination[2] = source[2];
    destination[3] = source[3];
}

void packRGBA8ToRGBA8Premultiply(const uint8_t* source, uint8_t* destination)
{
    float scaleFactor = source[3] / 255.0f;
    uint8_t sourceR = static_cast<uint8_t>(static_cast<float>(source[0]) * scaleFactor);
    uint8_t sourceG = static_cast<uint8_t>(static_cast<float>(source[1]) * scaleFactor);
    uint8_t sourceB = static_cast<uint8_t>(static_cast<float>(source[2]) * scaleFactor);
    destination[0] = sourceR;
    destination[1] = sourceG;
    destination[2] = sourceB;
    destination[3] = source[3];
}

// FIXME: this routine is lossy and must be removed.
void packRGBA8ToRGBA8Unmultiply(const uint8_t* source, uint8_t* destination)
{
    float scaleFactor = 1.0f / (source[3] ? source[3] / 255.0f : 1.0f);
    uint8_t sourceR = static_cast<uint8_t>(static_cast<float>(source[0]) * scaleFactor);
    uint8_t sourceG = static_cast<uint8_t>(static_cast<float>(source[1]) * scaleFactor);
    uint8_t sourceB = static_cast<uint8_t>(static_cast<float>(source[2]) * scaleFactor);
    destination[0] = sourceR;
    destination[1] = sourceG;
    destination[2] = sourceB;
    destination[3] = source[3];
}

void packRGBA8ToUnsignedShort4444(const uint8_t* source, uint16_t* destination)
{
    *destination = (((source[0] & 0xF0) << 8)
                    | ((source[1] & 0xF0) << 4)
                    | (source[2] & 0xF0)
                    | (source[3] >> 4));
}

void packRGBA8ToUnsignedShort4444Premultiply(const uint8_t* source, uint16_t* destination)
{
    float scaleFactor = source[3] / 255.0f;
    uint8_t sourceR = static_cast<uint8_t>(static_cast<float>(source[0]) * scaleFactor);
    uint8_t sourceG = static_cast<uint8_t>(static_cast<float>(source[1]) * scaleFactor);
    uint8_t sourceB = static_cast<uint8_t>(static_cast<float>(source[2]) * scaleFactor);
    *destination = (((sourceR & 0xF0) << 8)
                    | ((sourceG & 0xF0) << 4)
                    | (sourceB & 0xF0)
                    | (source[3] >> 4));
}

// FIXME: this routine is lossy and must be removed.
void packRGBA8ToUnsignedShort4444Unmultiply(const uint8_t* source, uint16_t* destination)
{
    float scaleFactor = 1.0f / (source[3] ? source[3] / 255.0f : 1.0f);
    uint8_t sourceR = static_cast<uint8_t>(static_cast<float>(source[0]) * scaleFactor);
    uint8_t sourceG = static_cast<uint8_t>(static_cast<float>(source[1]) * scaleFactor);
    uint8_t sourceB = static_cast<uint8_t>(static_cast<float>(source[2]) * scaleFactor);
    *destination = (((sourceR & 0xF0) << 8)
                    | ((sourceG & 0xF0) << 4)
                    | (sourceB & 0xF0)
                    | (source[3] >> 4));
}

void packRGBA8ToUnsignedShort5551(const uint8_t* source, uint16_t* destination)
{
    *destination = (((source[0] & 0xF8) << 8)
                    | ((source[1] & 0xF8) << 3)
                    | ((source[2] & 0xF8) >> 2)
                    | (source[3] >> 7));
}

void packRGBA8ToUnsignedShort5551Premultiply(const uint8_t* source, uint16_t* destination)
{
    float scaleFactor = source[3] / 255.0f;
    uint8_t sourceR = static_cast<uint8_t>(static_cast<float>(source[0]) * scaleFactor);
    uint8_t sourceG = static_cast<uint8_t>(static_cast<float>(source[1]) * scaleFactor);
    uint8_t sourceB = static_cast<uint8_t>(static_cast<float>(source[2]) * scaleFactor);
    *destination = (((sourceR & 0xF8) << 8)
                    | ((sourceG & 0xF8) << 3)
                    | ((sourceB & 0xF8) >> 2)
                    | (source[3] >> 7));
}

// FIXME: this routine is lossy and must be removed.
void packRGBA8ToUnsignedShort5551Unmultiply(const uint8_t* source, uint16_t* destination)
{
    float scaleFactor = 1.0f / (source[3] ? source[3] / 255.0f : 1.0f);
    uint8_t sourceR = static_cast<uint8_t>(static_cast<float>(source[0]) * scaleFactor);
    uint8_t sourceG = static_cast<uint8_t>(static_cast<float>(source[1]) * scaleFactor);
    uint8_t sourceB = static_cast<uint8_t>(static_cast<float>(source[2]) * scaleFactor);
    *destination = (((sourceR & 0xF8) << 8)
                    | ((sourceG & 0xF8) << 3)
                    | ((sourceB & 0xF8) >> 2)
                    | (source[3] >> 7));
}

void packRGBA8ToUnsignedShort565(const uint8_t* source, uint16_t* destination)
{
    *destination = (((source[0] & 0xF8) << 8)
                    | ((source[1] & 0xFC) << 3)
                    | ((source[2] & 0xF8) >> 3));
}

void packRGBA8ToUnsignedShort565Premultiply(const uint8_t* source, uint16_t* destination)
{
    float scaleFactor = source[3] / 255.0f;
    uint8_t sourceR = static_cast<uint8_t>(static_cast<float>(source[0]) * scaleFactor);
    uint8_t sourceG = static_cast<uint8_t>(static_cast<float>(source[1]) * scaleFactor);
    uint8_t sourceB = static_cast<uint8_t>(static_cast<float>(source[2]) * scaleFactor);
    *destination = (((sourceR & 0xF8) << 8)
                    | ((sourceG & 0xFC) << 3)
                    | ((sourceB & 0xF8) >> 3));
}

// FIXME: this routine is lossy and must be removed.
void packRGBA8ToUnsignedShort565Unmultiply(const uint8_t* source, uint16_t* destination)
{
    float scaleFactor = 1.0f / (source[3] ? source[3] / 255.0f : 1.0f);
    uint8_t sourceR = static_cast<uint8_t>(static_cast<float>(source[0]) * scaleFactor);
    uint8_t sourceG = static_cast<uint8_t>(static_cast<float>(source[1]) * scaleFactor);
    uint8_t sourceB = static_cast<uint8_t>(static_cast<float>(source[2]) * scaleFactor);
    *destination = (((sourceR & 0xF8) << 8)
                    | ((sourceG & 0xFC) << 3)
                    | ((sourceB & 0xF8) >> 3));
}

void packRGBA32FToRGB32F(const float* source, float* destination)
{
    destination[0] = source[0];
    destination[1] = source[1];
    destination[2] = source[2];
}

void packRGBA32FToRGB32FPremultiply(const float* source, float* destination)
{
    float scaleFactor = source[3];
    destination[0] = source[0] * scaleFactor;
    destination[1] = source[1] * scaleFactor;
    destination[2] = source[2] * scaleFactor;
}

void packRGBA32FToRGBA32FPremultiply(const float* source, float* destination)
{
    float scaleFactor = source[3];
    destination[0] = source[0] * scaleFactor;
    destination[1] = source[1] * scaleFactor;
    destination[2] = source[2] * scaleFactor;
    destination[3] = source[3];
}

void packRGBA32FToA32F(const float* source, float* destination)
{
    destination[0] = source[3];
}

void packRGBA32FToR32F(const float* source, float* destination)
{
    destination[0] = source[0];
}

void packRGBA32FToR32FPremultiply(const float* source, float* destination)
{
    float scaleFactor = source[3];
    destination[0] = source[0] * scaleFactor;
}


void packRGBA32FToRA32F(const float* source, float* destination)
{
    destination[0] = source[0];
    destination[1] = source[3];
}

void packRGBA32FToRA32FPremultiply(const float* source, float* destination)
{
    float scaleFactor = source[3];
    destination[0] = source[0] * scaleFactor;
    destination[1] = scaleFactor;
}

} // anonymous namespace

// This is used whenever unpacking is necessary; i.e., the source data
// is not in RGBA8/RGBA32F format, or the unpack alignment specifies
// that rows are not tightly packed.
template<typename SourceType, typename IntermediateType, typename DestType,
         void unpackingFunc(const SourceType*, IntermediateType*),
         void packingFunc(const IntermediateType*, DestType*)>
static void doUnpackingAndPacking(const SourceType* sourceData,
                                  unsigned int width,
                                  unsigned int height,
                                  unsigned int sourceElementsPerPixel,
                                  unsigned int sourceElementsPerRow,
                                  DestType* destinationData,
                                  unsigned int destinationElementsPerPixel)
{
    if (!sourceElementsPerRow) {
        unsigned int numElements = width * height * sourceElementsPerPixel;
        const SourceType* endPointer = sourceData + numElements;
        IntermediateType temporaryRGBAData[4];
        while (sourceData < endPointer) {
            unpackingFunc(sourceData, temporaryRGBAData);
            packingFunc(temporaryRGBAData, destinationData);
            sourceData += sourceElementsPerPixel;
            destinationData += destinationElementsPerPixel;
        }
    } else {
        IntermediateType temporaryRGBAData[4];
        for (unsigned int y = 0; y < height; ++y) {
            const SourceType* currentSource = sourceData;
            for (unsigned int x = 0; x < width; ++x) {
                unpackingFunc(currentSource, temporaryRGBAData);
                packingFunc(temporaryRGBAData, destinationData);
                currentSource += sourceElementsPerPixel;
                destinationData += destinationElementsPerPixel;
            }
            sourceData += sourceElementsPerRow;
        }
    }
}

template<typename SourceType>
static void computeIncrementParameters(unsigned int width,
                                       unsigned int bytesPerPixel,
                                       unsigned int unpackAlignment,
                                       unsigned int* sourceElementsPerPixel,
                                       unsigned int* sourceElementsPerRow)
{
    unsigned int elementSizeInBytes = sizeof(SourceType);
    ASSERT(elementSizeInBytes <= bytesPerPixel);
    ASSERT(!(bytesPerPixel % elementSizeInBytes));
    unsigned int validRowBytes = width * bytesPerPixel;
    unsigned int totalRowBytes = validRowBytes;
    if (unpackAlignment) {
        unsigned int remainder = validRowBytes % unpackAlignment;
        if (remainder)
            totalRowBytes += (unpackAlignment - remainder);
    }
    *sourceElementsPerPixel = bytesPerPixel / elementSizeInBytes;
    if (validRowBytes == totalRowBytes)
        *sourceElementsPerRow = 0;
    else
        *sourceElementsPerRow = totalRowBytes / elementSizeInBytes;
}

// This handles all conversions with a faster path for tightly packed RGBA8 source data.
template<typename DestType, void packingFunc(const uint8_t*, DestType*)>
static void doPacking(const void* sourceData,
                      GraphicsContext3D::SourceDataFormat sourceDataFormat,
                      unsigned int width,
                      unsigned int height,
                      unsigned int sourceUnpackAlignment,
                      DestType* destinationData,
                      unsigned int destinationElementsPerPixel)
{
    switch (sourceDataFormat) {
    case GraphicsContext3D::SourceFormatRGBA8: {
        unsigned int sourceElementsPerPixel, sourceElementsPerRow;
        computeIncrementParameters<uint8_t>(width, 4, sourceUnpackAlignment, &sourceElementsPerPixel, &sourceElementsPerRow);
        if (!sourceElementsPerRow) {
            const uint8_t* source = static_cast<const uint8_t*>(sourceData);
            unsigned int numElements = width * height * 4;
            const uint8_t* endPointer = source + numElements;
            while (source < endPointer) {
                packingFunc(source, destinationData);
                source += sourceElementsPerPixel;
                destinationData += destinationElementsPerPixel;
            }
        } else {
            doUnpackingAndPacking<uint8_t, uint8_t, DestType, unpackRGBA8ToRGBA8, packingFunc>(static_cast<const uint8_t*>(sourceData), width, height, sourceElementsPerPixel, sourceElementsPerRow, destinationData, destinationElementsPerPixel);
        }
        break;
    }
    case GraphicsContext3D::SourceFormatRGBA16Little: {
        unsigned int sourceElementsPerPixel, sourceElementsPerRow;
        computeIncrementParameters<uint16_t>(width, 8, sourceUnpackAlignment, &sourceElementsPerPixel, &sourceElementsPerRow);
        doUnpackingAndPacking<uint16_t, uint8_t, DestType, unpackRGBA16LittleToRGBA8, packingFunc>(static_cast<const uint16_t*>(sourceData), width, height, sourceElementsPerPixel, sourceElementsPerRow, destinationData, destinationElementsPerPixel);
        break;
    }
    case GraphicsContext3D::SourceFormatRGBA16Big: {
        unsigned int sourceElementsPerPixel, sourceElementsPerRow;
        computeIncrementParameters<uint16_t>(width, 8, sourceUnpackAlignment, &sourceElementsPerPixel, &sourceElementsPerRow);
        doUnpackingAndPacking<uint16_t, uint8_t, DestType, unpackRGBA16BigToRGBA8, packingFunc>(static_cast<const uint16_t*>(sourceData), width, height, sourceElementsPerPixel, sourceElementsPerRow, destinationData, destinationElementsPerPixel);
        break;
    }
    case GraphicsContext3D::SourceFormatRGB8: {
        unsigned int sourceElementsPerPixel, sourceElementsPerRow;
        computeIncrementParameters<uint8_t>(width, 3, sourceUnpackAlignment, &sourceElementsPerPixel, &sourceElementsPerRow);
        doUnpackingAndPacking<uint8_t, uint8_t, DestType, unpackRGB8ToRGBA8, packingFunc>(static_cast<const uint8_t*>(sourceData), width, height, sourceElementsPerPixel, sourceElementsPerRow, destinationData, destinationElementsPerPixel);
        break;
    }
    case GraphicsContext3D::SourceFormatRGB16Little: {
        unsigned int sourceElementsPerPixel, sourceElementsPerRow;
        computeIncrementParameters<uint16_t>(width, 6, sourceUnpackAlignment, &sourceElementsPerPixel, &sourceElementsPerRow);
        doUnpackingAndPacking<uint16_t, uint8_t, DestType, unpackRGB16LittleToRGBA8, packingFunc>(static_cast<const uint16_t*>(sourceData), width, height, sourceElementsPerPixel, sourceElementsPerRow, destinationData, destinationElementsPerPixel);
        break;
    }
    case GraphicsContext3D::SourceFormatRGB16Big: {
        unsigned int sourceElementsPerPixel, sourceElementsPerRow;
        computeIncrementParameters<uint16_t>(width, 6, sourceUnpackAlignment, &sourceElementsPerPixel, &sourceElementsPerRow);
        doUnpackingAndPacking<uint16_t, uint8_t, DestType, unpackRGB16BigToRGBA8, packingFunc>(static_cast<const uint16_t*>(sourceData), width, height, sourceElementsPerPixel, sourceElementsPerRow, destinationData, destinationElementsPerPixel);
        break;
    }
    case GraphicsContext3D::SourceFormatBGR8: {
        unsigned int sourceElementsPerPixel, sourceElementsPerRow;
        computeIncrementParameters<uint8_t>(width, 3, sourceUnpackAlignment, &sourceElementsPerPixel, &sourceElementsPerRow);
        doUnpackingAndPacking<uint8_t, uint8_t, DestType, unpackBGR8ToRGBA8, packingFunc>(static_cast<const uint8_t*>(sourceData), width, height, sourceElementsPerPixel, sourceElementsPerRow, destinationData, destinationElementsPerPixel);
        break;
    }
    case GraphicsContext3D::SourceFormatARGB8: {
        unsigned int sourceElementsPerPixel, sourceElementsPerRow;
        computeIncrementParameters<uint8_t>(width, 4, sourceUnpackAlignment, &sourceElementsPerPixel, &sourceElementsPerRow);
        doUnpackingAndPacking<uint8_t, uint8_t, DestType, unpackARGB8ToRGBA8, packingFunc>(static_cast<const uint8_t*>(sourceData), width, height, sourceElementsPerPixel, sourceElementsPerRow, destinationData, destinationElementsPerPixel);
        break;
    }
    case GraphicsContext3D::SourceFormatARGB16Little: {
        unsigned int sourceElementsPerPixel, sourceElementsPerRow;
        computeIncrementParameters<uint16_t>(width, 8, sourceUnpackAlignment, &sourceElementsPerPixel, &sourceElementsPerRow);
        doUnpackingAndPacking<uint16_t, uint8_t, DestType, unpackARGB16LittleToRGBA8, packingFunc>(static_cast<const uint16_t*>(sourceData), width, height, sourceElementsPerPixel, sourceElementsPerRow, destinationData, destinationElementsPerPixel);
        break;
    }
    case GraphicsContext3D::SourceFormatARGB16Big: {
        unsigned int sourceElementsPerPixel, sourceElementsPerRow;
        computeIncrementParameters<uint16_t>(width, 8, sourceUnpackAlignment, &sourceElementsPerPixel, &sourceElementsPerRow);
        doUnpackingAndPacking<uint16_t, uint8_t, DestType, unpackARGB16BigToRGBA8, packingFunc>(static_cast<const uint16_t*>(sourceData), width, height, sourceElementsPerPixel, sourceElementsPerRow, destinationData, destinationElementsPerPixel);
        break;
    }
    case GraphicsContext3D::SourceFormatABGR8: {
        unsigned int sourceElementsPerPixel, sourceElementsPerRow;
        computeIncrementParameters<uint8_t>(width, 4, sourceUnpackAlignment, &sourceElementsPerPixel, &sourceElementsPerRow);
        doUnpackingAndPacking<uint8_t, uint8_t, DestType, unpackABGR8ToRGBA8, packingFunc>(static_cast<const uint8_t*>(sourceData), width, height, sourceElementsPerPixel, sourceElementsPerRow, destinationData, destinationElementsPerPixel);
        break;
    }
    case GraphicsContext3D::SourceFormatBGRA8: {
        unsigned int sourceElementsPerPixel, sourceElementsPerRow;
        computeIncrementParameters<uint8_t>(width, 4, sourceUnpackAlignment, &sourceElementsPerPixel, &sourceElementsPerRow);
        doUnpackingAndPacking<uint8_t, uint8_t, DestType, unpackBGRA8ToRGBA8, packingFunc>(static_cast<const uint8_t*>(sourceData), width, height, sourceElementsPerPixel, sourceElementsPerRow, destinationData, destinationElementsPerPixel);
        break;
    }
    case GraphicsContext3D::SourceFormatBGRA16Little: {
        unsigned int sourceElementsPerPixel, sourceElementsPerRow;
        computeIncrementParameters<uint16_t>(width, 8, sourceUnpackAlignment, &sourceElementsPerPixel, &sourceElementsPerRow);
        doUnpackingAndPacking<uint16_t, uint8_t, DestType, unpackBGRA16LittleToRGBA8, packingFunc>(static_cast<const uint16_t*>(sourceData), width, height, sourceElementsPerPixel, sourceElementsPerRow, destinationData, destinationElementsPerPixel);
        break;
    }
    case GraphicsContext3D::SourceFormatBGRA16Big: {
        unsigned int sourceElementsPerPixel, sourceElementsPerRow;
        computeIncrementParameters<uint16_t>(width, 8, sourceUnpackAlignment, &sourceElementsPerPixel, &sourceElementsPerRow);
        doUnpackingAndPacking<uint16_t, uint8_t, DestType, unpackBGRA16BigToRGBA8, packingFunc>(static_cast<const uint16_t*>(sourceData), width, height, sourceElementsPerPixel, sourceElementsPerRow, destinationData, destinationElementsPerPixel);
        break;
    }
    case GraphicsContext3D::SourceFormatRGBA5551: {
        unsigned int sourceElementsPerPixel, sourceElementsPerRow;
        computeIncrementParameters<uint16_t>(width, 2, sourceUnpackAlignment, &sourceElementsPerPixel, &sourceElementsPerRow);
        doUnpackingAndPacking<uint16_t, uint8_t, DestType, unpackRGBA5551ToRGBA8, packingFunc>(static_cast<const uint16_t*>(sourceData), width, height, sourceElementsPerPixel, sourceElementsPerRow, destinationData, destinationElementsPerPixel);
        break;
    }
    case GraphicsContext3D::SourceFormatRGBA4444: {
        unsigned int sourceElementsPerPixel, sourceElementsPerRow;
        computeIncrementParameters<uint16_t>(width, 2, sourceUnpackAlignment, &sourceElementsPerPixel, &sourceElementsPerRow);
        doUnpackingAndPacking<uint16_t, uint8_t, DestType, unpackRGBA4444ToRGBA8, packingFunc>(static_cast<const uint16_t*>(sourceData), width, height, sourceElementsPerPixel, sourceElementsPerRow, destinationData, destinationElementsPerPixel);
        break;
    }
    case GraphicsContext3D::SourceFormatRGB565: {
        unsigned int sourceElementsPerPixel, sourceElementsPerRow;
        computeIncrementParameters<uint16_t>(width, 2, sourceUnpackAlignment, &sourceElementsPerPixel, &sourceElementsPerRow);
        doUnpackingAndPacking<uint16_t, uint8_t, DestType, unpackRGB565ToRGBA8, packingFunc>(static_cast<const uint16_t*>(sourceData), width, height, sourceElementsPerPixel, sourceElementsPerRow, destinationData, destinationElementsPerPixel);
        break;
    }
    case GraphicsContext3D::SourceFormatR8: {
        unsigned int sourceElementsPerPixel, sourceElementsPerRow;
        computeIncrementParameters<uint8_t>(width, 1, sourceUnpackAlignment, &sourceElementsPerPixel, &sourceElementsPerRow);
        doUnpackingAndPacking<uint8_t, uint8_t, DestType, unpackR8ToRGBA8, packingFunc>(static_cast<const uint8_t*>(sourceData), width, height, sourceElementsPerPixel, sourceElementsPerRow, destinationData, destinationElementsPerPixel);
        break;
    }
    case GraphicsContext3D::SourceFormatR16Little: {
        unsigned int sourceElementsPerPixel, sourceElementsPerRow;
        computeIncrementParameters<uint16_t>(width, 2, sourceUnpackAlignment, &sourceElementsPerPixel, &sourceElementsPerRow);
        doUnpackingAndPacking<uint16_t, uint8_t, DestType, unpackR16LittleToRGBA8, packingFunc>(static_cast<const uint16_t*>(sourceData), width, height, sourceElementsPerPixel, sourceElementsPerRow, destinationData, destinationElementsPerPixel);
        break;
    }
    case GraphicsContext3D::SourceFormatR16Big: {
        unsigned int sourceElementsPerPixel, sourceElementsPerRow;
        computeIncrementParameters<uint16_t>(width, 2, sourceUnpackAlignment, &sourceElementsPerPixel, &sourceElementsPerRow);
        doUnpackingAndPacking<uint16_t, uint8_t, DestType, unpackR16BigToRGBA8, packingFunc>(static_cast<const uint16_t*>(sourceData), width, height, sourceElementsPerPixel, sourceElementsPerRow, destinationData, destinationElementsPerPixel);
        break;
    }
    case GraphicsContext3D::SourceFormatRA8: {
        unsigned int sourceElementsPerPixel, sourceElementsPerRow;
        computeIncrementParameters<uint8_t>(width, 2, sourceUnpackAlignment, &sourceElementsPerPixel, &sourceElementsPerRow);
        doUnpackingAndPacking<uint8_t, uint8_t, DestType, unpackRA8ToRGBA8, packingFunc>(static_cast<const uint8_t*>(sourceData), width, height, sourceElementsPerPixel, sourceElementsPerRow, destinationData, destinationElementsPerPixel);
        break;
    }
    case GraphicsContext3D::SourceFormatRA16Little: {
        unsigned int sourceElementsPerPixel, sourceElementsPerRow;
        computeIncrementParameters<uint16_t>(width, 4, sourceUnpackAlignment, &sourceElementsPerPixel, &sourceElementsPerRow);
        doUnpackingAndPacking<uint16_t, uint8_t, DestType, unpackRA16LittleToRGBA8, packingFunc>(static_cast<const uint16_t*>(sourceData), width, height, sourceElementsPerPixel, sourceElementsPerRow, destinationData, destinationElementsPerPixel);
        break;
    }
    case GraphicsContext3D::SourceFormatRA16Big: {
        unsigned int sourceElementsPerPixel, sourceElementsPerRow;
        computeIncrementParameters<uint16_t>(width, 4, sourceUnpackAlignment, &sourceElementsPerPixel, &sourceElementsPerRow);
        doUnpackingAndPacking<uint16_t, uint8_t, DestType, unpackRA16BigToRGBA8, packingFunc>(static_cast<const uint16_t*>(sourceData), width, height, sourceElementsPerPixel, sourceElementsPerRow, destinationData, destinationElementsPerPixel);
        break;
    }
    case GraphicsContext3D::SourceFormatAR8: {
        unsigned int sourceElementsPerPixel, sourceElementsPerRow;
        computeIncrementParameters<uint8_t>(width, 2, sourceUnpackAlignment, &sourceElementsPerPixel, &sourceElementsPerRow);
        doUnpackingAndPacking<uint8_t, uint8_t, DestType, unpackAR8ToRGBA8, packingFunc>(static_cast<const uint8_t*>(sourceData), width, height, sourceElementsPerPixel, sourceElementsPerRow, destinationData, destinationElementsPerPixel);
        break;
    }
    case GraphicsContext3D::SourceFormatAR16Little: {
        unsigned int sourceElementsPerPixel, sourceElementsPerRow;
        computeIncrementParameters<uint16_t>(width, 4, sourceUnpackAlignment, &sourceElementsPerPixel, &sourceElementsPerRow);
        doUnpackingAndPacking<uint16_t, uint8_t, DestType, unpackAR16LittleToRGBA8, packingFunc>(static_cast<const uint16_t*>(sourceData), width, height, sourceElementsPerPixel, sourceElementsPerRow, destinationData, destinationElementsPerPixel);
        break;
    }
    case GraphicsContext3D::SourceFormatAR16Big: {
        unsigned int sourceElementsPerPixel, sourceElementsPerRow;
        computeIncrementParameters<uint16_t>(width, 4, sourceUnpackAlignment, &sourceElementsPerPixel, &sourceElementsPerRow);
        doUnpackingAndPacking<uint16_t, uint8_t, DestType, unpackAR16BigToRGBA8, packingFunc>(static_cast<const uint16_t*>(sourceData), width, height, sourceElementsPerPixel, sourceElementsPerRow, destinationData, destinationElementsPerPixel);
        break;
    }
    case GraphicsContext3D::SourceFormatA8: {
        unsigned int sourceElementsPerPixel, sourceElementsPerRow;
        computeIncrementParameters<uint8_t>(width, 1, sourceUnpackAlignment, &sourceElementsPerPixel, &sourceElementsPerRow);
        doUnpackingAndPacking<uint8_t, uint8_t, DestType, unpackA8ToRGBA8, packingFunc>(static_cast<const uint8_t*>(sourceData), width, height, sourceElementsPerPixel, sourceElementsPerRow, destinationData, destinationElementsPerPixel);
        break;
    }
    case GraphicsContext3D::SourceFormatA16Little: {
        unsigned int sourceElementsPerPixel, sourceElementsPerRow;
        computeIncrementParameters<uint16_t>(width, 2, sourceUnpackAlignment, &sourceElementsPerPixel, &sourceElementsPerRow);
        doUnpackingAndPacking<uint16_t, uint8_t, DestType, unpackA16LittleToRGBA8, packingFunc>(static_cast<const uint16_t*>(sourceData), width, height, sourceElementsPerPixel, sourceElementsPerRow, destinationData, destinationElementsPerPixel);
        break;
    }
    case GraphicsContext3D::SourceFormatA16Big: {
        unsigned int sourceElementsPerPixel, sourceElementsPerRow;
        computeIncrementParameters<uint16_t>(width, 2, sourceUnpackAlignment, &sourceElementsPerPixel, &sourceElementsPerRow);
        doUnpackingAndPacking<uint16_t, uint8_t, DestType, unpackA16BigToRGBA8, packingFunc>(static_cast<const uint16_t*>(sourceData), width, height, sourceElementsPerPixel, sourceElementsPerRow, destinationData, destinationElementsPerPixel);
        break;
    }
    default:
        ASSERT(false);
    }
}

// This specialized routine is used only for floating-point texture uploads. It
// does not need to be as general as doPacking, above; because there are
// currently no native floating-point image formats in WebKit, there are only a
// few upload paths.
template<void packingFunc(const float*, float*)>
static void doFloatingPointPacking(const void* sourceData,
                                   GraphicsContext3D::SourceDataFormat sourceDataFormat,
                                   unsigned int width,
                                   unsigned int height,
                                   unsigned int sourceUnpackAlignment,
                                   float* destinationData,
                                   unsigned int destinationElementsPerPixel)
{
    switch (sourceDataFormat) {
    case GraphicsContext3D::SourceFormatRGBA8: {
        unsigned int sourceElementsPerPixel, sourceElementsPerRow;
        computeIncrementParameters<float>(width, 4, sourceUnpackAlignment, &sourceElementsPerPixel, &sourceElementsPerRow);
        ASSERT(!sourceElementsPerRow); // Guaranteed because each color channel is sizeof(float) bytes.
        const float* source = static_cast<const float*>(sourceData);
        unsigned int numElements = width * height * 4;
        const float* endPointer = source + numElements;
        while (source < endPointer) {
            packingFunc(source, destinationData);
            source += sourceElementsPerPixel;
            destinationData += destinationElementsPerPixel;
        }
        break;
    }
    case GraphicsContext3D::SourceFormatRGB32F: {
        unsigned int sourceElementsPerPixel, sourceElementsPerRow;
        computeIncrementParameters<float>(width, 3, sourceUnpackAlignment, &sourceElementsPerPixel, &sourceElementsPerRow);
        doUnpackingAndPacking<float, float, float, unpackRGB32FToRGBA32F, packingFunc>(static_cast<const float*>(sourceData), width, height, sourceElementsPerPixel, sourceElementsPerRow, destinationData, destinationElementsPerPixel);
        break;
    }
    case GraphicsContext3D::SourceFormatR32F: {
        unsigned int sourceElementsPerPixel, sourceElementsPerRow;
        computeIncrementParameters<float>(width, 1, sourceUnpackAlignment, &sourceElementsPerPixel, &sourceElementsPerRow);
        doUnpackingAndPacking<float, float, float, unpackR32FToRGBA32F, packingFunc>(static_cast<const float*>(sourceData), width, height, sourceElementsPerPixel, sourceElementsPerRow, destinationData, destinationElementsPerPixel);
        break;
    }
    case GraphicsContext3D::SourceFormatRA32F: {
        unsigned int sourceElementsPerPixel, sourceElementsPerRow;
        computeIncrementParameters<float>(width, 2, sourceUnpackAlignment, &sourceElementsPerPixel, &sourceElementsPerRow);
        doUnpackingAndPacking<float, float, float, unpackRA32FToRGBA32F, packingFunc>(static_cast<const float*>(sourceData), width, height, sourceElementsPerPixel, sourceElementsPerRow, destinationData, destinationElementsPerPixel);
        break;
    }
    case GraphicsContext3D::SourceFormatA32F: {
        unsigned int sourceElementsPerPixel, sourceElementsPerRow;
        computeIncrementParameters<float>(width, 1, sourceUnpackAlignment, &sourceElementsPerPixel, &sourceElementsPerRow);
        doUnpackingAndPacking<float, float, float, unpackA32FToRGBA32F, packingFunc>(static_cast<const float*>(sourceData), width, height, sourceElementsPerPixel, sourceElementsPerRow, destinationData, destinationElementsPerPixel);
        break;
    }
    default:
        ASSERT_NOT_REACHED();
    }
}

bool GraphicsContext3D::packPixels(const uint8_t* sourceData,
                                   GraphicsContext3D::SourceDataFormat sourceDataFormat,
                                   unsigned int width,
                                   unsigned int height,
                                   unsigned int sourceUnpackAlignment,
                                   unsigned int destinationFormat,
                                   unsigned int destinationType,
                                   AlphaOp alphaOp,
                                   void* destinationData)
{
    switch (destinationType) {
    case UNSIGNED_BYTE: {
        uint8_t* destination = static_cast<uint8_t*>(destinationData);
        if (sourceDataFormat == SourceFormatRGBA8 && destinationFormat == RGBA && sourceUnpackAlignment <= 4 && alphaOp == AlphaDoNothing) {
            // No conversion necessary.
            memcpy(destinationData, sourceData, width * height * 4);
            break;
        }
        switch (destinationFormat) {
        case RGB:
            switch (alphaOp) {
            case AlphaDoNothing:
                doPacking<uint8_t, packRGBA8ToRGB8>(sourceData, sourceDataFormat, width, height, sourceUnpackAlignment, destination, 3);
                break;
            case AlphaDoPremultiply:
                doPacking<uint8_t, packRGBA8ToRGB8Premultiply>(sourceData, sourceDataFormat, width, height, sourceUnpackAlignment, destination, 3);
                break;
            case AlphaDoUnmultiply:
                doPacking<uint8_t, packRGBA8ToRGB8Unmultiply>(sourceData, sourceDataFormat, width, height, sourceUnpackAlignment, destination, 3);
                break;
            }
            break;
        case RGBA:
            switch (alphaOp) {
            case AlphaDoNothing:
                ASSERT(sourceDataFormat != SourceFormatRGBA8 || sourceUnpackAlignment > 4); // Handled above with fast case.
                doPacking<uint8_t, packRGBA8ToRGBA8>(sourceData, sourceDataFormat, width, height, sourceUnpackAlignment, destination, 4);
                break;
            case AlphaDoPremultiply:
                doPacking<uint8_t, packRGBA8ToRGBA8Premultiply>(sourceData, sourceDataFormat, width, height, sourceUnpackAlignment, destination, 4);
                break;
            case AlphaDoUnmultiply:
                doPacking<uint8_t, packRGBA8ToRGBA8Unmultiply>(sourceData, sourceDataFormat, width, height, sourceUnpackAlignment, destination, 4);
                break;
            default:
                ASSERT_NOT_REACHED();
            }
            break;
        case ALPHA:
            // From the desktop OpenGL conversion rules (OpenGL 2.1
            // specification, Table 3.15), the alpha channel is chosen
            // from the RGBA data.
            doPacking<uint8_t, packRGBA8ToA8>(sourceData, sourceDataFormat, width, height, sourceUnpackAlignment, destination, 1);
            break;
        case LUMINANCE:
            // From the desktop OpenGL conversion rules (OpenGL 2.1
            // specification, Table 3.15), the red channel is chosen
            // from the RGBA data.
            switch (alphaOp) {
            case AlphaDoNothing:
                doPacking<uint8_t, packRGBA8ToR8>(sourceData, sourceDataFormat, width, height, sourceUnpackAlignment, destination, 1);
                break;
            case AlphaDoPremultiply:
                doPacking<uint8_t, packRGBA8ToR8Premultiply>(sourceData, sourceDataFormat, width, height, sourceUnpackAlignment, destination, 1);
                break;
            case AlphaDoUnmultiply:
                doPacking<uint8_t, packRGBA8ToR8Unmultiply>(sourceData, sourceDataFormat, width, height, sourceUnpackAlignment, destination, 1);
                break;
            }
            break;
        case LUMINANCE_ALPHA:
            // From the desktop OpenGL conversion rules (OpenGL 2.1
            // specification, Table 3.15), the red and alpha channels
            // are chosen from the RGBA data.
            switch (alphaOp) {
            case AlphaDoNothing:
                doPacking<uint8_t, packRGBA8ToRA8>(sourceData, sourceDataFormat, width, height, sourceUnpackAlignment, destination, 2);
                break;
            case AlphaDoPremultiply:
                doPacking<uint8_t, packRGBA8ToRA8Premultiply>(sourceData, sourceDataFormat, width, height, sourceUnpackAlignment, destination, 2);
                break;
            case AlphaDoUnmultiply:
                doPacking<uint8_t, packRGBA8ToRA8Unmultiply>(sourceData, sourceDataFormat, width, height, sourceUnpackAlignment, destination, 2);
                break;
            }
            break;
        }
        break;
    }
    case UNSIGNED_SHORT_4_4_4_4: {
        uint16_t* destination = static_cast<uint16_t*>(destinationData);
        switch (alphaOp) {
        case AlphaDoNothing:
            doPacking<uint16_t, packRGBA8ToUnsignedShort4444>(sourceData, sourceDataFormat, width, height, sourceUnpackAlignment, destination, 1);
            break;
        case AlphaDoPremultiply:
            doPacking<uint16_t, packRGBA8ToUnsignedShort4444Premultiply>(sourceData, sourceDataFormat, width, height, sourceUnpackAlignment, destination, 1);
            break;
        case AlphaDoUnmultiply:
            doPacking<uint16_t, packRGBA8ToUnsignedShort4444Unmultiply>(sourceData, sourceDataFormat, width, height, sourceUnpackAlignment, destination, 1);
            break;
        }
        break;
    }
    case UNSIGNED_SHORT_5_5_5_1: {
        uint16_t* destination = static_cast<uint16_t*>(destinationData);
        switch (alphaOp) {
        case AlphaDoNothing:
            doPacking<uint16_t, packRGBA8ToUnsignedShort5551>(sourceData, sourceDataFormat, width, height, sourceUnpackAlignment, destination, 1);
            break;
        case AlphaDoPremultiply:
            doPacking<uint16_t, packRGBA8ToUnsignedShort5551Premultiply>(sourceData, sourceDataFormat, width, height, sourceUnpackAlignment, destination, 1);
            break;
        case AlphaDoUnmultiply:
            doPacking<uint16_t, packRGBA8ToUnsignedShort5551Unmultiply>(sourceData, sourceDataFormat, width, height, sourceUnpackAlignment, destination, 1);
            break;
        }
        break;
    }
    case UNSIGNED_SHORT_5_6_5: {
        uint16_t* destination = static_cast<uint16_t*>(destinationData);
        switch (alphaOp) {
        case AlphaDoNothing:
            doPacking<uint16_t, packRGBA8ToUnsignedShort565>(sourceData, sourceDataFormat, width, height, sourceUnpackAlignment, destination, 1);
            break;
        case AlphaDoPremultiply:
            doPacking<uint16_t, packRGBA8ToUnsignedShort565Premultiply>(sourceData, sourceDataFormat, width, height, sourceUnpackAlignment, destination, 1);
            break;
        case AlphaDoUnmultiply:
            doPacking<uint16_t, packRGBA8ToUnsignedShort565Unmultiply>(sourceData, sourceDataFormat, width, height, sourceUnpackAlignment, destination, 1);
            break;
        }
        break;
    }
    case FLOAT: {
        // OpenGL ES, and therefore WebGL, require that the format and
        // internalformat be identical, which implies that the source and
        // destination formats will both be floating-point in this branch -- at
        // least, until WebKit supports floating-point image formats natively.
        ASSERT(sourceDataFormat == SourceFormatRGBA32F || sourceDataFormat == SourceFormatRGB32F
               || sourceDataFormat == SourceFormatRA32F || sourceDataFormat == SourceFormatR32F
               || sourceDataFormat == SourceFormatA32F);
        // Because WebKit doesn't use floating-point color channels for anything
        // internally, there's no chance we have to do a (lossy) unmultiply
        // operation.
        ASSERT(alphaOp == AlphaDoNothing || alphaOp == AlphaDoPremultiply);
        // For the source formats with an even number of channels (RGBA32F,
        // RA32F) it is guaranteed that the pixel data is tightly packed because
        // unpack alignment <= sizeof(float) * number of channels.
        float* destination = static_cast<float*>(destinationData);
        if (alphaOp == AlphaDoNothing
            && ((sourceDataFormat == SourceFormatRGBA32F && destinationFormat == RGBA)
                || (sourceDataFormat == SourceFormatRA32F && destinationFormat == LUMINANCE_ALPHA))) {
            // No conversion necessary.
            int numChannels = (sourceDataFormat == SourceFormatRGBA32F ? 4 : 2);
            memcpy(destinationData, sourceData, width * height * numChannels * sizeof(float));
            break;
        }
        switch (destinationFormat) {
        case RGB:
            switch (alphaOp) {
            case AlphaDoNothing:
                doFloatingPointPacking<packRGBA32FToRGB32F>(sourceData, sourceDataFormat, width, height, sourceUnpackAlignment, destination, 3);
                break;
            case AlphaDoPremultiply:
                doFloatingPointPacking<packRGBA32FToRGB32FPremultiply>(sourceData, sourceDataFormat, width, height, sourceUnpackAlignment, destination, 3);
                break;
            default:
                ASSERT_NOT_REACHED();
            }
            break;
        case RGBA:
            // AlphaDoNothing is handled above with fast path.
            ASSERT(alphaOp == AlphaDoPremultiply);
            doFloatingPointPacking<packRGBA32FToRGBA32FPremultiply>(sourceData, sourceDataFormat, width, height, sourceUnpackAlignment, destination, 4);
            break;
        case ALPHA:
            // From the desktop OpenGL conversion rules (OpenGL 2.1
            // specification, Table 3.15), the alpha channel is chosen
            // from the RGBA data.
            doFloatingPointPacking<packRGBA32FToA32F>(sourceData, sourceDataFormat, width, height, sourceUnpackAlignment, destination, 1);
            break;
        case LUMINANCE:
            // From the desktop OpenGL conversion rules (OpenGL 2.1
            // specification, Table 3.15), the red channel is chosen
            // from the RGBA data.
            switch (alphaOp) {
            case AlphaDoNothing:
                doFloatingPointPacking<packRGBA32FToR32F>(sourceData, sourceDataFormat, width, height, sourceUnpackAlignment, destination, 1);
                break;
            case AlphaDoPremultiply:
                doFloatingPointPacking<packRGBA32FToR32FPremultiply>(sourceData, sourceDataFormat, width, height, sourceUnpackAlignment, destination, 1);
                break;
            default:
                ASSERT_NOT_REACHED();
            }
            break;
        case LUMINANCE_ALPHA:
            // From the desktop OpenGL conversion rules (OpenGL 2.1
            // specification, Table 3.15), the red and alpha channels
            // are chosen from the RGBA data.
            switch (alphaOp) {
            case AlphaDoNothing:
                doFloatingPointPacking<packRGBA32FToRA32F>(sourceData, sourceDataFormat, width, height, sourceUnpackAlignment, destination, 2);
                break;
            case AlphaDoPremultiply:
                doFloatingPointPacking<packRGBA32FToRA32FPremultiply>(sourceData, sourceDataFormat, width, height, sourceUnpackAlignment, destination, 2);
                break;
            default:
                ASSERT_NOT_REACHED();
            }
            break;
        }
        break;
    }
    }
    return true;
}

} // namespace WebCore

#endif // ENABLE(WEBGL)
