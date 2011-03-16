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
#include "JPEGImageEncoder.h"

#include "ImageData.h"
#include "IntSize.h"
#include "SkBitmap.h"
#include "SkColorPriv.h"
#include "SkUnPreMultiply.h"
extern "C" {
#include <stdio.h> // jpeglib.h needs stdio.h FILE
#include "jpeglib.h"
#include <setjmp.h>
}

namespace WebCore {

struct JPEGOutputBuffer : public jpeg_destination_mgr {
    Vector<unsigned char>* output;
    Vector<unsigned char> buffer;
};

static void prepareOutput(j_compress_ptr cinfo)
{
    JPEGOutputBuffer* out = static_cast<JPEGOutputBuffer*>(cinfo->dest);
    const size_t internalBufferSize = 8192;
    out->buffer.resize(internalBufferSize);
    out->next_output_byte = out->buffer.data();
    out->free_in_buffer = out->buffer.size();
}

static boolean writeOutput(j_compress_ptr cinfo)
{
    JPEGOutputBuffer* out = static_cast<JPEGOutputBuffer*>(cinfo->dest);
    out->output->append(out->buffer.data(), out->buffer.size());
    out->next_output_byte = out->buffer.data();
    out->free_in_buffer = out->buffer.size();
    return TRUE;
}

static void finishOutput(j_compress_ptr cinfo)
{
    JPEGOutputBuffer* out = static_cast<JPEGOutputBuffer*>(cinfo->dest);
    const size_t size = out->buffer.size() - out->free_in_buffer;
    out->output->append(out->buffer.data(), size);
}

static void handleError(j_common_ptr common)
{
    jmp_buf* jumpBufferPtr = static_cast<jmp_buf*>(common->client_data);
    longjmp(*jumpBufferPtr, -1);
}

static void preMultipliedBGRAtoRGB(const void* pixels, unsigned int pixelCount, unsigned char* output)
{
    const SkPMColor* input = static_cast<const SkPMColor*>(pixels);
    for (; pixelCount-- > 0; ++input) {
        *output++ = SkGetPackedR32(*input);
        *output++ = SkGetPackedG32(*input);
        *output++ = SkGetPackedB32(*input);
    }
}

static void RGBAtoRGB(const unsigned char* input, unsigned int pixels, unsigned char* output)
{
    for (; pixels-- > 0; input += 4) {
        *output++ = input[0];
        *output++ = input[1];
        *output++ = input[2];
    }
}

static bool encodePixels(const IntSize& inputSize, unsigned char* inputPixels,
                         bool premultiplied, int quality, Vector<unsigned char>* output)
{
    IntSize imageSize(inputSize);
    imageSize.clampNegativeToZero();
    JPEGOutputBuffer destination;
    destination.output = output;
    Vector<JSAMPLE> row;

    jpeg_compress_struct cinfo;
    jpeg_error_mgr error;
    cinfo.err = jpeg_std_error(&error);
    error.error_exit = handleError;
    jmp_buf jumpBuffer;
    cinfo.client_data = &jumpBuffer;

    if (setjmp(jumpBuffer)) {
        jpeg_destroy_compress(&cinfo);
        return false;
    }

    jpeg_create_compress(&cinfo);
    cinfo.dest = &destination;
    cinfo.dest->init_destination = prepareOutput;
    cinfo.dest->empty_output_buffer = writeOutput;
    cinfo.dest->term_destination = finishOutput;
    cinfo.image_height = imageSize.height();
    cinfo.image_width = imageSize.width();
    cinfo.in_color_space = JCS_RGB;
    cinfo.input_components = 3;

    jpeg_set_defaults(&cinfo);
    jpeg_set_quality(&cinfo, quality, TRUE);
    jpeg_start_compress(&cinfo, TRUE);

    unsigned char* pixels = inputPixels;
    row.resize(cinfo.image_width * cinfo.input_components);
    while (cinfo.next_scanline < cinfo.image_height) {
        if (premultiplied)
            preMultipliedBGRAtoRGB(pixels, cinfo.image_width, row.data());
        else 
            RGBAtoRGB(pixels, cinfo.image_width, row.data());
        jpeg_write_scanlines(&cinfo, row.dataSlot(), 1);
        pixels += cinfo.image_width * 4;
    }

    jpeg_finish_compress(&cinfo);
    jpeg_destroy_compress(&cinfo);
    return true;
}

bool JPEGImageEncoder::encode(const SkBitmap& bitmap, int quality, Vector<unsigned char>* output)
{
    if (bitmap.config() != SkBitmap::kARGB_8888_Config)
        return false; // Only support ARGB 32 bpp skia bitmaps.

    SkAutoLockPixels bitmapLock(bitmap);
    IntSize imageSize(bitmap.width(), bitmap.height());

    return encodePixels(imageSize, static_cast<unsigned char *>(bitmap.getPixels()),
                        true, quality, output);
}

bool JPEGImageEncoder::encode(const ImageData& imageData, int quality, Vector<unsigned char>* output)
{
    return encodePixels(imageData.size(), imageData.data()->data()->data(),
                        false, quality, output);
}

} // namespace WebCore
