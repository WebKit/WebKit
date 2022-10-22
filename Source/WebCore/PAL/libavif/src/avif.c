// Copyright 2019 Joe Drago. All rights reserved.
// SPDX-License-Identifier: BSD-2-Clause

#include "avif/internal.h"

#include <limits.h>
#include <stdint.h>
#include <string.h>

#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)
#define AVIF_VERSION_STRING (STR(AVIF_VERSION_MAJOR) "." STR(AVIF_VERSION_MINOR) "." STR(AVIF_VERSION_PATCH))

const char * avifVersion(void)
{
    return AVIF_VERSION_STRING;
}

const char * avifPixelFormatToString(avifPixelFormat format)
{
    switch (format) {
        case AVIF_PIXEL_FORMAT_YUV444:
            return "YUV444";
        case AVIF_PIXEL_FORMAT_YUV420:
            return "YUV420";
        case AVIF_PIXEL_FORMAT_YUV422:
            return "YUV422";
        case AVIF_PIXEL_FORMAT_YUV400:
            return "YUV400";
        case AVIF_PIXEL_FORMAT_NONE:
        case AVIF_PIXEL_FORMAT_COUNT:
        default:
            break;
    }
    return "Unknown";
}

void avifGetPixelFormatInfo(avifPixelFormat format, avifPixelFormatInfo * info)
{
    memset(info, 0, sizeof(avifPixelFormatInfo));

    switch (format) {
        case AVIF_PIXEL_FORMAT_YUV444:
            info->chromaShiftX = 0;
            info->chromaShiftY = 0;
            break;

        case AVIF_PIXEL_FORMAT_YUV422:
            info->chromaShiftX = 1;
            info->chromaShiftY = 0;
            break;

        case AVIF_PIXEL_FORMAT_YUV420:
            info->chromaShiftX = 1;
            info->chromaShiftY = 1;
            break;

        case AVIF_PIXEL_FORMAT_YUV400:
            info->chromaShiftX = 1;
            info->chromaShiftY = 1;
            info->monochrome = AVIF_TRUE;
            break;

        case AVIF_PIXEL_FORMAT_NONE:
        case AVIF_PIXEL_FORMAT_COUNT:
        default:
            break;
    }
}

const char * avifResultToString(avifResult result)
{
    // clang-format off
    switch (result) {
        case AVIF_RESULT_OK:                            return "OK";
        case AVIF_RESULT_INVALID_FTYP:                  return "Invalid ftyp";
        case AVIF_RESULT_NO_CONTENT:                    return "No content";
        case AVIF_RESULT_NO_YUV_FORMAT_SELECTED:        return "No YUV format selected";
        case AVIF_RESULT_REFORMAT_FAILED:               return "Reformat failed";
        case AVIF_RESULT_UNSUPPORTED_DEPTH:             return "Unsupported depth";
        case AVIF_RESULT_ENCODE_COLOR_FAILED:           return "Encoding of color planes failed";
        case AVIF_RESULT_ENCODE_ALPHA_FAILED:           return "Encoding of alpha plane failed";
        case AVIF_RESULT_BMFF_PARSE_FAILED:             return "BMFF parsing failed";
        case AVIF_RESULT_NO_AV1_ITEMS_FOUND:            return "No AV1 items found";
        case AVIF_RESULT_DECODE_COLOR_FAILED:           return "Decoding of color planes failed";
        case AVIF_RESULT_DECODE_ALPHA_FAILED:           return "Decoding of alpha plane failed";
        case AVIF_RESULT_COLOR_ALPHA_SIZE_MISMATCH:     return "Color and alpha planes size mismatch";
        case AVIF_RESULT_ISPE_SIZE_MISMATCH:            return "Plane sizes don't match ispe values";
        case AVIF_RESULT_NO_CODEC_AVAILABLE:            return "No codec available";
        case AVIF_RESULT_NO_IMAGES_REMAINING:           return "No images remaining";
        case AVIF_RESULT_INVALID_EXIF_PAYLOAD:          return "Invalid Exif payload";
        case AVIF_RESULT_INVALID_IMAGE_GRID:            return "Invalid image grid";
        case AVIF_RESULT_INVALID_CODEC_SPECIFIC_OPTION: return "Invalid codec-specific option";
        case AVIF_RESULT_TRUNCATED_DATA:                return "Truncated data";
        case AVIF_RESULT_IO_NOT_SET:                    return "IO not set";
        case AVIF_RESULT_IO_ERROR:                      return "IO Error";
        case AVIF_RESULT_WAITING_ON_IO:                 return "Waiting on IO";
        case AVIF_RESULT_INVALID_ARGUMENT:              return "Invalid argument";
        case AVIF_RESULT_NOT_IMPLEMENTED:               return "Not implemented";
        case AVIF_RESULT_OUT_OF_MEMORY:                 return "Out of memory";
        case AVIF_RESULT_CANNOT_CHANGE_SETTING:         return "Cannot change some setting during encoding";
        case AVIF_RESULT_INCOMPATIBLE_IMAGE:            return "The image is incompatible with already encoded images";
        case AVIF_RESULT_UNKNOWN_ERROR:
        default:
            break;
    }
    // clang-format on
    return "Unknown Error";
}

const char * avifProgressiveStateToString(avifProgressiveState progressiveState)
{
    // clang-format off
    switch (progressiveState) {
        case AVIF_PROGRESSIVE_STATE_UNAVAILABLE: return "Unavailable";
        case AVIF_PROGRESSIVE_STATE_AVAILABLE:   return "Available";
        case AVIF_PROGRESSIVE_STATE_ACTIVE:      return "Active";
        default:
            break;
    }
    // clang-format on
    return "Unknown";
}

static void avifImageSetDefaults(avifImage * image)
{
    memset(image, 0, sizeof(avifImage));
    image->yuvRange = AVIF_RANGE_FULL;
    image->colorPrimaries = AVIF_COLOR_PRIMARIES_UNSPECIFIED;
    image->transferCharacteristics = AVIF_TRANSFER_CHARACTERISTICS_UNSPECIFIED;
    image->matrixCoefficients = AVIF_MATRIX_COEFFICIENTS_UNSPECIFIED;
}

avifImage * avifImageCreate(uint32_t width, uint32_t height, uint32_t depth, avifPixelFormat yuvFormat)
{
    avifImage * image = (avifImage *)avifAlloc(sizeof(avifImage));
    avifImageSetDefaults(image);
    image->width = width;
    image->height = height;
    image->depth = depth;
    image->yuvFormat = yuvFormat;
    return image;
}

avifImage * avifImageCreateEmpty(void)
{
    return avifImageCreate(0, 0, 0, AVIF_PIXEL_FORMAT_NONE);
}

// Copies all fields that do not need to be freed/allocated from srcImage to dstImage.
static void avifImageCopyNoAlloc(avifImage * dstImage, const avifImage * srcImage)
{
    dstImage->width = srcImage->width;
    dstImage->height = srcImage->height;
    dstImage->depth = srcImage->depth;
    dstImage->yuvFormat = srcImage->yuvFormat;
    dstImage->yuvRange = srcImage->yuvRange;
    dstImage->yuvChromaSamplePosition = srcImage->yuvChromaSamplePosition;
    dstImage->alphaPremultiplied = srcImage->alphaPremultiplied;

    dstImage->colorPrimaries = srcImage->colorPrimaries;
    dstImage->transferCharacteristics = srcImage->transferCharacteristics;
    dstImage->matrixCoefficients = srcImage->matrixCoefficients;

    dstImage->transformFlags = srcImage->transformFlags;
    dstImage->pasp = srcImage->pasp;
    dstImage->clap = srcImage->clap;
    dstImage->irot = srcImage->irot;
    dstImage->imir = srcImage->imir;
}

avifResult avifImageCopy(avifImage * dstImage, const avifImage * srcImage, avifPlanesFlags planes)
{
    avifImageFreePlanes(dstImage, AVIF_PLANES_ALL);
    avifImageCopyNoAlloc(dstImage, srcImage);

    avifImageSetProfileICC(dstImage, srcImage->icc.data, srcImage->icc.size);

    avifRWDataSet(&dstImage->exif, srcImage->exif.data, srcImage->exif.size);
    avifImageSetMetadataXMP(dstImage, srcImage->xmp.data, srcImage->xmp.size);

    if ((planes & AVIF_PLANES_YUV) && srcImage->yuvPlanes[AVIF_CHAN_Y]) {
        const avifResult allocationResult = avifImageAllocatePlanes(dstImage, AVIF_PLANES_YUV);
        if (allocationResult != AVIF_RESULT_OK) {
            return allocationResult;
        }

        avifPixelFormatInfo formatInfo;
        avifGetPixelFormatInfo(srcImage->yuvFormat, &formatInfo);
        uint32_t uvHeight = (dstImage->height + formatInfo.chromaShiftY) >> formatInfo.chromaShiftY;
        for (int yuvPlane = 0; yuvPlane < 3; ++yuvPlane) {
            uint32_t planeHeight = (yuvPlane == AVIF_CHAN_Y) ? dstImage->height : uvHeight;

            if (!srcImage->yuvRowBytes[yuvPlane]) {
                // plane is absent. If we're copying from a source without
                // them, mimic the source image's state by removing our copy.
                avifFree(dstImage->yuvPlanes[yuvPlane]);
                dstImage->yuvPlanes[yuvPlane] = NULL;
                dstImage->yuvRowBytes[yuvPlane] = 0;
                continue;
            }

            for (uint32_t j = 0; j < planeHeight; ++j) {
                uint8_t * srcRow = &srcImage->yuvPlanes[yuvPlane][j * srcImage->yuvRowBytes[yuvPlane]];
                uint8_t * dstRow = &dstImage->yuvPlanes[yuvPlane][j * dstImage->yuvRowBytes[yuvPlane]];
                memcpy(dstRow, srcRow, dstImage->yuvRowBytes[yuvPlane]);
            }
        }
    }

    if ((planes & AVIF_PLANES_A) && srcImage->alphaPlane) {
        const avifResult allocationResult = avifImageAllocatePlanes(dstImage, AVIF_PLANES_A);
        if (allocationResult != AVIF_RESULT_OK) {
            return allocationResult;
        }
        for (uint32_t j = 0; j < dstImage->height; ++j) {
            uint8_t * srcAlphaRow = &srcImage->alphaPlane[j * srcImage->alphaRowBytes];
            uint8_t * dstAlphaRow = &dstImage->alphaPlane[j * dstImage->alphaRowBytes];
            memcpy(dstAlphaRow, srcAlphaRow, dstImage->alphaRowBytes);
        }
    }
    return AVIF_RESULT_OK;
}

avifResult avifImageSetViewRect(avifImage * dstImage, const avifImage * srcImage, const avifCropRect * rect)
{
    avifPixelFormatInfo formatInfo;
    avifGetPixelFormatInfo(srcImage->yuvFormat, &formatInfo);
    if ((rect->width > srcImage->width) || (rect->height > srcImage->height) || (rect->x > (srcImage->width - rect->width)) ||
        (rect->y > (srcImage->height - rect->height)) || (rect->x & formatInfo.chromaShiftX) || (rect->y & formatInfo.chromaShiftY)) {
        return AVIF_RESULT_INVALID_ARGUMENT;
    }
    avifImageFreePlanes(dstImage, AVIF_PLANES_ALL); // dstImage->imageOwnsYUVPlanes and dstImage->imageOwnsAlphaPlane set to AVIF_FALSE.
    avifImageCopyNoAlloc(dstImage, srcImage);
    dstImage->width = rect->width;
    dstImage->height = rect->height;
    const uint32_t pixelBytes = (srcImage->depth > 8) ? 2 : 1;
    if (srcImage->yuvPlanes[AVIF_CHAN_Y]) {
        for (int yuvPlane = 0; yuvPlane < 3; ++yuvPlane) {
            if (srcImage->yuvRowBytes[yuvPlane]) {
                const size_t planeX = (yuvPlane == AVIF_CHAN_Y) ? rect->x : (rect->x >> formatInfo.chromaShiftX);
                const size_t planeY = (yuvPlane == AVIF_CHAN_Y) ? rect->y : (rect->y >> formatInfo.chromaShiftY);
                dstImage->yuvPlanes[yuvPlane] =
                    srcImage->yuvPlanes[yuvPlane] + planeY * srcImage->yuvRowBytes[yuvPlane] + planeX * pixelBytes;
                dstImage->yuvRowBytes[yuvPlane] = srcImage->yuvRowBytes[yuvPlane];
            }
        }
    }
    if (srcImage->alphaPlane) {
        dstImage->alphaPlane = srcImage->alphaPlane + (size_t)rect->y * srcImage->alphaRowBytes + (size_t)rect->x * pixelBytes;
        dstImage->alphaRowBytes = srcImage->alphaRowBytes;
    }
    return AVIF_RESULT_OK;
}

void avifImageDestroy(avifImage * image)
{
    avifImageFreePlanes(image, AVIF_PLANES_ALL);
    avifRWDataFree(&image->icc);
    avifRWDataFree(&image->exif);
    avifRWDataFree(&image->xmp);
    avifFree(image);
}

void avifImageSetProfileICC(avifImage * image, const uint8_t * icc, size_t iccSize)
{
    avifRWDataSet(&image->icc, icc, iccSize);
}

void avifImageSetMetadataXMP(avifImage * image, const uint8_t * xmp, size_t xmpSize)
{
    avifRWDataSet(&image->xmp, xmp, xmpSize);
}

avifResult avifImageAllocatePlanes(avifImage * image, avifPlanesFlags planes)
{
    if (image->width == 0 || image->height == 0) {
        return AVIF_RESULT_INVALID_ARGUMENT;
    }
    const size_t channelSize = avifImageUsesU16(image) ? 2 : 1;
    if (image->width > SIZE_MAX / channelSize) {
        return AVIF_RESULT_INVALID_ARGUMENT;
    }
    const size_t fullRowBytes = channelSize * image->width;
    if ((fullRowBytes > UINT32_MAX) || (image->height > SIZE_MAX / fullRowBytes)) {
        return AVIF_RESULT_INVALID_ARGUMENT;
    }
    const size_t fullSize = fullRowBytes * image->height;

    if ((planes & AVIF_PLANES_YUV) && (image->yuvFormat != AVIF_PIXEL_FORMAT_NONE)) {
        avifPixelFormatInfo info;
        avifGetPixelFormatInfo(image->yuvFormat, &info);

        // Intermediary computation as 64 bits in case width or height is exactly UINT32_MAX.
        const uint32_t shiftedW = (uint32_t)(((uint64_t)image->width + info.chromaShiftX) >> info.chromaShiftX);
        const uint32_t shiftedH = (uint32_t)(((uint64_t)image->height + info.chromaShiftY) >> info.chromaShiftY);

        // These are less than or equal to fullRowBytes/fullSize. No need to check overflows.
        const size_t uvRowBytes = channelSize * shiftedW;
        const size_t uvSize = uvRowBytes * shiftedH;

        image->imageOwnsYUVPlanes = AVIF_TRUE;
        if (!image->yuvPlanes[AVIF_CHAN_Y]) {
            image->yuvRowBytes[AVIF_CHAN_Y] = (uint32_t)fullRowBytes;
            image->yuvPlanes[AVIF_CHAN_Y] = avifAlloc(fullSize);
            if (!image->yuvPlanes[AVIF_CHAN_Y]) {
                return AVIF_RESULT_OUT_OF_MEMORY;
            }
        }

        if (image->yuvFormat != AVIF_PIXEL_FORMAT_YUV400) {
            if (!image->yuvPlanes[AVIF_CHAN_U]) {
                image->yuvRowBytes[AVIF_CHAN_U] = (uint32_t)uvRowBytes;
                image->yuvPlanes[AVIF_CHAN_U] = avifAlloc(uvSize);
                if (!image->yuvPlanes[AVIF_CHAN_U]) {
                    return AVIF_RESULT_OUT_OF_MEMORY;
                }
            }
            if (!image->yuvPlanes[AVIF_CHAN_V]) {
                image->yuvRowBytes[AVIF_CHAN_V] = (uint32_t)uvRowBytes;
                image->yuvPlanes[AVIF_CHAN_V] = avifAlloc(uvSize);
                if (!image->yuvPlanes[AVIF_CHAN_V]) {
                    return AVIF_RESULT_OUT_OF_MEMORY;
                }
            }
        }
    }
    if (planes & AVIF_PLANES_A) {
        image->imageOwnsAlphaPlane = AVIF_TRUE;
        if (!image->alphaPlane) {
            image->alphaRowBytes = (uint32_t)fullRowBytes;
            image->alphaPlane = avifAlloc(fullSize);
            if (!image->alphaPlane) {
                return AVIF_RESULT_OUT_OF_MEMORY;
            }
        }
    }
    return AVIF_RESULT_OK;
}

void avifImageFreePlanes(avifImage * image, avifPlanesFlags planes)
{
    if ((planes & AVIF_PLANES_YUV) && (image->yuvFormat != AVIF_PIXEL_FORMAT_NONE)) {
        if (image->imageOwnsYUVPlanes) {
            avifFree(image->yuvPlanes[AVIF_CHAN_Y]);
            avifFree(image->yuvPlanes[AVIF_CHAN_U]);
            avifFree(image->yuvPlanes[AVIF_CHAN_V]);
        }
        image->yuvPlanes[AVIF_CHAN_Y] = NULL;
        image->yuvRowBytes[AVIF_CHAN_Y] = 0;
        image->yuvPlanes[AVIF_CHAN_U] = NULL;
        image->yuvRowBytes[AVIF_CHAN_U] = 0;
        image->yuvPlanes[AVIF_CHAN_V] = NULL;
        image->yuvRowBytes[AVIF_CHAN_V] = 0;
        image->imageOwnsYUVPlanes = AVIF_FALSE;
    }
    if (planes & AVIF_PLANES_A) {
        if (image->imageOwnsAlphaPlane) {
            avifFree(image->alphaPlane);
        }
        image->alphaPlane = NULL;
        image->alphaRowBytes = 0;
        image->imageOwnsAlphaPlane = AVIF_FALSE;
    }
}

void avifImageStealPlanes(avifImage * dstImage, avifImage * srcImage, avifPlanesFlags planes)
{
    avifImageFreePlanes(dstImage, planes);

    if (planes & AVIF_PLANES_YUV) {
        dstImage->yuvPlanes[AVIF_CHAN_Y] = srcImage->yuvPlanes[AVIF_CHAN_Y];
        dstImage->yuvRowBytes[AVIF_CHAN_Y] = srcImage->yuvRowBytes[AVIF_CHAN_Y];
        dstImage->yuvPlanes[AVIF_CHAN_U] = srcImage->yuvPlanes[AVIF_CHAN_U];
        dstImage->yuvRowBytes[AVIF_CHAN_U] = srcImage->yuvRowBytes[AVIF_CHAN_U];
        dstImage->yuvPlanes[AVIF_CHAN_V] = srcImage->yuvPlanes[AVIF_CHAN_V];
        dstImage->yuvRowBytes[AVIF_CHAN_V] = srcImage->yuvRowBytes[AVIF_CHAN_V];

        srcImage->yuvPlanes[AVIF_CHAN_Y] = NULL;
        srcImage->yuvRowBytes[AVIF_CHAN_Y] = 0;
        srcImage->yuvPlanes[AVIF_CHAN_U] = NULL;
        srcImage->yuvRowBytes[AVIF_CHAN_U] = 0;
        srcImage->yuvPlanes[AVIF_CHAN_V] = NULL;
        srcImage->yuvRowBytes[AVIF_CHAN_V] = 0;

        dstImage->yuvFormat = srcImage->yuvFormat;
        dstImage->imageOwnsYUVPlanes = srcImage->imageOwnsYUVPlanes;
        srcImage->imageOwnsYUVPlanes = AVIF_FALSE;
    }
    if (planes & AVIF_PLANES_A) {
        dstImage->alphaPlane = srcImage->alphaPlane;
        dstImage->alphaRowBytes = srcImage->alphaRowBytes;

        srcImage->alphaPlane = NULL;
        srcImage->alphaRowBytes = 0;

        dstImage->imageOwnsAlphaPlane = srcImage->imageOwnsAlphaPlane;
        srcImage->imageOwnsAlphaPlane = AVIF_FALSE;
    }
}

avifBool avifImageUsesU16(const avifImage * image)
{
    return (image->depth > 8);
}

avifBool avifDimensionsTooLarge(uint32_t width, uint32_t height, uint32_t imageSizeLimit, uint32_t imageDimensionLimit)
{
    if (width > (imageSizeLimit / height)) {
        return AVIF_TRUE;
    }
    if ((imageDimensionLimit != 0) && ((width > imageDimensionLimit) || (height > imageDimensionLimit))) {
        return AVIF_TRUE;
    }
    return AVIF_FALSE;
}

// avifCodecCreate*() functions are in their respective codec_*.c files

void avifCodecDestroy(avifCodec * codec)
{
    if (codec && codec->destroyInternal) {
        codec->destroyInternal(codec);
    }
    avifFree(codec);
}

// ---------------------------------------------------------------------------
// avifRGBImage

avifBool avifRGBFormatHasAlpha(avifRGBFormat format)
{
    return (format != AVIF_RGB_FORMAT_RGB) && (format != AVIF_RGB_FORMAT_BGR) && (format != AVIF_RGB_FORMAT_RGB_565);
}

uint32_t avifRGBFormatChannelCount(avifRGBFormat format)
{
    return avifRGBFormatHasAlpha(format) ? 4 : 3;
}

uint32_t avifRGBImagePixelSize(const avifRGBImage * rgb)
{
    if (rgb->format == AVIF_RGB_FORMAT_RGB_565) {
        return 2;
    }
    return avifRGBFormatChannelCount(rgb->format) * ((rgb->depth > 8) ? 2 : 1);
}

void avifRGBImageSetDefaults(avifRGBImage * rgb, const avifImage * image)
{
    rgb->width = image->width;
    rgb->height = image->height;
    rgb->depth = image->depth;
    rgb->format = AVIF_RGB_FORMAT_RGBA;
    rgb->chromaUpsampling = AVIF_CHROMA_UPSAMPLING_AUTOMATIC;
    rgb->chromaDownsampling = AVIF_CHROMA_DOWNSAMPLING_AUTOMATIC;
    rgb->avoidLibYUV = AVIF_FALSE;
    rgb->ignoreAlpha = AVIF_FALSE;
    rgb->pixels = NULL;
    rgb->rowBytes = 0;
    rgb->alphaPremultiplied = AVIF_FALSE; // Most expect RGBA output to *not* be premultiplied. Those that do can opt-in by
                                          // setting this to match image->alphaPremultiplied or forcing this to true
                                          // after calling avifRGBImageSetDefaults(),
    rgb->isFloat = AVIF_FALSE;
}

void avifRGBImageAllocatePixels(avifRGBImage * rgb)
{
    if (rgb->pixels) {
        avifFree(rgb->pixels);
    }

    rgb->rowBytes = rgb->width * avifRGBImagePixelSize(rgb);
    rgb->pixels = avifAlloc((size_t)rgb->rowBytes * rgb->height);
}

void avifRGBImageFreePixels(avifRGBImage * rgb)
{
    if (rgb->pixels) {
        avifFree(rgb->pixels);
    }

    rgb->pixels = NULL;
    rgb->rowBytes = 0;
}

// ---------------------------------------------------------------------------
// avifCropRect

typedef struct clapFraction
{
    int32_t n;
    int32_t d;
} clapFraction;

static clapFraction calcCenter(int32_t dim)
{
    clapFraction f;
    f.n = dim >> 1;
    f.d = 1;
    if ((dim % 2) != 0) {
        f.n = dim;
        f.d = 2;
    }
    return f;
}

// |a| and |b| hold int32_t values. The int64_t type is used so that we can negate INT32_MIN without
// overflowing int32_t.
static int64_t calcGCD(int64_t a, int64_t b)
{
    if (a < 0) {
        a *= -1;
    }
    if (b < 0) {
        b *= -1;
    }
    while (b != 0) {
        int64_t r = a % b;
        a = b;
        b = r;
    }
    return a;
}

static void clapFractionSimplify(clapFraction * f)
{
    int64_t gcd = calcGCD(f->n, f->d);
    if (gcd > 1) {
        f->n = (int32_t)(f->n / gcd);
        f->d = (int32_t)(f->d / gcd);
    }
}

static avifBool overflowsInt32(int64_t x)
{
    return (x < INT32_MIN) || (x > INT32_MAX);
}

// Make the fractions have a common denominator
static avifBool clapFractionCD(clapFraction * a, clapFraction * b)
{
    clapFractionSimplify(a);
    clapFractionSimplify(b);
    if (a->d != b->d) {
        const int64_t ad = a->d;
        const int64_t bd = b->d;
        const int64_t anNew = a->n * bd;
        const int64_t adNew = a->d * bd;
        const int64_t bnNew = b->n * ad;
        const int64_t bdNew = b->d * ad;
        if (overflowsInt32(anNew) || overflowsInt32(adNew) || overflowsInt32(bnNew) || overflowsInt32(bdNew)) {
            return AVIF_FALSE;
        }
        a->n = (int32_t)anNew;
        a->d = (int32_t)adNew;
        b->n = (int32_t)bnNew;
        b->d = (int32_t)bdNew;
    }
    return AVIF_TRUE;
}

static avifBool clapFractionAdd(clapFraction a, clapFraction b, clapFraction * result)
{
    if (!clapFractionCD(&a, &b)) {
        return AVIF_FALSE;
    }

    const int64_t resultN = (int64_t)a.n + b.n;
    if (overflowsInt32(resultN)) {
        return AVIF_FALSE;
    }
    result->n = (int32_t)resultN;
    result->d = a.d;

    clapFractionSimplify(result);
    return AVIF_TRUE;
}

static avifBool clapFractionSub(clapFraction a, clapFraction b, clapFraction * result)
{
    if (!clapFractionCD(&a, &b)) {
        return AVIF_FALSE;
    }

    const int64_t resultN = (int64_t)a.n - b.n;
    if (overflowsInt32(resultN)) {
        return AVIF_FALSE;
    }
    result->n = (int32_t)resultN;
    result->d = a.d;

    clapFractionSimplify(result);
    return AVIF_TRUE;
}

static avifBool avifCropRectIsValid(const avifCropRect * cropRect, uint32_t imageW, uint32_t imageH, avifPixelFormat yuvFormat, avifDiagnostics * diag)

{
    // ISO/IEC 23000-22:2019/DAM 2:2021, Section 7.3.6.7:
    //   The clean aperture property is restricted according to the chroma
    //   sampling format of the input image (4:4:4, 4:2:2:, 4:2:0, or 4:0:0) as
    //   follows:
    //   - when the image is 4:0:0 (monochrome) or 4:4:4, the horizontal and
    //     vertical cropped offsets and widths shall be integers;
    //   - when the image is 4:2:2 the horizontal cropped offset and width
    //     shall be even numbers and the vertical values shall be integers;
    //   - when the image is 4:2:0 both the horizontal and vertical cropped
    //     offsets and widths shall be even numbers.

    if ((cropRect->width == 0) || (cropRect->height == 0)) {
        avifDiagnosticsPrintf(diag, "[Strict] crop rect width and height must be nonzero");
        return AVIF_FALSE;
    }
    if ((cropRect->x > (UINT32_MAX - cropRect->width)) || ((cropRect->x + cropRect->width) > imageW) ||
        (cropRect->y > (UINT32_MAX - cropRect->height)) || ((cropRect->y + cropRect->height) > imageH)) {
        avifDiagnosticsPrintf(diag, "[Strict] crop rect is out of the image's bounds");
        return AVIF_FALSE;
    }

    if ((yuvFormat == AVIF_PIXEL_FORMAT_YUV420) || (yuvFormat == AVIF_PIXEL_FORMAT_YUV422)) {
        if (((cropRect->x % 2) != 0) || ((cropRect->width % 2) != 0)) {
            avifDiagnosticsPrintf(diag, "[Strict] crop rect X offset and width must both be even due to this image's YUV subsampling");
            return AVIF_FALSE;
        }
    }
    if (yuvFormat == AVIF_PIXEL_FORMAT_YUV420) {
        if (((cropRect->y % 2) != 0) || ((cropRect->height % 2) != 0)) {
            avifDiagnosticsPrintf(diag, "[Strict] crop rect Y offset and height must both be even due to this image's YUV subsampling");
            return AVIF_FALSE;
        }
    }
    return AVIF_TRUE;
}

avifBool avifCropRectConvertCleanApertureBox(avifCropRect * cropRect,
                                             const avifCleanApertureBox * clap,
                                             uint32_t imageW,
                                             uint32_t imageH,
                                             avifPixelFormat yuvFormat,
                                             avifDiagnostics * diag)
{
    avifDiagnosticsClearError(diag);

    // ISO/IEC 14496-12:2020, Section 12.1.4.1:
    //   For horizOff and vertOff, D shall be strictly positive and N may be
    //   positive or negative. For cleanApertureWidth and cleanApertureHeight,
    //   N shall be positive and D shall be strictly positive.

    const int32_t widthN = (int32_t)clap->widthN;
    const int32_t widthD = (int32_t)clap->widthD;
    const int32_t heightN = (int32_t)clap->heightN;
    const int32_t heightD = (int32_t)clap->heightD;
    const int32_t horizOffN = (int32_t)clap->horizOffN;
    const int32_t horizOffD = (int32_t)clap->horizOffD;
    const int32_t vertOffN = (int32_t)clap->vertOffN;
    const int32_t vertOffD = (int32_t)clap->vertOffD;
    if ((widthD <= 0) || (heightD <= 0) || (horizOffD <= 0) || (vertOffD <= 0)) {
        avifDiagnosticsPrintf(diag, "[Strict] clap contains a denominator that is not strictly positive");
        return AVIF_FALSE;
    }
    if ((widthN < 0) || (heightN < 0)) {
        avifDiagnosticsPrintf(diag, "[Strict] clap width or height is negative");
        return AVIF_FALSE;
    }

    if ((widthN % widthD) != 0) {
        avifDiagnosticsPrintf(diag, "[Strict] clap width %d/%d is not an integer", widthN, widthD);
        return AVIF_FALSE;
    }
    if ((heightN % heightD) != 0) {
        avifDiagnosticsPrintf(diag, "[Strict] clap height %d/%d is not an integer", heightN, heightD);
        return AVIF_FALSE;
    }
    const int32_t clapW = widthN / widthD;
    const int32_t clapH = heightN / heightD;

    if ((imageW > INT32_MAX) || (imageH > INT32_MAX)) {
        avifDiagnosticsPrintf(diag, "[Strict] image width %u or height %u is greater than INT32_MAX", imageW, imageH);
        return AVIF_FALSE;
    }
    clapFraction uncroppedCenterX = calcCenter((int32_t)imageW);
    clapFraction uncroppedCenterY = calcCenter((int32_t)imageH);

    clapFraction horizOff;
    horizOff.n = horizOffN;
    horizOff.d = horizOffD;
    clapFraction croppedCenterX;
    if (!clapFractionAdd(uncroppedCenterX, horizOff, &croppedCenterX)) {
        avifDiagnosticsPrintf(diag, "[Strict] croppedCenterX overflowed");
        return AVIF_FALSE;
    }

    clapFraction vertOff;
    vertOff.n = vertOffN;
    vertOff.d = vertOffD;
    clapFraction croppedCenterY;
    if (!clapFractionAdd(uncroppedCenterY, vertOff, &croppedCenterY)) {
        avifDiagnosticsPrintf(diag, "[Strict] croppedCenterY overflowed");
        return AVIF_FALSE;
    }

    clapFraction halfW;
    halfW.n = clapW;
    halfW.d = 2;
    clapFraction cropX;
    if (!clapFractionSub(croppedCenterX, halfW, &cropX)) {
        avifDiagnosticsPrintf(diag, "[Strict] cropX overflowed");
        return AVIF_FALSE;
    }
    if ((cropX.n % cropX.d) != 0) {
        avifDiagnosticsPrintf(diag, "[Strict] calculated crop X offset %d/%d is not an integer", cropX.n, cropX.d);
        return AVIF_FALSE;
    }

    clapFraction halfH;
    halfH.n = clapH;
    halfH.d = 2;
    clapFraction cropY;
    if (!clapFractionSub(croppedCenterY, halfH, &cropY)) {
        avifDiagnosticsPrintf(diag, "[Strict] cropY overflowed");
        return AVIF_FALSE;
    }
    if ((cropY.n % cropY.d) != 0) {
        avifDiagnosticsPrintf(diag, "[Strict] calculated crop Y offset %d/%d is not an integer", cropY.n, cropY.d);
        return AVIF_FALSE;
    }

    if ((cropX.n < 0) || (cropY.n < 0)) {
        avifDiagnosticsPrintf(diag, "[Strict] at least one crop offset is not positive");
        return AVIF_FALSE;
    }

    cropRect->x = (uint32_t)(cropX.n / cropX.d);
    cropRect->y = (uint32_t)(cropY.n / cropY.d);
    cropRect->width = (uint32_t)clapW;
    cropRect->height = (uint32_t)clapH;
    return avifCropRectIsValid(cropRect, imageW, imageH, yuvFormat, diag);
}

avifBool avifCleanApertureBoxConvertCropRect(avifCleanApertureBox * clap,
                                             const avifCropRect * cropRect,
                                             uint32_t imageW,
                                             uint32_t imageH,
                                             avifPixelFormat yuvFormat,
                                             avifDiagnostics * diag)
{
    avifDiagnosticsClearError(diag);

    if (!avifCropRectIsValid(cropRect, imageW, imageH, yuvFormat, diag)) {
        return AVIF_FALSE;
    }

    if ((imageW > INT32_MAX) || (imageH > INT32_MAX)) {
        avifDiagnosticsPrintf(diag, "[Strict] image width %u or height %u is greater than INT32_MAX", imageW, imageH);
        return AVIF_FALSE;
    }
    clapFraction uncroppedCenterX = calcCenter((int32_t)imageW);
    clapFraction uncroppedCenterY = calcCenter((int32_t)imageH);

    if ((cropRect->width > INT32_MAX) || (cropRect->height > INT32_MAX)) {
        avifDiagnosticsPrintf(diag,
                              "[Strict] crop rect width %u or height %u is greater than INT32_MAX",
                              cropRect->width,
                              cropRect->height);
        return AVIF_FALSE;
    }
    clapFraction croppedCenterX = calcCenter((int32_t)cropRect->width);
    const int64_t croppedCenterXN = croppedCenterX.n + (int64_t)cropRect->x * croppedCenterX.d;
    if (overflowsInt32(croppedCenterXN)) {
        avifDiagnosticsPrintf(diag, "[Strict] croppedCenterX overflowed");
        return AVIF_FALSE;
    }
    croppedCenterX.n = (int32_t)croppedCenterXN;
    clapFraction croppedCenterY = calcCenter((int32_t)cropRect->height);
    const int64_t croppedCenterYN = croppedCenterY.n + (int64_t)cropRect->y * croppedCenterY.d;
    if (overflowsInt32(croppedCenterYN)) {
        avifDiagnosticsPrintf(diag, "[Strict] croppedCenterY overflowed");
        return AVIF_FALSE;
    }
    croppedCenterY.n = (int32_t)croppedCenterYN;

    clapFraction horizOff;
    if (!clapFractionSub(croppedCenterX, uncroppedCenterX, &horizOff)) {
        avifDiagnosticsPrintf(diag, "[Strict] horizOff overflowed");
        return AVIF_FALSE;
    }
    clapFraction vertOff;
    if (!clapFractionSub(croppedCenterY, uncroppedCenterY, &vertOff)) {
        avifDiagnosticsPrintf(diag, "[Strict] vertOff overflowed");
        return AVIF_FALSE;
    }

    clap->widthN = cropRect->width;
    clap->widthD = 1;
    clap->heightN = cropRect->height;
    clap->heightD = 1;
    clap->horizOffN = horizOff.n;
    clap->horizOffD = horizOff.d;
    clap->vertOffN = vertOff.n;
    clap->vertOffD = vertOff.d;
    return AVIF_TRUE;
}

// ---------------------------------------------------------------------------

avifBool avifAreGridDimensionsValid(avifPixelFormat yuvFormat, uint32_t imageW, uint32_t imageH, uint32_t tileW, uint32_t tileH, avifDiagnostics * diag)
{
    // ISO/IEC 23000-22:2019, Section 7.3.11.4.2:
    //   - the tile_width shall be greater than or equal to 64, and should be a multiple of 64
    //   - the tile_height shall be greater than or equal to 64, and should be a multiple of 64
    // The "should" part is ignored here.
    if ((tileW < 64) || (tileH < 64)) {
        avifDiagnosticsPrintf(diag,
                              "Grid image tile width (%u) or height (%u) cannot be smaller than 64. "
                              "See MIAF (ISO/IEC 23000-22:2019), Section 7.3.11.4.2",
                              tileW,
                              tileH);
        return AVIF_FALSE;
    }

    // ISO/IEC 23000-22:2019, Section 7.3.11.4.2:
    //   - when the images are in the 4:2:2 chroma sampling format the horizontal tile offsets and widths,
    //     and the output width, shall be even numbers;
    //   - when the images are in the 4:2:0 chroma sampling format both the horizontal and vertical tile
    //     offsets and widths, and the output width and height, shall be even numbers.
    // If the rules above were not respected, the following problematic situation may happen:
    //   Some 4:2:0 image is 650 pixels wide and has 10 cell columns, each being 65 pixels wide.
    //   The chroma plane of the whole image is 325 pixels wide. The chroma plane of each cell is 33 pixels wide.
    //   33*10 - 325 gives 5 extra pixels with no specified destination in the reconstructed image.

    // Tile offsets are not enforced since they depend on tile size (ISO/IEC 23008-12:2017, Section 6.6.2.3.1):
    //   The reconstructed image is formed by tiling the input images into a grid [...] without gap or overlap
    if ((((yuvFormat == AVIF_PIXEL_FORMAT_YUV420) || (yuvFormat == AVIF_PIXEL_FORMAT_YUV422)) &&
         (((imageW % 2) != 0) || ((tileW % 2) != 0))) ||
        ((yuvFormat == AVIF_PIXEL_FORMAT_YUV420) && (((imageH % 2) != 0) || ((tileH % 2) != 0)))) {
        avifDiagnosticsPrintf(diag,
                              "Grid image width (%u) or height (%u) or tile width (%u) or height (%u) "
                              "shall be even if chroma is subsampled in that dimension. "
                              "See MIAF (ISO/IEC 23000-22:2019), Section 7.3.11.4.2",
                              imageW,
                              imageH,
                              tileW,
                              tileH);
        return AVIF_FALSE;
    }
    return AVIF_TRUE;
}

// ---------------------------------------------------------------------------
// avifCodecSpecificOption

static char * avifStrdup(const char * str)
{
    size_t len = strlen(str);
    char * dup = avifAlloc(len + 1);
    memcpy(dup, str, len + 1);
    return dup;
}

avifCodecSpecificOptions * avifCodecSpecificOptionsCreate(void)
{
    avifCodecSpecificOptions * ava = avifAlloc(sizeof(avifCodecSpecificOptions));
    if (!avifArrayCreate(ava, sizeof(avifCodecSpecificOption), 4)) {
        goto error;
    }
    return ava;

error:
    avifFree(ava);
    return NULL;
}

void avifCodecSpecificOptionsClear(avifCodecSpecificOptions * csOptions)
{
    for (uint32_t i = 0; i < csOptions->count; ++i) {
        avifCodecSpecificOption * entry = &csOptions->entries[i];
        avifFree(entry->key);
        avifFree(entry->value);
    }

    csOptions->count = 0;
}

void avifCodecSpecificOptionsDestroy(avifCodecSpecificOptions * csOptions)
{
    if (!csOptions) {
        return;
    }

    avifCodecSpecificOptionsClear(csOptions);
    avifArrayDestroy(csOptions);
    avifFree(csOptions);
}

void avifCodecSpecificOptionsSet(avifCodecSpecificOptions * csOptions, const char * key, const char * value)
{
    // Check to see if a key must be replaced
    for (uint32_t i = 0; i < csOptions->count; ++i) {
        avifCodecSpecificOption * entry = &csOptions->entries[i];
        if (!strcmp(entry->key, key)) {
            if (value) {
                // Update the value
                avifFree(entry->value);
                entry->value = avifStrdup(value);
            } else {
                // Delete the value
                avifFree(entry->key);
                avifFree(entry->value);
                --csOptions->count;
                if (csOptions->count > 0) {
                    memmove(&csOptions->entries[i], &csOptions->entries[i + 1], (csOptions->count - i) * (size_t)csOptions->elementSize);
                }
            }
            return;
        }
    }

    if (value) {
        // Add a new key
        avifCodecSpecificOption * entry = (avifCodecSpecificOption *)avifArrayPushPtr(csOptions);
        entry->key = avifStrdup(key);
        entry->value = avifStrdup(value);
    }
}

// ---------------------------------------------------------------------------
// Codec availability and versions

typedef const char * (*versionFunc)(void);
typedef avifCodec * (*avifCodecCreateFunc)(void);

struct AvailableCodec
{
    avifCodecChoice choice;
    const char * name;
    versionFunc version;
    avifCodecCreateFunc create;
    uint32_t flags;
};

// This is the main codec table; it determines all usage/availability in libavif.

static struct AvailableCodec availableCodecs[] = {
// Ordered by preference (for AUTO)

#if defined(AVIF_CODEC_DAV1D)
    { AVIF_CODEC_CHOICE_DAV1D, "dav1d", avifCodecVersionDav1d, avifCodecCreateDav1d, AVIF_CODEC_FLAG_CAN_DECODE },
#endif
#if defined(AVIF_CODEC_LIBGAV1)
    { AVIF_CODEC_CHOICE_LIBGAV1, "libgav1", avifCodecVersionGav1, avifCodecCreateGav1, AVIF_CODEC_FLAG_CAN_DECODE },
#endif
#if defined(AVIF_CODEC_AOM)
    { AVIF_CODEC_CHOICE_AOM,
      "aom",
      avifCodecVersionAOM,
      avifCodecCreateAOM,
#if defined(AVIF_CODEC_AOM_DECODE) && defined(AVIF_CODEC_AOM_ENCODE)
      AVIF_CODEC_FLAG_CAN_DECODE | AVIF_CODEC_FLAG_CAN_ENCODE
#elif defined(AVIF_CODEC_AOM_DECODE)
      AVIF_CODEC_FLAG_CAN_DECODE
#elif defined(AVIF_CODEC_AOM_ENCODE)
      AVIF_CODEC_FLAG_CAN_ENCODE
#else
#error AVIF_CODEC_AOM_DECODE or AVIF_CODEC_AOM_ENCODE must be defined
#endif
    },
#endif
#if defined(AVIF_CODEC_RAV1E)
    { AVIF_CODEC_CHOICE_RAV1E, "rav1e", avifCodecVersionRav1e, avifCodecCreateRav1e, AVIF_CODEC_FLAG_CAN_ENCODE },
#endif
#if defined(AVIF_CODEC_SVT)
    { AVIF_CODEC_CHOICE_SVT, "svt", avifCodecVersionSvt, avifCodecCreateSvt, AVIF_CODEC_FLAG_CAN_ENCODE },
#endif
    { AVIF_CODEC_CHOICE_AUTO, NULL, NULL, NULL, 0 }
};

static const int availableCodecsCount = (sizeof(availableCodecs) / sizeof(availableCodecs[0])) - 1;

static struct AvailableCodec * findAvailableCodec(avifCodecChoice choice, avifCodecFlags requiredFlags)
{
    for (int i = 0; i < availableCodecsCount; ++i) {
        if ((choice != AVIF_CODEC_CHOICE_AUTO) && (availableCodecs[i].choice != choice)) {
            continue;
        }
        if (requiredFlags && ((availableCodecs[i].flags & requiredFlags) != requiredFlags)) {
            continue;
        }
        return &availableCodecs[i];
    }
    return NULL;
}

const char * avifCodecName(avifCodecChoice choice, avifCodecFlags requiredFlags)
{
    struct AvailableCodec * availableCodec = findAvailableCodec(choice, requiredFlags);
    if (availableCodec) {
        return availableCodec->name;
    }
    return NULL;
}

avifCodecChoice avifCodecChoiceFromName(const char * name)
{
    for (int i = 0; i < availableCodecsCount; ++i) {
        if (!strcmp(availableCodecs[i].name, name)) {
            return availableCodecs[i].choice;
        }
    }
    return AVIF_CODEC_CHOICE_AUTO;
}

avifCodec * avifCodecCreate(avifCodecChoice choice, avifCodecFlags requiredFlags)
{
    struct AvailableCodec * availableCodec = findAvailableCodec(choice, requiredFlags);
    if (availableCodec) {
        return availableCodec->create();
    }
    return NULL;
}

static void append(char ** writePos, size_t * remainingLen, const char * appendStr)
{
    size_t appendLen = strlen(appendStr);
    if (appendLen > *remainingLen) {
        appendLen = *remainingLen;
    }

    memcpy(*writePos, appendStr, appendLen);
    *remainingLen -= appendLen;
    *writePos += appendLen;
    *(*writePos) = 0;
}

void avifCodecVersions(char outBuffer[256])
{
    size_t remainingLen = 255;
    char * writePos = outBuffer;
    *writePos = 0;

    for (int i = 0; i < availableCodecsCount; ++i) {
        if (i > 0) {
            append(&writePos, &remainingLen, ", ");
        }
        append(&writePos, &remainingLen, availableCodecs[i].name);
        if ((availableCodecs[i].flags & (AVIF_CODEC_FLAG_CAN_ENCODE | AVIF_CODEC_FLAG_CAN_DECODE)) ==
            (AVIF_CODEC_FLAG_CAN_ENCODE | AVIF_CODEC_FLAG_CAN_DECODE)) {
            append(&writePos, &remainingLen, " [enc/dec]");
        } else if (availableCodecs[i].flags & AVIF_CODEC_FLAG_CAN_ENCODE) {
            append(&writePos, &remainingLen, " [enc]");
        } else if (availableCodecs[i].flags & AVIF_CODEC_FLAG_CAN_DECODE) {
            append(&writePos, &remainingLen, " [dec]");
        }
        append(&writePos, &remainingLen, ":");
        append(&writePos, &remainingLen, availableCodecs[i].version());
    }
}
