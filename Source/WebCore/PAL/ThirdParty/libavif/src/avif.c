// Copyright 2019 Joe Drago. All rights reserved.
// SPDX-License-Identifier: BSD-2-Clause

#include "avif/internal.h"

#include <assert.h>
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
            info->monochrome = AVIF_TRUE;
            // The nonexistent chroma is considered as subsampled in each dimension
            // according to the AV1 specification. See sections 5.5.2 and 6.4.2.
            info->chromaShiftX = 1;
            info->chromaShiftY = 1;
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
        case AVIF_RESULT_MISSING_IMAGE_ITEM:            return "Missing or empty image item";
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
        case AVIF_RESULT_INTERNAL_ERROR:                return "Internal error";
#if defined(AVIF_ENABLE_EXPERIMENTAL_GAIN_MAP)
        case AVIF_RESULT_ENCODE_GAIN_MAP_FAILED:        return "Encoding of gain map planes failed";
        case AVIF_RESULT_DECODE_GAIN_MAP_FAILED:        return "Decoding of gain map planes failed";
        case AVIF_RESULT_INVALID_TONE_MAPPED_IMAGE:     return "Invalid tone mapped image item";
#endif
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

void avifImageSetDefaults(avifImage * image)
{
    memset(image, 0, sizeof(avifImage));
    image->yuvRange = AVIF_RANGE_FULL;
    image->colorPrimaries = AVIF_COLOR_PRIMARIES_UNSPECIFIED;
    image->transferCharacteristics = AVIF_TRANSFER_CHARACTERISTICS_UNSPECIFIED;
    image->matrixCoefficients = AVIF_MATRIX_COEFFICIENTS_UNSPECIFIED;
}

avifImage * avifImageCreate(uint32_t width, uint32_t height, uint32_t depth, avifPixelFormat yuvFormat)
{
    // width and height are checked when actually used, for example by avifImageAllocatePlanes().
    AVIF_CHECKERR(depth <= 16, NULL); // avifImage only supports up to 16 bits per sample. See avifImageUsesU16().
    // Cast to silence "comparison of unsigned expression is always true" warning.
    AVIF_CHECKERR((int)yuvFormat >= AVIF_PIXEL_FORMAT_NONE && yuvFormat < AVIF_PIXEL_FORMAT_COUNT, NULL);

    avifImage * image = (avifImage *)avifAlloc(sizeof(avifImage));
    AVIF_CHECKERR(image, NULL);
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

void avifImageCopyNoAlloc(avifImage * dstImage, const avifImage * srcImage)
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
    dstImage->clli = srcImage->clli;

    dstImage->transformFlags = srcImage->transformFlags;
    dstImage->pasp = srcImage->pasp;
    dstImage->clap = srcImage->clap;
    dstImage->irot = srcImage->irot;
    dstImage->imir = srcImage->imir;
}

void avifImageCopySamples(avifImage * dstImage, const avifImage * srcImage, avifPlanesFlags planes)
{
    assert(srcImage->depth == dstImage->depth);
    if (planes & AVIF_PLANES_YUV) {
        assert(srcImage->yuvFormat == dstImage->yuvFormat);
        // Note that there may be a mismatch between srcImage->yuvRange and dstImage->yuvRange
        // because libavif allows for 'colr' and AV1 OBU video range values to differ.
    }
    const size_t bytesPerPixel = avifImageUsesU16(srcImage) ? 2 : 1;

    const avifBool skipColor = !(planes & AVIF_PLANES_YUV);
    const avifBool skipAlpha = !(planes & AVIF_PLANES_A);
    for (int c = AVIF_CHAN_Y; c <= AVIF_CHAN_A; ++c) {
        const avifBool alpha = c == AVIF_CHAN_A;
        if ((skipColor && !alpha) || (skipAlpha && alpha)) {
            continue;
        }

        const uint32_t planeWidth = avifImagePlaneWidth(srcImage, c);
        const uint32_t planeHeight = avifImagePlaneHeight(srcImage, c);
        const uint8_t * srcRow = avifImagePlane(srcImage, c);
        uint8_t * dstRow = avifImagePlane(dstImage, c);
        const uint32_t srcRowBytes = avifImagePlaneRowBytes(srcImage, c);
        const uint32_t dstRowBytes = avifImagePlaneRowBytes(dstImage, c);
        assert(!srcRow == !dstRow);
        if (!srcRow) {
            continue;
        }
        assert(planeWidth == avifImagePlaneWidth(dstImage, c));
        assert(planeHeight == avifImagePlaneHeight(dstImage, c));

        const size_t planeWidthBytes = planeWidth * bytesPerPixel;
        for (uint32_t y = 0; y < planeHeight; ++y) {
            memcpy(dstRow, srcRow, planeWidthBytes);
            srcRow += srcRowBytes;
            dstRow += dstRowBytes;
        }
    }
}

avifResult avifImageCopy(avifImage * dstImage, const avifImage * srcImage, avifPlanesFlags planes)
{
    avifImageFreePlanes(dstImage, AVIF_PLANES_ALL);
    avifImageCopyNoAlloc(dstImage, srcImage);

    AVIF_CHECKRES(avifImageSetProfileICC(dstImage, srcImage->icc.data, srcImage->icc.size));

    AVIF_CHECKRES(avifRWDataSet(&dstImage->exif, srcImage->exif.data, srcImage->exif.size));
    AVIF_CHECKRES(avifImageSetMetadataXMP(dstImage, srcImage->xmp.data, srcImage->xmp.size));

    if ((planes & AVIF_PLANES_YUV) && srcImage->yuvPlanes[AVIF_CHAN_Y]) {
        if ((srcImage->yuvFormat != AVIF_PIXEL_FORMAT_YUV400) &&
            (!srcImage->yuvPlanes[AVIF_CHAN_U] || !srcImage->yuvPlanes[AVIF_CHAN_V])) {
            return AVIF_RESULT_INVALID_ARGUMENT;
        }
        const avifResult allocationResult = avifImageAllocatePlanes(dstImage, AVIF_PLANES_YUV);
        if (allocationResult != AVIF_RESULT_OK) {
            return allocationResult;
        }
    }
    if ((planes & AVIF_PLANES_A) && srcImage->alphaPlane) {
        const avifResult allocationResult = avifImageAllocatePlanes(dstImage, AVIF_PLANES_A);
        if (allocationResult != AVIF_RESULT_OK) {
            return allocationResult;
        }
    }
    avifImageCopySamples(dstImage, srcImage, planes);

#if defined(AVIF_ENABLE_EXPERIMENTAL_GAIN_MAP)
    if (srcImage->gainMap) {
        if (!dstImage->gainMap) {
            dstImage->gainMap = avifGainMapCreate();
            AVIF_CHECKERR(dstImage->gainMap, AVIF_RESULT_OUT_OF_MEMORY);
        }
        dstImage->gainMap->metadata = srcImage->gainMap->metadata;
        AVIF_CHECKRES(avifRWDataSet(&dstImage->gainMap->altICC, srcImage->gainMap->altICC.data, srcImage->gainMap->altICC.size));
        dstImage->gainMap->altColorPrimaries = srcImage->gainMap->altColorPrimaries;
        dstImage->gainMap->altTransferCharacteristics = srcImage->gainMap->altTransferCharacteristics;
        dstImage->gainMap->altMatrixCoefficients = srcImage->gainMap->altMatrixCoefficients;
        dstImage->gainMap->altDepth = srcImage->gainMap->altDepth;
        dstImage->gainMap->altPlaneCount = srcImage->gainMap->altPlaneCount;
        dstImage->gainMap->altCLLI = srcImage->gainMap->altCLLI;

        if (srcImage->gainMap->image) {
            if (!dstImage->gainMap->image) {
                dstImage->gainMap->image = avifImageCreateEmpty();
            }
            AVIF_CHECKRES(avifImageCopy(dstImage->gainMap->image, srcImage->gainMap->image, planes));
        } else if (dstImage->gainMap->image) {
            avifImageDestroy(dstImage->gainMap->image);
            dstImage->gainMap->image = NULL;
        }
    } else if (dstImage->gainMap) {
        avifGainMapDestroy(dstImage->gainMap);
        dstImage->gainMap = NULL;
    }
#endif // defined(AVIF_ENABLE_EXPERIMENTAL_GAIN_MAP)

    return AVIF_RESULT_OK;
}

avifResult avifImageSetViewRect(avifImage * dstImage, const avifImage * srcImage, const avifCropRect * rect)
{
    avifPixelFormatInfo formatInfo;
    avifGetPixelFormatInfo(srcImage->yuvFormat, &formatInfo);
    if ((rect->width > srcImage->width) || (rect->height > srcImage->height) || (rect->x > (srcImage->width - rect->width)) ||
        (rect->y > (srcImage->height - rect->height))) {
        return AVIF_RESULT_INVALID_ARGUMENT;
    }
    if (!formatInfo.monochrome && ((rect->x & formatInfo.chromaShiftX) || (rect->y & formatInfo.chromaShiftY))) {
        return AVIF_RESULT_INVALID_ARGUMENT;
    }
    avifImageFreePlanes(dstImage, AVIF_PLANES_ALL); // dstImage->imageOwnsYUVPlanes and dstImage->imageOwnsAlphaPlane set to AVIF_FALSE.
    avifImageCopyNoAlloc(dstImage, srcImage);
    dstImage->width = rect->width;
    dstImage->height = rect->height;
    const uint32_t pixelBytes = (srcImage->depth > 8) ? 2 : 1;
    if (srcImage->yuvPlanes[AVIF_CHAN_Y]) {
        for (int yuvPlane = AVIF_CHAN_Y; yuvPlane <= AVIF_CHAN_V; ++yuvPlane) {
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
#if defined(AVIF_ENABLE_EXPERIMENTAL_GAIN_MAP)
    if (image->gainMap) {
        avifGainMapDestroy(image->gainMap);
    }
#endif
    avifImageFreePlanes(image, AVIF_PLANES_ALL);
    avifRWDataFree(&image->icc);
    avifRWDataFree(&image->exif);
    avifRWDataFree(&image->xmp);
    avifFree(image);
}

avifResult avifImageSetProfileICC(avifImage * image, const uint8_t * icc, size_t iccSize)
{
    return avifRWDataSet(&image->icc, icc, iccSize);
}

avifResult avifImageSetMetadataXMP(avifImage * image, const uint8_t * xmp, size_t xmpSize)
{
    return avifRWDataSet(&image->xmp, xmp, xmpSize);
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

        image->imageOwnsYUVPlanes = AVIF_TRUE;
        if (!image->yuvPlanes[AVIF_CHAN_Y]) {
            image->yuvRowBytes[AVIF_CHAN_Y] = (uint32_t)fullRowBytes;
            image->yuvPlanes[AVIF_CHAN_Y] = avifAlloc(fullSize);
            if (!image->yuvPlanes[AVIF_CHAN_Y]) {
                return AVIF_RESULT_OUT_OF_MEMORY;
            }
        }

        if (!info.monochrome) {
            // Intermediary computation as 64 bits in case width or height is exactly UINT32_MAX.
            const uint32_t shiftedW = (uint32_t)(((uint64_t)image->width + info.chromaShiftX) >> info.chromaShiftX);
            const uint32_t shiftedH = (uint32_t)(((uint64_t)image->height + info.chromaShiftY) >> info.chromaShiftY);

            // These are less than or equal to fullRowBytes/fullSize. No need to check overflows.
            const size_t uvRowBytes = channelSize * shiftedW;
            const size_t uvSize = uvRowBytes * shiftedH;

            for (int uvPlane = AVIF_CHAN_U; uvPlane <= AVIF_CHAN_V; ++uvPlane) {
                if (!image->yuvPlanes[uvPlane]) {
                    image->yuvRowBytes[uvPlane] = (uint32_t)uvRowBytes;
                    image->yuvPlanes[uvPlane] = avifAlloc(uvSize);
                    if (!image->yuvPlanes[uvPlane]) {
                        return AVIF_RESULT_OUT_OF_MEMORY;
                    }
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

avifBool avifImageIsOpaque(const avifImage * image)
{
    if (!image->alphaPlane) {
        return AVIF_TRUE;
    }

    const uint32_t opaqueValue = (1u << image->depth) - 1u;
    const uint8_t * row = image->alphaPlane;
    for (uint32_t y = 0; y < image->height; ++y) {
        if (avifImageUsesU16(image)) {
            const uint16_t * row16 = (const uint16_t *)row;
            for (uint32_t x = 0; x < image->width; ++x) {
                if (row16[x] != opaqueValue) {
                    return AVIF_FALSE;
                }
            }
        } else {
            for (uint32_t x = 0; x < image->width; ++x) {
                if (row[x] != opaqueValue) {
                    return AVIF_FALSE;
                }
            }
        }
        row += image->alphaRowBytes;
    }
    return AVIF_TRUE;
}

uint8_t * avifImagePlane(const avifImage * image, int channel)
{
    if ((channel == AVIF_CHAN_Y) || (channel == AVIF_CHAN_U) || (channel == AVIF_CHAN_V)) {
        return image->yuvPlanes[channel];
    }
    if (channel == AVIF_CHAN_A) {
        return image->alphaPlane;
    }
    return NULL;
}

uint32_t avifImagePlaneRowBytes(const avifImage * image, int channel)
{
    if ((channel == AVIF_CHAN_Y) || (channel == AVIF_CHAN_U) || (channel == AVIF_CHAN_V)) {
        return image->yuvRowBytes[channel];
    }
    if (channel == AVIF_CHAN_A) {
        return image->alphaRowBytes;
    }
    return 0;
}

uint32_t avifImagePlaneWidth(const avifImage * image, int channel)
{
    if (channel == AVIF_CHAN_Y) {
        return image->width;
    }
    if ((channel == AVIF_CHAN_U) || (channel == AVIF_CHAN_V)) {
        avifPixelFormatInfo formatInfo;
        avifGetPixelFormatInfo(image->yuvFormat, &formatInfo);
        if (formatInfo.monochrome) {
            return 0;
        }
        return (image->width + formatInfo.chromaShiftX) >> formatInfo.chromaShiftX;
    }
    if ((channel == AVIF_CHAN_A) && image->alphaPlane) {
        return image->width;
    }
    return 0;
}

uint32_t avifImagePlaneHeight(const avifImage * image, int channel)
{
    if (channel == AVIF_CHAN_Y) {
        return image->height;
    }
    if ((channel == AVIF_CHAN_U) || (channel == AVIF_CHAN_V)) {
        avifPixelFormatInfo formatInfo;
        avifGetPixelFormatInfo(image->yuvFormat, &formatInfo);
        if (formatInfo.monochrome) {
            return 0;
        }
        return (image->height + formatInfo.chromaShiftY) >> formatInfo.chromaShiftY;
    }
    if ((channel == AVIF_CHAN_A) && image->alphaPlane) {
        return image->height;
    }
    return 0;
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
    rgb->maxThreads = 1;
}

avifResult avifRGBImageAllocatePixels(avifRGBImage * rgb)
{
    avifRGBImageFreePixels(rgb);
    const uint32_t rowBytes = rgb->width * avifRGBImagePixelSize(rgb);
    rgb->pixels = avifAlloc((size_t)rowBytes * rgb->height);
    AVIF_CHECKERR(rgb->pixels, AVIF_RESULT_OUT_OF_MEMORY);
    rgb->rowBytes = rowBytes;
    return AVIF_RESULT_OK;
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

static avifFraction calcCenter(int32_t dim)
{
    avifFraction f;
    f.n = dim >> 1;
    f.d = 1;
    if ((dim % 2) != 0) {
        f.n = dim;
        f.d = 2;
    }
    return f;
}

static avifBool overflowsInt32(int64_t x)
{
    return (x < INT32_MIN) || (x > INT32_MAX);
}

static avifBool avifCropRectIsValid(const avifCropRect * cropRect, uint32_t imageW, uint32_t imageH, avifPixelFormat yuvFormat, avifDiagnostics * diag)

{
    // ISO/IEC 23000-22:2019/Amd. 2:2021, Section 7.3.6.7:
    //   The clean aperture property is restricted according to the chroma
    //   sampling format of the input image (4:4:4, 4:2:2:, 4:2:0, or 4:0:0) as
    //   follows:
    //   ...
    //   - If chroma is subsampled horizontally (i.e., 4:2:2 and 4:2:0), the
    //     leftmost pixel of the clean aperture shall be even numbers;
    //   - If chroma is subsampled vertically (i.e., 4:2:0), the topmost line
    //     of the clean aperture shall be even numbers.

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
        if ((cropRect->x % 2) != 0) {
            avifDiagnosticsPrintf(diag, "[Strict] crop rect X offset must be even due to this image's YUV subsampling");
            return AVIF_FALSE;
        }
    }
    if (yuvFormat == AVIF_PIXEL_FORMAT_YUV420) {
        if ((cropRect->y % 2) != 0) {
            avifDiagnosticsPrintf(diag, "[Strict] crop rect Y offset must be even due to this image's YUV subsampling");
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
    avifFraction uncroppedCenterX = calcCenter((int32_t)imageW);
    avifFraction uncroppedCenterY = calcCenter((int32_t)imageH);

    avifFraction horizOff;
    horizOff.n = horizOffN;
    horizOff.d = horizOffD;
    avifFraction croppedCenterX;
    if (!avifFractionAdd(uncroppedCenterX, horizOff, &croppedCenterX)) {
        avifDiagnosticsPrintf(diag, "[Strict] croppedCenterX overflowed");
        return AVIF_FALSE;
    }

    avifFraction vertOff;
    vertOff.n = vertOffN;
    vertOff.d = vertOffD;
    avifFraction croppedCenterY;
    if (!avifFractionAdd(uncroppedCenterY, vertOff, &croppedCenterY)) {
        avifDiagnosticsPrintf(diag, "[Strict] croppedCenterY overflowed");
        return AVIF_FALSE;
    }

    avifFraction halfW;
    halfW.n = clapW;
    halfW.d = 2;
    avifFraction cropX;
    if (!avifFractionSub(croppedCenterX, halfW, &cropX)) {
        avifDiagnosticsPrintf(diag, "[Strict] cropX overflowed");
        return AVIF_FALSE;
    }
    if ((cropX.n % cropX.d) != 0) {
        avifDiagnosticsPrintf(diag, "[Strict] calculated crop X offset %d/%d is not an integer", cropX.n, cropX.d);
        return AVIF_FALSE;
    }

    avifFraction halfH;
    halfH.n = clapH;
    halfH.d = 2;
    avifFraction cropY;
    if (!avifFractionSub(croppedCenterY, halfH, &cropY)) {
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
    avifFraction uncroppedCenterX = calcCenter((int32_t)imageW);
    avifFraction uncroppedCenterY = calcCenter((int32_t)imageH);

    if ((cropRect->width > INT32_MAX) || (cropRect->height > INT32_MAX)) {
        avifDiagnosticsPrintf(diag,
                              "[Strict] crop rect width %u or height %u is greater than INT32_MAX",
                              cropRect->width,
                              cropRect->height);
        return AVIF_FALSE;
    }
    avifFraction croppedCenterX = calcCenter((int32_t)cropRect->width);
    const int64_t croppedCenterXN = croppedCenterX.n + (int64_t)cropRect->x * croppedCenterX.d;
    if (overflowsInt32(croppedCenterXN)) {
        avifDiagnosticsPrintf(diag, "[Strict] croppedCenterX overflowed");
        return AVIF_FALSE;
    }
    croppedCenterX.n = (int32_t)croppedCenterXN;
    avifFraction croppedCenterY = calcCenter((int32_t)cropRect->height);
    const int64_t croppedCenterYN = croppedCenterY.n + (int64_t)cropRect->y * croppedCenterY.d;
    if (overflowsInt32(croppedCenterYN)) {
        avifDiagnosticsPrintf(diag, "[Strict] croppedCenterY overflowed");
        return AVIF_FALSE;
    }
    croppedCenterY.n = (int32_t)croppedCenterYN;

    avifFraction horizOff;
    if (!avifFractionSub(croppedCenterX, uncroppedCenterX, &horizOff)) {
        avifDiagnosticsPrintf(diag, "[Strict] horizOff overflowed");
        return AVIF_FALSE;
    }
    avifFraction vertOff;
    if (!avifFractionSub(croppedCenterY, uncroppedCenterY, &vertOff)) {
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

// Returns NULL if a memory allocation failed.
static char * avifStrdup(const char * str)
{
    size_t len = strlen(str);
    char * dup = avifAlloc(len + 1);
    if (!dup) {
        return NULL;
    }
    memcpy(dup, str, len + 1);
    return dup;
}

avifCodecSpecificOptions * avifCodecSpecificOptionsCreate(void)
{
    avifCodecSpecificOptions * ava = avifAlloc(sizeof(avifCodecSpecificOptions));
    if (!ava || !avifArrayCreate(ava, sizeof(avifCodecSpecificOption), 4)) {
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
    avifCodecSpecificOptionsClear(csOptions);
    avifArrayDestroy(csOptions);
    avifFree(csOptions);
}

avifResult avifCodecSpecificOptionsSet(avifCodecSpecificOptions * csOptions, const char * key, const char * value)
{
    // Check to see if a key must be replaced
    for (uint32_t i = 0; i < csOptions->count; ++i) {
        avifCodecSpecificOption * entry = &csOptions->entries[i];
        if (!strcmp(entry->key, key)) {
            if (value) {
                // Update the value
                avifFree(entry->value);
                entry->value = avifStrdup(value);
                AVIF_CHECKERR(entry->value, AVIF_RESULT_OUT_OF_MEMORY);
            } else {
                // Delete the value
                avifFree(entry->key);
                avifFree(entry->value);
                --csOptions->count;
                if (csOptions->count > 0) {
                    memmove(&csOptions->entries[i], &csOptions->entries[i + 1], (csOptions->count - i) * (size_t)csOptions->elementSize);
                }
            }
            return AVIF_RESULT_OK;
        }
    }

    if (value) {
        // Add a new key
        avifCodecSpecificOption * entry = (avifCodecSpecificOption *)avifArrayPush(csOptions);
        AVIF_CHECKERR(entry, AVIF_RESULT_OUT_OF_MEMORY);
        entry->key = avifStrdup(key);
        AVIF_CHECKERR(entry->key, AVIF_RESULT_OUT_OF_MEMORY);
        entry->value = avifStrdup(value);
        AVIF_CHECKERR(entry->value, AVIF_RESULT_OUT_OF_MEMORY);
    }
    return AVIF_RESULT_OK;
}

// ---------------------------------------------------------------------------
// Codec availability and versions

typedef const char * (*versionFunc)(void);
typedef avifCodec * (*avifCodecCreateFunc)(void);

struct AvailableCodec
{
    avifCodecChoice choice;
    avifCodecType type;
    const char * name;
    versionFunc version;
    avifCodecCreateFunc create;
    uint32_t flags;
};

// This is the main codec table; it determines all usage/availability in libavif.

static struct AvailableCodec availableCodecs[] = {
// Ordered by preference (for AUTO)

#if defined(AVIF_CODEC_DAV1D)
    { AVIF_CODEC_CHOICE_DAV1D, AVIF_CODEC_TYPE_AV1, "dav1d", avifCodecVersionDav1d, avifCodecCreateDav1d, AVIF_CODEC_FLAG_CAN_DECODE },
#endif
#if defined(AVIF_CODEC_LIBGAV1)
    { AVIF_CODEC_CHOICE_LIBGAV1, AVIF_CODEC_TYPE_AV1, "libgav1", avifCodecVersionGav1, avifCodecCreateGav1, AVIF_CODEC_FLAG_CAN_DECODE },
#endif
#if defined(AVIF_CODEC_AOM)
    { AVIF_CODEC_CHOICE_AOM,
      AVIF_CODEC_TYPE_AV1,
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
    { AVIF_CODEC_CHOICE_RAV1E, AVIF_CODEC_TYPE_AV1, "rav1e", avifCodecVersionRav1e, avifCodecCreateRav1e, AVIF_CODEC_FLAG_CAN_ENCODE },
#endif
#if defined(AVIF_CODEC_SVT)
    { AVIF_CODEC_CHOICE_SVT, AVIF_CODEC_TYPE_AV1, "svt", avifCodecVersionSvt, avifCodecCreateSvt, AVIF_CODEC_FLAG_CAN_ENCODE },
#endif
#if defined(AVIF_CODEC_AVM)
    { AVIF_CODEC_CHOICE_AVM, AVIF_CODEC_TYPE_AV2, "avm", avifCodecVersionAVM, avifCodecCreateAVM, AVIF_CODEC_FLAG_CAN_DECODE | AVIF_CODEC_FLAG_CAN_ENCODE },
#endif
    { AVIF_CODEC_CHOICE_AUTO, AVIF_CODEC_TYPE_UNKNOWN, NULL, NULL, NULL, 0 }
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
        if ((choice == AVIF_CODEC_CHOICE_AUTO) && (availableCodecs[i].choice == AVIF_CODEC_CHOICE_AVM)) {
            // AV2 is experimental and cannot be the default, it must be explicitly selected.
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

avifCodecType avifCodecTypeFromChoice(avifCodecChoice choice, avifCodecFlags requiredFlags)
{
    struct AvailableCodec * availableCodec = findAvailableCodec(choice, requiredFlags);
    if (availableCodec) {
        return availableCodec->type;
    }
    return AVIF_CODEC_TYPE_UNKNOWN;
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

avifResult avifCodecCreate(avifCodecChoice choice, avifCodecFlags requiredFlags, avifCodec ** codec)
{
    *codec = NULL;
    struct AvailableCodec * availableCodec = findAvailableCodec(choice, requiredFlags);
    AVIF_CHECKERR(availableCodec != NULL, AVIF_RESULT_NO_CODEC_AVAILABLE);
    *codec = availableCodec->create();
    AVIF_CHECKERR(*codec != NULL, AVIF_RESULT_OUT_OF_MEMORY);
    return AVIF_RESULT_OK;
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

#if defined(AVIF_ENABLE_EXPERIMENTAL_GAIN_MAP)
avifGainMap * avifGainMapCreate()
{
    avifGainMap * gainMap = (avifGainMap *)avifAlloc(sizeof(avifGainMap));
    if (!gainMap) {
        return NULL;
    }
    memset(gainMap, 0, sizeof(avifGainMap));
    gainMap->altColorPrimaries = AVIF_COLOR_PRIMARIES_UNSPECIFIED;
    gainMap->altTransferCharacteristics = AVIF_TRANSFER_CHARACTERISTICS_UNSPECIFIED;
    gainMap->altMatrixCoefficients = AVIF_MATRIX_COEFFICIENTS_UNSPECIFIED;
    gainMap->altYUVRange = AVIF_RANGE_FULL;
    gainMap->metadata.useBaseColorSpace = AVIF_TRUE;
    return gainMap;
}

void avifGainMapDestroy(avifGainMap * gainMap)
{
    if (gainMap->image) {
        avifImageDestroy(gainMap->image);
    }
    avifRWDataFree(&gainMap->altICC);
    avifFree(gainMap);
}
#endif // AVIF_ENABLE_EXPERIMENTAL_GAIN_MAP
