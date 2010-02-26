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

#include "Image.h"
#include "ImageData.h"

namespace WebCore {

bool GraphicsContext3D::extractImageData(Image* image,
                                         bool flipY,
                                         bool premultiplyAlpha,
                                         Vector<uint8_t>& imageData,
                                         unsigned int* format,
                                         unsigned int* internalFormat)
{
    if (!image)
        return false;
    AlphaOp alphaOp = kAlphaDoNothing;
    bool hasAlphaChannel = true;
    if (!getImageData(image, imageData, premultiplyAlpha,
                      &hasAlphaChannel, &alphaOp, format))
        return false;
    processImageData(imageData.data(),
                     image->width(),
                     image->height(),
                     flipY,
                     alphaOp);
    *internalFormat = (hasAlphaChannel ? RGBA : RGB);
    return true;
}

bool GraphicsContext3D::extractImageData(ImageData* imageData,
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
    uint8_t* dst = data.data();
    uint8_t* src = imageData->data()->data()->data();
    memcpy(dst, src, dataBytes);
    processImageData(dst,
                     width,
                     height,
                     flipY,
                     premultiplyAlpha ? kAlphaDoPremultiply : kAlphaDoNothing);
    return true;
}

void GraphicsContext3D::processImageData(uint8_t* imageData,
                                         unsigned width,
                                         unsigned height,
                                         bool flipVertically,
                                         AlphaOp alphaOp)
{
    switch (alphaOp) {
    case kAlphaDoPremultiply:
        premultiplyAlpha(imageData, width * height);
        break;
    case kAlphaDoUnmultiply:
        unmultiplyAlpha(imageData, width * height);
        break;
    default:
        break;
    }

    if (flipVertically && width && height) {
        int rowBytes = width * 4;
        uint8_t* tempRow = new uint8_t[rowBytes];
        for (unsigned i = 0; i < height / 2; i++) {
            uint8_t* lowRow = imageData + (rowBytes * i);
            uint8_t* highRow = imageData + (rowBytes * (height - i - 1));
            memcpy(tempRow, lowRow, rowBytes);
            memcpy(lowRow, highRow, rowBytes);
            memcpy(highRow, tempRow, rowBytes);
        }
        delete[] tempRow;
    }
}

// Premultiply alpha into color channels.
void GraphicsContext3D::premultiplyAlpha(unsigned char* rgbaData, int numPixels)
{
    for (int j = 0; j < numPixels; j++) {
        float r = rgbaData[4*j+0] / 255.0f;
        float g = rgbaData[4*j+1] / 255.0f;
        float b = rgbaData[4*j+2] / 255.0f;
        float a = rgbaData[4*j+3] / 255.0f;
        rgbaData[4*j+0] = (unsigned char) (r * a * 255.0f);
        rgbaData[4*j+1] = (unsigned char) (g * a * 255.0f);
        rgbaData[4*j+2] = (unsigned char) (b * a * 255.0f);
    }
}

// Remove premultiplied alpha from color channels.
// FIXME: this is lossy. Must retrieve original values from HTMLImageElement.
void GraphicsContext3D::unmultiplyAlpha(unsigned char* rgbaData, int numPixels)
{
    for (int j = 0; j < numPixels; j++) {
        float r = rgbaData[4*j+0] / 255.0f;
        float g = rgbaData[4*j+1] / 255.0f;
        float b = rgbaData[4*j+2] / 255.0f;
        float a = rgbaData[4*j+3] / 255.0f;
        if (a > 0.0f) {
            r /= a;
            g /= a;
            b /= a;
            r = (r > 1.0f) ? 1.0f : r;
            g = (g > 1.0f) ? 1.0f : g;
            b = (b > 1.0f) ? 1.0f : b;
            rgbaData[4*j+0] = (unsigned char) (r * 255.0f);
            rgbaData[4*j+1] = (unsigned char) (g * 255.0f);
            rgbaData[4*j+2] = (unsigned char) (b * 255.0f);
        }
    }
}

} // namespace WebCore

#endif // ENABLE(3D_CANVAS)
