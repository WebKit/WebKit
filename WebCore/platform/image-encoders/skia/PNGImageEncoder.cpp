/*
 * Copyright (c) 2006-2009, Google Inc. All rights reserved.
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

#include "IntSize.h"
#include "OwnArrayPtr.h"
#include "PNGImageEncoder.h"
#include "Vector.h"

#include "SkBitmap.h"

extern "C" {
#include "png.h"
}

namespace WebCore {

// Converts BGRA->RGBA and RGBA->BGRA.
static void convertBetweenBGRAandRGBA(const unsigned char* input, int numberOfPixels,
                                      unsigned char* output)
{
    for (int x = 0; x < numberOfPixels; x++) {
        const unsigned char* pixelIn = &input[x * 4];
        unsigned char* pixelOut = &output[x * 4];
        pixelOut[0] = pixelIn[2];
        pixelOut[1] = pixelIn[1];
        pixelOut[2] = pixelIn[0];
        pixelOut[3] = pixelIn[3];
    }
}

// Encoder --------------------------------------------------------------------
//
// This section of the code is based on nsPNGEncoder.cpp in Mozilla
// (Copyright 2005 Google Inc.)

// Passed around as the io_ptr in the png structs so our callbacks know where
// to write data.
struct PNGEncoderState {
    PNGEncoderState(Vector<unsigned char>* o) : m_out(o) {}
    Vector<unsigned char>* m_out;
};

// Called by libpng to flush its internal buffer to ours.
void encoderWriteCallback(png_structp png, png_bytep data, png_size_t size)
{
    PNGEncoderState* state = static_cast<PNGEncoderState*>(png_get_io_ptr(png));
    ASSERT(state->m_out);

    size_t oldSize = state->m_out->size();
    state->m_out->resize(oldSize + size);
    memcpy(&(*state->m_out)[oldSize], data, size);
}

// Automatically destroys the given write structs on destruction to make
// cleanup and error handling code cleaner.
class PNGWriteStructDestroyer {
public:
    PNGWriteStructDestroyer(png_struct** ps, png_info** pi)
        : m_pngStruct(ps)
        , m_pngInfo(pi) {
    }

    ~PNGWriteStructDestroyer() {
        png_destroy_write_struct(m_pngStruct, m_pngInfo);
    }

private:
    png_struct** m_pngStruct;
    png_info** m_pngInfo;
};

// static
bool PNGImageEncoder::encode(const SkBitmap& image, Vector<unsigned char>* output)
{
    if (image.config() != SkBitmap::kARGB_8888_Config)
        return false;  // Only support ARGB at 8 bpp now.

    image.lockPixels();
    bool result = PNGImageEncoder::encode(static_cast<unsigned char*>(
        image.getPixels()), IntSize(image.width(), image.height()),
        image.rowBytes(), output);
    image.unlockPixels();
    return result;
}

// static
bool PNGImageEncoder::encode(const unsigned char* input, const IntSize& size,
                             int bytesPerRow,
                             Vector<unsigned char>* output)
{
    int inputColorComponents = 4;
    int outputColorComponents = 4;
    int pngOutputColorType = PNG_COLOR_TYPE_RGB_ALPHA;
    IntSize imageSize(size);
    imageSize.clampNegativeToZero();

    // Row stride should be at least as long as the length of the data.
    if (inputColorComponents * imageSize.width() > bytesPerRow) {
        ASSERT(false);
        return false;
    }

    png_struct* pngPtr = png_create_write_struct(PNG_LIBPNG_VER_STRING, 0, 0, 0);
    if (!pngPtr)
        return false;

    png_info* infoPtr = png_create_info_struct(pngPtr);
    if (!infoPtr) {
        png_destroy_write_struct(&pngPtr, NULL);
        return false;
    }
    PNGWriteStructDestroyer destroyer(&pngPtr, &infoPtr);

    if (setjmp(png_jmpbuf(pngPtr))) {
        // The destroyer will ensure that the structures are cleaned up in this
        // case, even though we may get here as a jump from random parts of the
        // PNG library called below.
        return false;
    }

    // Set our callback for libpng to give us the data.
    PNGEncoderState state(output);
    png_set_write_fn(pngPtr, &state, encoderWriteCallback, NULL);

    png_set_IHDR(pngPtr, infoPtr, imageSize.width(), imageSize.height(), 8, pngOutputColorType,
                 PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT,
                 PNG_FILTER_TYPE_DEFAULT);
    png_write_info(pngPtr, infoPtr);

    OwnArrayPtr<unsigned char> rowPixels(new unsigned char[imageSize.width() * outputColorComponents]);
    for (int y = 0; y < imageSize.height(); y ++) {
        convertBetweenBGRAandRGBA(&input[y * bytesPerRow], imageSize.width(), rowPixels.get());
        png_write_row(pngPtr, rowPixels.get());
    }

    png_write_end(pngPtr, infoPtr);
    return true;
}

}  // namespace WebCore
