/*
 * Copyright (c) 2010, Google Inc. All rights reserved.
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
#include "PNGImageEncoder.h"

#include "IntSize.h"
#include "SkBitmap.h"
#include "SkUnPreMultiply.h"
extern "C" {
#include "png.h"
}

namespace WebCore {

static void writeOutput(png_structp png, png_bytep data, png_size_t size)
{
    static_cast<Vector<unsigned char>*>(png->io_ptr)->append(data, size);
}

static void preMultipliedBGRAtoRGBA(const SkPMColor* input, int pixels, unsigned char* output)
{
    while (pixels-- > 0) {
        SkColor unmultiplied = SkUnPreMultiply::PMColorToColor(*input++);
        *output++ = SkColorGetR(unmultiplied);
        *output++ = SkColorGetG(unmultiplied);
        *output++ = SkColorGetB(unmultiplied);
        *output++ = SkColorGetA(unmultiplied);
    }
}

bool PNGImageEncoder::encode(const SkBitmap& bitmap, Vector<unsigned char>* output)
{
    if (bitmap.config() != SkBitmap::kARGB_8888_Config)
        return false; // Only support ARGB 32 bpp skia bitmaps.

    SkAutoLockPixels bitmapLock(bitmap);
    IntSize imageSize(bitmap.width(), bitmap.height());
    imageSize.clampNegativeToZero();
    Vector<unsigned char> row;

    png_struct* png = png_create_write_struct(PNG_LIBPNG_VER_STRING, 0, 0, 0);
    png_info* info = png_create_info_struct(png);
    if (!png || !info || setjmp(png_jmpbuf(png))) {
        png_destroy_write_struct(png ? &png : 0, info ? &info : 0);
        return false;
    }

    png_set_write_fn(png, output, writeOutput, 0);
    png_set_IHDR(png, info, imageSize.width(), imageSize.height(),
                 8, PNG_COLOR_TYPE_RGB_ALPHA, 0, 0, 0);
    png_write_info(png, info);

    const SkPMColor* pixels = static_cast<SkPMColor*>(bitmap.getPixels());
    row.resize(imageSize.width() * bitmap.bytesPerPixel());
    for (int y = 0; y < imageSize.height(); ++y) {
        preMultipliedBGRAtoRGBA(pixels, imageSize.width(), row.data());
        png_write_row(png, row.data());
        pixels += imageSize.width();
    }

    png_write_end(png, info);
    png_destroy_write_struct(&png, &info);
    return true;
}

} // namespace WebCore
