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
 * \brief Image library.
 *//*--------------------------------------------------------------------*/

#include "deImage.h"
#include "deMemory.h"
#include "deInt32.h"
#include "deMath.h"

static int deImageFormat_getBytesPerPixel(deImageFormat format)
{
    DE_ASSERT(format == DE_IMAGEFORMAT_XRGB8888 || format == DE_IMAGEFORMAT_ARGB8888);
    DE_UNREF(format);
    return 4;
}

static void *getPixelAddress(const deImage *image, int x, int y)
{
    int offset = ((y * image->width) + x) * deImageFormat_getBytesPerPixel(image->format);
    DE_ASSERT(deInBounds32(x, 0, image->width));
    DE_ASSERT(deInBounds32(y, 0, image->height));
    return (void *)((uint8_t *)image->pixels + offset);
}

deImage *deImage_create(int width, int height, deImageFormat format)
{
    deImage *image = DE_NEW(deImage);
    int bpp        = deImageFormat_getBytesPerPixel(format);
    if (!image)
        return NULL;

    image->width  = width;
    image->height = height;
    image->format = format;
    image->pixels = deMalloc(width * height * bpp);
    if (!image->pixels)
    {
        deFree(image);
        return NULL;
    }
    memset(image->pixels, 0, width * height * bpp);

    return image;
}

void deImage_destroy(deImage *image)
{
    deFree(image->pixels);
    deFree(image);
}

deARGB deImage_getPixel(const deImage *image, int x, int y)
{
    void *addr = getPixelAddress(image, x, y);
    switch (image->format)
    {
    case DE_IMAGEFORMAT_XRGB8888:
        return *(deARGB *)addr;
    case DE_IMAGEFORMAT_ARGB8888:
        return *(deARGB *)addr;
    default:
        DE_FATAL("deImage_getPixel(): invalid format");
        return deARGB_black();
    }
}

void deImage_setPixel(deImage *image, int x, int y, deARGB argb)
{
    void *addr = getPixelAddress(image, x, y);
    switch (image->format)
    {
    case DE_IMAGEFORMAT_XRGB8888:
        *(deARGB *)addr = argb;
        break;
    case DE_IMAGEFORMAT_ARGB8888:
        *(deARGB *)addr = argb;
        break;
    default:
        DE_FATAL("deImage_getPixel(): invalid format");
    }
}

deImage *deImage_convertFormat(const deImage *image, deImageFormat format)
{
    int width          = image->width;
    int height         = image->height;
    deImage *converted = deImage_create(width, height, format);
    if (!converted)
        return NULL;

    if (format == image->format)
        memcpy(converted->pixels, image->pixels, width * height * deImageFormat_getBytesPerPixel(format));
    else
    {
        int x, y;
        for (y = 0; y < height; y++)
            for (x = 0; x < width; x++)
                deImage_setPixel(converted, x, y, deImage_getPixel(image, x, y));
    }

    return converted;
}

deImage *deImage_scale(const deImage *srcImage, int dstWidth, int dstHeight)
{
    int srcWidth    = srcImage->width;
    int srcHeight   = srcImage->height;
    deImage *result = deImage_create(dstWidth, dstHeight, srcImage->format);
    int x, y;

    for (y = 0; y < dstHeight; y++)
    {
        for (x = 0; x < dstWidth; x++)
        {
            float xFloat = ((float)x + 0.5f) / (float)dstWidth * (float)srcImage->width - 0.5f;
            float yFloat = ((float)y + 0.5f) / (float)dstHeight * (float)srcImage->height - 0.5f;
            int xFixed   = deFloorFloatToInt32(xFloat * 256.0f);
            int yFixed   = deFloorFloatToInt32(yFloat * 256.0f);
            int xFactor  = (xFixed & 0xFF);
            int yFactor  = (yFixed & 0xFF);
            int f00      = ((256 - xFactor) * (256 - yFactor)) >> 8;
            int f10      = ((256 - xFactor) * yFactor) >> 8;
            int f01      = (xFactor * (256 - yFactor)) >> 8;
            int f11      = (xFactor * yFactor) >> 8;
            int x0       = (xFixed >> 8);
            int y0       = (yFixed >> 8);
            int x1       = deClamp32(x0 + 1, 0, srcWidth - 1);
            int y1       = deClamp32(y0 + 1, 0, srcHeight - 1);
            DE_ASSERT(deInBounds32(x0, 0, srcWidth));
            DE_ASSERT(deInBounds32(y0, 0, srcHeight));

            /* Filtering. */
            {
                deARGB p00 = deImage_getPixel(srcImage, x0, y0);
                deARGB p10 = deImage_getPixel(srcImage, x1, y0);
                deARGB p01 = deImage_getPixel(srcImage, x0, y1);
                deARGB p11 = deImage_getPixel(srcImage, x1, y1);
                deARGB pix = deARGB_add(deARGB_add(deARGB_multiply(p00, f00), deARGB_multiply(p10, f10)),
                                        deARGB_add(deARGB_multiply(p01, f01), deARGB_multiply(p11, f11)));
                deImage_setPixel(result, x, y, pix);
            }
        }
    }

    return result;
}

void deImage_copyToUint8RGBA(const deImage *image, uint8_t *pixels)
{
    int width  = image->width;
    int height = image->height;
    int x, y;

    for (y = 0; y < height; y++)
        for (x = 0; x < width; x++)
        {
            deARGB pixel        = deImage_getPixel(image, x, y);
            int ndx             = (y * width) + x;
            pixels[4 * ndx + 0] = (uint8_t)deARGB_getRed(pixel);
            pixels[4 * ndx + 1] = (uint8_t)deARGB_getGreen(pixel);
            pixels[4 * ndx + 2] = (uint8_t)deARGB_getBlue(pixel);
            pixels[4 * ndx + 3] = (uint8_t)deARGB_getAlpha(pixel);
        }
}

void *deImage_getPixelPtr(const deImage *image)
{
    return image->pixels;
}

int deImage_getWidth(const deImage *image)
{
    return image->width;
}

int deImage_getHeight(const deImage *image)
{
    return image->height;
}
