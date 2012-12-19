/*
 * Copyright (c) 2011, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WEBPImageEncoder.h"

#if USE(WEBP)

#include "ImageData.h"
#include "IntSize.h"
#include "SkBitmap.h"
#include "SkColorPriv.h"
#include "webp/encode.h"

typedef int (*WebPImporter)(WebPPicture* const, const uint8_t* const data, int rowStride);

namespace WebCore {

static int writeOutput(const uint8_t* data, size_t size, const WebPPicture* const picture)
{
    static_cast<Vector<unsigned char>*>(picture->custom_ptr)->append(data, size);
    return 1;
}

static bool rgbPictureImport(const unsigned char* pixels, bool premultiplied, WebPImporter importRGBX, WebPImporter importRGB, WebPPicture* picture)
{
    if (premultiplied)
        return importRGBX(picture, pixels, picture->width * 4);

    // Write the RGB pixels to an rgb data buffer, alpha premultiplied, then import the rgb data.

    Vector<unsigned char> rgb;
    size_t pixelCount = picture->height * picture->width;
    rgb.reserveInitialCapacity(pixelCount * 3);

    for (unsigned char* data = rgb.data(); pixelCount-- > 0; pixels += 4) {
        unsigned char alpha = pixels[3];
        if (alpha != 255) {
            *data++ = SkMulDiv255Round(pixels[0], alpha);
            *data++ = SkMulDiv255Round(pixels[1], alpha);
            *data++ = SkMulDiv255Round(pixels[2], alpha);
        } else {
            *data++ = pixels[0];
            *data++ = pixels[1];
            *data++ = pixels[2];
        }
    }

    return importRGB(picture, rgb.data(), picture->width * 3);
}

template <bool Premultiplied> inline bool importPictureBGRX(const unsigned char* pixels, WebPPicture* picture)
{
    return rgbPictureImport(pixels, Premultiplied, &WebPPictureImportBGRX, &WebPPictureImportBGR, picture);
}

template <bool Premultiplied> inline bool importPictureRGBX(const unsigned char* pixels, WebPPicture* picture)
{
    return rgbPictureImport(pixels, Premultiplied, &WebPPictureImportRGBX, &WebPPictureImportRGB, picture);
}

static bool encodePixels(IntSize imageSize, const unsigned char* pixels, bool premultiplied, int quality, Vector<unsigned char>* output)
{
    WebPConfig config;
    if (!WebPConfigInit(&config))
        return false;
    WebPPicture picture;
    if (!WebPPictureInit(&picture))
        return false;

    imageSize.clampNegativeToZero();
    if (!imageSize.width() || imageSize.width() > WEBP_MAX_DIMENSION)
        return false;
    picture.width = imageSize.width();
    if (!imageSize.height() || imageSize.height() > WEBP_MAX_DIMENSION)
        return false;
    picture.height = imageSize.height();

    if (premultiplied && !importPictureBGRX<true>(pixels, &picture))
        return false;
    if (!premultiplied && !importPictureRGBX<false>(pixels, &picture))
        return false;

    picture.custom_ptr = output;
    picture.writer = &writeOutput;
    config.quality = quality;
    config.method = 3;

    bool success = WebPEncode(&config, &picture);
    WebPPictureFree(&picture);
    return success;
}

bool WEBPImageEncoder::encode(const SkBitmap& bitmap, int quality, Vector<unsigned char>* output)
{
    SkAutoLockPixels bitmapLock(bitmap);

    if (bitmap.config() != SkBitmap::kARGB_8888_Config || !bitmap.getPixels())
        return false; // Only support 32 bit/pixel skia bitmaps.

    return encodePixels(IntSize(bitmap.width(), bitmap.height()), static_cast<unsigned char *>(bitmap.getPixels()), true, quality, output);
}

bool WEBPImageEncoder::encode(const ImageData& imageData, int quality, Vector<unsigned char>* output)
{
    return encodePixels(imageData.size(), imageData.data()->data(), false, quality, output);
}

} // namespace WebCore

#endif
