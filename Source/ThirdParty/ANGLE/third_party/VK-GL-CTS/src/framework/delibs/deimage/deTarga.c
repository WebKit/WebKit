/*-------------------------------------------------------------------------
 * drawElements Image Library
 * --------------------------
 *
 * Copyright 2014 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *//*!
 * \file
 * \brief Targa file operations.
 *//*--------------------------------------------------------------------*/

#include "deImage.h"
#include "deMemory.h"
#include "deInt32.h"

#include <stdio.h>

deImage *deImage_loadTarga(const char *fileName)
{
    deImage *image = NULL;
    FILE *file;

    file = fopen(fileName, "rb");

    if (file != NULL)
    {
        int bytesRead;
        int width;
        int height;
        int bufSize;
        int stride;
        int bitsPerPixel;
        uint8_t *buffer;
        deImageFormat format;
        bool yFlipped;

        uint8_t tgaHeader[18];

        bytesRead = (int)fread(&tgaHeader, 1, 18, file);
        DE_TEST_ASSERT(bytesRead == 18);
        DE_TEST_ASSERT(tgaHeader[2] == 2); /* truecolor, no encoding */
        DE_TEST_ASSERT(tgaHeader[17] == 0x00 ||
                       tgaHeader[17] == 0x20); /* both y-directions supported, non-interlaced */

        yFlipped = (tgaHeader[17] & 0x20) == 0;

        /* Decode header. */
        width        = (int)(tgaHeader[12]) | ((int)(tgaHeader[13]) << 8);
        height       = (int)(tgaHeader[14]) | ((int)(tgaHeader[15]) << 8);
        bitsPerPixel = tgaHeader[16];
        stride       = width * bitsPerPixel / 8;

        /* Allocate buffer. */
        bufSize = stride;
        buffer  = deMalloc(bufSize);
        DE_TEST_ASSERT(buffer);

        /* Figure out format. */
        DE_TEST_ASSERT(bitsPerPixel == 24 || bitsPerPixel == 32);
        format = (bitsPerPixel == 32) ? DE_IMAGEFORMAT_ARGB8888 : DE_IMAGEFORMAT_XRGB8888;

        /* Create image. */
        image = deImage_create(width, height, format);
        DE_TEST_ASSERT(image);

        /* Copy pixel data. */
        {
            int bpp = 4;
            int x, y;

            for (y = 0; y < height; y++)
            {
                const uint8_t *src = buffer;
                int dstY           = yFlipped ? (height - 1 - y) : y;
                deARGB *dst        = (uint32_t *)((uint8_t *)image->pixels + dstY * image->width * bpp);
                fread(buffer, 1, bufSize, file);

                if (bitsPerPixel == 24)
                {
                    for (x = 0; x < width; x++)
                    {
                        uint8_t b = *src++;
                        uint8_t g = *src++;
                        uint8_t r = *src++;
                        *dst++    = deARGB_set(r, g, b, 0xFF);
                    }
                }
                else
                {
                    /* \todo [petri] Component order? */
                    uint8_t a = *src++;
                    uint8_t b = *src++;
                    uint8_t g = *src++;
                    uint8_t r = *src++;
                    DE_ASSERT(bitsPerPixel == 32);
                    *dst++ = deARGB_set(r, g, b, a);
                }
            }
        }

        deFree(buffer);
        fclose(file);
    }

    return image;
}

bool deImage_saveTarga(const deImage *image, const char *fileName)
{
    deImage *imageCopy = NULL;
    int width          = image->width;
    int height         = image->height;
    char tgaHeader[18];
    FILE *file;

    /* \todo [petri] Handle non-alpha images. */
    if (image->format != DE_IMAGEFORMAT_ARGB8888)
    {
        imageCopy = deImage_convertFormat(image, DE_IMAGEFORMAT_ARGB8888);
        if (!imageCopy)
            return false;

        image = imageCopy;
    }

    file = fopen(fileName, "wb");
    if (!file)
        return false;

    /* Set unused fields of header to 0 */
    memset(tgaHeader, 0, sizeof(tgaHeader));

    tgaHeader[1] = 0; /* no palette */
    tgaHeader[2] = 2; /* uncompressed RGB */

    tgaHeader[12] = (char)(width & 0xFF);
    tgaHeader[13] = (char)(width >> 8);
    tgaHeader[14] = (char)(height & 0xFF);
    tgaHeader[15] = (char)(height >> 8);
    tgaHeader[16] = 24;   /* bytes per pixel */
    tgaHeader[17] = 0x20; /* Top-down, non-interlaced */

    fwrite(tgaHeader, 1, 18, file);

    /* Store pixels. */
    {
        const uint32_t *pixels = image->pixels;
        int ndx;

        for (ndx = 0; ndx < width * height; ndx++)
        {
            uint32_t c = pixels[ndx];
            fputc((uint8_t)(c >> 0), file);
            fputc((uint8_t)(c >> 8), file);
            fputc((uint8_t)(c >> 16), file);
        }
    }

    /* Cleanup and return. */
    fclose(file);
    if (imageCopy)
        deImage_destroy(imageCopy);

    return true;
}
