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

#if ENABLE(3D_CANVAS)

#include "GraphicsContext3D.h"

#include "ArrayBufferView.h"
#include "Image.h"
#include "ImageData.h"

namespace WebCore {

bool GraphicsContext3D::computeFormatAndTypeParameters(unsigned int format,
                                                       unsigned int type,
                                                       unsigned long* componentsPerPixel,
                                                       unsigned long* bytesPerComponent)
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
        *componentsPerPixel = 4;
        break;
    default:
        return false;
    }
    switch (type) {
    case GraphicsContext3D::UNSIGNED_BYTE:
        *bytesPerComponent = sizeof(unsigned char);
        break;
    case GraphicsContext3D::UNSIGNED_SHORT_5_6_5:
    case GraphicsContext3D::UNSIGNED_SHORT_4_4_4_4:
    case GraphicsContext3D::UNSIGNED_SHORT_5_5_5_1:
        *componentsPerPixel = 1;
        *bytesPerComponent = sizeof(unsigned short);
        break;
    default:
        return false;
    }
    return true;
}

bool GraphicsContext3D::extractImageData(Image* image,
                                         unsigned int format,
                                         unsigned int type,
                                         bool flipY,
                                         bool premultiplyAlpha,
                                         Vector<uint8_t>& data)
{
    if (!image)
        return false;
    if (!getImageData(image, format, type, premultiplyAlpha, data))
        return false;
    if (flipY) {
        unsigned long componentsPerPixel, bytesPerComponent;
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
                                         unsigned int format,
                                         unsigned int type,
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
                    kSourceFormatRGBA8,
                    width,
                    height,
                    0,
                    format,
                    type,
                    premultiplyAlpha ? kAlphaDoPremultiply : kAlphaDoNothing,
                    data.data()))
        return false;
    if (flipY) {
        unsigned long componentsPerPixel, bytesPerComponent;
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
                                           unsigned int format, unsigned int type,
                                           unsigned int unpackAlignment,
                                           bool flipY, bool premultiplyAlpha,
                                           const void* pixels,
                                           Vector<uint8_t>& data)
{
    // Assumes format, type, etc. have already been validated.
    SourceDataFormat sourceDataFormat = kSourceFormatRGBA8;
    switch (type) {
    case UNSIGNED_BYTE:
        switch (format) {
        case RGBA:
            sourceDataFormat = kSourceFormatRGBA8;
            break;
        case RGB:
            sourceDataFormat = kSourceFormatRGB8;
            break;
        case ALPHA:
            sourceDataFormat = kSourceFormatA8;
            break;
        case LUMINANCE:
            sourceDataFormat = kSourceFormatR8;
            break;
        case LUMINANCE_ALPHA:
            sourceDataFormat = kSourceFormatRA8;
            break;
        default:
            ASSERT_NOT_REACHED();
        }
        break;
    case UNSIGNED_SHORT_5_5_5_1:
        sourceDataFormat = kSourceFormatRGBA5551;
        break;
    case UNSIGNED_SHORT_4_4_4_4:
        sourceDataFormat = kSourceFormatRGBA4444;
        break;
    case UNSIGNED_SHORT_5_6_5:
        sourceDataFormat = kSourceFormatRGB565;
        break;
    default:
        ASSERT_NOT_REACHED();
    }

    // Resize the output buffer.
    unsigned long componentsPerPixel, bytesPerComponent;
    if (!computeFormatAndTypeParameters(format, type,
                                        &componentsPerPixel,
                                        &bytesPerComponent))
        return false;
    unsigned long bytesPerPixel = componentsPerPixel * bytesPerComponent;
    data.resize(width * height * bytesPerPixel);

    if (!packPixels(static_cast<const uint8_t*>(pixels),
                    sourceDataFormat,
                    width, height, unpackAlignment,
                    format, type,
                    (premultiplyAlpha ? kAlphaDoPremultiply : kAlphaDoNothing),
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

void unpackRGB8ToRGBA8(const uint8_t* source, uint8_t* destination)
{
    destination[0] = source[0];
    destination[1] = source[1];
    destination[2] = source[2];
    destination[3] = 0xFF;
}

void unpackBGRA8ToRGBA8(const uint8_t* source, uint8_t* destination)
{
    destination[0] = source[2];
    destination[1] = source[1];
    destination[2] = source[0];
    destination[3] = source[3];
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

void unpackRA8ToRGBA8(const uint8_t* source, uint8_t* destination)
{
    destination[0] = source[0];
    destination[1] = source[0];
    destination[2] = source[0];
    destination[3] = source[1];
}

void unpackA8ToRGBA8(const uint8_t* source, uint8_t* destination)
{
    destination[0] = 0x0;
    destination[1] = 0x0;
    destination[2] = 0x0;
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

// This is only used when the source format is different than kSourceFormatRGBA8.
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

} // anonymous namespace

// This is used whenever unpacking is necessary; i.e., the source data
// is not in RGBA8 format, or the unpack alignment specifies that rows
// are not tightly packed.
template<typename SourceType, typename DestType,
         void unpackingFunc(const SourceType*, uint8_t*),
         void packingFunc(const uint8_t*, DestType*)>
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
        uint8_t temporaryRGBAData[4];
        while (sourceData < endPointer) {
            unpackingFunc(sourceData, temporaryRGBAData);
            packingFunc(temporaryRGBAData, destinationData);
            sourceData += sourceElementsPerPixel;
            destinationData += destinationElementsPerPixel;
        }
    } else {
        uint8_t temporaryRGBAData[4];
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
    case GraphicsContext3D::kSourceFormatRGBA8: {
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
            doUnpackingAndPacking<uint8_t, DestType, unpackRGBA8ToRGBA8, packingFunc>(static_cast<const uint8_t*>(sourceData), width, height, sourceElementsPerPixel, sourceElementsPerRow, destinationData, destinationElementsPerPixel);
        }
        break;
    }
    case GraphicsContext3D::kSourceFormatRGB8: {
        unsigned int sourceElementsPerPixel, sourceElementsPerRow;
        computeIncrementParameters<uint8_t>(width, 3, sourceUnpackAlignment, &sourceElementsPerPixel, &sourceElementsPerRow);
        doUnpackingAndPacking<uint8_t, DestType, unpackRGB8ToRGBA8, packingFunc>(static_cast<const uint8_t*>(sourceData), width, height, sourceElementsPerPixel, sourceElementsPerRow, destinationData, destinationElementsPerPixel);
        break;
    }
    case GraphicsContext3D::kSourceFormatBGRA8: {
        unsigned int sourceElementsPerPixel, sourceElementsPerRow;
        computeIncrementParameters<uint8_t>(width, 4, sourceUnpackAlignment, &sourceElementsPerPixel, &sourceElementsPerRow);
        doUnpackingAndPacking<uint8_t, DestType, unpackBGRA8ToRGBA8, packingFunc>(static_cast<const uint8_t*>(sourceData), width, height, sourceElementsPerPixel, sourceElementsPerRow, destinationData, destinationElementsPerPixel);
        break;
    }
    case GraphicsContext3D::kSourceFormatRGBA5551: {
        unsigned int sourceElementsPerPixel, sourceElementsPerRow;
        computeIncrementParameters<uint16_t>(width, 2, sourceUnpackAlignment, &sourceElementsPerPixel, &sourceElementsPerRow);
        doUnpackingAndPacking<uint16_t, DestType, unpackRGBA5551ToRGBA8, packingFunc>(static_cast<const uint16_t*>(sourceData), width, height, sourceElementsPerPixel, sourceElementsPerRow, destinationData, destinationElementsPerPixel);
        break;
    }
    case GraphicsContext3D::kSourceFormatRGBA4444: {
        unsigned int sourceElementsPerPixel, sourceElementsPerRow;
        computeIncrementParameters<uint16_t>(width, 2, sourceUnpackAlignment, &sourceElementsPerPixel, &sourceElementsPerRow);
        doUnpackingAndPacking<uint16_t, DestType, unpackRGBA4444ToRGBA8, packingFunc>(static_cast<const uint16_t*>(sourceData), width, height, sourceElementsPerPixel, sourceElementsPerRow, destinationData, destinationElementsPerPixel);
        break;
    }
    case GraphicsContext3D::kSourceFormatRGB565: {
        unsigned int sourceElementsPerPixel, sourceElementsPerRow;
        computeIncrementParameters<uint16_t>(width, 2, sourceUnpackAlignment, &sourceElementsPerPixel, &sourceElementsPerRow);
        doUnpackingAndPacking<uint16_t, DestType, unpackRGB565ToRGBA8, packingFunc>(static_cast<const uint16_t*>(sourceData), width, height, sourceElementsPerPixel, sourceElementsPerRow, destinationData, destinationElementsPerPixel);
        break;
    }
    case GraphicsContext3D::kSourceFormatR8: {
        unsigned int sourceElementsPerPixel, sourceElementsPerRow;
        computeIncrementParameters<uint8_t>(width, 1, sourceUnpackAlignment, &sourceElementsPerPixel, &sourceElementsPerRow);
        doUnpackingAndPacking<uint8_t, DestType, unpackR8ToRGBA8, packingFunc>(static_cast<const uint8_t*>(sourceData), width, height, sourceElementsPerPixel, sourceElementsPerRow, destinationData, destinationElementsPerPixel);
        break;
    }
    case GraphicsContext3D::kSourceFormatRA8: {
        unsigned int sourceElementsPerPixel, sourceElementsPerRow;
        computeIncrementParameters<uint8_t>(width, 2, sourceUnpackAlignment, &sourceElementsPerPixel, &sourceElementsPerRow);
        doUnpackingAndPacking<uint8_t, DestType, unpackRA8ToRGBA8, packingFunc>(static_cast<const uint8_t*>(sourceData), width, height, sourceElementsPerPixel, sourceElementsPerRow, destinationData, destinationElementsPerPixel);
        break;
    }
    case GraphicsContext3D::kSourceFormatA8: {
        unsigned int sourceElementsPerPixel, sourceElementsPerRow;
        computeIncrementParameters<uint8_t>(width, 1, sourceUnpackAlignment, &sourceElementsPerPixel, &sourceElementsPerRow);
        doUnpackingAndPacking<uint8_t, DestType, unpackA8ToRGBA8, packingFunc>(static_cast<const uint8_t*>(sourceData), width, height, sourceElementsPerPixel, sourceElementsPerRow, destinationData, destinationElementsPerPixel);
        break;
    }
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
        if (sourceDataFormat == kSourceFormatRGBA8 && destinationFormat == RGBA && sourceUnpackAlignment <= 4 && alphaOp == kAlphaDoNothing) {
            // No conversion necessary.
            memcpy(destinationData, sourceData, width * height * 4);
            break;
        }
        switch (destinationFormat) {
        case RGB:
            switch (alphaOp) {
            case kAlphaDoNothing:
                doPacking<uint8_t, packRGBA8ToRGB8>(sourceData, sourceDataFormat, width, height, sourceUnpackAlignment, destination, 3);
                break;
            case kAlphaDoPremultiply:
                doPacking<uint8_t, packRGBA8ToRGB8Premultiply>(sourceData, sourceDataFormat, width, height, sourceUnpackAlignment, destination, 3);
                break;
            case kAlphaDoUnmultiply:
                doPacking<uint8_t, packRGBA8ToRGB8Unmultiply>(sourceData, sourceDataFormat, width, height, sourceUnpackAlignment, destination, 3);
                break;
            }
            break;
        case RGBA:
            switch (alphaOp) {
            case kAlphaDoNothing:
                ASSERT(sourceDataFormat != kSourceFormatRGBA8 || sourceUnpackAlignment > 4); // Handled above with fast case.
                doPacking<uint8_t, packRGBA8ToRGBA8>(sourceData, sourceDataFormat, width, height, sourceUnpackAlignment, destination, 4);
                break;
            case kAlphaDoPremultiply:
                doPacking<uint8_t, packRGBA8ToRGBA8Premultiply>(sourceData, sourceDataFormat, width, height, sourceUnpackAlignment, destination, 4);
                break;
            case kAlphaDoUnmultiply:
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
            case kAlphaDoNothing:
                doPacking<uint8_t, packRGBA8ToR8>(sourceData, sourceDataFormat, width, height, sourceUnpackAlignment, destination, 1);
                break;
            case kAlphaDoPremultiply:
                doPacking<uint8_t, packRGBA8ToR8Premultiply>(sourceData, sourceDataFormat, width, height, sourceUnpackAlignment, destination, 1);
                break;
            case kAlphaDoUnmultiply:
                doPacking<uint8_t, packRGBA8ToR8Unmultiply>(sourceData, sourceDataFormat, width, height, sourceUnpackAlignment, destination, 1);
                break;
            }
            break;
        case LUMINANCE_ALPHA:
            // From the desktop OpenGL conversion rules (OpenGL 2.1
            // specification, Table 3.15), the red and alpha channels
            // are chosen from the RGBA data.
            switch (alphaOp) {
            case kAlphaDoNothing:
                doPacking<uint8_t, packRGBA8ToRA8>(sourceData, sourceDataFormat, width, height, sourceUnpackAlignment, destination, 2);
                break;
            case kAlphaDoPremultiply:
                doPacking<uint8_t, packRGBA8ToRA8Premultiply>(sourceData, sourceDataFormat, width, height, sourceUnpackAlignment, destination, 2);
                break;
            case kAlphaDoUnmultiply:
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
        case kAlphaDoNothing:
            doPacking<uint16_t, packRGBA8ToUnsignedShort4444>(sourceData, sourceDataFormat, width, height, sourceUnpackAlignment, destination, 1);
            break;
        case kAlphaDoPremultiply:
            doPacking<uint16_t, packRGBA8ToUnsignedShort4444Premultiply>(sourceData, sourceDataFormat, width, height, sourceUnpackAlignment, destination, 1);
            break;
        case kAlphaDoUnmultiply:
            doPacking<uint16_t, packRGBA8ToUnsignedShort4444Unmultiply>(sourceData, sourceDataFormat, width, height, sourceUnpackAlignment, destination, 1);
            break;
        }
        break;
    }
    case UNSIGNED_SHORT_5_5_5_1: {
        uint16_t* destination = static_cast<uint16_t*>(destinationData);
        switch (alphaOp) {
        case kAlphaDoNothing:
            doPacking<uint16_t, packRGBA8ToUnsignedShort5551>(sourceData, sourceDataFormat, width, height, sourceUnpackAlignment, destination, 1);
            break;
        case kAlphaDoPremultiply:
            doPacking<uint16_t, packRGBA8ToUnsignedShort5551Premultiply>(sourceData, sourceDataFormat, width, height, sourceUnpackAlignment, destination, 1);
            break;
        case kAlphaDoUnmultiply:
            doPacking<uint16_t, packRGBA8ToUnsignedShort5551Unmultiply>(sourceData, sourceDataFormat, width, height, sourceUnpackAlignment, destination, 1);
            break;
        }
        break;
    }
    case UNSIGNED_SHORT_5_6_5: {
        uint16_t* destination = static_cast<uint16_t*>(destinationData);
        switch (alphaOp) {
        case kAlphaDoNothing:
            doPacking<uint16_t, packRGBA8ToUnsignedShort565>(sourceData, sourceDataFormat, width, height, sourceUnpackAlignment, destination, 1);
            break;
        case kAlphaDoPremultiply:
            doPacking<uint16_t, packRGBA8ToUnsignedShort565Premultiply>(sourceData, sourceDataFormat, width, height, sourceUnpackAlignment, destination, 1);
            break;
        case kAlphaDoUnmultiply:
            doPacking<uint16_t, packRGBA8ToUnsignedShort565Unmultiply>(sourceData, sourceDataFormat, width, height, sourceUnpackAlignment, destination, 1);
            break;
        }
        break;
    }
    }
    return true;
}

#if !PLATFORM(CHROMIUM)
bool GraphicsContext3D::supportsBGRA()
{
    // For OpenGL ES2.0, this requires checking for
    // GL_EXT_texture_format_BGRA8888 and GL_EXT_read_format_bgra.
    // For desktop GL, BGRA has been supported since OpenGL 1.2.

    // However, note that the GL ES2 extension requires the internalFormat to
    // glTexImage2D() be GL_BGRA, while desktop GL will not accept GL_BGRA
    // (must be GL_RGBA), so this must be checked on each platform.
    // Returning false for now to be safe.
    return false;
}

bool GraphicsContext3D::supportsMapSubCHROMIUM()
{
    // We don't claim support for this extension at this time.
    return false;
}

void* GraphicsContext3D::mapBufferSubDataCHROMIUM(unsigned, int, int, unsigned)
{
    return 0;
}

void GraphicsContext3D::unmapBufferSubDataCHROMIUM(const void*)
{
}

void* GraphicsContext3D::mapTexSubImage2DCHROMIUM(unsigned, int, int, int, int, int, unsigned, unsigned, unsigned)
{
    return 0;
}

void GraphicsContext3D::unmapTexSubImage2DCHROMIUM(const void*)
{
}

bool GraphicsContext3D::supportsCopyTextureToParentTextureCHROMIUM()
{
    // We don't claim support for this extension at this time.
    return false;
}

void GraphicsContext3D::copyTextureToParentTextureCHROMIUM(unsigned, unsigned)
{
}
#endif

} // namespace WebCore

#endif // ENABLE(3D_CANVAS)
