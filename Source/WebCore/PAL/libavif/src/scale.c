// Copyright 2021 Joe Drago. All rights reserved.
// SPDX-License-Identifier: BSD-2-Clause

#include "avif/internal.h"

#if !defined(AVIF_LIBYUV_ENABLED)

avifBool avifImageScale(avifImage * image,
                        uint32_t dstWidth,
                        uint32_t dstHeight,
                        uint32_t imageSizeLimit,
                        uint32_t imageDimensionLimit,
                        avifDiagnostics * diag)
{
    (void)image;
    (void)dstWidth;
    (void)dstHeight;
    (void)imageSizeLimit;
    (void)imageDimensionLimit;
    avifDiagnosticsPrintf(diag, "avifImageScale() called, but is unimplemented without libyuv!");
    return AVIF_FALSE;
}

#else

#include <limits.h>

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wstrict-prototypes" // "this function declaration is not a prototype"
// The newline at the end of libyuv/version.h was accidentally deleted in version 1792 and restored
// in version 1813:
// https://chromium-review.googlesource.com/c/libyuv/libyuv/+/3183182
// https://chromium-review.googlesource.com/c/libyuv/libyuv/+/3527834
#pragma clang diagnostic ignored "-Wnewline-eof"       // "no newline at end of file"
#endif
#include <libyuv.h>
#if defined(__clang__)
#pragma clang diagnostic pop
#endif

// This should be configurable and/or smarter. kFilterBox has the highest quality but is the slowest.
#define AVIF_LIBYUV_FILTER_MODE kFilterBox

avifBool avifImageScale(avifImage * image,
                        uint32_t dstWidth,
                        uint32_t dstHeight,
                        uint32_t imageSizeLimit,
                        uint32_t imageDimensionLimit,
                        avifDiagnostics * diag)
{
    if ((image->width == dstWidth) && (image->height == dstHeight)) {
        // Nothing to do
        return AVIF_TRUE;
    }

    if ((dstWidth == 0) || (dstHeight == 0)) {
        avifDiagnosticsPrintf(diag, "avifImageScale requested invalid dst dimensions [%ux%u]", dstWidth, dstHeight);
        return AVIF_FALSE;
    }
    if (avifDimensionsTooLarge(dstWidth, dstHeight, imageSizeLimit, imageDimensionLimit)) {
        avifDiagnosticsPrintf(diag, "avifImageScale requested dst dimensions that are too large [%ux%u]", dstWidth, dstHeight);
        return AVIF_FALSE;
    }

    uint8_t * srcYUVPlanes[AVIF_PLANE_COUNT_YUV];
    uint32_t srcYUVRowBytes[AVIF_PLANE_COUNT_YUV];
    for (int i = 0; i < AVIF_PLANE_COUNT_YUV; ++i) {
        srcYUVPlanes[i] = image->yuvPlanes[i];
        image->yuvPlanes[i] = NULL;
        srcYUVRowBytes[i] = image->yuvRowBytes[i];
        image->yuvRowBytes[i] = 0;
    }
    const avifBool srcImageOwnsYUVPlanes = image->imageOwnsYUVPlanes;
    image->imageOwnsYUVPlanes = AVIF_FALSE;

    uint8_t * srcAlphaPlane = image->alphaPlane;
    image->alphaPlane = NULL;
    uint32_t srcAlphaRowBytes = image->alphaRowBytes;
    image->alphaRowBytes = 0;
    const avifBool srcImageOwnsAlphaPlane = image->imageOwnsAlphaPlane;
    image->imageOwnsAlphaPlane = AVIF_FALSE;

    const uint32_t srcWidth = image->width;
    image->width = dstWidth;
    const uint32_t srcHeight = image->height;
    image->height = dstHeight;

    if (srcYUVPlanes[0] || srcAlphaPlane) {
        // A simple conservative check to avoid integer overflows in libyuv's ScalePlane() and
        // ScalePlane_12() functions.
        if (srcWidth > 16384) {
            avifDiagnosticsPrintf(diag, "avifImageScale requested invalid width scale for libyuv [%u -> %u]", srcWidth, dstWidth);
            return AVIF_FALSE;
        }
        if (srcHeight > 16384) {
            avifDiagnosticsPrintf(diag, "avifImageScale requested invalid height scale for libyuv [%u -> %u]", srcHeight, dstHeight);
            return AVIF_FALSE;
        }
    }

    if (srcYUVPlanes[0]) {
        const avifResult allocationResult = avifImageAllocatePlanes(image, AVIF_PLANES_YUV);
        if (allocationResult != AVIF_RESULT_OK) {
            avifDiagnosticsPrintf(diag, "Allocation of YUV planes failed: %s", avifResultToString(allocationResult));
            return AVIF_FALSE;
        }

        avifPixelFormatInfo formatInfo;
        avifGetPixelFormatInfo(image->yuvFormat, &formatInfo);
        const uint32_t srcUVWidth = (srcWidth + formatInfo.chromaShiftX) >> formatInfo.chromaShiftX;
        const uint32_t srcUVHeight = (srcHeight + formatInfo.chromaShiftY) >> formatInfo.chromaShiftY;
        const uint32_t dstUVWidth = (dstWidth + formatInfo.chromaShiftX) >> formatInfo.chromaShiftX;
        const uint32_t dstUVHeight = (dstHeight + formatInfo.chromaShiftY) >> formatInfo.chromaShiftY;

        for (int i = 0; i < AVIF_PLANE_COUNT_YUV; ++i) {
            if (!srcYUVPlanes[i]) {
                continue;
            }

            const uint32_t srcW = (i == AVIF_CHAN_Y) ? srcWidth : srcUVWidth;
            const uint32_t srcH = (i == AVIF_CHAN_Y) ? srcHeight : srcUVHeight;
            const uint32_t dstW = (i == AVIF_CHAN_Y) ? dstWidth : dstUVWidth;
            const uint32_t dstH = (i == AVIF_CHAN_Y) ? dstHeight : dstUVHeight;
            if (image->depth > 8) {
                uint16_t * const srcPlane = (uint16_t *)srcYUVPlanes[i];
                const uint32_t srcStride = srcYUVRowBytes[i] / 2;
                uint16_t * const dstPlane = (uint16_t *)image->yuvPlanes[i];
                const uint32_t dstStride = image->yuvRowBytes[i] / 2;
#if LIBYUV_VERSION >= 1774
                ScalePlane_12(srcPlane, srcStride, srcW, srcH, dstPlane, dstStride, dstW, dstH, AVIF_LIBYUV_FILTER_MODE);
#else
                ScalePlane_16(srcPlane, srcStride, srcW, srcH, dstPlane, dstStride, dstW, dstH, AVIF_LIBYUV_FILTER_MODE);
#endif
            } else {
                uint8_t * const srcPlane = srcYUVPlanes[i];
                const uint32_t srcStride = srcYUVRowBytes[i];
                uint8_t * const dstPlane = image->yuvPlanes[i];
                const uint32_t dstStride = image->yuvRowBytes[i];
                ScalePlane(srcPlane, srcStride, srcW, srcH, dstPlane, dstStride, dstW, dstH, AVIF_LIBYUV_FILTER_MODE);
            }

            if (srcImageOwnsYUVPlanes) {
                avifFree(srcYUVPlanes[i]);
            }
        }
    }

    if (srcAlphaPlane) {
        const avifResult allocationResult = avifImageAllocatePlanes(image, AVIF_PLANES_A);
        if (allocationResult != AVIF_RESULT_OK) {
            avifDiagnosticsPrintf(diag, "Allocation of alpha plane failed: %s", avifResultToString(allocationResult));
            return AVIF_FALSE;
        }

        if (image->depth > 8) {
            uint16_t * const srcPlane = (uint16_t *)srcAlphaPlane;
            const uint32_t srcStride = srcAlphaRowBytes / 2;
            uint16_t * const dstPlane = (uint16_t *)image->alphaPlane;
            const uint32_t dstStride = image->alphaRowBytes / 2;
#if LIBYUV_VERSION >= 1774
            ScalePlane_12(srcPlane, srcStride, srcWidth, srcHeight, dstPlane, dstStride, dstWidth, dstHeight, AVIF_LIBYUV_FILTER_MODE);
#else
            ScalePlane_16(srcPlane, srcStride, srcWidth, srcHeight, dstPlane, dstStride, dstWidth, dstHeight, AVIF_LIBYUV_FILTER_MODE);
#endif
        } else {
            uint8_t * const srcPlane = srcAlphaPlane;
            const uint32_t srcStride = srcAlphaRowBytes;
            uint8_t * const dstPlane = image->alphaPlane;
            const uint32_t dstStride = image->alphaRowBytes;
            ScalePlane(srcPlane, srcStride, srcWidth, srcHeight, dstPlane, dstStride, dstWidth, dstHeight, AVIF_LIBYUV_FILTER_MODE);
        }

        if (srcImageOwnsAlphaPlane) {
            avifFree(srcAlphaPlane);
        }
    }

    return AVIF_TRUE;
}

#endif
