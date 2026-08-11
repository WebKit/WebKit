#ifndef _DEIMAGE_H
#define _DEIMAGE_H
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

#include "deDefs.h"
#include "deARGB.h"

typedef enum deImageFormat_e
{
    DE_IMAGEFORMAT_XRGB8888 = 0,
    DE_IMAGEFORMAT_ARGB8888,

    DE_IMAGEFORMAT_LAST
} deImageFormat;

typedef struct deImage_s
{
    int width;
    int height;
    deImageFormat format;
    void *pixels;
} deImage;

deImage *deImage_create(int width, int height, deImageFormat format);
void deImage_destroy(deImage *image);

int deImage_getWidth(const deImage *image);
int deImage_getHeight(const deImage *image);
void *deImage_getPixelPtr(const deImage *image);

deARGB deImage_getPixel(const deImage *image, int x, int y);
void deImage_setPixel(deImage *image, int x, int y, deARGB argb);

deImage *deImage_loadTarga(const char *fileName);
bool deImage_saveTarga(const deImage *image, const char *fileName);

deImage *deImage_convertFormat(const deImage *image, deImageFormat format);
deImage *deImage_scale(const deImage *image, int dstWidth, int dstHeight);
void deImage_copyToUint8RGBA(const deImage *image, uint8_t *pixels);

#endif /* _DEIMAGE_H */
