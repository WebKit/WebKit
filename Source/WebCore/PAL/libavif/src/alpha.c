// Copyright 2020 Joe Drago. All rights reserved.
// SPDX-License-Identifier: BSD-2-Clause

#include "avif/internal.h"

#include <assert.h>
#include <string.h>

avifBool avifFillAlpha(const avifAlphaParams * const params)
{
    if (params->dstDepth > 8) {
        const uint16_t maxChannel = (uint16_t)((1 << params->dstDepth) - 1);
        for (uint32_t j = 0; j < params->height; ++j) {
            uint8_t * dstRow = &params->dstPlane[params->dstOffsetBytes + (j * params->dstRowBytes)];
            for (uint32_t i = 0; i < params->width; ++i) {
                *((uint16_t *)dstRow) = maxChannel;
                dstRow += params->dstPixelBytes;
            }
        }
    } else {
        // In this case, (1 << params->dstDepth) - 1 is always equal to 255.
        const uint8_t maxChannel = 255;
        for (uint32_t j = 0; j < params->height; ++j) {
            uint8_t * dstRow = &params->dstPlane[params->dstOffsetBytes + (j * params->dstRowBytes)];
            for (uint32_t i = 0; i < params->width; ++i) {
                *dstRow = maxChannel;
                dstRow += params->dstPixelBytes;
            }
        }
    }
    return AVIF_TRUE;
}

avifBool avifReformatAlpha(const avifAlphaParams * const params)
{
    const int srcMaxChannel = (1 << params->srcDepth) - 1;
    const int dstMaxChannel = (1 << params->dstDepth) - 1;
    const float srcMaxChannelF = (float)srcMaxChannel;
    const float dstMaxChannelF = (float)dstMaxChannel;

    if (params->srcDepth == params->dstDepth) {
        // no depth rescale

        if (params->srcDepth > 8) {
            // no depth rescale, uint16_t -> uint16_t

            for (uint32_t j = 0; j < params->height; ++j) {
                uint8_t * srcRow = &params->srcPlane[params->srcOffsetBytes + (j * params->srcRowBytes)];
                uint8_t * dstRow = &params->dstPlane[params->dstOffsetBytes + (j * params->dstRowBytes)];
                for (uint32_t i = 0; i < params->width; ++i) {
                    *((uint16_t *)&dstRow[i * params->dstPixelBytes]) = *((uint16_t *)&srcRow[i * params->srcPixelBytes]);
                }
            }
        } else {
            // no depth rescale, uint8_t -> uint8_t

            for (uint32_t j = 0; j < params->height; ++j) {
                uint8_t * srcRow = &params->srcPlane[params->srcOffsetBytes + (j * params->srcRowBytes)];
                uint8_t * dstRow = &params->dstPlane[params->dstOffsetBytes + (j * params->dstRowBytes)];
                for (uint32_t i = 0; i < params->width; ++i) {
                    dstRow[i * params->dstPixelBytes] = srcRow[i * params->srcPixelBytes];
                }
            }
        }
    } else {
        // depth rescale

        if (params->srcDepth > 8) {
            if (params->dstDepth > 8) {
                // depth rescale, uint16_t -> uint16_t

                for (uint32_t j = 0; j < params->height; ++j) {
                    uint8_t * srcRow = &params->srcPlane[params->srcOffsetBytes + (j * params->srcRowBytes)];
                    uint8_t * dstRow = &params->dstPlane[params->dstOffsetBytes + (j * params->dstRowBytes)];
                    for (uint32_t i = 0; i < params->width; ++i) {
                        int srcAlpha = *((uint16_t *)&srcRow[i * params->srcPixelBytes]);
                        float alphaF = (float)srcAlpha / srcMaxChannelF;
                        int dstAlpha = (int)(0.5f + (alphaF * dstMaxChannelF));
                        dstAlpha = AVIF_CLAMP(dstAlpha, 0, dstMaxChannel);
                        *((uint16_t *)&dstRow[i * params->dstPixelBytes]) = (uint16_t)dstAlpha;
                    }
                }
            } else {
                // depth rescale, uint16_t -> uint8_t

                for (uint32_t j = 0; j < params->height; ++j) {
                    uint8_t * srcRow = &params->srcPlane[params->srcOffsetBytes + (j * params->srcRowBytes)];
                    uint8_t * dstRow = &params->dstPlane[params->dstOffsetBytes + (j * params->dstRowBytes)];
                    for (uint32_t i = 0; i < params->width; ++i) {
                        int srcAlpha = *((uint16_t *)&srcRow[i * params->srcPixelBytes]);
                        float alphaF = (float)srcAlpha / srcMaxChannelF;
                        int dstAlpha = (int)(0.5f + (alphaF * dstMaxChannelF));
                        dstAlpha = AVIF_CLAMP(dstAlpha, 0, dstMaxChannel);
                        dstRow[i * params->dstPixelBytes] = (uint8_t)dstAlpha;
                    }
                }
            }
        } else {
            // If (srcDepth == 8), dstDepth must be >8 otherwise we'd be in the (params->srcDepth == params->dstDepth) block above.
            assert(params->dstDepth > 8);

            // depth rescale, uint8_t -> uint16_t
            for (uint32_t j = 0; j < params->height; ++j) {
                uint8_t * srcRow = &params->srcPlane[params->srcOffsetBytes + (j * params->srcRowBytes)];
                uint8_t * dstRow = &params->dstPlane[params->dstOffsetBytes + (j * params->dstRowBytes)];
                for (uint32_t i = 0; i < params->width; ++i) {
                    int srcAlpha = srcRow[i * params->srcPixelBytes];
                    float alphaF = (float)srcAlpha / srcMaxChannelF;
                    int dstAlpha = (int)(0.5f + (alphaF * dstMaxChannelF));
                    dstAlpha = AVIF_CLAMP(dstAlpha, 0, dstMaxChannel);
                    *((uint16_t *)&dstRow[i * params->dstPixelBytes]) = (uint16_t)dstAlpha;
                }
            }
        }
    }

    return AVIF_TRUE;
}

avifResult avifRGBImagePremultiplyAlpha(avifRGBImage * rgb)
{
    // no data
    if (!rgb->pixels || !rgb->rowBytes) {
        return AVIF_RESULT_REFORMAT_FAILED;
    }

    // no alpha.
    if (!avifRGBFormatHasAlpha(rgb->format)) {
        return AVIF_RESULT_INVALID_ARGUMENT;
    }

    avifResult libyuvResult = avifRGBImagePremultiplyAlphaLibYUV(rgb);
    if (libyuvResult != AVIF_RESULT_NOT_IMPLEMENTED) {
        return libyuvResult;
    }

    assert(rgb->depth >= 8 && rgb->depth <= 16);

    uint32_t max = (1 << rgb->depth) - 1;
    float maxF = (float)max;

    if (rgb->depth > 8) {
        if (rgb->format == AVIF_RGB_FORMAT_RGBA || rgb->format == AVIF_RGB_FORMAT_BGRA) {
            for (uint32_t j = 0; j < rgb->height; ++j) {
                uint8_t * row = &rgb->pixels[j * rgb->rowBytes];
                for (uint32_t i = 0; i < rgb->width; ++i) {
                    uint16_t * pixel = (uint16_t *)&row[i * 8];
                    uint16_t a = pixel[3];
                    if (a >= max) {
                        // opaque is no-op
                        continue;
                    } else if (a == 0) {
                        // result must be zero
                        pixel[0] = 0;
                        pixel[1] = 0;
                        pixel[2] = 0;
                    } else {
                        // a < maxF is always true now, so we don't need clamp here
                        pixel[0] = (uint16_t)avifRoundf((float)pixel[0] * (float)a / maxF);
                        pixel[1] = (uint16_t)avifRoundf((float)pixel[1] * (float)a / maxF);
                        pixel[2] = (uint16_t)avifRoundf((float)pixel[2] * (float)a / maxF);
                    }
                }
            }
        } else {
            for (uint32_t j = 0; j < rgb->height; ++j) {
                uint8_t * row = &rgb->pixels[j * rgb->rowBytes];
                for (uint32_t i = 0; i < rgb->width; ++i) {
                    uint16_t * pixel = (uint16_t *)&row[i * 8];
                    uint16_t a = pixel[0];
                    if (a >= max) {
                        continue;
                    } else if (a == 0) {
                        pixel[1] = 0;
                        pixel[2] = 0;
                        pixel[3] = 0;
                    } else {
                        pixel[1] = (uint16_t)avifRoundf((float)pixel[1] * (float)a / maxF);
                        pixel[2] = (uint16_t)avifRoundf((float)pixel[2] * (float)a / maxF);
                        pixel[3] = (uint16_t)avifRoundf((float)pixel[3] * (float)a / maxF);
                    }
                }
            }
        }
    } else {
        if (rgb->format == AVIF_RGB_FORMAT_RGBA || rgb->format == AVIF_RGB_FORMAT_BGRA) {
            for (uint32_t j = 0; j < rgb->height; ++j) {
                uint8_t * row = &rgb->pixels[j * rgb->rowBytes];
                for (uint32_t i = 0; i < rgb->width; ++i) {
                    uint8_t * pixel = &row[i * 4];
                    uint8_t a = pixel[3];
                    // uint8_t can't exceed 255
                    if (a == max) {
                        continue;
                    } else if (a == 0) {
                        pixel[0] = 0;
                        pixel[1] = 0;
                        pixel[2] = 0;
                    } else {
                        pixel[0] = (uint8_t)avifRoundf((float)pixel[0] * (float)a / maxF);
                        pixel[1] = (uint8_t)avifRoundf((float)pixel[1] * (float)a / maxF);
                        pixel[2] = (uint8_t)avifRoundf((float)pixel[2] * (float)a / maxF);
                    }
                }
            }
        } else {
            for (uint32_t j = 0; j < rgb->height; ++j) {
                uint8_t * row = &rgb->pixels[j * rgb->rowBytes];
                for (uint32_t i = 0; i < rgb->width; ++i) {
                    uint8_t * pixel = &row[i * 4];
                    uint8_t a = pixel[0];
                    if (a == max) {
                        continue;
                    } else if (a == 0) {
                        pixel[1] = 0;
                        pixel[2] = 0;
                        pixel[3] = 0;
                    } else {
                        pixel[1] = (uint8_t)avifRoundf((float)pixel[1] * (float)a / maxF);
                        pixel[2] = (uint8_t)avifRoundf((float)pixel[2] * (float)a / maxF);
                        pixel[3] = (uint8_t)avifRoundf((float)pixel[3] * (float)a / maxF);
                    }
                }
            }
        }
    }

    return AVIF_RESULT_OK;
}

avifResult avifRGBImageUnpremultiplyAlpha(avifRGBImage * rgb)
{
    // no data
    if (!rgb->pixels || !rgb->rowBytes) {
        return AVIF_RESULT_REFORMAT_FAILED;
    }

    // no alpha.
    if (!avifRGBFormatHasAlpha(rgb->format)) {
        return AVIF_RESULT_REFORMAT_FAILED;
    }

    avifResult libyuvResult = avifRGBImageUnpremultiplyAlphaLibYUV(rgb);
    if (libyuvResult != AVIF_RESULT_NOT_IMPLEMENTED) {
        return libyuvResult;
    }

    assert(rgb->depth >= 8 && rgb->depth <= 16);

    uint32_t max = (1 << rgb->depth) - 1;
    float maxF = (float)max;

    if (rgb->depth > 8) {
        if (rgb->format == AVIF_RGB_FORMAT_RGBA || rgb->format == AVIF_RGB_FORMAT_BGRA) {
            for (uint32_t j = 0; j < rgb->height; ++j) {
                uint8_t * row = &rgb->pixels[j * rgb->rowBytes];
                for (uint32_t i = 0; i < rgb->width; ++i) {
                    uint16_t * pixel = (uint16_t *)&row[i * 8];
                    uint16_t a = pixel[3];
                    if (a >= max) {
                        // opaque is no-op
                        continue;
                    } else if (a == 0) {
                        // prevent division by zero
                        pixel[0] = 0;
                        pixel[1] = 0;
                        pixel[2] = 0;
                    } else {
                        float c1 = avifRoundf((float)pixel[0] * maxF / (float)a);
                        float c2 = avifRoundf((float)pixel[1] * maxF / (float)a);
                        float c3 = avifRoundf((float)pixel[2] * maxF / (float)a);
                        pixel[0] = (uint16_t)AVIF_MIN(c1, maxF);
                        pixel[1] = (uint16_t)AVIF_MIN(c2, maxF);
                        pixel[2] = (uint16_t)AVIF_MIN(c3, maxF);
                    }
                }
            }
        } else {
            for (uint32_t j = 0; j < rgb->height; ++j) {
                uint8_t * row = &rgb->pixels[j * rgb->rowBytes];
                for (uint32_t i = 0; i < rgb->width; ++i) {
                    uint16_t * pixel = (uint16_t *)&row[i * 8];
                    uint16_t a = pixel[0];
                    if (a >= max) {
                        continue;
                    } else if (a == 0) {
                        pixel[1] = 0;
                        pixel[2] = 0;
                        pixel[3] = 0;
                    } else {
                        float c1 = avifRoundf((float)pixel[1] * maxF / (float)a);
                        float c2 = avifRoundf((float)pixel[2] * maxF / (float)a);
                        float c3 = avifRoundf((float)pixel[3] * maxF / (float)a);
                        pixel[1] = (uint16_t)AVIF_MIN(c1, maxF);
                        pixel[2] = (uint16_t)AVIF_MIN(c2, maxF);
                        pixel[3] = (uint16_t)AVIF_MIN(c3, maxF);
                    }
                }
            }
        }
    } else {
        if (rgb->format == AVIF_RGB_FORMAT_RGBA || rgb->format == AVIF_RGB_FORMAT_BGRA) {
            for (uint32_t j = 0; j < rgb->height; ++j) {
                uint8_t * row = &rgb->pixels[j * rgb->rowBytes];
                for (uint32_t i = 0; i < rgb->width; ++i) {
                    uint8_t * pixel = &row[i * 4];
                    uint8_t a = pixel[3];
                    if (a == max) {
                        continue;
                    } else if (a == 0) {
                        pixel[0] = 0;
                        pixel[1] = 0;
                        pixel[2] = 0;
                    } else {
                        float c1 = avifRoundf((float)pixel[0] * maxF / (float)a);
                        float c2 = avifRoundf((float)pixel[1] * maxF / (float)a);
                        float c3 = avifRoundf((float)pixel[2] * maxF / (float)a);
                        pixel[0] = (uint8_t)AVIF_MIN(c1, maxF);
                        pixel[1] = (uint8_t)AVIF_MIN(c2, maxF);
                        pixel[2] = (uint8_t)AVIF_MIN(c3, maxF);
                    }
                }
            }
        } else {
            for (uint32_t j = 0; j < rgb->height; ++j) {
                uint8_t * row = &rgb->pixels[j * rgb->rowBytes];
                for (uint32_t i = 0; i < rgb->width; ++i) {
                    uint8_t * pixel = &row[i * 4];
                    uint8_t a = pixel[0];
                    if (a == max) {
                        continue;
                    } else if (a == 0) {
                        pixel[1] = 0;
                        pixel[2] = 0;
                        pixel[3] = 0;
                    } else {
                        float c1 = avifRoundf((float)pixel[1] * maxF / (float)a);
                        float c2 = avifRoundf((float)pixel[2] * maxF / (float)a);
                        float c3 = avifRoundf((float)pixel[3] * maxF / (float)a);
                        pixel[1] = (uint8_t)AVIF_MIN(c1, maxF);
                        pixel[2] = (uint8_t)AVIF_MIN(c2, maxF);
                        pixel[3] = (uint8_t)AVIF_MIN(c3, maxF);
                    }
                }
            }
        }
    }

    return AVIF_RESULT_OK;
}
