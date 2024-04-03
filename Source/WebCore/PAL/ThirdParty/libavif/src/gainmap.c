// Copyright 2023 Google LLC
// SPDX-License-Identifier: BSD-2-Clause

#include "avif/internal.h"
#include <assert.h>
#include <float.h>
#include <math.h>
#include <string.h>

#if defined(AVIF_ENABLE_EXPERIMENTAL_GAIN_MAP)

avifBool avifGainMapMetadataDoubleToFractions(avifGainMapMetadata * dst, const avifGainMapMetadataDouble * src)
{
    AVIF_CHECK(dst != NULL && src != NULL);

    for (int i = 0; i < 3; ++i) {
        AVIF_CHECK(avifDoubleToSignedFraction(src->gainMapMin[i], &dst->gainMapMinN[i], &dst->gainMapMinD[i]));
        AVIF_CHECK(avifDoubleToSignedFraction(src->gainMapMax[i], &dst->gainMapMaxN[i], &dst->gainMapMaxD[i]));
        AVIF_CHECK(avifDoubleToUnsignedFraction(src->gainMapGamma[i], &dst->gainMapGammaN[i], &dst->gainMapGammaD[i]));
        AVIF_CHECK(avifDoubleToSignedFraction(src->baseOffset[i], &dst->baseOffsetN[i], &dst->baseOffsetD[i]));
        AVIF_CHECK(avifDoubleToSignedFraction(src->alternateOffset[i], &dst->alternateOffsetN[i], &dst->alternateOffsetD[i]));
    }
    AVIF_CHECK(avifDoubleToUnsignedFraction(src->baseHdrHeadroom, &dst->baseHdrHeadroomN, &dst->baseHdrHeadroomD));
    AVIF_CHECK(avifDoubleToUnsignedFraction(src->alternateHdrHeadroom, &dst->alternateHdrHeadroomN, &dst->alternateHdrHeadroomD));
    dst->backwardDirection = src->backwardDirection;
    dst->useBaseColorSpace = src->useBaseColorSpace;
    return AVIF_TRUE;
}

avifBool avifGainMapMetadataFractionsToDouble(avifGainMapMetadataDouble * dst, const avifGainMapMetadata * src)
{
    AVIF_CHECK(dst != NULL && src != NULL);

    AVIF_CHECK(src->baseHdrHeadroomD != 0);
    AVIF_CHECK(src->alternateHdrHeadroomD != 0);
    for (int i = 0; i < 3; ++i) {
        AVIF_CHECK(src->gainMapMaxD[i] != 0);
        AVIF_CHECK(src->gainMapGammaD[i] != 0);
        AVIF_CHECK(src->gainMapMinD[i] != 0);
        AVIF_CHECK(src->baseOffsetD[i] != 0);
        AVIF_CHECK(src->alternateOffsetD[i] != 0);
    }

    for (int i = 0; i < 3; ++i) {
        dst->gainMapMin[i] = (double)src->gainMapMinN[i] / src->gainMapMinD[i];
        dst->gainMapMax[i] = (double)src->gainMapMaxN[i] / src->gainMapMaxD[i];
        dst->gainMapGamma[i] = (double)src->gainMapGammaN[i] / src->gainMapGammaD[i];
        dst->baseOffset[i] = (double)src->baseOffsetN[i] / src->baseOffsetD[i];
        dst->alternateOffset[i] = (double)src->alternateOffsetN[i] / src->alternateOffsetD[i];
    }
    dst->baseHdrHeadroom = (double)src->baseHdrHeadroomN / src->baseHdrHeadroomD;
    dst->alternateHdrHeadroom = (double)src->alternateHdrHeadroomN / src->alternateHdrHeadroomD;
    dst->backwardDirection = src->backwardDirection;
    dst->useBaseColorSpace = src->useBaseColorSpace;
    return AVIF_TRUE;
}

static void avifGainMapMetadataSetDefaults(avifGainMapMetadataDouble * metadata)
{
    memset(metadata, 0, sizeof(avifGainMapMetadata));
    for (int i = 0; i < 3; ++i) {
        metadata->baseOffset[i] = 0.015625;      // 1/64
        metadata->alternateOffset[i] = 0.015625; // 1/64
        metadata->gainMapGamma[i] = 1.0;
    }
    metadata->baseHdrHeadroom = 0.0;
    metadata->alternateHdrHeadroom = 1.0;
    metadata->useBaseColorSpace = AVIF_TRUE;
}

// ---------------------------------------------------------------------------
// Apply a gain map.

// Returns a weight in [-1.0, 1.0] that represents how much the gain map should be applied.
static float avifGetGainMapWeight(float hdrHeadroom, const avifGainMapMetadataDouble * metadata)
{
    const float baseHdrHeadroom = (float)metadata->baseHdrHeadroom;
    const float alternateHdrHeadroom = (float)metadata->alternateHdrHeadroom;
    if (baseHdrHeadroom == alternateHdrHeadroom) {
        // Do not apply the gain map if the HDR headroom is the same.
        // This case is not handled in the specification and does not make practical sense.
        return 0.0f;
    }
    float w = AVIF_CLAMP((hdrHeadroom - baseHdrHeadroom) / (alternateHdrHeadroom - baseHdrHeadroom), 0.0f, 1.0f);
    if (metadata->backwardDirection) {
        w *= -1.0f;
    }
    return w;
}

// Linear interpolation between 'a' and 'b' (returns 'a' if w == 0.0f, returns 'b' if w == 1.0f).
static inline float lerp(float a, float b, float w)
{
    return (1.0f - w) * a + w * b;
}

#define SDR_WHITE_NITS 203.0f

avifResult avifRGBImageApplyGainMap(const avifRGBImage * baseImage,
                                    avifColorPrimaries baseColorPrimaries,
                                    avifTransferCharacteristics baseTransferCharacteristics,
                                    const avifGainMap * gainMap,
                                    float hdrHeadroom,
                                    avifColorPrimaries outputColorPrimaries,
                                    avifTransferCharacteristics outputTransferCharacteristics,
                                    avifRGBImage * toneMappedImage,
                                    avifContentLightLevelInformationBox * clli,
                                    avifDiagnostics * diag)
{
    avifDiagnosticsClearError(diag);

    if (hdrHeadroom < 0.0f) {
        avifDiagnosticsPrintf(diag, "hdrHeadroom should be >= 0, got %f", hdrHeadroom);
        return AVIF_RESULT_INVALID_ARGUMENT;
    }
    if (baseImage == NULL || gainMap == NULL || toneMappedImage == NULL) {
        avifDiagnosticsPrintf(diag, "NULL input image");
        return AVIF_RESULT_INVALID_ARGUMENT;
    }

    avifGainMapMetadataDouble metadata;
    if (!avifGainMapMetadataFractionsToDouble(&metadata, &gainMap->metadata)) {
        avifDiagnosticsPrintf(diag, "Invalid gain map metadata, a denominator value is zero");
        return AVIF_RESULT_INVALID_ARGUMENT;
    }
    for (int i = 0; i < 3; ++i) {
        if (metadata.gainMapGamma[i] <= 0) {
            avifDiagnosticsPrintf(diag, "Invalid gain map metadata, gamma should be strictly positive");
            return AVIF_RESULT_INVALID_ARGUMENT;
        }
    }

    const uint32_t width = baseImage->width;
    const uint32_t height = baseImage->height;

    const avifBool useBaseColorSpace = gainMap->metadata.useBaseColorSpace;
    const avifColorPrimaries gainMapMathPrimaries =
        (useBaseColorSpace || (gainMap->altColorPrimaries == AVIF_COLOR_PRIMARIES_UNSPECIFIED)) ? baseColorPrimaries
                                                                                                : gainMap->altColorPrimaries;
    const avifBool needsInputColorConversion = (baseColorPrimaries != gainMapMathPrimaries);
    const avifBool needsOutputColorConversion = (gainMapMathPrimaries != outputColorPrimaries);

    avifImage * rescaledGainMap = NULL;
    avifRGBImage rgbGainMap;
    // Basic zero-initialization for now, avifRGBImageSetDefaults() is called later on.
    memset(&rgbGainMap, 0, sizeof(rgbGainMap));

    avifResult res = AVIF_RESULT_OK;
    toneMappedImage->width = width;
    toneMappedImage->height = height;
    AVIF_CHECKRES(avifRGBImageAllocatePixels(toneMappedImage));

    // --- After this point, the function should exit with 'goto cleanup' to free allocated pixels.

    const float weight = avifGetGainMapWeight(hdrHeadroom, &metadata);

    // Early exit if the gain map does not need to be applied and the pixel format is the same.
    if (weight == 0.0f && outputTransferCharacteristics == baseTransferCharacteristics &&
        outputColorPrimaries == baseColorPrimaries && baseImage->format == toneMappedImage->format &&
        baseImage->depth == toneMappedImage->depth && baseImage->isFloat == toneMappedImage->isFloat) {
        assert(baseImage->rowBytes == toneMappedImage->rowBytes);
        assert(baseImage->height == toneMappedImage->height);
        // Copy the base image.
        memcpy(toneMappedImage->pixels, baseImage->pixels, baseImage->rowBytes * baseImage->height);
        goto cleanup;
    }

    avifRGBColorSpaceInfo baseRGBInfo;
    avifRGBColorSpaceInfo toneMappedPixelRGBInfo;
    if (!avifGetRGBColorSpaceInfo(baseImage, &baseRGBInfo) || !avifGetRGBColorSpaceInfo(toneMappedImage, &toneMappedPixelRGBInfo)) {
        avifDiagnosticsPrintf(diag, "Unsupported RGB color space");
        res = AVIF_RESULT_NOT_IMPLEMENTED;
        goto cleanup;
    }

    const avifTransferFunction gammaToLinear = avifTransferCharacteristicsGetGammaToLinearFunction(baseTransferCharacteristics);
    const avifTransferFunction linearToGamma = avifTransferCharacteristicsGetLinearToGammaFunction(outputTransferCharacteristics);

    // Early exit if the gain map does not need to be applied.
    if (weight == 0.0f) {
        const avifBool primariesDiffer = (baseColorPrimaries != outputColorPrimaries);
        double conversionCoeffs[3][3];
        if (primariesDiffer && !avifColorPrimariesComputeRGBToRGBMatrix(baseColorPrimaries, outputColorPrimaries, conversionCoeffs)) {
            avifDiagnosticsPrintf(diag, "Unsupported RGB color space conversion");
            res = AVIF_RESULT_NOT_IMPLEMENTED;
            goto cleanup;
        }
        // Just convert from one rgb format to another.
        for (uint32_t j = 0; j < height; ++j) {
            for (uint32_t i = 0; i < width; ++i) {
                float basePixelRGBA[4];
                avifGetRGBAPixel(baseImage, i, j, &baseRGBInfo, basePixelRGBA);
                if (outputTransferCharacteristics != baseTransferCharacteristics || primariesDiffer) {
                    for (int c = 0; c < 3; ++c) {
                        basePixelRGBA[c] = gammaToLinear(basePixelRGBA[c]);
                    }
                    if (primariesDiffer) {
                        avifLinearRGBConvertColorSpace(basePixelRGBA, conversionCoeffs);
                    }
                    for (int c = 0; c < 3; ++c) {
                        basePixelRGBA[c] = AVIF_CLAMP(linearToGamma(basePixelRGBA[c]), 0.0f, 1.0f);
                    }
                }
                avifSetRGBAPixel(toneMappedImage, i, j, &toneMappedPixelRGBInfo, basePixelRGBA);
            }
        }
        goto cleanup;
    }

    double inputConversionCoeffs[3][3];
    double outputConversionCoeffs[3][3];
    if (needsInputColorConversion &&
        !avifColorPrimariesComputeRGBToRGBMatrix(baseColorPrimaries, gainMapMathPrimaries, inputConversionCoeffs)) {
        avifDiagnosticsPrintf(diag, "Unsupported RGB color space conversion");
        res = AVIF_RESULT_NOT_IMPLEMENTED;
        goto cleanup;
    }
    if (needsOutputColorConversion &&
        !avifColorPrimariesComputeRGBToRGBMatrix(gainMapMathPrimaries, outputColorPrimaries, outputConversionCoeffs)) {
        avifDiagnosticsPrintf(diag, "Unsupported RGB color space conversion");
        res = AVIF_RESULT_NOT_IMPLEMENTED;
        goto cleanup;
    }

    if (gainMap->image->width != width || gainMap->image->height != height) {
        rescaledGainMap = avifImageCreateEmpty();
        const avifCropRect rect = { 0, 0, gainMap->image->width, gainMap->image->height };
        res = avifImageSetViewRect(rescaledGainMap, gainMap->image, &rect);
        if (res != AVIF_RESULT_OK) {
            goto cleanup;
        }
        res = avifImageScale(rescaledGainMap, width, height, diag);
        if (res != AVIF_RESULT_OK) {
            goto cleanup;
        }
    }
    const avifImage * const gainMapImage = (rescaledGainMap != NULL) ? rescaledGainMap : gainMap->image;

    avifRGBImageSetDefaults(&rgbGainMap, gainMapImage);
    res = avifRGBImageAllocatePixels(&rgbGainMap);
    if (res != AVIF_RESULT_OK) {
        goto cleanup;
    }
    res = avifImageYUVToRGB(gainMapImage, &rgbGainMap);
    if (res != AVIF_RESULT_OK) {
        goto cleanup;
    }

    avifRGBColorSpaceInfo gainMapRGBInfo;
    if (!avifGetRGBColorSpaceInfo(&rgbGainMap, &gainMapRGBInfo)) {
        avifDiagnosticsPrintf(diag, "Unsupported RGB color space");
        res = AVIF_RESULT_NOT_IMPLEMENTED;
        goto cleanup;
    }

    float rgbMaxLinear = 0; // Max tone mapped pixel value across R, G and B channels.
    float rgbSumLinear = 0; // Sum of max(r, g, b) for mapped pixels.
    const float gammaInv[3] = { 1.0f / (float)metadata.gainMapGamma[0],
                                1.0f / (float)metadata.gainMapGamma[1],
                                1.0f / (float)metadata.gainMapGamma[2] };
    for (uint32_t j = 0; j < height; ++j) {
        for (uint32_t i = 0; i < width; ++i) {
            float basePixelRGBA[4];
            avifGetRGBAPixel(baseImage, i, j, &baseRGBInfo, basePixelRGBA);
            float gainMapRGBA[4];
            avifGetRGBAPixel(&rgbGainMap, i, j, &gainMapRGBInfo, gainMapRGBA);

            // Apply gain map.
            float toneMappedPixelRGBA[4];
            float pixelRgbMaxLinear = 0.0f; //  = max(r, g, b) for this pixel

            for (int c = 0; c < 3; ++c) {
                basePixelRGBA[c] = gammaToLinear(basePixelRGBA[c]);
            }

            if (needsInputColorConversion) {
                // Convert basePixelRGBA to gainMapMathPrimaries.
                avifLinearRGBConvertColorSpace(basePixelRGBA, inputConversionCoeffs);
            }

            for (int c = 0; c < 3; ++c) {
                const float baseLinear = basePixelRGBA[c];
                const float gainMapValue = gainMapRGBA[c];

                // Undo gamma & affine transform; the result is in log2 space.
                const float gainMapLog2 =
                    lerp((float)metadata.gainMapMin[c], (float)metadata.gainMapMax[c], powf(gainMapValue, gammaInv[c]));
                const float toneMappedLinear = (baseLinear + (float)metadata.baseOffset[c]) * exp2f(gainMapLog2 * weight) -
                                               (float)metadata.alternateOffset[c];

                if (toneMappedLinear > rgbMaxLinear) {
                    rgbMaxLinear = toneMappedLinear;
                }
                if (toneMappedLinear > pixelRgbMaxLinear) {
                    pixelRgbMaxLinear = toneMappedLinear;
                }

                toneMappedPixelRGBA[c] = toneMappedLinear;
            }

            if (needsOutputColorConversion) {
                // Convert toneMappedPixelRGBA to outputColorPrimaries.
                avifLinearRGBConvertColorSpace(toneMappedPixelRGBA, outputConversionCoeffs);
            }

            for (int c = 0; c < 3; ++c) {
                toneMappedPixelRGBA[c] = AVIF_CLAMP(linearToGamma(toneMappedPixelRGBA[c]), 0.0f, 1.0f);
            }

            toneMappedPixelRGBA[3] = basePixelRGBA[3]; // Alpha is unaffected by tone mapping.
            rgbSumLinear += pixelRgbMaxLinear;
            avifSetRGBAPixel(toneMappedImage, i, j, &toneMappedPixelRGBInfo, toneMappedPixelRGBA);
        }
    }
    if (clli != NULL) {
        // For exact CLLI value definitions, see ISO/IEC 23008-2 section D.3.35
        // at https://standards.iso.org/ittf/PubliclyAvailableStandards/index.html
        // See also discussion in https://github.com/AOMediaCodec/libavif/issues/1727

        // Convert extended SDR (where 1.0 is SDR white) to nits.
        clli->maxCLL = (uint16_t)AVIF_CLAMP(avifRoundf(rgbMaxLinear * SDR_WHITE_NITS), 0.0f, (float)UINT16_MAX);
        const float rgbAverageLinear = rgbSumLinear / (width * height);
        clli->maxPALL = (uint16_t)AVIF_CLAMP(avifRoundf(rgbAverageLinear * SDR_WHITE_NITS), 0.0f, (float)UINT16_MAX);
    }

cleanup:
    avifRGBImageFreePixels(&rgbGainMap);
    if (rescaledGainMap != NULL) {
        avifImageDestroy(rescaledGainMap);
    }

    return res;
}

avifResult avifImageApplyGainMap(const avifImage * baseImage,
                                 const avifGainMap * gainMap,
                                 float hdrHeadroom,
                                 avifColorPrimaries outputColorPrimaries,
                                 avifTransferCharacteristics outputTransferCharacteristics,
                                 avifRGBImage * toneMappedImage,
                                 avifContentLightLevelInformationBox * clli,
                                 avifDiagnostics * diag)
{
    avifDiagnosticsClearError(diag);

    if (baseImage->icc.size > 0 || gainMap->altICC.size > 0) {
        avifDiagnosticsPrintf(diag, "Tone mapping for images with ICC profiles is not supported");
        return AVIF_RESULT_NOT_IMPLEMENTED;
    }

    avifRGBImage baseImageRgb;
    avifRGBImageSetDefaults(&baseImageRgb, baseImage);
    AVIF_CHECKRES(avifRGBImageAllocatePixels(&baseImageRgb));
    avifResult res = avifImageYUVToRGB(baseImage, &baseImageRgb);
    if (res != AVIF_RESULT_OK) {
        goto cleanup;
    }

    res = avifRGBImageApplyGainMap(&baseImageRgb,
                                   baseImage->colorPrimaries,
                                   baseImage->transferCharacteristics,
                                   gainMap,
                                   hdrHeadroom,
                                   outputColorPrimaries,
                                   outputTransferCharacteristics,
                                   toneMappedImage,
                                   clli,
                                   diag);

cleanup:
    avifRGBImageFreePixels(&baseImageRgb);

    return res;
}

// ---------------------------------------------------------------------------
// Create a gain map.

// Returns the index of the histogram bucket for a given value, for a histogram with 'numBuckets' buckets,
// and values ranging in [bucketMin, bucketMax] (values outside of the range are added to the first/last buckets).
static int avifValueToBucketIdx(float v, float bucketMin, float bucketMax, int numBuckets)
{
    v = AVIF_CLAMP(v, bucketMin, bucketMax);
    return AVIF_MIN((int)avifRoundf((v - bucketMin) / (bucketMax - bucketMin) * numBuckets), numBuckets - 1);
}
// Returns the lower end of the value range belonging to the given histogram bucket.
static float avifBucketIdxToValue(int idx, float bucketMin, float bucketMax, int numBuckets)
{
    return idx * (bucketMax - bucketMin) / numBuckets + bucketMin;
}

avifResult avifFindMinMaxWithoutOutliers(const float * gainMapF, int numPixels, float * rangeMin, float * rangeMax)
{
    const float bucketSize = 0.01f;        // Size of one bucket. Empirical value.
    const float maxOutliersRatio = 0.001f; // 0.1%
    const int maxOutliersOnEachSide = (int)avifRoundf(numPixels * maxOutliersRatio / 2.0f);

    float min = gainMapF[0];
    float max = gainMapF[0];
    for (int i = 0; i < numPixels; ++i) {
        min = AVIF_MIN(min, gainMapF[i]);
        max = AVIF_MAX(max, gainMapF[i]);
    }

    *rangeMin = min;
    *rangeMax = max;
    if ((max - min) <= (bucketSize * 2) || maxOutliersOnEachSide == 0) {
        return AVIF_RESULT_OK;
    }

    const int maxNumBuckets = 10000;
    const int numBuckets = AVIF_MIN((int)ceilf((max - min) / bucketSize), maxNumBuckets);
    int * histogram = avifAlloc(sizeof(int) * numBuckets);
    if (histogram == NULL) {
        return AVIF_RESULT_OUT_OF_MEMORY;
    }
    memset(histogram, 0, sizeof(int) * numBuckets);
    for (int i = 0; i < numPixels; ++i) {
        ++(histogram[avifValueToBucketIdx(gainMapF[i], min, max, numBuckets)]);
    }

    int leftOutliers = 0;
    for (int i = 0; i < numBuckets; ++i) {
        leftOutliers += histogram[i];
        if (leftOutliers > maxOutliersOnEachSide) {
            break;
        }
        if (histogram[i] == 0) {
            // +1 to get the higher end of the bucket.
            *rangeMin = avifBucketIdxToValue(i + 1, min, max, numBuckets);
        }
    }

    int rightOutliers = 0;
    for (int i = numBuckets - 1; i >= 0; --i) {
        rightOutliers += histogram[i];
        if (rightOutliers > maxOutliersOnEachSide) {
            break;
        }
        if (histogram[i] == 0) {
            *rangeMax = avifBucketIdxToValue(i, min, max, numBuckets);
        }
    }

    avifFree(histogram);
    return AVIF_RESULT_OK;
}

static const float kEpsilon = 1e-10f;

// Decides which of 'basePrimaries' or 'altPrimaries' should be used for doing gain map math when creating a gain map.
// The other image (base or alternate) will be converted to this color space before computing
// the ratio between the two images.
// If a pixel color is outside of the target color space, some of the converted channel values will be negative.
// This should be avoided, as the negative values must either be clamped or offset before computing the log2()
// (since log2 only works on > 0 values). But a large offset causes artefacts when partially applying the gain map.
// Therefore we want to do gain map math in the larger of the two color spaces.
static avifResult avifChooseColorSpaceForGainMapMath(avifColorPrimaries basePrimaries,
                                                     avifColorPrimaries altPrimaries,
                                                     avifColorPrimaries * gainMapMathColorSpace)
{
    if (basePrimaries == altPrimaries) {
        *gainMapMathColorSpace = basePrimaries;
        return AVIF_RESULT_OK;
    }
    // Color convert pure red, pure green and pure blue in turn and see if they result in negative values.
    float rgba[4] = { 0 };
    double baseToAltCoeffs[3][3];
    double altToBaseCoeffs[3][3];
    if (!avifColorPrimariesComputeRGBToRGBMatrix(basePrimaries, altPrimaries, baseToAltCoeffs) ||
        !avifColorPrimariesComputeRGBToRGBMatrix(altPrimaries, basePrimaries, altToBaseCoeffs)) {
        return AVIF_RESULT_NOT_IMPLEMENTED;
    }

    float baseColorspaceChannelMin = 0;
    float altColorspaceChannelMin = 0;
    for (int c = 0; c < 3; ++c) {
        rgba[0] = rgba[1] = rgba[2] = 0;
        rgba[c] = 1.0f;
        avifLinearRGBConvertColorSpace(rgba, altToBaseCoeffs);
        for (int i = 0; i < 3; ++i) {
            baseColorspaceChannelMin = AVIF_MIN(baseColorspaceChannelMin, rgba[i]);
        }
        rgba[0] = rgba[1] = rgba[2] = 0;
        rgba[c] = 1.0f;
        avifLinearRGBConvertColorSpace(rgba, baseToAltCoeffs);
        for (int i = 0; i < 3; ++i) {
            altColorspaceChannelMin = AVIF_MIN(altColorspaceChannelMin, rgba[i]);
        }
    }
    // Pick the colorspace that has the largest min value (which is more or less the largest color space).
    *gainMapMathColorSpace = (altColorspaceChannelMin <= baseColorspaceChannelMin) ? basePrimaries : altPrimaries;
    return AVIF_RESULT_OK;
}

avifResult avifRGBImageComputeGainMap(const avifRGBImage * baseRgbImage,
                                      avifColorPrimaries baseColorPrimaries,
                                      avifTransferCharacteristics baseTransferCharacteristics,
                                      const avifRGBImage * altRgbImage,
                                      avifColorPrimaries altColorPrimaries,
                                      avifTransferCharacteristics altTransferCharacteristics,
                                      avifGainMap * gainMap,
                                      avifDiagnostics * diag)
{
    avifDiagnosticsClearError(diag);

    AVIF_CHECKERR(baseRgbImage != NULL && altRgbImage != NULL && gainMap != NULL && gainMap->image != NULL, AVIF_RESULT_INVALID_ARGUMENT);
    if (baseRgbImage->width != altRgbImage->width || baseRgbImage->height != altRgbImage->height) {
        avifDiagnosticsPrintf(diag, "Both images should have the same dimensions");
        return AVIF_RESULT_INVALID_ARGUMENT;
    }
    if (gainMap->image->width == 0 || gainMap->image->height == 0 || gainMap->image->depth == 0 ||
        gainMap->image->yuvFormat <= AVIF_PIXEL_FORMAT_NONE || gainMap->image->yuvFormat >= AVIF_PIXEL_FORMAT_COUNT) {
        avifDiagnosticsPrintf(diag, "gainMap->image should be non null with desired width, height, depth and yuvFormat set");
        return AVIF_RESULT_INVALID_ARGUMENT;
    }
    const avifBool colorSpacesDiffer = (baseColorPrimaries != altColorPrimaries);
    avifColorPrimaries gainMapMathPrimaries;
    AVIF_CHECKRES(avifChooseColorSpaceForGainMapMath(baseColorPrimaries, altColorPrimaries, &gainMapMathPrimaries));
    const avifBool useBaseColorSpace = (gainMapMathPrimaries == baseColorPrimaries);

    const int width = baseRgbImage->width;
    const int height = baseRgbImage->height;

    avifRGBColorSpaceInfo baseRGBInfo;
    avifRGBColorSpaceInfo altRGBInfo;
    if (!avifGetRGBColorSpaceInfo(baseRgbImage, &baseRGBInfo) || !avifGetRGBColorSpaceInfo(altRgbImage, &altRGBInfo)) {
        avifDiagnosticsPrintf(diag, "Unsupported RGB color space");
        return AVIF_RESULT_NOT_IMPLEMENTED;
    }

    float * gainMapF[3] = { 0 }; // Temporary buffers for the gain map as floating point values, one per RGB channel.
    avifRGBImage gainMapRGB;
    memset(&gainMapRGB, 0, sizeof(gainMapRGB));
    avifImage * gainMapImage = gainMap->image;

    avifResult res = AVIF_RESULT_OK;
    // --- After this point, the function should exit with 'goto cleanup' to free allocated resources.

    const avifBool singleChannel = (gainMap->image->yuvFormat == AVIF_PIXEL_FORMAT_YUV400);
    const int numGainMapChannels = singleChannel ? 1 : 3;
    for (int c = 0; c < numGainMapChannels; ++c) {
        gainMapF[c] = avifAlloc(width * height * sizeof(float));
        if (gainMapF[c] == NULL) {
            res = AVIF_RESULT_OUT_OF_MEMORY;
            goto cleanup;
        }
    }

    avifGainMapMetadataDouble gainMapMetadata;
    avifGainMapMetadataSetDefaults(&gainMapMetadata);

    float (*baseGammaToLinear)(float) = avifTransferCharacteristicsGetGammaToLinearFunction(baseTransferCharacteristics);
    float (*altGammaToLinear)(float) = avifTransferCharacteristicsGetGammaToLinearFunction(altTransferCharacteristics);
    float yCoeffs[3];
    avifColorPrimariesComputeYCoeffs(gainMapMathPrimaries, yCoeffs);

    double rgbConversionCoeffs[3][3];
    if (colorSpacesDiffer) {
        if (useBaseColorSpace) {
            if (!avifColorPrimariesComputeRGBToRGBMatrix(altColorPrimaries, baseColorPrimaries, rgbConversionCoeffs)) {
                avifDiagnosticsPrintf(diag, "Unsupported RGB color space conversion");
                res = AVIF_RESULT_NOT_IMPLEMENTED;
                goto cleanup;
            }
        } else {
            if (!avifColorPrimariesComputeRGBToRGBMatrix(baseColorPrimaries, altColorPrimaries, rgbConversionCoeffs)) {
                avifDiagnosticsPrintf(diag, "Unsupported RGB color space conversion");
                res = AVIF_RESULT_NOT_IMPLEMENTED;
                goto cleanup;
            }
        }
    }

    // If we are converting from one colorspace to another, some RGB values may be negative and an offset must be added to
    // avoid clamping (although the choice of color space to do the gain map computation with
    // avifChooseColorSpaceForGainMapMath() should mostly avoid this).
    if (colorSpacesDiffer) {
        // Color convert pure red, pure green and pure blue in turn and see if they result in negative values.
        float rgba[4] = { 0 };
        float channelMin[3] = { 0 };
        for (int j = 0; j < height; ++j) {
            for (int i = 0; i < width; ++i) {
                avifGetRGBAPixel(useBaseColorSpace ? altRgbImage : baseRgbImage, i, j, useBaseColorSpace ? &altRGBInfo : &baseRGBInfo, rgba);

                // Convert to linear.
                for (int c = 0; c < 3; ++c) {
                    if (useBaseColorSpace) {
                        rgba[c] = altGammaToLinear(rgba[c]);
                    } else {
                        rgba[c] = baseGammaToLinear(rgba[c]);
                    }
                }
                avifLinearRGBConvertColorSpace(rgba, rgbConversionCoeffs);
                for (int c = 0; c < 3; ++c) {
                    channelMin[c] = AVIF_MIN(channelMin[c], rgba[c]);
                }
            }
        }

        for (int c = 0; c < 3; ++c) {
            // Large offsets cause artefacts when partially applying the gain map, so set a max (empirical) offset value.
            // If the offset is clamped, some gain map values will get clamped as well.
            const float maxOffset = 0.1f;
            if (channelMin[c] < -kEpsilon) {
                // Increase the offset to avoid negative values.
                if (useBaseColorSpace) {
                    gainMapMetadata.alternateOffset[c] = AVIF_MIN(gainMapMetadata.alternateOffset[c] - channelMin[c], maxOffset);
                } else {
                    gainMapMetadata.baseOffset[c] = AVIF_MIN(gainMapMetadata.baseOffset[c] - channelMin[c], maxOffset);
                }
            }
        }
    }

    // Compute raw gain map values.
    float baseMax = 1.0f;
    float altMax = 1.0f;
    for (int j = 0; j < height; ++j) {
        for (int i = 0; i < width; ++i) {
            float baseRGBA[4];
            avifGetRGBAPixel(baseRgbImage, i, j, &baseRGBInfo, baseRGBA);
            float altRGBA[4];
            avifGetRGBAPixel(altRgbImage, i, j, &altRGBInfo, altRGBA);

            // Convert to linear.
            for (int c = 0; c < 3; ++c) {
                baseRGBA[c] = baseGammaToLinear(baseRGBA[c]);
                altRGBA[c] = altGammaToLinear(altRGBA[c]);
            }

            if (colorSpacesDiffer) {
                if (useBaseColorSpace) {
                    // convert altRGBA to baseRGBA's color space
                    avifLinearRGBConvertColorSpace(altRGBA, rgbConversionCoeffs);
                } else {
                    // convert baseRGBA to altRGBA's color space
                    avifLinearRGBConvertColorSpace(baseRGBA, rgbConversionCoeffs);
                }
            }

            for (int c = 0; c < numGainMapChannels; ++c) {
                float base = baseRGBA[c];
                float alt = altRGBA[c];
                if (singleChannel) {
                    // Convert to grayscale.
                    base = yCoeffs[0] * baseRGBA[0] + yCoeffs[1] * baseRGBA[1] + yCoeffs[2] * baseRGBA[2];
                    alt = yCoeffs[0] * altRGBA[0] + yCoeffs[1] * altRGBA[1] + yCoeffs[2] * altRGBA[2];
                }
                if (base > baseMax) {
                    baseMax = base;
                }
                if (alt > altMax) {
                    altMax = alt;
                }
                const float ratio = (alt + (float)gainMapMetadata.alternateOffset[c]) / (base + (float)gainMapMetadata.baseOffset[c]);
                const float ratioLog2 = log2f(AVIF_MAX(ratio, kEpsilon));
                gainMapF[c][j * width + i] = ratioLog2;
            }
        }
    }

    // Find approximate min/max for each channel, discarding outliers.
    float gainMapMinLog2[3] = { 0.0f, 0.0f, 0.0f };
    float gainMapMaxLog2[3] = { 0.0f, 0.0f, 0.0f };
    for (int c = 0; c < numGainMapChannels; ++c) {
        res = avifFindMinMaxWithoutOutliers(gainMapF[c], width * height, &gainMapMinLog2[c], &gainMapMaxLog2[c]);
        if (res != AVIF_RESULT_OK) {
            goto cleanup;
        }
    }

    // Fill in the gain map's metadata.
    for (int c = 0; c < 3; ++c) {
        gainMapMetadata.gainMapMin[c] = gainMapMinLog2[singleChannel ? 0 : c];
        gainMapMetadata.gainMapMax[c] = gainMapMaxLog2[singleChannel ? 0 : c];
        gainMapMetadata.baseHdrHeadroom = log2f(AVIF_MAX(baseMax, kEpsilon));
        gainMapMetadata.alternateHdrHeadroom = log2f(AVIF_MAX(altMax, kEpsilon));
        // baseOffset, alternateOffset and gainMapGamma are all left to their default values.
        // They could be tweaked based on the images to optimize quality/compression.
    }
    gainMapMetadata.useBaseColorSpace = useBaseColorSpace;
    if (!avifGainMapMetadataDoubleToFractions(&gainMap->metadata, &gainMapMetadata)) {
        res = AVIF_RESULT_UNKNOWN_ERROR;
        goto cleanup;
    }

    // Scale the gain map values to map [min, max] range to [0, 1].
    for (int c = 0; c < numGainMapChannels; ++c) {
        const float range = gainMapMaxLog2[c] - gainMapMinLog2[c];
        if (range <= 0.0f) {
            continue;
        }

        for (int j = 0; j < height; ++j) {
            for (int i = 0; i < width; ++i) {
                // Remap [min; max] range to [0; 1]
                const float v = AVIF_CLAMP(gainMapF[c][j * width + i], gainMapMinLog2[c], gainMapMaxLog2[c]);
                gainMapF[c][j * width + i] = powf((v - gainMapMinLog2[c]) / range, (float)gainMapMetadata.gainMapGamma[c]);
            }
        }
    }

    // Convert the gain map to YUV.
    const uint32_t requestedWidth = gainMapImage->width;
    const uint32_t requestedHeight = gainMapImage->height;
    gainMapImage->width = width;
    gainMapImage->height = height;

    avifImageFreePlanes(gainMapImage, AVIF_PLANES_ALL); // Free planes in case they were already allocated.
    res = avifImageAllocatePlanes(gainMapImage, AVIF_PLANES_YUV);
    if (res != AVIF_RESULT_OK) {
        goto cleanup;
    }

    avifRGBImageSetDefaults(&gainMapRGB, gainMapImage);
    res = avifRGBImageAllocatePixels(&gainMapRGB);
    if (res != AVIF_RESULT_OK) {
        goto cleanup;
    }

    avifRGBColorSpaceInfo gainMapRGBInfo;
    if (!avifGetRGBColorSpaceInfo(&gainMapRGB, &gainMapRGBInfo)) {
        avifDiagnosticsPrintf(diag, "Unsupported RGB color space");
        return AVIF_RESULT_NOT_IMPLEMENTED;
    }
    for (int j = 0; j < height; ++j) {
        for (int i = 0; i < width; ++i) {
            const int offset = j * width + i;
            const float r = gainMapF[0][offset];
            const float g = singleChannel ? r : gainMapF[1][offset];
            const float b = singleChannel ? r : gainMapF[2][offset];
            const float rgbaPixel[4] = { r, g, b, 1.0f };
            avifSetRGBAPixel(&gainMapRGB, i, j, &gainMapRGBInfo, rgbaPixel);
        }
    }

    res = avifImageRGBToYUV(gainMapImage, &gainMapRGB);
    if (res != AVIF_RESULT_OK) {
        goto cleanup;
    }

    // Scale down the gain map if requested.
    // Another way would be to scale the source images, but it seems to perform worse.
    if (requestedWidth != gainMapImage->width || requestedHeight != gainMapImage->height) {
        AVIF_CHECKRES(avifImageScale(gainMap->image, requestedWidth, requestedHeight, diag));
    }

cleanup:
    for (int c = 0; c < 3; ++c) {
        avifFree(gainMapF[c]);
    }
    avifRGBImageFreePixels(&gainMapRGB);
    if (res != AVIF_RESULT_OK) {
        avifImageFreePlanes(gainMapImage, AVIF_PLANES_ALL);
    }

    return res;
}

avifResult avifImageComputeGainMap(const avifImage * baseImage, const avifImage * altImage, avifGainMap * gainMap, avifDiagnostics * diag)
{
    avifDiagnosticsClearError(diag);

    if (baseImage == NULL || altImage == NULL || gainMap == NULL) {
        return AVIF_RESULT_INVALID_ARGUMENT;
    }
    if (baseImage->icc.size > 0 || altImage->icc.size > 0) {
        avifDiagnosticsPrintf(diag, "Computing gain maps for images with ICC profiles is not supported");
        return AVIF_RESULT_NOT_IMPLEMENTED;
    }
    if (baseImage->width != altImage->width || baseImage->height != altImage->height) {
        avifDiagnosticsPrintf(diag,
                              "Image dimensions don't match, got %dx%d and %dx%d",
                              baseImage->width,
                              baseImage->height,
                              altImage->width,
                              altImage->height);
        return AVIF_RESULT_INVALID_ARGUMENT;
    }

    avifResult res = AVIF_RESULT_OK;

    avifRGBImage baseImageRgb;
    avifRGBImageSetDefaults(&baseImageRgb, baseImage);
    avifRGBImage altImageRgb;
    avifRGBImageSetDefaults(&altImageRgb, altImage);

    AVIF_CHECKRES(avifRGBImageAllocatePixels(&baseImageRgb));
    // --- After this point, the function should exit with 'goto cleanup' to free allocated resources.

    res = avifImageYUVToRGB(baseImage, &baseImageRgb);
    if (res != AVIF_RESULT_OK) {
        goto cleanup;
    }
    res = avifRGBImageAllocatePixels(&altImageRgb);
    if (res != AVIF_RESULT_OK) {
        goto cleanup;
    }
    res = avifImageYUVToRGB(altImage, &altImageRgb);
    if (res != AVIF_RESULT_OK) {
        goto cleanup;
    }

    res = avifRGBImageComputeGainMap(&baseImageRgb,
                                     baseImage->colorPrimaries,
                                     baseImage->transferCharacteristics,
                                     &altImageRgb,
                                     altImage->colorPrimaries,
                                     altImage->transferCharacteristics,
                                     gainMap,
                                     diag);

    if (res != AVIF_RESULT_OK) {
        goto cleanup;
    }

    AVIF_CHECKRES(avifRWDataSet(&gainMap->altICC, altImage->icc.data, altImage->icc.size));
    gainMap->altColorPrimaries = altImage->colorPrimaries;
    gainMap->altTransferCharacteristics = altImage->transferCharacteristics;
    gainMap->altMatrixCoefficients = altImage->matrixCoefficients;
    gainMap->altDepth = altImage->depth;
    gainMap->altPlaneCount = (altImage->yuvFormat == AVIF_PIXEL_FORMAT_YUV400) ? 1 : 3;
    gainMap->altCLLI = altImage->clli;

cleanup:
    avifRGBImageFreePixels(&baseImageRgb);
    avifRGBImageFreePixels(&altImageRgb);
    return res;
}

#endif // AVIF_ENABLE_EXPERIMENTAL_GAIN_MAP
