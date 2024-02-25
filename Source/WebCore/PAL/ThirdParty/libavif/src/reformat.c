// Copyright 2019 Joe Drago. All rights reserved.
// SPDX-License-Identifier: BSD-2-Clause

#include "avif/internal.h"

#include <assert.h>
#include <stdint.h>
#include <string.h>

#if defined(_WIN32)
#include <process.h>
#include <windows.h>
#else
#include <pthread.h>
#endif

struct YUVBlock
{
    float y;
    float u;
    float v;
};

avifBool avifGetRGBColorSpaceInfo(const avifRGBImage * rgb, avifRGBColorSpaceInfo * info)
{
    AVIF_CHECK(rgb->depth == 8 || rgb->depth == 10 || rgb->depth == 12 || rgb->depth == 16);
    if (rgb->isFloat) {
        AVIF_CHECK(rgb->depth == 16);
    }
    if (rgb->format == AVIF_RGB_FORMAT_RGB_565) {
        AVIF_CHECK(rgb->depth == 8);
    }
    // Cast to silence "comparison of unsigned expression is always true" warning.
    AVIF_CHECK((int)rgb->format >= AVIF_RGB_FORMAT_RGB && rgb->format < AVIF_RGB_FORMAT_COUNT);

    info->channelBytes = (rgb->depth > 8) ? 2 : 1;
    info->pixelBytes = avifRGBImagePixelSize(rgb);

    switch (rgb->format) {
        case AVIF_RGB_FORMAT_RGB:
            info->offsetBytesR = info->channelBytes * 0;
            info->offsetBytesG = info->channelBytes * 1;
            info->offsetBytesB = info->channelBytes * 2;
            info->offsetBytesA = 0;
            break;
        case AVIF_RGB_FORMAT_RGBA:
            info->offsetBytesR = info->channelBytes * 0;
            info->offsetBytesG = info->channelBytes * 1;
            info->offsetBytesB = info->channelBytes * 2;
            info->offsetBytesA = info->channelBytes * 3;
            break;
        case AVIF_RGB_FORMAT_ARGB:
            info->offsetBytesA = info->channelBytes * 0;
            info->offsetBytesR = info->channelBytes * 1;
            info->offsetBytesG = info->channelBytes * 2;
            info->offsetBytesB = info->channelBytes * 3;
            break;
        case AVIF_RGB_FORMAT_BGR:
            info->offsetBytesB = info->channelBytes * 0;
            info->offsetBytesG = info->channelBytes * 1;
            info->offsetBytesR = info->channelBytes * 2;
            info->offsetBytesA = 0;
            break;
        case AVIF_RGB_FORMAT_BGRA:
            info->offsetBytesB = info->channelBytes * 0;
            info->offsetBytesG = info->channelBytes * 1;
            info->offsetBytesR = info->channelBytes * 2;
            info->offsetBytesA = info->channelBytes * 3;
            break;
        case AVIF_RGB_FORMAT_ABGR:
            info->offsetBytesA = info->channelBytes * 0;
            info->offsetBytesB = info->channelBytes * 1;
            info->offsetBytesG = info->channelBytes * 2;
            info->offsetBytesR = info->channelBytes * 3;
            break;
        case AVIF_RGB_FORMAT_RGB_565:
            // Since RGB_565 consists of two bytes per RGB pixel, we simply use
            // the pointer to the red channel to populate the entire pixel value
            // as a uint16_t. As a result only offsetBytesR is used and the
            // other offsets are unused.
            info->offsetBytesR = 0;
            info->offsetBytesG = 0;
            info->offsetBytesB = 0;
            info->offsetBytesA = 0;
            break;

        case AVIF_RGB_FORMAT_COUNT:
            return AVIF_FALSE;
    }

    info->maxChannel = (1 << rgb->depth) - 1;
    info->maxChannelF = (float)info->maxChannel;

    return AVIF_TRUE;
}

avifBool avifGetYUVColorSpaceInfo(const avifImage * image, avifYUVColorSpaceInfo * info)
{
#if defined(AVIF_ENABLE_EXPERIMENTAL_YCGCO_R)
    const avifBool useYCgCo = (image->matrixCoefficients == AVIF_MATRIX_COEFFICIENTS_YCGCO_RE) ||
                              (image->matrixCoefficients == AVIF_MATRIX_COEFFICIENTS_YCGCO_RO);
#else
    const avifBool useYCgCo = AVIF_FALSE;
#endif

    AVIF_CHECK(image->depth == 8 || image->depth == 10 || image->depth == 12 || image->depth == 16);
    AVIF_CHECK(image->yuvFormat >= AVIF_PIXEL_FORMAT_YUV444 && image->yuvFormat < AVIF_PIXEL_FORMAT_COUNT);
    AVIF_CHECK(image->yuvRange == AVIF_RANGE_LIMITED || image->yuvRange == AVIF_RANGE_FULL);

    // These matrix coefficients values are currently unsupported. Revise this list as more support is added.
    //
    // YCgCo performs limited-full range adjustment on R,G,B but the current implementation performs range adjustment
    // on Y,U,V. So YCgCo with limited range is unsupported.
    if ((image->matrixCoefficients == 3 /* CICP reserved */) ||
        ((image->matrixCoefficients == AVIF_MATRIX_COEFFICIENTS_YCGCO || useYCgCo) && (image->yuvRange == AVIF_RANGE_LIMITED)) ||
        (image->matrixCoefficients == AVIF_MATRIX_COEFFICIENTS_BT2020_CL) ||
        (image->matrixCoefficients == AVIF_MATRIX_COEFFICIENTS_SMPTE2085) ||
        (image->matrixCoefficients == AVIF_MATRIX_COEFFICIENTS_CHROMA_DERIVED_CL) ||
        (image->matrixCoefficients == AVIF_MATRIX_COEFFICIENTS_ICTCP) || (image->matrixCoefficients >= AVIF_MATRIX_COEFFICIENTS_LAST)) {
        return AVIF_FALSE;
    }

    if ((image->matrixCoefficients == AVIF_MATRIX_COEFFICIENTS_IDENTITY) && (image->yuvFormat != AVIF_PIXEL_FORMAT_YUV444) &&
        (image->yuvFormat != AVIF_PIXEL_FORMAT_YUV400)) {
        return AVIF_FALSE;
    }

    avifGetPixelFormatInfo(image->yuvFormat, &info->formatInfo);
    avifCalcYUVCoefficients(image, &info->kr, &info->kg, &info->kb);

    info->channelBytes = (image->depth > 8) ? 2 : 1;

    info->depth = image->depth;
    info->range = image->yuvRange;
    info->maxChannel = (1 << image->depth) - 1;
    info->biasY = (info->range == AVIF_RANGE_LIMITED) ? (float)(16 << (info->depth - 8)) : 0.0f;
    info->biasUV = (float)(1 << (info->depth - 1));
    info->rangeY = (float)((info->range == AVIF_RANGE_LIMITED) ? (219 << (info->depth - 8)) : info->maxChannel);
    info->rangeUV = (float)((info->range == AVIF_RANGE_LIMITED) ? (224 << (info->depth - 8)) : info->maxChannel);

    return AVIF_TRUE;
}

static avifBool avifPrepareReformatState(const avifImage * image, const avifRGBImage * rgb, avifReformatState * state)
{
#if defined(AVIF_ENABLE_EXPERIMENTAL_YCGCO_R)
    const avifBool useYCgCoRe = (image->matrixCoefficients == AVIF_MATRIX_COEFFICIENTS_YCGCO_RE);
    const avifBool useYCgCoRo = (image->matrixCoefficients == AVIF_MATRIX_COEFFICIENTS_YCGCO_RO);
#else
    const avifBool useYCgCoRe = AVIF_FALSE;
    const avifBool useYCgCoRo = AVIF_FALSE;
#endif
    if (useYCgCoRe || useYCgCoRo) {
        const int bitOffset = (useYCgCoRe) ? 2 : 1;
        if (image->depth - bitOffset != rgb->depth) {
            return AVIF_FALSE;
        }
    }

    AVIF_CHECK(avifGetRGBColorSpaceInfo(rgb, &state->rgb));
    AVIF_CHECK(avifGetYUVColorSpaceInfo(image, &state->yuv));

    state->yuv.mode = AVIF_REFORMAT_MODE_YUV_COEFFICIENTS;

    if (image->matrixCoefficients == AVIF_MATRIX_COEFFICIENTS_IDENTITY) {
        state->yuv.mode = AVIF_REFORMAT_MODE_IDENTITY;
    } else if (image->matrixCoefficients == AVIF_MATRIX_COEFFICIENTS_YCGCO) {
        state->yuv.mode = AVIF_REFORMAT_MODE_YCGCO;
#if defined(AVIF_ENABLE_EXPERIMENTAL_YCGCO_R)
    } else if (useYCgCoRe) {
        state->yuv.mode = AVIF_REFORMAT_MODE_YCGCO_RE;
    } else if (useYCgCoRo) {
        state->yuv.mode = AVIF_REFORMAT_MODE_YCGCO_RO;
#endif
    }

    if (state->yuv.mode != AVIF_REFORMAT_MODE_YUV_COEFFICIENTS) {
        state->yuv.kr = 0.0f;
        state->yuv.kg = 0.0f;
        state->yuv.kb = 0.0f;
    }

    return AVIF_TRUE;
}

// Formulas 20-31 from https://www.itu.int/rec/T-REC-H.273-201612-I/en
static int avifYUVColorSpaceInfoYToUNorm(avifYUVColorSpaceInfo * info, float v)
{
    int unorm = (int)avifRoundf(v * info->rangeY + info->biasY);
    return AVIF_CLAMP(unorm, 0, info->maxChannel);
}

static int avifYUVColorSpaceInfoUVToUNorm(avifYUVColorSpaceInfo * info, float v)
{
    int unorm;

    // YCgCo performs limited-full range adjustment on R,G,B but the current implementation performs range adjustment
    // on Y,U,V. So YCgCo with limited range is unsupported.
#if defined(AVIF_ENABLE_EXPERIMENTAL_YCGCO_R)
    assert((info->mode != AVIF_REFORMAT_MODE_YCGCO && info->mode != AVIF_REFORMAT_MODE_YCGCO_RE && info->mode != AVIF_REFORMAT_MODE_YCGCO_RO) ||
           (info->range == AVIF_RANGE_FULL));
#else
    assert((info->mode != AVIF_REFORMAT_MODE_YCGCO) || (info->range == AVIF_RANGE_FULL));
#endif

    if (info->mode == AVIF_REFORMAT_MODE_IDENTITY) {
        unorm = (int)avifRoundf(v * info->rangeY + info->biasY);
    } else {
        unorm = (int)avifRoundf(v * info->rangeUV + info->biasUV);
    }

    return AVIF_CLAMP(unorm, 0, info->maxChannel);
}

avifResult avifImageRGBToYUV(avifImage * image, const avifRGBImage * rgb)
{
    if (!rgb->pixels || rgb->format == AVIF_RGB_FORMAT_RGB_565) {
        return AVIF_RESULT_REFORMAT_FAILED;
    }

    avifReformatState state;
    if (!avifPrepareReformatState(image, rgb, &state)) {
        return AVIF_RESULT_REFORMAT_FAILED;
    }

    if (rgb->isFloat) {
        return AVIF_RESULT_NOT_IMPLEMENTED;
    }

    const avifBool hasAlpha = avifRGBFormatHasAlpha(rgb->format) && !rgb->ignoreAlpha;
    avifResult allocationResult = avifImageAllocatePlanes(image, hasAlpha ? AVIF_PLANES_ALL : AVIF_PLANES_YUV);
    if (allocationResult != AVIF_RESULT_OK) {
        return allocationResult;
    }

    avifAlphaMultiplyMode alphaMode = AVIF_ALPHA_MULTIPLY_MODE_NO_OP;
    if (hasAlpha) {
        if (!rgb->alphaPremultiplied && image->alphaPremultiplied) {
            alphaMode = AVIF_ALPHA_MULTIPLY_MODE_MULTIPLY;
        } else if (rgb->alphaPremultiplied && !image->alphaPremultiplied) {
            alphaMode = AVIF_ALPHA_MULTIPLY_MODE_UNMULTIPLY;
        }
    }

    avifBool converted = AVIF_FALSE;

    // Try converting with libsharpyuv.
    if ((rgb->chromaDownsampling == AVIF_CHROMA_DOWNSAMPLING_SHARP_YUV) && (image->yuvFormat == AVIF_PIXEL_FORMAT_YUV420)) {
        const avifResult libSharpYUVResult = avifImageRGBToYUVLibSharpYUV(image, rgb, &state);
        if (libSharpYUVResult != AVIF_RESULT_OK) {
            // Return the error if sharpyuv was requested but failed for any reason, including libsharpyuv not being available.
            return libSharpYUVResult;
        }
        converted = AVIF_TRUE;
    }

    if (!converted && !rgb->avoidLibYUV && (alphaMode == AVIF_ALPHA_MULTIPLY_MODE_NO_OP)) {
        avifResult libyuvResult = avifImageRGBToYUVLibYUV(image, rgb);
        if (libyuvResult == AVIF_RESULT_OK) {
            converted = AVIF_TRUE;
        } else if (libyuvResult != AVIF_RESULT_NOT_IMPLEMENTED) {
            return libyuvResult;
        }
    }

    if (!converted) {
        const float kr = state.yuv.kr;
        const float kg = state.yuv.kg;
        const float kb = state.yuv.kb;

        struct YUVBlock yuvBlock[2][2];
        float rgbPixel[3];
        const float rgbMaxChannelF = state.rgb.maxChannelF;
        uint8_t ** yuvPlanes = image->yuvPlanes;
        uint32_t * yuvRowBytes = image->yuvRowBytes;
        for (uint32_t outerJ = 0; outerJ < image->height; outerJ += 2) {
            for (uint32_t outerI = 0; outerI < image->width; outerI += 2) {
                int blockW = 2, blockH = 2;
                if ((outerI + 1) >= image->width) {
                    blockW = 1;
                }
                if ((outerJ + 1) >= image->height) {
                    blockH = 1;
                }

                // Convert an entire 2x2 block to YUV, and populate any fully sampled channels as we go
                for (int bJ = 0; bJ < blockH; ++bJ) {
                    for (int bI = 0; bI < blockW; ++bI) {
                        int i = outerI + bI;
                        int j = outerJ + bJ;

                        // Unpack RGB into normalized float
                        if (state.rgb.channelBytes > 1) {
                            rgbPixel[0] =
                                *((uint16_t *)(&rgb->pixels[state.rgb.offsetBytesR + (i * state.rgb.pixelBytes) + (j * rgb->rowBytes)])) /
                                rgbMaxChannelF;
                            rgbPixel[1] =
                                *((uint16_t *)(&rgb->pixels[state.rgb.offsetBytesG + (i * state.rgb.pixelBytes) + (j * rgb->rowBytes)])) /
                                rgbMaxChannelF;
                            rgbPixel[2] =
                                *((uint16_t *)(&rgb->pixels[state.rgb.offsetBytesB + (i * state.rgb.pixelBytes) + (j * rgb->rowBytes)])) /
                                rgbMaxChannelF;
                        } else {
                            rgbPixel[0] = rgb->pixels[state.rgb.offsetBytesR + (i * state.rgb.pixelBytes) + (j * rgb->rowBytes)] /
                                          rgbMaxChannelF;
                            rgbPixel[1] = rgb->pixels[state.rgb.offsetBytesG + (i * state.rgb.pixelBytes) + (j * rgb->rowBytes)] /
                                          rgbMaxChannelF;
                            rgbPixel[2] = rgb->pixels[state.rgb.offsetBytesB + (i * state.rgb.pixelBytes) + (j * rgb->rowBytes)] /
                                          rgbMaxChannelF;
                        }

                        if (alphaMode != AVIF_ALPHA_MULTIPLY_MODE_NO_OP) {
                            float a;
                            if (state.rgb.channelBytes > 1) {
                                a = *((uint16_t *)(&rgb->pixels[state.rgb.offsetBytesA + (i * state.rgb.pixelBytes) + (j * rgb->rowBytes)])) /
                                    rgbMaxChannelF;
                            } else {
                                a = rgb->pixels[state.rgb.offsetBytesA + (i * state.rgb.pixelBytes) + (j * rgb->rowBytes)] / rgbMaxChannelF;
                            }

                            if (alphaMode == AVIF_ALPHA_MULTIPLY_MODE_MULTIPLY) {
                                if (a == 0) {
                                    rgbPixel[0] = 0;
                                    rgbPixel[1] = 0;
                                    rgbPixel[2] = 0;
                                } else if (a < 1.0f) {
                                    rgbPixel[0] *= a;
                                    rgbPixel[1] *= a;
                                    rgbPixel[2] *= a;
                                }
                            } else {
                                // alphaMode == AVIF_ALPHA_MULTIPLY_MODE_UNMULTIPLY
                                if (a == 0) {
                                    rgbPixel[0] = 0;
                                    rgbPixel[1] = 0;
                                    rgbPixel[2] = 0;
                                } else if (a < 1.0f) {
                                    rgbPixel[0] /= a;
                                    rgbPixel[1] /= a;
                                    rgbPixel[2] /= a;
                                    rgbPixel[0] = AVIF_MIN(rgbPixel[0], 1.0f);
                                    rgbPixel[1] = AVIF_MIN(rgbPixel[1], 1.0f);
                                    rgbPixel[2] = AVIF_MIN(rgbPixel[2], 1.0f);
                                }
                            }
                        }

                        // RGB -> YUV conversion
                        if (state.yuv.mode == AVIF_REFORMAT_MODE_IDENTITY) {
                            // Formulas 41,42,43 from https://www.itu.int/rec/T-REC-H.273-201612-I/en
                            yuvBlock[bI][bJ].y = rgbPixel[1]; // G
                            yuvBlock[bI][bJ].u = rgbPixel[2]; // B
                            yuvBlock[bI][bJ].v = rgbPixel[0]; // R
                        } else if (state.yuv.mode == AVIF_REFORMAT_MODE_YCGCO) {
                            // Formulas 44,45,46 from https://www.itu.int/rec/T-REC-H.273-201612-I/en
                            yuvBlock[bI][bJ].y = 0.5f * rgbPixel[1] + 0.25f * (rgbPixel[0] + rgbPixel[2]);
                            yuvBlock[bI][bJ].u = 0.5f * rgbPixel[1] - 0.25f * (rgbPixel[0] + rgbPixel[2]);
                            yuvBlock[bI][bJ].v = 0.5f * (rgbPixel[0] - rgbPixel[2]);
#if defined(AVIF_ENABLE_EXPERIMENTAL_YCGCO_R)
                        } else if (state.yuv.mode == AVIF_REFORMAT_MODE_YCGCO_RE || state.yuv.mode == AVIF_REFORMAT_MODE_YCGCO_RO) {
                            // Formulas from JVET-U0093.
                            const int R = (int)avifRoundf(AVIF_CLAMP(rgbPixel[0] * rgbMaxChannelF, 0.0f, rgbMaxChannelF));
                            const int G = (int)avifRoundf(AVIF_CLAMP(rgbPixel[1] * rgbMaxChannelF, 0.0f, rgbMaxChannelF));
                            const int B = (int)avifRoundf(AVIF_CLAMP(rgbPixel[2] * rgbMaxChannelF, 0.0f, rgbMaxChannelF));
                            const int Co = R - B;
                            const int t = B + (Co >> 1);
                            const int Cg = G - t;
                            yuvBlock[bI][bJ].y = (t + (Cg >> 1)) / state.yuv.rangeY;
                            yuvBlock[bI][bJ].u = Cg / state.yuv.rangeUV;
                            yuvBlock[bI][bJ].v = Co / state.yuv.rangeUV;
#endif
                        } else {
                            float Y = (kr * rgbPixel[0]) + (kg * rgbPixel[1]) + (kb * rgbPixel[2]);
                            yuvBlock[bI][bJ].y = Y;
                            yuvBlock[bI][bJ].u = (rgbPixel[2] - Y) / (2 * (1 - kb));
                            yuvBlock[bI][bJ].v = (rgbPixel[0] - Y) / (2 * (1 - kr));
                        }

                        if (state.yuv.channelBytes > 1) {
                            uint16_t * pY = (uint16_t *)&yuvPlanes[AVIF_CHAN_Y][(i * 2) + (j * yuvRowBytes[AVIF_CHAN_Y])];
                            *pY = (uint16_t)avifYUVColorSpaceInfoYToUNorm(&state.yuv, yuvBlock[bI][bJ].y);
                            if (image->yuvFormat == AVIF_PIXEL_FORMAT_YUV444) {
                                // YUV444, full chroma
                                uint16_t * pU = (uint16_t *)&yuvPlanes[AVIF_CHAN_U][(i * 2) + (j * yuvRowBytes[AVIF_CHAN_U])];
                                *pU = (uint16_t)avifYUVColorSpaceInfoUVToUNorm(&state.yuv, yuvBlock[bI][bJ].u);
                                uint16_t * pV = (uint16_t *)&yuvPlanes[AVIF_CHAN_V][(i * 2) + (j * yuvRowBytes[AVIF_CHAN_V])];
                                *pV = (uint16_t)avifYUVColorSpaceInfoUVToUNorm(&state.yuv, yuvBlock[bI][bJ].v);
                            }
                        } else {
                            yuvPlanes[AVIF_CHAN_Y][i + (j * yuvRowBytes[AVIF_CHAN_Y])] =
                                (uint8_t)avifYUVColorSpaceInfoYToUNorm(&state.yuv, yuvBlock[bI][bJ].y);
                            if (image->yuvFormat == AVIF_PIXEL_FORMAT_YUV444) {
                                // YUV444, full chroma
                                yuvPlanes[AVIF_CHAN_U][i + (j * yuvRowBytes[AVIF_CHAN_U])] =
                                    (uint8_t)avifYUVColorSpaceInfoUVToUNorm(&state.yuv, yuvBlock[bI][bJ].u);
                                yuvPlanes[AVIF_CHAN_V][i + (j * yuvRowBytes[AVIF_CHAN_V])] =
                                    (uint8_t)avifYUVColorSpaceInfoUVToUNorm(&state.yuv, yuvBlock[bI][bJ].v);
                            }
                        }
                    }
                }

                // Populate any subsampled channels with averages from the 2x2 block
                if (image->yuvFormat == AVIF_PIXEL_FORMAT_YUV400) {
                    // Do nothing on chroma planes.
                } else if (image->yuvFormat == AVIF_PIXEL_FORMAT_YUV420) {
                    // YUV420, average 4 samples (2x2)

                    float sumU = 0.0f;
                    float sumV = 0.0f;
                    for (int bJ = 0; bJ < blockH; ++bJ) {
                        for (int bI = 0; bI < blockW; ++bI) {
                            sumU += yuvBlock[bI][bJ].u;
                            sumV += yuvBlock[bI][bJ].v;
                        }
                    }
                    float totalSamples = (float)(blockW * blockH);
                    float avgU = sumU / totalSamples;
                    float avgV = sumV / totalSamples;

                    const int chromaShiftX = 1;
                    const int chromaShiftY = 1;
                    int uvI = outerI >> chromaShiftX;
                    int uvJ = outerJ >> chromaShiftY;
                    if (state.yuv.channelBytes > 1) {
                        uint16_t * pU = (uint16_t *)&yuvPlanes[AVIF_CHAN_U][(uvI * 2) + (uvJ * yuvRowBytes[AVIF_CHAN_U])];
                        *pU = (uint16_t)avifYUVColorSpaceInfoUVToUNorm(&state.yuv, avgU);
                        uint16_t * pV = (uint16_t *)&yuvPlanes[AVIF_CHAN_V][(uvI * 2) + (uvJ * yuvRowBytes[AVIF_CHAN_V])];
                        *pV = (uint16_t)avifYUVColorSpaceInfoUVToUNorm(&state.yuv, avgV);
                    } else {
                        yuvPlanes[AVIF_CHAN_U][uvI + (uvJ * yuvRowBytes[AVIF_CHAN_U])] =
                            (uint8_t)avifYUVColorSpaceInfoUVToUNorm(&state.yuv, avgU);
                        yuvPlanes[AVIF_CHAN_V][uvI + (uvJ * yuvRowBytes[AVIF_CHAN_V])] =
                            (uint8_t)avifYUVColorSpaceInfoUVToUNorm(&state.yuv, avgV);
                    }
                } else if (image->yuvFormat == AVIF_PIXEL_FORMAT_YUV422) {
                    // YUV422, average 2 samples (1x2), twice

                    for (int bJ = 0; bJ < blockH; ++bJ) {
                        float sumU = 0.0f;
                        float sumV = 0.0f;
                        for (int bI = 0; bI < blockW; ++bI) {
                            sumU += yuvBlock[bI][bJ].u;
                            sumV += yuvBlock[bI][bJ].v;
                        }
                        float totalSamples = (float)blockW;
                        float avgU = sumU / totalSamples;
                        float avgV = sumV / totalSamples;

                        const int chromaShiftX = 1;
                        int uvI = outerI >> chromaShiftX;
                        int uvJ = outerJ + bJ;
                        if (state.yuv.channelBytes > 1) {
                            uint16_t * pU = (uint16_t *)&yuvPlanes[AVIF_CHAN_U][(uvI * 2) + (uvJ * yuvRowBytes[AVIF_CHAN_U])];
                            *pU = (uint16_t)avifYUVColorSpaceInfoUVToUNorm(&state.yuv, avgU);
                            uint16_t * pV = (uint16_t *)&yuvPlanes[AVIF_CHAN_V][(uvI * 2) + (uvJ * yuvRowBytes[AVIF_CHAN_V])];
                            *pV = (uint16_t)avifYUVColorSpaceInfoUVToUNorm(&state.yuv, avgV);
                        } else {
                            yuvPlanes[AVIF_CHAN_U][uvI + (uvJ * yuvRowBytes[AVIF_CHAN_U])] =
                                (uint8_t)avifYUVColorSpaceInfoUVToUNorm(&state.yuv, avgU);
                            yuvPlanes[AVIF_CHAN_V][uvI + (uvJ * yuvRowBytes[AVIF_CHAN_V])] =
                                (uint8_t)avifYUVColorSpaceInfoUVToUNorm(&state.yuv, avgV);
                        }
                    }
                }
            }
        }
    }

    if (image->alphaPlane && image->alphaRowBytes) {
        avifAlphaParams params;

        params.width = image->width;
        params.height = image->height;
        params.dstDepth = image->depth;
        params.dstPlane = image->alphaPlane;
        params.dstRowBytes = image->alphaRowBytes;
        params.dstOffsetBytes = 0;
        params.dstPixelBytes = state.yuv.channelBytes;

        if (avifRGBFormatHasAlpha(rgb->format) && !rgb->ignoreAlpha) {
            params.srcDepth = rgb->depth;
            params.srcPlane = rgb->pixels;
            params.srcRowBytes = rgb->rowBytes;
            params.srcOffsetBytes = state.rgb.offsetBytesA;
            params.srcPixelBytes = state.rgb.pixelBytes;

            avifReformatAlpha(&params);
        } else {
            // libyuv does not fill alpha when converting from RGB to YUV so
            // fill it regardless of the value of convertedWithLibYUV.
            avifFillAlpha(&params);
        }
    }
    return AVIF_RESULT_OK;
}

// Allocates and fills look-up tables for going from YUV limited/full unorm -> full range RGB FP32.
// Review this when implementing YCgCo limited range support.
static avifBool avifCreateYUVToRGBLookUpTables(float ** unormFloatTableY, float ** unormFloatTableUV, uint32_t depth, const avifReformatState * state)
{
    const size_t cpCount = (size_t)1 << depth;

    assert(unormFloatTableY);
    *unormFloatTableY = avifAlloc(cpCount * sizeof(**unormFloatTableY));
    AVIF_CHECK(*unormFloatTableY);
    for (uint32_t cp = 0; cp < cpCount; ++cp) {
        (*unormFloatTableY)[cp] = ((float)cp - state->yuv.biasY) / state->yuv.rangeY;
    }

    if (unormFloatTableUV) {
        if (state->yuv.mode == AVIF_REFORMAT_MODE_IDENTITY) {
            // Just reuse the luma table since the chroma values are the same.
            *unormFloatTableUV = *unormFloatTableY;
        } else {
            *unormFloatTableUV = avifAlloc(cpCount * sizeof(**unormFloatTableUV));
            if (!*unormFloatTableUV) {
                avifFree(*unormFloatTableY);
                *unormFloatTableY = NULL;
                return AVIF_FALSE;
            }
            for (uint32_t cp = 0; cp < cpCount; ++cp) {
                (*unormFloatTableUV)[cp] = ((float)cp - state->yuv.biasUV) / state->yuv.rangeUV;
            }
        }
    }
    return AVIF_TRUE;
}

// Frees look-up tables allocated with avifCreateYUVToRGBLookUpTables().
static void avifFreeYUVToRGBLookUpTables(float ** unormFloatTableY, float ** unormFloatTableUV)
{
    if (unormFloatTableUV) {
        if (*unormFloatTableUV != *unormFloatTableY) {
            avifFree(*unormFloatTableUV);
        }
        *unormFloatTableUV = NULL;
    }

    avifFree(*unormFloatTableY);
    *unormFloatTableY = NULL;
}

#define RGB565(R, G, B) ((uint16_t)(((B) >> 3) | (((G) >> 2) << 5) | (((R) >> 3) << 11)))

static void avifStoreRGB8Pixel(avifRGBFormat format, uint8_t R, uint8_t G, uint8_t B, uint8_t * ptrR, uint8_t * ptrG, uint8_t * ptrB)
{
    if (format == AVIF_RGB_FORMAT_RGB_565) {
        // References for RGB565 color conversion:
        // * https://docs.microsoft.com/en-us/windows/win32/directshow/working-with-16-bit-rgb
        // * https://chromium.googlesource.com/libyuv/libyuv/+/9892d70c965678381d2a70a1c9002d1cf136ee78/source/row_common.cc#2362
        *(uint16_t *)ptrR = RGB565(R, G, B);
        return;
    }
    *ptrR = R;
    *ptrG = G;
    *ptrB = B;
}

static void avifGetRGB565(const uint8_t * ptrR, uint8_t * R, uint8_t * G, uint8_t * B)
{
    // References for RGB565 color conversion:
    // * https://docs.microsoft.com/en-us/windows/win32/directshow/working-with-16-bit-rgb
    // * https://chromium.googlesource.com/libyuv/libyuv/+/331c361581896292fb46c8c6905e41262b7ca95f/source/row_common.cc#185
    const uint16_t rgb656 = ((const uint16_t *)ptrR)[0];
    const uint16_t r5 = (rgb656 & 0xF800) >> 11;
    const uint16_t g6 = (rgb656 & 0x07E0) >> 5;
    const uint16_t b5 = (rgb656 & 0x001F);
    *R = (uint8_t)((r5 << 3) | (r5 >> 2));
    *G = (uint8_t)((g6 << 2) | (g6 >> 4));
    *B = (uint8_t)((b5 << 3) | (b5 >> 2));
}

// Note: This function handles alpha (un)multiply.
static avifResult avifImageYUVAnyToRGBAnySlow(const avifImage * image,
                                              avifRGBImage * rgb,
                                              const avifReformatState * state,
                                              avifAlphaMultiplyMode alphaMultiplyMode)
{
    // Aliases for some state
    const float kr = state->yuv.kr;
    const float kg = state->yuv.kg;
    const float kb = state->yuv.kb;
    float * unormFloatTableY = NULL;
    float * unormFloatTableUV = NULL;
    AVIF_CHECKERR(avifCreateYUVToRGBLookUpTables(&unormFloatTableY, &unormFloatTableUV, image->depth, state), AVIF_RESULT_OUT_OF_MEMORY);
    const uint32_t yuvChannelBytes = state->yuv.channelBytes;
    const uint32_t rgbPixelBytes = state->rgb.pixelBytes;

    // Aliases for plane data
    const uint8_t * yPlane = image->yuvPlanes[AVIF_CHAN_Y];
    const uint8_t * uPlane = image->yuvPlanes[AVIF_CHAN_U];
    const uint8_t * vPlane = image->yuvPlanes[AVIF_CHAN_V];
    const uint8_t * aPlane = image->alphaPlane;
    const uint32_t yRowBytes = image->yuvRowBytes[AVIF_CHAN_Y];
    const uint32_t uRowBytes = image->yuvRowBytes[AVIF_CHAN_U];
    const uint32_t vRowBytes = image->yuvRowBytes[AVIF_CHAN_V];
    const uint32_t aRowBytes = image->alphaRowBytes;

    // Various observations and limits
    const avifBool hasColor = (uPlane && vPlane && (image->yuvFormat != AVIF_PIXEL_FORMAT_YUV400));
    const uint16_t yuvMaxChannel = (uint16_t)state->yuv.maxChannel;
    const float rgbMaxChannelF = state->rgb.maxChannelF;

    // If toRGBAlphaMode is active (not no-op), assert that the alpha plane is present. The end of
    // the avifPrepareReformatState() function should ensure this, but this assert makes it clear
    // to clang's analyzer.
    assert((alphaMultiplyMode == AVIF_ALPHA_MULTIPLY_MODE_NO_OP) || aPlane);

    for (uint32_t j = 0; j < image->height; ++j) {
        // uvJ is used only when hasColor is true.
        const uint32_t uvJ = hasColor ? (j >> state->yuv.formatInfo.chromaShiftY) : 0;
        const uint8_t * ptrY8 = &yPlane[j * yRowBytes];
        const uint8_t * ptrU8 = uPlane ? &uPlane[(uvJ * uRowBytes)] : NULL;
        const uint8_t * ptrV8 = vPlane ? &vPlane[(uvJ * vRowBytes)] : NULL;
        const uint8_t * ptrA8 = aPlane ? &aPlane[j * aRowBytes] : NULL;
        const uint16_t * ptrY16 = (const uint16_t *)ptrY8;
        const uint16_t * ptrU16 = (const uint16_t *)ptrU8;
        const uint16_t * ptrV16 = (const uint16_t *)ptrV8;
        const uint16_t * ptrA16 = (const uint16_t *)ptrA8;

        uint8_t * ptrR = &rgb->pixels[state->rgb.offsetBytesR + (j * rgb->rowBytes)];
        uint8_t * ptrG = &rgb->pixels[state->rgb.offsetBytesG + (j * rgb->rowBytes)];
        uint8_t * ptrB = &rgb->pixels[state->rgb.offsetBytesB + (j * rgb->rowBytes)];

        for (uint32_t i = 0; i < image->width; ++i) {
            float Y, Cb = 0.5f, Cr = 0.5f;

            // Calculate Y
            uint16_t unormY;
            if (image->depth == 8) {
                unormY = ptrY8[i];
            } else {
                // clamp incoming data to protect against bad LUT lookups
                unormY = AVIF_MIN(ptrY16[i], yuvMaxChannel);
            }
            Y = unormFloatTableY[unormY];

            // Calculate Cb and Cr
            if (hasColor) {
                const uint32_t uvI = i >> state->yuv.formatInfo.chromaShiftX;
                if (image->yuvFormat == AVIF_PIXEL_FORMAT_YUV444) {
                    uint16_t unormU, unormV;

                    if (image->depth == 8) {
                        unormU = ptrU8[uvI];
                        unormV = ptrV8[uvI];
                    } else {
                        // clamp incoming data to protect against bad LUT lookups
                        unormU = AVIF_MIN(ptrU16[uvI], yuvMaxChannel);
                        unormV = AVIF_MIN(ptrV16[uvI], yuvMaxChannel);
                    }

                    Cb = unormFloatTableUV[unormU];
                    Cr = unormFloatTableUV[unormV];
                } else {
                    // Upsample to 444:
                    //
                    // *   *   *   *
                    //   A       B
                    // *   1   2   *
                    //
                    // *   3   4   *
                    //   C       D
                    // *   *   *   *
                    //
                    // When converting from YUV420 to RGB, for any given "high-resolution" RGB
                    // coordinate (1,2,3,4,*), there are up to four "low-resolution" UV samples
                    // (A,B,C,D) that are "nearest" to the pixel. For RGB pixel #1, A is the closest
                    // UV sample, B and C are "adjacent" to it on the same row and column, and D is
                    // the diagonal. For RGB pixel 3, C is the closest UV sample, A and D are
                    // adjacent, and B is the diagonal. Sometimes the adjacent pixel on the same row
                    // is to the left or right, and sometimes the adjacent pixel on the same column
                    // is up or down. For any edge or corner, there might only be only one or two
                    // samples nearby, so they'll be duplicated.
                    //
                    // The following code attempts to find all four nearest UV samples and put them
                    // in the following unormU and unormV grid as follows:
                    //
                    // unorm[0][0] = closest         ( weights: bilinear: 9/16, nearest: 1 )
                    // unorm[1][0] = adjacent col    ( weights: bilinear: 3/16, nearest: 0 )
                    // unorm[0][1] = adjacent row    ( weights: bilinear: 3/16, nearest: 0 )
                    // unorm[1][1] = diagonal        ( weights: bilinear: 1/16, nearest: 0 )
                    //
                    // It then weights them according to the requested upsampling set in avifRGBImage.

                    uint16_t unormU[2][2], unormV[2][2];

                    // How many bytes to add to a uint8_t pointer index to get to the adjacent (lesser) sample in a given direction
                    int uAdjCol, vAdjCol, uAdjRow, vAdjRow;
                    if ((i == 0) || ((i == (image->width - 1)) && ((i % 2) != 0))) {
                        uAdjCol = 0;
                        vAdjCol = 0;
                    } else {
                        if ((i % 2) != 0) {
                            uAdjCol = yuvChannelBytes;
                            vAdjCol = yuvChannelBytes;
                        } else {
                            uAdjCol = -1 * yuvChannelBytes;
                            vAdjCol = -1 * yuvChannelBytes;
                        }
                    }

                    // For YUV422, uvJ will always be a fresh value (always corresponds to j), so
                    // we'll simply duplicate the sample as if we were on the top or bottom row and
                    // it'll behave as plain old linear (1D) upsampling, which is all we want.
                    if ((j == 0) || ((j == (image->height - 1)) && ((j % 2) != 0)) || (image->yuvFormat == AVIF_PIXEL_FORMAT_YUV422)) {
                        uAdjRow = 0;
                        vAdjRow = 0;
                    } else {
                        if ((j % 2) != 0) {
                            uAdjRow = (int)uRowBytes;
                            vAdjRow = (int)vRowBytes;
                        } else {
                            uAdjRow = -1 * (int)uRowBytes;
                            vAdjRow = -1 * (int)vRowBytes;
                        }
                    }

                    if (image->depth == 8) {
                        unormU[0][0] = uPlane[(uvJ * uRowBytes) + (uvI * yuvChannelBytes)];
                        unormV[0][0] = vPlane[(uvJ * vRowBytes) + (uvI * yuvChannelBytes)];
                        unormU[1][0] = uPlane[(uvJ * uRowBytes) + (uvI * yuvChannelBytes) + uAdjCol];
                        unormV[1][0] = vPlane[(uvJ * vRowBytes) + (uvI * yuvChannelBytes) + vAdjCol];
                        unormU[0][1] = uPlane[(uvJ * uRowBytes) + (uvI * yuvChannelBytes) + uAdjRow];
                        unormV[0][1] = vPlane[(uvJ * vRowBytes) + (uvI * yuvChannelBytes) + vAdjRow];
                        unormU[1][1] = uPlane[(uvJ * uRowBytes) + (uvI * yuvChannelBytes) + uAdjCol + uAdjRow];
                        unormV[1][1] = vPlane[(uvJ * vRowBytes) + (uvI * yuvChannelBytes) + vAdjCol + vAdjRow];
                    } else {
                        unormU[0][0] = *((const uint16_t *)&uPlane[(uvJ * uRowBytes) + (uvI * yuvChannelBytes)]);
                        unormV[0][0] = *((const uint16_t *)&vPlane[(uvJ * vRowBytes) + (uvI * yuvChannelBytes)]);
                        unormU[1][0] = *((const uint16_t *)&uPlane[(uvJ * uRowBytes) + (uvI * yuvChannelBytes) + uAdjCol]);
                        unormV[1][0] = *((const uint16_t *)&vPlane[(uvJ * vRowBytes) + (uvI * yuvChannelBytes) + vAdjCol]);
                        unormU[0][1] = *((const uint16_t *)&uPlane[(uvJ * uRowBytes) + (uvI * yuvChannelBytes) + uAdjRow]);
                        unormV[0][1] = *((const uint16_t *)&vPlane[(uvJ * vRowBytes) + (uvI * yuvChannelBytes) + vAdjRow]);
                        unormU[1][1] = *((const uint16_t *)&uPlane[(uvJ * uRowBytes) + (uvI * yuvChannelBytes) + uAdjCol + uAdjRow]);
                        unormV[1][1] = *((const uint16_t *)&vPlane[(uvJ * vRowBytes) + (uvI * yuvChannelBytes) + vAdjCol + vAdjRow]);

                        // clamp incoming data to protect against bad LUT lookups
                        for (int bJ = 0; bJ < 2; ++bJ) {
                            for (int bI = 0; bI < 2; ++bI) {
                                unormU[bI][bJ] = AVIF_MIN(unormU[bI][bJ], yuvMaxChannel);
                                unormV[bI][bJ] = AVIF_MIN(unormV[bI][bJ], yuvMaxChannel);
                            }
                        }
                    }

                    if ((rgb->chromaUpsampling == AVIF_CHROMA_UPSAMPLING_FASTEST) ||
                        (rgb->chromaUpsampling == AVIF_CHROMA_UPSAMPLING_NEAREST)) {
                        // Nearest neighbor; ignore all UVs but the closest one
                        Cb = unormFloatTableUV[unormU[0][0]];
                        Cr = unormFloatTableUV[unormV[0][0]];
                    } else {
                        // Bilinear filtering with weights
                        Cb = (unormFloatTableUV[unormU[0][0]] * (9.0f / 16.0f)) + (unormFloatTableUV[unormU[1][0]] * (3.0f / 16.0f)) +
                             (unormFloatTableUV[unormU[0][1]] * (3.0f / 16.0f)) + (unormFloatTableUV[unormU[1][1]] * (1.0f / 16.0f));
                        Cr = (unormFloatTableUV[unormV[0][0]] * (9.0f / 16.0f)) + (unormFloatTableUV[unormV[1][0]] * (3.0f / 16.0f)) +
                             (unormFloatTableUV[unormV[0][1]] * (3.0f / 16.0f)) + (unormFloatTableUV[unormV[1][1]] * (1.0f / 16.0f));
                    }
                }
            }

            float R, G, B;
            if (hasColor) {
                if (state->yuv.mode == AVIF_REFORMAT_MODE_IDENTITY) {
                    // Identity (GBR): Formulas 41,42,43 from https://www.itu.int/rec/T-REC-H.273-201612-I/en
                    G = Y;
                    B = Cb;
                    R = Cr;
                } else if (state->yuv.mode == AVIF_REFORMAT_MODE_YCGCO) {
                    // YCgCo: Formulas 47,48,49,50 from https://www.itu.int/rec/T-REC-H.273-201612-I/en
                    const float t = Y - Cb;
                    G = Y + Cb;
                    B = t - Cr;
                    R = t + Cr;
#if defined(AVIF_ENABLE_EXPERIMENTAL_YCGCO_R)
                } else if (state->yuv.mode == AVIF_REFORMAT_MODE_YCGCO_RE || state->yuv.mode == AVIF_REFORMAT_MODE_YCGCO_RO) {
                    const int YY = unormY;
                    const int Cg = (int)avifRoundf(Cb * yuvMaxChannel);
                    const int Co = (int)avifRoundf(Cr * yuvMaxChannel);
                    const int t = YY - (Cg >> 1);
                    G = (float)AVIF_CLAMP(t + Cg, 0, state->rgb.maxChannel);
                    B = (float)AVIF_CLAMP(t - (Co >> 1), 0, state->rgb.maxChannel);
                    R = (float)AVIF_CLAMP(B + Co, 0, state->rgb.maxChannel);
                    G /= rgbMaxChannelF;
                    B /= rgbMaxChannelF;
                    R /= rgbMaxChannelF;
#endif
                } else {
                    // Normal YUV
                    R = Y + (2 * (1 - kr)) * Cr;
                    B = Y + (2 * (1 - kb)) * Cb;
                    G = Y - ((2 * ((kr * (1 - kr) * Cr) + (kb * (1 - kb) * Cb))) / kg);
                }
            } else {
                // Monochrome: just populate all channels with luma (state->yuv.mode is irrelevant)
                R = Y;
                G = Y;
                B = Y;
            }

            float Rc = AVIF_CLAMP(R, 0.0f, 1.0f);
            float Gc = AVIF_CLAMP(G, 0.0f, 1.0f);
            float Bc = AVIF_CLAMP(B, 0.0f, 1.0f);

            if (alphaMultiplyMode != AVIF_ALPHA_MULTIPLY_MODE_NO_OP) {
                // Calculate A
                uint16_t unormA;
                if (image->depth == 8) {
                    unormA = ptrA8[i];
                } else {
                    unormA = AVIF_MIN(ptrA16[i], yuvMaxChannel);
                }
                const float A = unormA / ((float)state->yuv.maxChannel);
                const float Ac = AVIF_CLAMP(A, 0.0f, 1.0f);

                if (alphaMultiplyMode == AVIF_ALPHA_MULTIPLY_MODE_MULTIPLY) {
                    if (Ac == 0.0f) {
                        Rc = 0.0f;
                        Gc = 0.0f;
                        Bc = 0.0f;
                    } else if (Ac < 1.0f) {
                        Rc *= Ac;
                        Gc *= Ac;
                        Bc *= Ac;
                    }
                } else {
                    // alphaMultiplyMode == AVIF_ALPHA_MULTIPLY_MODE_UNMULTIPLY
                    if (Ac == 0.0f) {
                        Rc = 0.0f;
                        Gc = 0.0f;
                        Bc = 0.0f;
                    } else if (Ac < 1.0f) {
                        Rc /= Ac;
                        Gc /= Ac;
                        Bc /= Ac;
                        Rc = AVIF_MIN(Rc, 1.0f);
                        Gc = AVIF_MIN(Gc, 1.0f);
                        Bc = AVIF_MIN(Bc, 1.0f);
                    }
                }
            }

            if (rgb->depth == 8) {
                avifStoreRGB8Pixel(rgb->format,
                                   (uint8_t)(0.5f + (Rc * rgbMaxChannelF)),
                                   (uint8_t)(0.5f + (Gc * rgbMaxChannelF)),
                                   (uint8_t)(0.5f + (Bc * rgbMaxChannelF)),
                                   ptrR,
                                   ptrG,
                                   ptrB);
            } else {
                *((uint16_t *)ptrR) = (uint16_t)(0.5f + (Rc * rgbMaxChannelF));
                *((uint16_t *)ptrG) = (uint16_t)(0.5f + (Gc * rgbMaxChannelF));
                *((uint16_t *)ptrB) = (uint16_t)(0.5f + (Bc * rgbMaxChannelF));
            }
            ptrR += rgbPixelBytes;
            ptrG += rgbPixelBytes;
            ptrB += rgbPixelBytes;
        }
    }
    avifFreeYUVToRGBLookUpTables(&unormFloatTableY, &unormFloatTableUV);
    return AVIF_RESULT_OK;
}

static avifResult avifImageYUV16ToRGB16Color(const avifImage * image, avifRGBImage * rgb, avifReformatState * state)
{
    const float kr = state->yuv.kr;
    const float kg = state->yuv.kg;
    const float kb = state->yuv.kb;
    const uint32_t rgbPixelBytes = state->rgb.pixelBytes;
    float * unormFloatTableY = NULL;
    float * unormFloatTableUV = NULL;
    AVIF_CHECKERR(avifCreateYUVToRGBLookUpTables(&unormFloatTableY, &unormFloatTableUV, image->depth, state), AVIF_RESULT_OUT_OF_MEMORY);

    const uint16_t yuvMaxChannel = (uint16_t)state->yuv.maxChannel;
    const float rgbMaxChannelF = state->rgb.maxChannelF;
    for (uint32_t j = 0; j < image->height; ++j) {
        const uint32_t uvJ = j >> state->yuv.formatInfo.chromaShiftY;
        const uint16_t * const ptrY = (uint16_t *)&image->yuvPlanes[AVIF_CHAN_Y][(j * image->yuvRowBytes[AVIF_CHAN_Y])];
        const uint16_t * const ptrU = (uint16_t *)&image->yuvPlanes[AVIF_CHAN_U][(uvJ * image->yuvRowBytes[AVIF_CHAN_U])];
        const uint16_t * const ptrV = (uint16_t *)&image->yuvPlanes[AVIF_CHAN_V][(uvJ * image->yuvRowBytes[AVIF_CHAN_V])];
        uint8_t * ptrR = &rgb->pixels[state->rgb.offsetBytesR + (j * rgb->rowBytes)];
        uint8_t * ptrG = &rgb->pixels[state->rgb.offsetBytesG + (j * rgb->rowBytes)];
        uint8_t * ptrB = &rgb->pixels[state->rgb.offsetBytesB + (j * rgb->rowBytes)];

        for (uint32_t i = 0; i < image->width; ++i) {
            uint32_t uvI = i >> state->yuv.formatInfo.chromaShiftX;

            // clamp incoming data to protect against bad LUT lookups
            const uint16_t unormY = AVIF_MIN(ptrY[i], yuvMaxChannel);
            const uint16_t unormU = AVIF_MIN(ptrU[uvI], yuvMaxChannel);
            const uint16_t unormV = AVIF_MIN(ptrV[uvI], yuvMaxChannel);

            // Convert unorm to float
            const float Y = unormFloatTableY[unormY];
            const float Cb = unormFloatTableUV[unormU];
            const float Cr = unormFloatTableUV[unormV];

            const float R = Y + (2 * (1 - kr)) * Cr;
            const float B = Y + (2 * (1 - kb)) * Cb;
            const float G = Y - ((2 * ((kr * (1 - kr) * Cr) + (kb * (1 - kb) * Cb))) / kg);
            const float Rc = AVIF_CLAMP(R, 0.0f, 1.0f);
            const float Gc = AVIF_CLAMP(G, 0.0f, 1.0f);
            const float Bc = AVIF_CLAMP(B, 0.0f, 1.0f);

            *((uint16_t *)ptrR) = (uint16_t)(0.5f + (Rc * rgbMaxChannelF));
            *((uint16_t *)ptrG) = (uint16_t)(0.5f + (Gc * rgbMaxChannelF));
            *((uint16_t *)ptrB) = (uint16_t)(0.5f + (Bc * rgbMaxChannelF));

            ptrR += rgbPixelBytes;
            ptrG += rgbPixelBytes;
            ptrB += rgbPixelBytes;
        }
    }
    avifFreeYUVToRGBLookUpTables(&unormFloatTableY, &unormFloatTableUV);
    return AVIF_RESULT_OK;
}

static avifResult avifImageYUV16ToRGB16Mono(const avifImage * image, avifRGBImage * rgb, avifReformatState * state)
{
    const float kr = state->yuv.kr;
    const float kg = state->yuv.kg;
    const float kb = state->yuv.kb;
    const uint32_t rgbPixelBytes = state->rgb.pixelBytes;
    float * unormFloatTableY = NULL;
    AVIF_CHECKERR(avifCreateYUVToRGBLookUpTables(&unormFloatTableY, NULL, image->depth, state), AVIF_RESULT_OUT_OF_MEMORY);

    const uint16_t maxChannel = (uint16_t)state->yuv.maxChannel;
    const float maxChannelF = state->rgb.maxChannelF;
    for (uint32_t j = 0; j < image->height; ++j) {
        const uint16_t * const ptrY = (uint16_t *)&image->yuvPlanes[AVIF_CHAN_Y][(j * image->yuvRowBytes[AVIF_CHAN_Y])];
        uint8_t * ptrR = &rgb->pixels[state->rgb.offsetBytesR + (j * rgb->rowBytes)];
        uint8_t * ptrG = &rgb->pixels[state->rgb.offsetBytesG + (j * rgb->rowBytes)];
        uint8_t * ptrB = &rgb->pixels[state->rgb.offsetBytesB + (j * rgb->rowBytes)];

        for (uint32_t i = 0; i < image->width; ++i) {
            // clamp incoming data to protect against bad LUT lookups
            const uint16_t unormY = AVIF_MIN(ptrY[i], maxChannel);

            // Convert unorm to float
            const float Y = unormFloatTableY[unormY];
            const float Cb = 0.0f;
            const float Cr = 0.0f;

            const float R = Y + (2 * (1 - kr)) * Cr;
            const float B = Y + (2 * (1 - kb)) * Cb;
            const float G = Y - ((2 * ((kr * (1 - kr) * Cr) + (kb * (1 - kb) * Cb))) / kg);
            const float Rc = AVIF_CLAMP(R, 0.0f, 1.0f);
            const float Gc = AVIF_CLAMP(G, 0.0f, 1.0f);
            const float Bc = AVIF_CLAMP(B, 0.0f, 1.0f);

            *((uint16_t *)ptrR) = (uint16_t)(0.5f + (Rc * maxChannelF));
            *((uint16_t *)ptrG) = (uint16_t)(0.5f + (Gc * maxChannelF));
            *((uint16_t *)ptrB) = (uint16_t)(0.5f + (Bc * maxChannelF));

            ptrR += rgbPixelBytes;
            ptrG += rgbPixelBytes;
            ptrB += rgbPixelBytes;
        }
    }
    avifFreeYUVToRGBLookUpTables(&unormFloatTableY, NULL);
    return AVIF_RESULT_OK;
}

static avifResult avifImageYUV16ToRGB8Color(const avifImage * image, avifRGBImage * rgb, avifReformatState * state)
{
    const float kr = state->yuv.kr;
    const float kg = state->yuv.kg;
    const float kb = state->yuv.kb;
    const uint32_t rgbPixelBytes = state->rgb.pixelBytes;
    float * unormFloatTableY = NULL;
    float * unormFloatTableUV = NULL;
    AVIF_CHECKERR(avifCreateYUVToRGBLookUpTables(&unormFloatTableY, &unormFloatTableUV, image->depth, state), AVIF_RESULT_OUT_OF_MEMORY);

    const uint16_t yuvMaxChannel = (uint16_t)state->yuv.maxChannel;
    const float rgbMaxChannelF = state->rgb.maxChannelF;
    for (uint32_t j = 0; j < image->height; ++j) {
        const uint32_t uvJ = j >> state->yuv.formatInfo.chromaShiftY;
        const uint16_t * const ptrY = (uint16_t *)&image->yuvPlanes[AVIF_CHAN_Y][(j * image->yuvRowBytes[AVIF_CHAN_Y])];
        const uint16_t * const ptrU = (uint16_t *)&image->yuvPlanes[AVIF_CHAN_U][(uvJ * image->yuvRowBytes[AVIF_CHAN_U])];
        const uint16_t * const ptrV = (uint16_t *)&image->yuvPlanes[AVIF_CHAN_V][(uvJ * image->yuvRowBytes[AVIF_CHAN_V])];
        uint8_t * ptrR = &rgb->pixels[state->rgb.offsetBytesR + (j * rgb->rowBytes)];
        uint8_t * ptrG = &rgb->pixels[state->rgb.offsetBytesG + (j * rgb->rowBytes)];
        uint8_t * ptrB = &rgb->pixels[state->rgb.offsetBytesB + (j * rgb->rowBytes)];

        for (uint32_t i = 0; i < image->width; ++i) {
            uint32_t uvI = i >> state->yuv.formatInfo.chromaShiftX;

            // clamp incoming data to protect against bad LUT lookups
            const uint16_t unormY = AVIF_MIN(ptrY[i], yuvMaxChannel);
            const uint16_t unormU = AVIF_MIN(ptrU[uvI], yuvMaxChannel);
            const uint16_t unormV = AVIF_MIN(ptrV[uvI], yuvMaxChannel);

            // Convert unorm to float
            const float Y = unormFloatTableY[unormY];
            const float Cb = unormFloatTableUV[unormU];
            const float Cr = unormFloatTableUV[unormV];

            const float R = Y + (2 * (1 - kr)) * Cr;
            const float B = Y + (2 * (1 - kb)) * Cb;
            const float G = Y - ((2 * ((kr * (1 - kr) * Cr) + (kb * (1 - kb) * Cb))) / kg);
            const float Rc = AVIF_CLAMP(R, 0.0f, 1.0f);
            const float Gc = AVIF_CLAMP(G, 0.0f, 1.0f);
            const float Bc = AVIF_CLAMP(B, 0.0f, 1.0f);

            avifStoreRGB8Pixel(rgb->format,
                               (uint8_t)(0.5f + (Rc * rgbMaxChannelF)),
                               (uint8_t)(0.5f + (Gc * rgbMaxChannelF)),
                               (uint8_t)(0.5f + (Bc * rgbMaxChannelF)),
                               ptrR,
                               ptrG,
                               ptrB);

            ptrR += rgbPixelBytes;
            ptrG += rgbPixelBytes;
            ptrB += rgbPixelBytes;
        }
    }
    avifFreeYUVToRGBLookUpTables(&unormFloatTableY, &unormFloatTableUV);
    return AVIF_RESULT_OK;
}

static avifResult avifImageYUV16ToRGB8Mono(const avifImage * image, avifRGBImage * rgb, avifReformatState * state)
{
    const float kr = state->yuv.kr;
    const float kg = state->yuv.kg;
    const float kb = state->yuv.kb;
    const uint32_t rgbPixelBytes = state->rgb.pixelBytes;
    float * unormFloatTableY = NULL;
    AVIF_CHECKERR(avifCreateYUVToRGBLookUpTables(&unormFloatTableY, NULL, image->depth, state), AVIF_RESULT_OUT_OF_MEMORY);

    const uint16_t yuvMaxChannel = (uint16_t)state->yuv.maxChannel;
    const float rgbMaxChannelF = state->rgb.maxChannelF;
    for (uint32_t j = 0; j < image->height; ++j) {
        const uint16_t * const ptrY = (uint16_t *)&image->yuvPlanes[AVIF_CHAN_Y][(j * image->yuvRowBytes[AVIF_CHAN_Y])];
        uint8_t * ptrR = &rgb->pixels[state->rgb.offsetBytesR + (j * rgb->rowBytes)];
        uint8_t * ptrG = &rgb->pixels[state->rgb.offsetBytesG + (j * rgb->rowBytes)];
        uint8_t * ptrB = &rgb->pixels[state->rgb.offsetBytesB + (j * rgb->rowBytes)];

        for (uint32_t i = 0; i < image->width; ++i) {
            // clamp incoming data to protect against bad LUT lookups
            const uint16_t unormY = AVIF_MIN(ptrY[i], yuvMaxChannel);

            // Convert unorm to float
            const float Y = unormFloatTableY[unormY];
            const float Cb = 0.0f;
            const float Cr = 0.0f;

            const float R = Y + (2 * (1 - kr)) * Cr;
            const float B = Y + (2 * (1 - kb)) * Cb;
            const float G = Y - ((2 * ((kr * (1 - kr) * Cr) + (kb * (1 - kb) * Cb))) / kg);
            const float Rc = AVIF_CLAMP(R, 0.0f, 1.0f);
            const float Gc = AVIF_CLAMP(G, 0.0f, 1.0f);
            const float Bc = AVIF_CLAMP(B, 0.0f, 1.0f);

            avifStoreRGB8Pixel(rgb->format,
                               (uint8_t)(0.5f + (Rc * rgbMaxChannelF)),
                               (uint8_t)(0.5f + (Gc * rgbMaxChannelF)),
                               (uint8_t)(0.5f + (Bc * rgbMaxChannelF)),
                               ptrR,
                               ptrG,
                               ptrB);

            ptrR += rgbPixelBytes;
            ptrG += rgbPixelBytes;
            ptrB += rgbPixelBytes;
        }
    }
    avifFreeYUVToRGBLookUpTables(&unormFloatTableY, NULL);
    return AVIF_RESULT_OK;
}

static avifResult avifImageYUV8ToRGB16Color(const avifImage * image, avifRGBImage * rgb, avifReformatState * state)
{
    const float kr = state->yuv.kr;
    const float kg = state->yuv.kg;
    const float kb = state->yuv.kb;
    const uint32_t rgbPixelBytes = state->rgb.pixelBytes;
    float * unormFloatTableY = NULL;
    float * unormFloatTableUV = NULL;
    AVIF_CHECKERR(avifCreateYUVToRGBLookUpTables(&unormFloatTableY, &unormFloatTableUV, image->depth, state), AVIF_RESULT_OUT_OF_MEMORY);

    const float rgbMaxChannelF = state->rgb.maxChannelF;
    for (uint32_t j = 0; j < image->height; ++j) {
        const uint32_t uvJ = j >> state->yuv.formatInfo.chromaShiftY;
        const uint8_t * const ptrY = &image->yuvPlanes[AVIF_CHAN_Y][(j * image->yuvRowBytes[AVIF_CHAN_Y])];
        const uint8_t * const ptrU = &image->yuvPlanes[AVIF_CHAN_U][(uvJ * image->yuvRowBytes[AVIF_CHAN_U])];
        const uint8_t * const ptrV = &image->yuvPlanes[AVIF_CHAN_V][(uvJ * image->yuvRowBytes[AVIF_CHAN_V])];
        uint8_t * ptrR = &rgb->pixels[state->rgb.offsetBytesR + (j * rgb->rowBytes)];
        uint8_t * ptrG = &rgb->pixels[state->rgb.offsetBytesG + (j * rgb->rowBytes)];
        uint8_t * ptrB = &rgb->pixels[state->rgb.offsetBytesB + (j * rgb->rowBytes)];

        for (uint32_t i = 0; i < image->width; ++i) {
            uint32_t uvI = i >> state->yuv.formatInfo.chromaShiftX;

            // Convert unorm to float (no clamp necessary, the full uint8_t range is a legal lookup)
            const float Y = unormFloatTableY[ptrY[i]];
            const float Cb = unormFloatTableUV[ptrU[uvI]];
            const float Cr = unormFloatTableUV[ptrV[uvI]];

            const float R = Y + (2 * (1 - kr)) * Cr;
            const float B = Y + (2 * (1 - kb)) * Cb;
            const float G = Y - ((2 * ((kr * (1 - kr) * Cr) + (kb * (1 - kb) * Cb))) / kg);
            const float Rc = AVIF_CLAMP(R, 0.0f, 1.0f);
            const float Gc = AVIF_CLAMP(G, 0.0f, 1.0f);
            const float Bc = AVIF_CLAMP(B, 0.0f, 1.0f);

            *((uint16_t *)ptrR) = (uint16_t)(0.5f + (Rc * rgbMaxChannelF));
            *((uint16_t *)ptrG) = (uint16_t)(0.5f + (Gc * rgbMaxChannelF));
            *((uint16_t *)ptrB) = (uint16_t)(0.5f + (Bc * rgbMaxChannelF));

            ptrR += rgbPixelBytes;
            ptrG += rgbPixelBytes;
            ptrB += rgbPixelBytes;
        }
    }
    avifFreeYUVToRGBLookUpTables(&unormFloatTableY, &unormFloatTableUV);
    return AVIF_RESULT_OK;
}

static avifResult avifImageYUV8ToRGB16Mono(const avifImage * image, avifRGBImage * rgb, avifReformatState * state)
{
    const float kr = state->yuv.kr;
    const float kg = state->yuv.kg;
    const float kb = state->yuv.kb;
    const uint32_t rgbPixelBytes = state->rgb.pixelBytes;
    float * unormFloatTableY = NULL;
    AVIF_CHECKERR(avifCreateYUVToRGBLookUpTables(&unormFloatTableY, NULL, image->depth, state), AVIF_RESULT_OUT_OF_MEMORY);

    const float rgbMaxChannelF = state->rgb.maxChannelF;
    for (uint32_t j = 0; j < image->height; ++j) {
        const uint8_t * const ptrY = &image->yuvPlanes[AVIF_CHAN_Y][(j * image->yuvRowBytes[AVIF_CHAN_Y])];
        uint8_t * ptrR = &rgb->pixels[state->rgb.offsetBytesR + (j * rgb->rowBytes)];
        uint8_t * ptrG = &rgb->pixels[state->rgb.offsetBytesG + (j * rgb->rowBytes)];
        uint8_t * ptrB = &rgb->pixels[state->rgb.offsetBytesB + (j * rgb->rowBytes)];

        for (uint32_t i = 0; i < image->width; ++i) {
            // Convert unorm to float (no clamp necessary, the full uint8_t range is a legal lookup)
            const float Y = unormFloatTableY[ptrY[i]];
            const float Cb = 0.0f;
            const float Cr = 0.0f;

            const float R = Y + (2 * (1 - kr)) * Cr;
            const float B = Y + (2 * (1 - kb)) * Cb;
            const float G = Y - ((2 * ((kr * (1 - kr) * Cr) + (kb * (1 - kb) * Cb))) / kg);
            const float Rc = AVIF_CLAMP(R, 0.0f, 1.0f);
            const float Gc = AVIF_CLAMP(G, 0.0f, 1.0f);
            const float Bc = AVIF_CLAMP(B, 0.0f, 1.0f);

            *((uint16_t *)ptrR) = (uint16_t)(0.5f + (Rc * rgbMaxChannelF));
            *((uint16_t *)ptrG) = (uint16_t)(0.5f + (Gc * rgbMaxChannelF));
            *((uint16_t *)ptrB) = (uint16_t)(0.5f + (Bc * rgbMaxChannelF));

            ptrR += rgbPixelBytes;
            ptrG += rgbPixelBytes;
            ptrB += rgbPixelBytes;
        }
    }
    avifFreeYUVToRGBLookUpTables(&unormFloatTableY, NULL);
    return AVIF_RESULT_OK;
}

static avifResult avifImageIdentity8ToRGB8ColorFullRange(const avifImage * image, avifRGBImage * rgb, avifReformatState * state)
{
    const uint32_t rgbPixelBytes = state->rgb.pixelBytes;
    for (uint32_t j = 0; j < image->height; ++j) {
        const uint8_t * const ptrY = &image->yuvPlanes[AVIF_CHAN_Y][(j * image->yuvRowBytes[AVIF_CHAN_Y])];
        const uint8_t * const ptrU = &image->yuvPlanes[AVIF_CHAN_U][(j * image->yuvRowBytes[AVIF_CHAN_U])];
        const uint8_t * const ptrV = &image->yuvPlanes[AVIF_CHAN_V][(j * image->yuvRowBytes[AVIF_CHAN_V])];
        uint8_t * ptrR = &rgb->pixels[state->rgb.offsetBytesR + (j * rgb->rowBytes)];
        uint8_t * ptrG = &rgb->pixels[state->rgb.offsetBytesG + (j * rgb->rowBytes)];
        uint8_t * ptrB = &rgb->pixels[state->rgb.offsetBytesB + (j * rgb->rowBytes)];

        // This is intentionally a per-row conditional instead of a per-pixel
        // conditional. This makes the "else" path (much more common than the
        // "if" path) much faster than having a per-pixel branch.
        if (rgb->format == AVIF_RGB_FORMAT_RGB_565) {
            for (uint32_t i = 0; i < image->width; ++i) {
                *(uint16_t *)ptrR = RGB565(ptrV[i], ptrY[i], ptrU[i]);
                ptrR += rgbPixelBytes;
            }
        } else {
            for (uint32_t i = 0; i < image->width; ++i) {
                *ptrR = ptrV[i];
                *ptrG = ptrY[i];
                *ptrB = ptrU[i];
                ptrR += rgbPixelBytes;
                ptrG += rgbPixelBytes;
                ptrB += rgbPixelBytes;
            }
        }
    }
    return AVIF_RESULT_OK;
}

static avifResult avifImageYUV8ToRGB8Color(const avifImage * image, avifRGBImage * rgb, avifReformatState * state)
{
    const float kr = state->yuv.kr;
    const float kg = state->yuv.kg;
    const float kb = state->yuv.kb;
    const uint32_t rgbPixelBytes = state->rgb.pixelBytes;
    float * unormFloatTableY = NULL;
    float * unormFloatTableUV = NULL;
    AVIF_CHECKERR(avifCreateYUVToRGBLookUpTables(&unormFloatTableY, &unormFloatTableUV, image->depth, state), AVIF_RESULT_OUT_OF_MEMORY);

    const float rgbMaxChannelF = state->rgb.maxChannelF;
    for (uint32_t j = 0; j < image->height; ++j) {
        const uint32_t uvJ = j >> state->yuv.formatInfo.chromaShiftY;
        const uint8_t * const ptrY = &image->yuvPlanes[AVIF_CHAN_Y][(j * image->yuvRowBytes[AVIF_CHAN_Y])];
        const uint8_t * const ptrU = &image->yuvPlanes[AVIF_CHAN_U][(uvJ * image->yuvRowBytes[AVIF_CHAN_U])];
        const uint8_t * const ptrV = &image->yuvPlanes[AVIF_CHAN_V][(uvJ * image->yuvRowBytes[AVIF_CHAN_V])];
        uint8_t * ptrR = &rgb->pixels[state->rgb.offsetBytesR + (j * rgb->rowBytes)];
        uint8_t * ptrG = &rgb->pixels[state->rgb.offsetBytesG + (j * rgb->rowBytes)];
        uint8_t * ptrB = &rgb->pixels[state->rgb.offsetBytesB + (j * rgb->rowBytes)];

        for (uint32_t i = 0; i < image->width; ++i) {
            uint32_t uvI = i >> state->yuv.formatInfo.chromaShiftX;

            // Convert unorm to float (no clamp necessary, the full uint8_t range is a legal lookup)
            const float Y = unormFloatTableY[ptrY[i]];
            const float Cb = unormFloatTableUV[ptrU[uvI]];
            const float Cr = unormFloatTableUV[ptrV[uvI]];

            const float R = Y + (2 * (1 - kr)) * Cr;
            const float B = Y + (2 * (1 - kb)) * Cb;
            const float G = Y - ((2 * ((kr * (1 - kr) * Cr) + (kb * (1 - kb) * Cb))) / kg);
            const float Rc = AVIF_CLAMP(R, 0.0f, 1.0f);
            const float Gc = AVIF_CLAMP(G, 0.0f, 1.0f);
            const float Bc = AVIF_CLAMP(B, 0.0f, 1.0f);

            avifStoreRGB8Pixel(rgb->format,
                               (uint8_t)(0.5f + (Rc * rgbMaxChannelF)),
                               (uint8_t)(0.5f + (Gc * rgbMaxChannelF)),
                               (uint8_t)(0.5f + (Bc * rgbMaxChannelF)),
                               ptrR,
                               ptrG,
                               ptrB);

            ptrR += rgbPixelBytes;
            ptrG += rgbPixelBytes;
            ptrB += rgbPixelBytes;
        }
    }
    avifFreeYUVToRGBLookUpTables(&unormFloatTableY, &unormFloatTableUV);
    return AVIF_RESULT_OK;
}

static avifResult avifImageYUV8ToRGB8Mono(const avifImage * image, avifRGBImage * rgb, avifReformatState * state)
{
    const float kr = state->yuv.kr;
    const float kg = state->yuv.kg;
    const float kb = state->yuv.kb;
    const uint32_t rgbPixelBytes = state->rgb.pixelBytes;
    float * unormFloatTableY = NULL;
    AVIF_CHECKERR(avifCreateYUVToRGBLookUpTables(&unormFloatTableY, NULL, image->depth, state), AVIF_RESULT_OUT_OF_MEMORY);

    const float rgbMaxChannelF = state->rgb.maxChannelF;
    for (uint32_t j = 0; j < image->height; ++j) {
        const uint8_t * const ptrY = &image->yuvPlanes[AVIF_CHAN_Y][(j * image->yuvRowBytes[AVIF_CHAN_Y])];
        uint8_t * ptrR = &rgb->pixels[state->rgb.offsetBytesR + (j * rgb->rowBytes)];
        uint8_t * ptrG = &rgb->pixels[state->rgb.offsetBytesG + (j * rgb->rowBytes)];
        uint8_t * ptrB = &rgb->pixels[state->rgb.offsetBytesB + (j * rgb->rowBytes)];

        for (uint32_t i = 0; i < image->width; ++i) {
            // Convert unorm to float (no clamp necessary, the full uint8_t range is a legal lookup)
            const float Y = unormFloatTableY[ptrY[i]];
            const float Cb = 0.0f;
            const float Cr = 0.0f;

            const float R = Y + (2 * (1 - kr)) * Cr;
            const float B = Y + (2 * (1 - kb)) * Cb;
            const float G = Y - ((2 * ((kr * (1 - kr) * Cr) + (kb * (1 - kb) * Cb))) / kg);
            const float Rc = AVIF_CLAMP(R, 0.0f, 1.0f);
            const float Gc = AVIF_CLAMP(G, 0.0f, 1.0f);
            const float Bc = AVIF_CLAMP(B, 0.0f, 1.0f);

            avifStoreRGB8Pixel(rgb->format,
                               (uint8_t)(0.5f + (Rc * rgbMaxChannelF)),
                               (uint8_t)(0.5f + (Gc * rgbMaxChannelF)),
                               (uint8_t)(0.5f + (Bc * rgbMaxChannelF)),
                               ptrR,
                               ptrG,
                               ptrB);

            ptrR += rgbPixelBytes;
            ptrG += rgbPixelBytes;
            ptrB += rgbPixelBytes;
        }
    }
    avifFreeYUVToRGBLookUpTables(&unormFloatTableY, NULL);
    return AVIF_RESULT_OK;
}

// This constant comes from libyuv. For details, see here:
// https://chromium.googlesource.com/libyuv/libyuv/+/2f87e9a7/source/row_common.cc#3537
#define F16_MULTIPLIER 1.9259299444e-34f

typedef union avifF16
{
    float f;
    uint32_t u32;
} avifF16;

static avifResult avifRGBImageToF16(avifRGBImage * rgb)
{
    avifResult libyuvResult = AVIF_RESULT_NOT_IMPLEMENTED;
    if (!rgb->avoidLibYUV) {
        libyuvResult = avifRGBImageToF16LibYUV(rgb);
    }
    if (libyuvResult != AVIF_RESULT_NOT_IMPLEMENTED) {
        return libyuvResult;
    }
    const uint32_t channelCount = avifRGBFormatChannelCount(rgb->format);
    const float scale = 1.0f / ((1 << rgb->depth) - 1);
    const float multiplier = F16_MULTIPLIER * scale;
    uint16_t * pixelRowBase = (uint16_t *)rgb->pixels;
    const uint32_t stride = rgb->rowBytes >> 1;
    for (uint32_t j = 0; j < rgb->height; ++j) {
        uint16_t * pixel = pixelRowBase;
        for (uint32_t i = 0; i < rgb->width * channelCount; ++i, ++pixel) {
            avifF16 f16;
            f16.f = *pixel * multiplier;
            *pixel = (uint16_t)(f16.u32 >> 13);
        }
        pixelRowBase += stride;
    }
    return AVIF_RESULT_OK;
}

static avifResult avifImageYUVToRGBImpl(const avifImage * image, avifRGBImage * rgb, avifReformatState * state, avifAlphaMultiplyMode alphaMultiplyMode)
{
    avifBool convertedWithLibYUV = AVIF_FALSE;
    // Reformat alpha, if user asks for it, or (un)multiply processing needs it.
    avifBool reformatAlpha = avifRGBFormatHasAlpha(rgb->format) &&
                             (!rgb->ignoreAlpha || (alphaMultiplyMode != AVIF_ALPHA_MULTIPLY_MODE_NO_OP));
    // This value is used only when reformatAlpha is true.
    avifBool alphaReformattedWithLibYUV = AVIF_FALSE;
    if (!rgb->avoidLibYUV && ((alphaMultiplyMode == AVIF_ALPHA_MULTIPLY_MODE_NO_OP) || avifRGBFormatHasAlpha(rgb->format))) {
        avifResult libyuvResult = avifImageYUVToRGBLibYUV(image, rgb, reformatAlpha, &alphaReformattedWithLibYUV);
        if (libyuvResult == AVIF_RESULT_OK) {
            convertedWithLibYUV = AVIF_TRUE;
        } else {
            if (libyuvResult != AVIF_RESULT_NOT_IMPLEMENTED) {
                return libyuvResult;
            }
        }
    }

    if (reformatAlpha && !alphaReformattedWithLibYUV) {
        avifAlphaParams params;

        params.width = rgb->width;
        params.height = rgb->height;
        params.dstDepth = rgb->depth;
        params.dstPlane = rgb->pixels;
        params.dstRowBytes = rgb->rowBytes;
        params.dstOffsetBytes = state->rgb.offsetBytesA;
        params.dstPixelBytes = state->rgb.pixelBytes;

        if (image->alphaPlane && image->alphaRowBytes) {
            params.srcDepth = image->depth;
            params.srcPlane = image->alphaPlane;
            params.srcRowBytes = image->alphaRowBytes;
            params.srcOffsetBytes = 0;
            params.srcPixelBytes = state->yuv.channelBytes;

            avifReformatAlpha(&params);
        } else {
            avifFillAlpha(&params);
        }
    }

    if (!convertedWithLibYUV) {
        // libyuv is either unavailable or unable to perform the specific conversion required here.
        // Look over the available built-in "fast" routines for YUV->RGB conversion and see if one
        // fits the current combination, or as a last resort, call avifImageYUVAnyToRGBAnySlow(),
        // which handles every possibly YUV->RGB combination, but very slowly (in comparison).

        avifResult convertResult = AVIF_RESULT_NOT_IMPLEMENTED;

        const avifBool hasColor =
            (image->yuvRowBytes[AVIF_CHAN_U] && image->yuvRowBytes[AVIF_CHAN_V] && (image->yuvFormat != AVIF_PIXEL_FORMAT_YUV400));

        if ((!hasColor || (image->yuvFormat == AVIF_PIXEL_FORMAT_YUV444) ||
             ((rgb->chromaUpsampling == AVIF_CHROMA_UPSAMPLING_FASTEST) || (rgb->chromaUpsampling == AVIF_CHROMA_UPSAMPLING_NEAREST))) &&
            (alphaMultiplyMode == AVIF_ALPHA_MULTIPLY_MODE_NO_OP || avifRGBFormatHasAlpha(rgb->format))) {
            // Explanations on the above conditional:
            // * None of these fast paths currently support bilinear upsampling, so avoid all of them
            //   unless the YUV data isn't subsampled or they explicitly requested AVIF_CHROMA_UPSAMPLING_NEAREST.
            // * None of these fast paths currently handle alpha (un)multiply, so avoid all of them
            //   if we can't do alpha (un)multiply as a separated post step (destination format doesn't have alpha).

            if (state->yuv.mode == AVIF_REFORMAT_MODE_IDENTITY) {
                if ((image->depth == 8) && (rgb->depth == 8) && (image->yuvFormat == AVIF_PIXEL_FORMAT_YUV444) &&
                    (image->yuvRange == AVIF_RANGE_FULL)) {
                    convertResult = avifImageIdentity8ToRGB8ColorFullRange(image, rgb, state);
                }

                // TODO: Add more fast paths for identity
            } else if (state->yuv.mode == AVIF_REFORMAT_MODE_YUV_COEFFICIENTS) {
                if (image->depth > 8) {
                    // yuv:u16

                    if (rgb->depth > 8) {
                        // yuv:u16, rgb:u16

                        if (hasColor) {
                            convertResult = avifImageYUV16ToRGB16Color(image, rgb, state);
                        } else {
                            convertResult = avifImageYUV16ToRGB16Mono(image, rgb, state);
                        }
                    } else {
                        // yuv:u16, rgb:u8

                        if (hasColor) {
                            convertResult = avifImageYUV16ToRGB8Color(image, rgb, state);
                        } else {
                            convertResult = avifImageYUV16ToRGB8Mono(image, rgb, state);
                        }
                    }
                } else {
                    // yuv:u8

                    if (rgb->depth > 8) {
                        // yuv:u8, rgb:u16

                        if (hasColor) {
                            convertResult = avifImageYUV8ToRGB16Color(image, rgb, state);
                        } else {
                            convertResult = avifImageYUV8ToRGB16Mono(image, rgb, state);
                        }
                    } else {
                        // yuv:u8, rgb:u8

                        if (hasColor) {
                            convertResult = avifImageYUV8ToRGB8Color(image, rgb, state);
                        } else {
                            convertResult = avifImageYUV8ToRGB8Mono(image, rgb, state);
                        }
                    }
                }
            }
        }

        if (convertResult == AVIF_RESULT_NOT_IMPLEMENTED) {
            // If we get here, there is no fast path for this combination. Time to be slow!
            convertResult = avifImageYUVAnyToRGBAnySlow(image, rgb, state, alphaMultiplyMode);

            // The slow path also handles alpha (un)multiply, so forget the operation here.
            alphaMultiplyMode = AVIF_ALPHA_MULTIPLY_MODE_NO_OP;
        }

        if (convertResult != AVIF_RESULT_OK) {
            return convertResult;
        }
    }

    // Process alpha premultiplication, if necessary
    if (alphaMultiplyMode == AVIF_ALPHA_MULTIPLY_MODE_MULTIPLY) {
        avifResult result = avifRGBImagePremultiplyAlpha(rgb);
        if (result != AVIF_RESULT_OK) {
            return result;
        }
    } else if (alphaMultiplyMode == AVIF_ALPHA_MULTIPLY_MODE_UNMULTIPLY) {
        avifResult result = avifRGBImageUnpremultiplyAlpha(rgb);
        if (result != AVIF_RESULT_OK) {
            return result;
        }
    }

    // Convert pixels to half floats (F16), if necessary.
    if (rgb->isFloat) {
        return avifRGBImageToF16(rgb);
    }

    return AVIF_RESULT_OK;
}

typedef struct
{
#if defined(_WIN32)
    HANDLE thread;
#else
    pthread_t thread;
#endif
    avifImage image;
    avifRGBImage rgb;
    avifReformatState * state;
    avifAlphaMultiplyMode alphaMultiplyMode;
    avifResult result;
    avifBool threadCreated;
} YUVToRGBThreadData;

#if defined(_WIN32)
static unsigned int __stdcall avifImageYUVToRGBThreadWorker(void * arg)
#else
static void * avifImageYUVToRGBThreadWorker(void * arg)
#endif
{
    YUVToRGBThreadData * data = (YUVToRGBThreadData *)arg;
    data->result = avifImageYUVToRGBImpl(&data->image, &data->rgb, data->state, data->alphaMultiplyMode);
#if defined(_WIN32)
    return 0;
#else
    return NULL;
#endif
}

static avifBool avifCreateYUVToRGBThread(YUVToRGBThreadData * tdata)
{
#if defined(_WIN32)
    tdata->thread = (HANDLE)_beginthreadex(/*security=*/NULL,
                                           /*stack_size=*/0,
                                           &avifImageYUVToRGBThreadWorker,
                                           tdata,
                                           /*initflag=*/0,
                                           /*thrdaddr=*/NULL);
    return tdata->thread != NULL;
#else
    // TODO: Set the thread name for ease of debugging.
    return pthread_create(&tdata->thread, NULL, &avifImageYUVToRGBThreadWorker, tdata) == 0;
#endif
}

static avifBool avifJoinYUVToRGBThread(YUVToRGBThreadData * tdata)
{
#if defined(_WIN32)
    return WaitForSingleObject(tdata->thread, INFINITE) == WAIT_OBJECT_0 && CloseHandle(tdata->thread) != 0;
#else
    return pthread_join(tdata->thread, NULL) == 0;
#endif
}

avifResult avifImageYUVToRGB(const avifImage * image, avifRGBImage * rgb)
{
    // It is okay for rgb->maxThreads to be equal to zero in order to allow clients to zero initialize the avifRGBImage struct
    // with memset.
    if (!image->yuvPlanes[AVIF_CHAN_Y] || rgb->maxThreads < 0) {
        return AVIF_RESULT_REFORMAT_FAILED;
    }

    avifReformatState state;
    if (!avifPrepareReformatState(image, rgb, &state)) {
        return AVIF_RESULT_REFORMAT_FAILED;
    }

    avifAlphaMultiplyMode alphaMultiplyMode = AVIF_ALPHA_MULTIPLY_MODE_NO_OP;
    if (image->alphaPlane) {
        if (!avifRGBFormatHasAlpha(rgb->format) || rgb->ignoreAlpha) {
            // if we are converting some image with alpha into a format without alpha, we should do 'premultiply alpha' before
            // discarding alpha plane. This has the same effect of rendering this image on a black background, which makes sense.
            if (!image->alphaPremultiplied) {
                alphaMultiplyMode = AVIF_ALPHA_MULTIPLY_MODE_MULTIPLY;
            }
        } else {
            if (!image->alphaPremultiplied && rgb->alphaPremultiplied) {
                alphaMultiplyMode = AVIF_ALPHA_MULTIPLY_MODE_MULTIPLY;
            } else if (image->alphaPremultiplied && !rgb->alphaPremultiplied) {
                alphaMultiplyMode = AVIF_ALPHA_MULTIPLY_MODE_UNMULTIPLY;
            }
        }
    }

    // In practice, we rarely need more than 8 threads for YUV to RGB conversion.
    uint32_t jobs = AVIF_CLAMP(rgb->maxThreads, 1, 8);

    // When yuv format is 420 and chromaUpsampling could be BILINEAR, there is a dependency across the horizontal borders of each
    // job. So we disallow multithreading in that case.
    if (image->yuvFormat == AVIF_PIXEL_FORMAT_YUV420 && (rgb->chromaUpsampling == AVIF_CHROMA_UPSAMPLING_AUTOMATIC ||
                                                         rgb->chromaUpsampling == AVIF_CHROMA_UPSAMPLING_BEST_QUALITY ||
                                                         rgb->chromaUpsampling == AVIF_CHROMA_UPSAMPLING_BILINEAR)) {
        jobs = 1;
    }

    // Each thread worker needs at least 2 Y rows (to account for potential U/V subsampling).
    if (jobs == 1 || (image->height / 2) < jobs) {
        return avifImageYUVToRGBImpl(image, rgb, &state, alphaMultiplyMode);
    }

    const size_t byteCount = sizeof(YUVToRGBThreadData) * jobs;
    YUVToRGBThreadData * threadData = avifAlloc(byteCount);
    if (!threadData) {
        return AVIF_RESULT_OUT_OF_MEMORY;
    }
    memset(threadData, 0, byteCount);
    int rowsPerJob = image->height / jobs;
    if (rowsPerJob % 2) {
        ++rowsPerJob;
        jobs = (image->height + rowsPerJob - 1) / rowsPerJob; // ceil
    }
    const int rowsForLastJob = image->height - rowsPerJob * (jobs - 1);
    int startRow = 0;
    uint32_t i;
    for (i = 0; i < jobs; ++i, startRow += rowsPerJob) {
        YUVToRGBThreadData * tdata = &threadData[i];
        const avifCropRect rect = { .x = 0, .y = startRow, .width = image->width, .height = (i == jobs - 1) ? rowsForLastJob : rowsPerJob };
        if (avifImageSetViewRect(&tdata->image, image, &rect) != AVIF_RESULT_OK) {
            tdata->result = AVIF_RESULT_REFORMAT_FAILED;
            break;
        }

        tdata->rgb = *rgb;
        tdata->rgb.pixels += startRow * (size_t)rgb->rowBytes;
        tdata->rgb.height = tdata->image.height;

        tdata->state = &state;
        tdata->alphaMultiplyMode = alphaMultiplyMode;

        if (i > 0) {
            tdata->threadCreated = avifCreateYUVToRGBThread(tdata);
            if (!tdata->threadCreated) {
                tdata->result = AVIF_RESULT_REFORMAT_FAILED;
                break;
            }
        }
    }
    // If above loop ran successfully, run the first job in the current thread.
    if (i == jobs) {
        avifImageYUVToRGBThreadWorker(&threadData[0]);
    }
    avifResult result = AVIF_RESULT_OK;
    for (i = 0; i < jobs; ++i) {
        YUVToRGBThreadData * tdata = &threadData[i];
        if (tdata->threadCreated && !avifJoinYUVToRGBThread(tdata)) {
            result = AVIF_RESULT_REFORMAT_FAILED;
        }
        if (tdata->result != AVIF_RESULT_OK) {
            result = tdata->result;
        }
    }
    avifFree(threadData);
    return result;
}

// Limited -> Full
// Plan: subtract limited offset, then multiply by ratio of FULLSIZE/LIMITEDSIZE (rounding), then clamp.
// RATIO = (FULLY - 0) / (MAXLIMITEDY - MINLIMITEDY)
// -----------------------------------------
// ( ( (v - MINLIMITEDY)                    | subtract limited offset
//     * FULLY                              | multiply numerator of ratio
//   ) + ((MAXLIMITEDY - MINLIMITEDY) / 2)  | add 0.5 (half of denominator) to round
// ) / (MAXLIMITEDY - MINLIMITEDY)          | divide by denominator of ratio
// AVIF_CLAMP(v, 0, FULLY)                  | clamp to full range
// -----------------------------------------
#define LIMITED_TO_FULL(MINLIMITEDY, MAXLIMITEDY, FULLY)                                                 \
    v = (((v - MINLIMITEDY) * FULLY) + ((MAXLIMITEDY - MINLIMITEDY) / 2)) / (MAXLIMITEDY - MINLIMITEDY); \
    v = AVIF_CLAMP(v, 0, FULLY)

// Full -> Limited
// Plan: multiply by ratio of LIMITEDSIZE/FULLSIZE (rounding), then add limited offset, then clamp.
// RATIO = (MAXLIMITEDY - MINLIMITEDY) / (FULLY - 0)
// -----------------------------------------
// ( ( (v * (MAXLIMITEDY - MINLIMITEDY))    | multiply numerator of ratio
//     + (FULLY / 2)                        | add 0.5 (half of denominator) to round
//   ) / FULLY                              | divide by denominator of ratio
// ) + MINLIMITEDY                          | add limited offset
//  AVIF_CLAMP(v, MINLIMITEDY, MAXLIMITEDY) | clamp to limited range
// -----------------------------------------
#define FULL_TO_LIMITED(MINLIMITEDY, MAXLIMITEDY, FULLY)                           \
    v = (((v * (MAXLIMITEDY - MINLIMITEDY)) + (FULLY / 2)) / FULLY) + MINLIMITEDY; \
    v = AVIF_CLAMP(v, MINLIMITEDY, MAXLIMITEDY)

int avifLimitedToFullY(uint32_t depth, int v)
{
    switch (depth) {
        case 8:
            LIMITED_TO_FULL(16, 235, 255);
            break;
        case 10:
            LIMITED_TO_FULL(64, 940, 1023);
            break;
        case 12:
            LIMITED_TO_FULL(256, 3760, 4095);
            break;
    }
    return v;
}

int avifLimitedToFullUV(uint32_t depth, int v)
{
    switch (depth) {
        case 8:
            LIMITED_TO_FULL(16, 240, 255);
            break;
        case 10:
            LIMITED_TO_FULL(64, 960, 1023);
            break;
        case 12:
            LIMITED_TO_FULL(256, 3840, 4095);
            break;
    }
    return v;
}

int avifFullToLimitedY(uint32_t depth, int v)
{
    switch (depth) {
        case 8:
            FULL_TO_LIMITED(16, 235, 255);
            break;
        case 10:
            FULL_TO_LIMITED(64, 940, 1023);
            break;
        case 12:
            FULL_TO_LIMITED(256, 3760, 4095);
            break;
    }
    return v;
}

int avifFullToLimitedUV(uint32_t depth, int v)
{
    switch (depth) {
        case 8:
            FULL_TO_LIMITED(16, 240, 255);
            break;
        case 10:
            FULL_TO_LIMITED(64, 960, 1023);
            break;
        case 12:
            FULL_TO_LIMITED(256, 3840, 4095);
            break;
    }
    return v;
}

static inline uint16_t avifFloatToF16(float v)
{
    avifF16 f16;
    f16.f = v * F16_MULTIPLIER;
    return (uint16_t)(f16.u32 >> 13);
}

static inline float avifF16ToFloat(uint16_t v)
{
    avifF16 f16;
    f16.u32 = v << 13;
    return f16.f / F16_MULTIPLIER;
}

void avifGetRGBAPixel(const avifRGBImage * src, uint32_t x, uint32_t y, const avifRGBColorSpaceInfo * info, float rgbaPixel[4])
{
    assert(src != NULL);
    assert(!src->isFloat || src->depth == 16);
    assert(src->format != AVIF_RGB_FORMAT_RGB_565 || src->depth == 8);

    const uint8_t * const srcPixel = &src->pixels[y * src->rowBytes + x * info->pixelBytes];
    if (info->channelBytes > 1) {
        uint16_t r = *((uint16_t *)(&srcPixel[info->offsetBytesR]));
        uint16_t g = *((uint16_t *)(&srcPixel[info->offsetBytesG]));
        uint16_t b = *((uint16_t *)(&srcPixel[info->offsetBytesB]));
        uint16_t a = avifRGBFormatHasAlpha(src->format) ? *((uint16_t *)(&srcPixel[info->offsetBytesA])) : (uint16_t)info->maxChannel;
        if (src->isFloat) {
            rgbaPixel[0] = avifF16ToFloat(r);
            rgbaPixel[1] = avifF16ToFloat(g);
            rgbaPixel[2] = avifF16ToFloat(b);
            rgbaPixel[3] = avifRGBFormatHasAlpha(src->format) ? avifF16ToFloat(a) : 1.0f;
        } else {
            rgbaPixel[0] = r / info->maxChannelF;
            rgbaPixel[1] = g / info->maxChannelF;
            rgbaPixel[2] = b / info->maxChannelF;
            rgbaPixel[3] = a / info->maxChannelF;
        }
    } else {
        if (src->format == AVIF_RGB_FORMAT_RGB_565) {
            uint8_t r, g, b;
            avifGetRGB565(&srcPixel[info->offsetBytesR], &r, &g, &b);
            rgbaPixel[0] = r / info->maxChannelF;
            rgbaPixel[1] = g / info->maxChannelF;
            rgbaPixel[2] = b / info->maxChannelF;
            rgbaPixel[3] = 1.0f;
        } else {
            rgbaPixel[0] = srcPixel[info->offsetBytesR] / info->maxChannelF;
            rgbaPixel[1] = srcPixel[info->offsetBytesG] / info->maxChannelF;
            rgbaPixel[2] = srcPixel[info->offsetBytesB] / info->maxChannelF;
            rgbaPixel[3] = avifRGBFormatHasAlpha(src->format) ? (srcPixel[info->offsetBytesA] / info->maxChannelF) : 1.0f;
        }
    }
}

void avifSetRGBAPixel(const avifRGBImage * dst, uint32_t x, uint32_t y, const avifRGBColorSpaceInfo * info, const float rgbaPixel[4])
{
    assert(dst != NULL);
    assert(!dst->isFloat || dst->depth == 16);
    assert(dst->format != AVIF_RGB_FORMAT_RGB_565 || dst->depth == 8);
    assert(rgbaPixel[0] >= 0.0f && rgbaPixel[0] <= 1.0f);
    assert(rgbaPixel[1] >= 0.0f && rgbaPixel[1] <= 1.0f);
    assert(rgbaPixel[2] >= 0.0f && rgbaPixel[2] <= 1.0f);

    uint8_t * const dstPixel = &dst->pixels[y * dst->rowBytes + x * info->pixelBytes];

    uint8_t * const ptrR = &dstPixel[info->offsetBytesR];
    uint8_t * const ptrG = &dstPixel[info->offsetBytesG];
    uint8_t * const ptrB = &dstPixel[info->offsetBytesB];
    uint8_t * const ptrA = avifRGBFormatHasAlpha(dst->format) ? &dstPixel[info->offsetBytesA] : NULL;
    if (dst->depth > 8) {
        if (dst->isFloat) {
            *((uint16_t *)ptrR) = avifFloatToF16(rgbaPixel[0]);
            *((uint16_t *)ptrG) = avifFloatToF16(rgbaPixel[1]);
            *((uint16_t *)ptrB) = avifFloatToF16(rgbaPixel[2]);
            if (ptrA) {
                *((uint16_t *)ptrA) = avifFloatToF16(rgbaPixel[3]);
            }
        } else {
            *((uint16_t *)ptrR) = (uint16_t)(0.5f + (rgbaPixel[0] * info->maxChannelF));
            *((uint16_t *)ptrG) = (uint16_t)(0.5f + (rgbaPixel[1] * info->maxChannelF));
            *((uint16_t *)ptrB) = (uint16_t)(0.5f + (rgbaPixel[2] * info->maxChannelF));
            if (ptrA) {
                *((uint16_t *)ptrA) = (uint16_t)(0.5f + (rgbaPixel[3] * info->maxChannelF));
            }
        }
    } else {
        avifStoreRGB8Pixel(dst->format,
                           (uint8_t)(0.5f + (rgbaPixel[0] * info->maxChannelF)),
                           (uint8_t)(0.5f + (rgbaPixel[1] * info->maxChannelF)),
                           (uint8_t)(0.5f + (rgbaPixel[2] * info->maxChannelF)),
                           ptrR,
                           ptrG,
                           ptrB);
        if (ptrA) {
            *ptrA = (uint8_t)(0.5f + (rgbaPixel[3] * info->maxChannelF));
        }
    }
}
