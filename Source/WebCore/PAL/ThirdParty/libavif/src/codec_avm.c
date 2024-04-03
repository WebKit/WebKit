// Copyright 2023 Google LLC
// SPDX-License-Identifier: BSD-2-Clause

#include "avif/internal.h"

#include "aom/aom_decoder.h"
#include "aom/aom_encoder.h"
#include "aom/aomcx.h"
#include "aom/aomdx.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

struct avifCodecInternal
{
    avifBool decoderInitialized;
    aom_codec_ctx_t decoder;
    aom_codec_iter_t iter;
    aom_image_t * image;

    avifBool encoderInitialized;
    aom_codec_ctx_t encoder;
    struct aom_codec_enc_cfg cfg;
    avifPixelFormatInfo formatInfo;
    aom_img_fmt_t aomFormat;
    avifBool monochromeEnabled;
    // Whether 'tuning' (of the specified distortion metric) was set with an
    // avifEncoderSetCodecSpecificOption(encoder, "tune", value) call.
    avifBool tuningSet;
    uint32_t currentLayer;
};

static void avmCodecDestroyInternal(avifCodec * codec)
{
    if (codec->internal->decoderInitialized) {
        aom_codec_destroy(&codec->internal->decoder);
    }

    if (codec->internal->encoderInitialized) {
        aom_codec_destroy(&codec->internal->encoder);
    }

    avifFree(codec->internal);
}

static avifResult avifCheckCodecVersionAVM()
{
    // The minimum supported version of avm is the anchor 4.0.0.
    // aom_codec.h says: aom_codec_version() == (major<<16 | minor<<8 | patch)
    AVIF_CHECKERR((aom_codec_version() >> 16) >= 4, AVIF_RESULT_NO_CODEC_AVAILABLE);
    return AVIF_RESULT_OK;
}

static avifBool avmCodecGetNextImage(struct avifCodec * codec,
                                     struct avifDecoder * decoder,
                                     const avifDecodeSample * sample,
                                     avifBool alpha,
                                     avifBool * isLimitedRangeAlpha,
                                     avifImage * image)
{
    if (!codec->internal->decoderInitialized) {
        AVIF_CHECKRES(avifCheckCodecVersionAVM());

        aom_codec_dec_cfg_t cfg;
        memset(&cfg, 0, sizeof(aom_codec_dec_cfg_t));
        cfg.threads = decoder->maxThreads;

        aom_codec_iface_t * decoder_interface = aom_codec_av1_dx();
        if (aom_codec_dec_init(&codec->internal->decoder, decoder_interface, &cfg, 0)) {
            return AVIF_FALSE;
        }
        codec->internal->decoderInitialized = AVIF_TRUE;

        if (aom_codec_control(&codec->internal->decoder, AV1D_SET_OUTPUT_ALL_LAYERS, codec->allLayers)) {
            return AVIF_FALSE;
        }
        if (aom_codec_control(&codec->internal->decoder, AV1D_SET_OPERATING_POINT, codec->operatingPoint)) {
            return AVIF_FALSE;
        }

        codec->internal->iter = NULL;
    }

    aom_image_t * nextFrame = NULL;
    uint8_t spatialID = AVIF_SPATIAL_ID_UNSET;
    for (;;) {
        nextFrame = aom_codec_get_frame(&codec->internal->decoder, &codec->internal->iter);
        if (nextFrame) {
            if (spatialID != AVIF_SPATIAL_ID_UNSET) {
                if (spatialID == nextFrame->spatial_id) {
                    // Found the correct spatial_id.
                    break;
                }
            } else {
                // Got an image!
                break;
            }
        } else if (sample) {
            codec->internal->iter = NULL;
            if (aom_codec_decode(&codec->internal->decoder, sample->data.data, sample->data.size, NULL)) {
                return AVIF_FALSE;
            }
            spatialID = sample->spatialID;
            sample = NULL;
        } else {
            break;
        }
    }

    if (nextFrame) {
        codec->internal->image = nextFrame;
    } else {
        if (alpha && codec->internal->image) {
            // Special case: reuse last alpha frame
        } else {
            return AVIF_FALSE;
        }
    }

    avifBool isColor = !alpha;
    if (isColor) {
        // Color (YUV) planes - set image to correct size / format, fill color

        avifPixelFormat yuvFormat = AVIF_PIXEL_FORMAT_NONE;
        switch (codec->internal->image->fmt) {
            case AOM_IMG_FMT_I420:
            case AOM_IMG_FMT_AOMI420:
            case AOM_IMG_FMT_I42016:
                yuvFormat = AVIF_PIXEL_FORMAT_YUV420;
                break;
            case AOM_IMG_FMT_I422:
            case AOM_IMG_FMT_I42216:
                yuvFormat = AVIF_PIXEL_FORMAT_YUV422;
                break;
            case AOM_IMG_FMT_I444:
            case AOM_IMG_FMT_I44416:
                yuvFormat = AVIF_PIXEL_FORMAT_YUV444;
                break;
            case AOM_IMG_FMT_NONE:
            case AOM_IMG_FMT_YV12:
            case AOM_IMG_FMT_AOMYV12:
            case AOM_IMG_FMT_YV1216:
            default:
                return AVIF_FALSE;
        }
        if (codec->internal->image->monochrome) {
            yuvFormat = AVIF_PIXEL_FORMAT_YUV400;
        }

        if (image->width && image->height) {
            if ((image->width != codec->internal->image->d_w) || (image->height != codec->internal->image->d_h) ||
                (image->depth != codec->internal->image->bit_depth) || (image->yuvFormat != yuvFormat)) {
                // Throw it all out
                avifImageFreePlanes(image, AVIF_PLANES_ALL);
            }
        }
        image->width = codec->internal->image->d_w;
        image->height = codec->internal->image->d_h;
        image->depth = codec->internal->image->bit_depth;

        image->yuvFormat = yuvFormat;
        image->yuvRange = (codec->internal->image->range == AOM_CR_STUDIO_RANGE) ? AVIF_RANGE_LIMITED : AVIF_RANGE_FULL;
        image->yuvChromaSamplePosition = (avifChromaSamplePosition)codec->internal->image->csp;

        image->colorPrimaries = (avifColorPrimaries)codec->internal->image->cp;
        image->transferCharacteristics = (avifTransferCharacteristics)codec->internal->image->tc;
        image->matrixCoefficients = (avifMatrixCoefficients)codec->internal->image->mc;

        avifImageFreePlanes(image, AVIF_PLANES_YUV);
        int yuvPlaneCount = (yuvFormat == AVIF_PIXEL_FORMAT_YUV400) ? 1 : 3;

        // avifImage assumes that a depth of 8 bits means an 8-bit buffer.
        // aom_image does not. The buffer depth depends on fmt|AOM_IMG_FMT_HIGHBITDEPTH, even for 8-bit values.
        if (!avifImageUsesU16(image) && (codec->internal->image->fmt & AOM_IMG_FMT_HIGHBITDEPTH)) {
            AVIF_CHECK(avifImageAllocatePlanes(image, AVIF_PLANES_YUV) == AVIF_RESULT_OK);
            for (int yuvPlane = 0; yuvPlane < yuvPlaneCount; ++yuvPlane) {
                const uint32_t planeWidth = avifImagePlaneWidth(image, yuvPlane);
                const uint32_t planeHeight = avifImagePlaneHeight(image, yuvPlane);
                const uint8_t * srcRow = codec->internal->image->planes[yuvPlane];
                uint8_t * dstRow = avifImagePlane(image, yuvPlane);
                const uint32_t dstRowBytes = avifImagePlaneRowBytes(image, yuvPlane);
                for (uint32_t y = 0; y < planeHeight; ++y) {
                    const uint16_t * srcRow16 = (const uint16_t *)srcRow;
                    for (uint32_t x = 0; x < planeWidth; ++x) {
                        dstRow[x] = (uint8_t)srcRow16[x];
                    }
                    srcRow += codec->internal->image->stride[yuvPlane];
                    dstRow += dstRowBytes;
                }
            }
        } else {
            // Steal the pointers from the decoder's image directly
            for (int yuvPlane = 0; yuvPlane < yuvPlaneCount; ++yuvPlane) {
                image->yuvPlanes[yuvPlane] = codec->internal->image->planes[yuvPlane];
                image->yuvRowBytes[yuvPlane] = codec->internal->image->stride[yuvPlane];
            }
            image->imageOwnsYUVPlanes = AVIF_FALSE;
        }
    } else {
        // Alpha plane - ensure image is correct size, fill color

        if (image->width && image->height) {
            if ((image->width != codec->internal->image->d_w) || (image->height != codec->internal->image->d_h) ||
                (image->depth != codec->internal->image->bit_depth)) {
                // Alpha plane doesn't match previous alpha plane decode, bail out
                return AVIF_FALSE;
            }
        }
        image->width = codec->internal->image->d_w;
        image->height = codec->internal->image->d_h;
        image->depth = codec->internal->image->bit_depth;

        avifImageFreePlanes(image, AVIF_PLANES_A);

        if (!avifImageUsesU16(image) && (codec->internal->image->fmt & AOM_IMG_FMT_HIGHBITDEPTH)) {
            AVIF_CHECK(avifImageAllocatePlanes(image, AVIF_PLANES_A) == AVIF_RESULT_OK);
            const uint8_t * srcRow = codec->internal->image->planes[0];
            uint8_t * dstRow = image->alphaPlane;
            for (uint32_t y = 0; y < image->height; ++y) {
                const uint16_t * srcRow16 = (const uint16_t *)srcRow;
                for (uint32_t x = 0; x < image->width; ++x) {
                    dstRow[x] = (uint8_t)srcRow16[x];
                }
                srcRow += codec->internal->image->stride[0];
                dstRow += image->alphaRowBytes;
            }
        } else {
            image->alphaPlane = codec->internal->image->planes[0];
            image->alphaRowBytes = codec->internal->image->stride[0];
            image->imageOwnsAlphaPlane = AVIF_FALSE;
        }
        *isLimitedRangeAlpha = (codec->internal->image->range == AOM_CR_STUDIO_RANGE);
    }

    return AVIF_TRUE;
}

static aom_img_fmt_t avifImageCalcAOMFmt(const avifImage * image, avifBool alpha)
{
    aom_img_fmt_t fmt;
    if (alpha) {
        // We're going monochrome, who cares about chroma quality
        fmt = AOM_IMG_FMT_I420;
    } else {
        switch (image->yuvFormat) {
            case AVIF_PIXEL_FORMAT_YUV444:
                fmt = AOM_IMG_FMT_I444;
                break;
            case AVIF_PIXEL_FORMAT_YUV422:
                fmt = AOM_IMG_FMT_I422;
                break;
            case AVIF_PIXEL_FORMAT_YUV420:
            case AVIF_PIXEL_FORMAT_YUV400:
                fmt = AOM_IMG_FMT_I420;
                break;
            case AVIF_PIXEL_FORMAT_NONE:
            case AVIF_PIXEL_FORMAT_COUNT:
            default:
                return AOM_IMG_FMT_NONE;
        }
    }

    if (image->depth > 8) {
        fmt |= AOM_IMG_FMT_HIGHBITDEPTH;
    }

    return fmt;
}

static avifBool aomOptionParseInt(const char * str, int * val)
{
    char * endptr;
    const long rawval = strtol(str, &endptr, 10);

    if (str[0] != '\0' && endptr[0] == '\0' && rawval >= INT_MIN && rawval <= INT_MAX) {
        *val = (int)rawval;
        return AVIF_TRUE;
    }

    return AVIF_FALSE;
}

static avifBool aomOptionParseUInt(const char * str, unsigned int * val)
{
    char * endptr;
    const unsigned long rawval = strtoul(str, &endptr, 10);

    if (str[0] != '\0' && endptr[0] == '\0' && rawval <= UINT_MAX) {
        *val = (unsigned int)rawval;
        return AVIF_TRUE;
    }

    return AVIF_FALSE;
}

struct aomOptionEnumList
{
    const char * name;
    int val;
};

static avifBool aomOptionParseEnum(const char * str, const struct aomOptionEnumList * enums, int * val)
{
    const struct aomOptionEnumList * listptr;
    long int rawval;
    char * endptr;

    // First see if the value can be parsed as a raw value.
    rawval = strtol(str, &endptr, 10);
    if (str[0] != '\0' && endptr[0] == '\0') {
        // Got a raw value, make sure it's valid.
        for (listptr = enums; listptr->name; listptr++)
            if (listptr->val == rawval) {
                *val = (int)rawval;
                return AVIF_TRUE;
            }
    }

    // Next see if it can be parsed as a string.
    for (listptr = enums; listptr->name; listptr++) {
        if (!strcmp(str, listptr->name)) {
            *val = listptr->val;
            return AVIF_TRUE;
        }
    }

    return AVIF_FALSE;
}

static const struct aomOptionEnumList endUsageEnum[] = { //
    { "vbr", AOM_VBR },                                  // Variable Bit Rate (VBR) mode
    { "cbr", AOM_CBR },                                  // Constant Bit Rate (CBR) mode
    { "cq", AOM_CQ },                                    // Constrained Quality (CQ) mode
    { "q", AOM_Q },                                      // Constant Quality (Q) mode
    { NULL, 0 }
};

// Returns true if <key> equals <name> or <prefix><name>, where <prefix> is "color:" or "alpha:"
// or the abbreviated form "c:" or "a:".
static avifBool avifKeyEqualsName(const char * key, const char * name, avifBool alpha)
{
    const char * prefix = alpha ? "alpha:" : "color:";
    size_t prefixLen = 6;
    const char * shortPrefix = alpha ? "a:" : "c:";
    size_t shortPrefixLen = 2;
    return !strcmp(key, name) || (!strncmp(key, prefix, prefixLen) && !strcmp(key + prefixLen, name)) ||
           (!strncmp(key, shortPrefix, shortPrefixLen) && !strcmp(key + shortPrefixLen, name));
}

static avifBool avifProcessAOMOptionsPreInit(avifCodec * codec, avifBool alpha, struct aom_codec_enc_cfg * cfg)
{
    for (uint32_t i = 0; i < codec->csOptions->count; ++i) {
        avifCodecSpecificOption * entry = &codec->csOptions->entries[i];
        int val;
        if (avifKeyEqualsName(entry->key, "end-usage", alpha)) { // Rate control mode
            if (!aomOptionParseEnum(entry->value, endUsageEnum, &val)) {
                avifDiagnosticsPrintf(codec->diag, "Invalid value for end-usage: %s", entry->value);
                return AVIF_FALSE;
            }
            cfg->rc_end_usage = val;
        }
    }
    return AVIF_TRUE;
}

typedef enum
{
    AVIF_AOM_OPTION_NUL = 0,
    AVIF_AOM_OPTION_STR,
    AVIF_AOM_OPTION_INT,
    AVIF_AOM_OPTION_UINT,
    AVIF_AOM_OPTION_ENUM,
} aomOptionType;

struct aomOptionDef
{
    const char * name;
    int controlId;
    aomOptionType type;
    // If type is AVIF_AOM_OPTION_ENUM, this must be set. Otherwise should be NULL.
    const struct aomOptionEnumList * enums;
};

static const struct aomOptionEnumList tuningEnum[] = { //
    { "psnr", AOM_TUNE_PSNR },                         //
    { "ssim", AOM_TUNE_SSIM },                         //
    { NULL, 0 }
};

static const struct aomOptionDef aomOptionDefs[] = {
    // Adaptive quantization mode
    { "aq-mode", AV1E_SET_AQ_MODE, AVIF_AOM_OPTION_UINT, NULL },
    // Constant/Constrained Quality level
    { "qp-level", AOME_SET_QP, AVIF_AOM_OPTION_UINT, NULL },
    // Enable delta quantization in chroma planes
    { "enable-chroma-deltaq", AV1E_SET_ENABLE_CHROMA_DELTAQ, AVIF_AOM_OPTION_INT, NULL },
    // Bias towards block sharpness in rate-distortion optimization of transform coefficients
    { "sharpness", AOME_SET_SHARPNESS, AVIF_AOM_OPTION_UINT, NULL },
    // Tune distortion metric
    { "tune", AOME_SET_TUNING, AVIF_AOM_OPTION_ENUM, tuningEnum },
    // Film grain test vector
    { "film-grain-test", AV1E_SET_FILM_GRAIN_TEST_VECTOR, AVIF_AOM_OPTION_INT, NULL },
    // Film grain table file
    { "film-grain-table", AV1E_SET_FILM_GRAIN_TABLE, AVIF_AOM_OPTION_STR, NULL },

    // Sentinel
    { NULL, 0, AVIF_AOM_OPTION_NUL, NULL }
};

static avifBool avifProcessAOMOptionsPostInit(avifCodec * codec, avifBool alpha)
{
    for (uint32_t i = 0; i < codec->csOptions->count; ++i) {
        avifCodecSpecificOption * entry = &codec->csOptions->entries[i];
        // Skip options for the other kind of plane.
        const char * otherPrefix = alpha ? "color:" : "alpha:";
        size_t otherPrefixLen = 6;
        const char * otherShortPrefix = alpha ? "c:" : "a:";
        size_t otherShortPrefixLen = 2;
        if (!strncmp(entry->key, otherPrefix, otherPrefixLen) || !strncmp(entry->key, otherShortPrefix, otherShortPrefixLen)) {
            continue;
        }

        // Skip options processed by avifProcessAOMOptionsPreInit.
        if (avifKeyEqualsName(entry->key, "end-usage", alpha)) {
            continue;
        }

        const char * prefix = alpha ? "alpha:" : "color:";
        size_t prefixLen = 6;
        const char * shortPrefix = alpha ? "a:" : "c:";
        size_t shortPrefixLen = 2;
        const char * key = entry->key;
        if (!strncmp(key, prefix, prefixLen)) {
            key += prefixLen;
        } else if (!strncmp(key, shortPrefix, shortPrefixLen)) {
            key += shortPrefixLen;
        }
        if (aom_codec_set_option(&codec->internal->encoder, key, entry->value) != AOM_CODEC_OK) {
            avifDiagnosticsPrintf(codec->diag,
                                  "aom_codec_set_option(\"%s\", \"%s\") failed: %s: %s",
                                  key,
                                  entry->value,
                                  aom_codec_error(&codec->internal->encoder),
                                  aom_codec_error_detail(&codec->internal->encoder));
            return AVIF_FALSE;
        }
        if (!strcmp(key, "tune")) {
            codec->internal->tuningSet = AVIF_TRUE;
        }

        avifBool match = AVIF_FALSE;
        for (int j = 0; aomOptionDefs[j].name; ++j) {
            if (avifKeyEqualsName(entry->key, aomOptionDefs[j].name, alpha)) {
                match = AVIF_TRUE;
                avifBool success = AVIF_FALSE;
                int valInt;
                unsigned int valUInt;
                switch (aomOptionDefs[j].type) {
                    case AVIF_AOM_OPTION_NUL:
                        success = AVIF_FALSE;
                        break;
                    case AVIF_AOM_OPTION_STR:
                        success = aom_codec_control(&codec->internal->encoder, aomOptionDefs[j].controlId, entry->value) == AOM_CODEC_OK;
                        break;
                    case AVIF_AOM_OPTION_INT:
                        success = aomOptionParseInt(entry->value, &valInt) &&
                                  aom_codec_control(&codec->internal->encoder, aomOptionDefs[j].controlId, valInt) == AOM_CODEC_OK;
                        break;
                    case AVIF_AOM_OPTION_UINT:
                        success = aomOptionParseUInt(entry->value, &valUInt) &&
                                  aom_codec_control(&codec->internal->encoder, aomOptionDefs[j].controlId, valUInt) == AOM_CODEC_OK;
                        break;
                    case AVIF_AOM_OPTION_ENUM:
                        success = aomOptionParseEnum(entry->value, aomOptionDefs[j].enums, &valInt) &&
                                  aom_codec_control(&codec->internal->encoder, aomOptionDefs[j].controlId, valInt) == AOM_CODEC_OK;
                        break;
                }
                if (!success) {
                    return AVIF_FALSE;
                }
                if (aomOptionDefs[j].controlId == AOME_SET_TUNING) {
                    codec->internal->tuningSet = AVIF_TRUE;
                }
                break;
            }
        }
        if (!match) {
            return AVIF_FALSE;
        }
    }
    return AVIF_TRUE;
}

struct aomScalingModeMapList
{
    avifFraction avifMode;
    AOM_SCALING_MODE aomMode;
};

static const struct aomScalingModeMapList scalingModeMap[] = {
    { { 1, 1 }, AOME_NORMAL },    { { 1, 2 }, AOME_ONETWO },    { { 1, 4 }, AOME_ONEFOUR },  { { 1, 8 }, AOME_ONEEIGHT },
    { { 3, 4 }, AOME_THREEFOUR }, { { 3, 5 }, AOME_THREEFIVE }, { { 4, 5 }, AOME_FOURFIVE },
};

static const int scalingModeMapSize = sizeof(scalingModeMap) / sizeof(scalingModeMap[0]);

static avifBool avifFindAOMScalingMode(const avifFraction * avifMode, AOM_SCALING_MODE * aomMode)
{
    avifFraction simplifiedFraction = *avifMode;
    avifFractionSimplify(&simplifiedFraction);
    for (int i = 0; i < scalingModeMapSize; ++i) {
        if (scalingModeMap[i].avifMode.n == simplifiedFraction.n && scalingModeMap[i].avifMode.d == simplifiedFraction.d) {
            *aomMode = scalingModeMap[i].aomMode;
            return AVIF_TRUE;
        }
    }

    return AVIF_FALSE;
}

// Scales from aom's [0:63] to avm's [0:255]. TODO(yguyon): Accept [0:255] directly in avifEncoder.
static int avmScaleQuantizer(int quantizer)
{
    return AVIF_CLAMP((quantizer * 255 + 31) / 63, 0, 255);
}

static avifBool avmCodecEncodeFinish(avifCodec * codec, avifCodecEncodeOutput * output);

static avifResult avmCodecEncodeImage(avifCodec * codec,
                                      avifEncoder * encoder,
                                      const avifImage * image,
                                      avifBool alpha,
                                      int tileRowsLog2,
                                      int tileColsLog2,
                                      int quantizer,
                                      avifEncoderChanges encoderChanges,
                                      avifBool disableLaggedOutput,
                                      avifAddImageFlags addImageFlags,
                                      avifCodecEncodeOutput * output)
{
    struct aom_codec_enc_cfg * cfg = &codec->internal->cfg;
    avifBool quantizerUpdated = AVIF_FALSE;

    // For encoder->scalingMode.horizontal and encoder->scalingMode.vertical to take effect in AV2
    // encoder, config should be applied for each frame, so we don't care about changes on these
    // two fields.
    encoderChanges &= ~AVIF_ENCODER_CHANGE_SCALING_MODE;

    if (!codec->internal->encoderInitialized) {
        AVIF_CHECKRES(avifCheckCodecVersionAVM());

        int aomCpuUsed = -1;
        if (encoder->speed != AVIF_SPEED_DEFAULT) {
            aomCpuUsed = AVIF_CLAMP(encoder->speed, 0, 9);
        }

        codec->internal->aomFormat = avifImageCalcAOMFmt(image, alpha);
        if (codec->internal->aomFormat == AOM_IMG_FMT_NONE) {
            return AVIF_RESULT_UNKNOWN_ERROR;
        }

        avifGetPixelFormatInfo(image->yuvFormat, &codec->internal->formatInfo);

        aom_codec_iface_t * encoderInterface = aom_codec_av1_cx();
        aom_codec_err_t err = aom_codec_enc_config_default(encoderInterface, cfg, AOM_USAGE_GOOD_QUALITY);
        if (err != AOM_CODEC_OK) {
            avifDiagnosticsPrintf(codec->diag, "aom_codec_enc_config_default() failed: %s", aom_codec_err_to_string(err));
            return AVIF_RESULT_UNKNOWN_ERROR;
        }

        // avm's default is AOM_VBR. Change the default to AOM_Q since we don't need to hit a certain target bit rate.
        // It's easier to control the worst quality in Q mode.
        cfg->rc_end_usage = AOM_Q;

        // Profile 0.  8-bit and 10-bit 4:2:0 and 4:0:0 only.
        // Profile 1.  8-bit and 10-bit 4:4:4
        // Profile 2.  8-bit and 10-bit 4:2:2
        //            12-bit 4:0:0, 4:2:0, 4:2:2 and 4:4:4
        uint8_t seqProfile = 0;
        if (image->depth == 12) {
            // Only seqProfile 2 can handle 12 bit
            seqProfile = 2;
        } else {
            // 8-bit or 10-bit

            if (alpha) {
                seqProfile = 0;
            } else {
                switch (image->yuvFormat) {
                    case AVIF_PIXEL_FORMAT_YUV444:
                        seqProfile = 1;
                        break;
                    case AVIF_PIXEL_FORMAT_YUV422:
                        seqProfile = 2;
                        break;
                    case AVIF_PIXEL_FORMAT_YUV420:
                        seqProfile = 0;
                        break;
                    case AVIF_PIXEL_FORMAT_YUV400:
                        seqProfile = 0;
                        break;
                    case AVIF_PIXEL_FORMAT_NONE:
                    case AVIF_PIXEL_FORMAT_COUNT:
                    default:
                        break;
                }
            }
        }

        cfg->g_profile = seqProfile;
        cfg->g_bit_depth = image->depth;
        cfg->g_input_bit_depth = image->depth;
        cfg->g_w = image->width;
        cfg->g_h = image->height;
        if (addImageFlags & AVIF_ADD_IMAGE_FLAG_SINGLE) {
            // Set the maximum number of frames to encode to 1. This instructs
            // libavm to set still_picture and reduced_still_picture_header to
            // 1 in AV1 sequence headers.
            cfg->g_limit = 1;

            // Use the default settings of the new AOM_USAGE_ALL_INTRA (added in
            // https://crbug.com/aomedia/2959).
            //
            // Set g_lag_in_frames to 0 to reduce the number of frame buffers
            // (from 20 to 2) in libavm's lookahead structure. This reduces
            // memory consumption when encoding a single image.
            cfg->g_lag_in_frames = 0;
            // Disable automatic placement of key frames by the encoder.
            cfg->kf_mode = AOM_KF_DISABLED;
            // Tell libavm that all frames will be key frames.
            cfg->kf_max_dist = 0;
        }
        if (encoder->extraLayerCount > 0) {
            cfg->g_limit = encoder->extraLayerCount + 1;
            // For layered image, disable lagged encoding to always get output
            // frame for each input frame.
            cfg->g_lag_in_frames = 0;
        }
        if (disableLaggedOutput) {
            cfg->g_lag_in_frames = 0;
        }
        if (encoder->maxThreads > 1) {
            // libavm fails if cfg->g_threads is greater than 64 threads. See MAX_NUM_THREADS in
            // avm/aom_util/aom_thread.h.
            cfg->g_threads = AVIF_MIN(encoder->maxThreads, 64);
        }

        // avm does not handle monochrome as of research-v4.0.0.
        // TODO(yguyon): Enable when fixed upstream
        codec->internal->monochromeEnabled = AVIF_FALSE;

        if (!avifProcessAOMOptionsPreInit(codec, alpha, cfg)) {
            return AVIF_RESULT_INVALID_CODEC_SPECIFIC_OPTION;
        }

        int minQuantizer;
        int maxQuantizer;
        if (alpha) {
            minQuantizer = encoder->minQuantizerAlpha;
            maxQuantizer = encoder->maxQuantizerAlpha;
        } else {
            minQuantizer = encoder->minQuantizer;
            maxQuantizer = encoder->maxQuantizer;
        }
        // Scale from aom's [0:63] to avm's [0:255]. TODO(yguyon): Accept [0:255] directly in avifEncoder.
        minQuantizer = avmScaleQuantizer(minQuantizer);
        maxQuantizer = avmScaleQuantizer(maxQuantizer);
        if ((cfg->rc_end_usage == AOM_VBR) || (cfg->rc_end_usage == AOM_CBR)) {
            // cq-level is ignored in these two end-usage modes, so adjust minQuantizer and
            // maxQuantizer to the target quantizer.
            if (quantizer == AVIF_QUANTIZER_LOSSLESS) {
                minQuantizer = AVIF_QUANTIZER_LOSSLESS;
                maxQuantizer = AVIF_QUANTIZER_LOSSLESS;
            } else {
                minQuantizer = AVIF_MAX(quantizer - 4, minQuantizer);
                maxQuantizer = AVIF_MIN(quantizer + 4, maxQuantizer);
            }
        }
        cfg->rc_min_quantizer = minQuantizer;
        cfg->rc_max_quantizer = maxQuantizer;
        quantizerUpdated = AVIF_TRUE;

        if (aom_codec_enc_init(&codec->internal->encoder, encoderInterface, cfg, /*flags=*/0) != AOM_CODEC_OK) {
            avifDiagnosticsPrintf(codec->diag,
                                  "aom_codec_enc_init() failed: %s: %s",
                                  aom_codec_error(&codec->internal->encoder),
                                  aom_codec_error_detail(&codec->internal->encoder));
            return AVIF_RESULT_UNKNOWN_ERROR;
        }
        codec->internal->encoderInitialized = AVIF_TRUE;

        if ((cfg->rc_end_usage == AOM_CQ) || (cfg->rc_end_usage == AOM_Q)) {
            aom_codec_control(&codec->internal->encoder, AOME_SET_QP, quantizer);
        }
        avifBool lossless = (quantizer == AVIF_QUANTIZER_LOSSLESS);
        if (lossless) {
            aom_codec_control(&codec->internal->encoder, AV1E_SET_LOSSLESS, 1);
        }
        if (encoder->maxThreads > 1) {
            aom_codec_control(&codec->internal->encoder, AV1E_SET_ROW_MT, 1);
        }
        if (tileRowsLog2 != 0) {
            aom_codec_control(&codec->internal->encoder, AV1E_SET_TILE_ROWS, tileRowsLog2);
        }
        if (tileColsLog2 != 0) {
            aom_codec_control(&codec->internal->encoder, AV1E_SET_TILE_COLUMNS, tileColsLog2);
        }
        if (encoder->extraLayerCount > 0) {
            int layerCount = encoder->extraLayerCount + 1;
            if (aom_codec_control(&codec->internal->encoder, AOME_SET_NUMBER_SPATIAL_LAYERS, layerCount) != AOM_CODEC_OK) {
                return AVIF_RESULT_UNKNOWN_ERROR;
            };
        }
        if (aomCpuUsed != -1) {
            if (aom_codec_control(&codec->internal->encoder, AOME_SET_CPUUSED, aomCpuUsed) != AOM_CODEC_OK) {
                return AVIF_RESULT_UNKNOWN_ERROR;
            }
        }

        // Set color_config() in the sequence header OBU.
        if (alpha) {
            aom_codec_control(&codec->internal->encoder, AV1E_SET_COLOR_RANGE, AOM_CR_FULL_RANGE);
        } else {
            // libavm's defaults are AOM_CICP_CP_UNSPECIFIED, AOM_CICP_TC_UNSPECIFIED,
            // AOM_CICP_MC_UNSPECIFIED, AOM_CSP_UNKNOWN, and 0 (studio/limited range). Call
            // aom_codec_control() only if the values are not the defaults.
            if (image->colorPrimaries != AVIF_COLOR_PRIMARIES_UNSPECIFIED) {
                aom_codec_control(&codec->internal->encoder, AV1E_SET_COLOR_PRIMARIES, (int)image->colorPrimaries);
            }
            if (image->transferCharacteristics != AVIF_TRANSFER_CHARACTERISTICS_UNSPECIFIED) {
                aom_codec_control(&codec->internal->encoder, AV1E_SET_TRANSFER_CHARACTERISTICS, (int)image->transferCharacteristics);
            }
            if (image->matrixCoefficients != AVIF_MATRIX_COEFFICIENTS_UNSPECIFIED) {
                aom_codec_control(&codec->internal->encoder, AV1E_SET_MATRIX_COEFFICIENTS, (int)image->matrixCoefficients);
            }
            if (image->yuvChromaSamplePosition != AVIF_CHROMA_SAMPLE_POSITION_UNKNOWN) {
                aom_codec_control(&codec->internal->encoder, AV1E_SET_CHROMA_SAMPLE_POSITION, (int)image->yuvChromaSamplePosition);
            }
            if (image->yuvRange != AVIF_RANGE_LIMITED) {
                aom_codec_control(&codec->internal->encoder, AV1E_SET_COLOR_RANGE, (int)image->yuvRange);
            }
        }

        if (!avifProcessAOMOptionsPostInit(codec, alpha)) {
            return AVIF_RESULT_INVALID_CODEC_SPECIFIC_OPTION;
        }
        if (!codec->internal->tuningSet) {
            if (aom_codec_control(&codec->internal->encoder, AOME_SET_TUNING, AOM_TUNE_SSIM) != AOM_CODEC_OK) {
                return AVIF_RESULT_UNKNOWN_ERROR;
            }
        }
    } else {
        avifBool dimensionsChanged = AVIF_FALSE;
        if ((cfg->g_w != image->width) || (cfg->g_h != image->height)) {
            // We are not ready for dimension change for now.
            return AVIF_RESULT_NOT_IMPLEMENTED;
        }
        if (alpha) {
            if (encoderChanges & (AVIF_ENCODER_CHANGE_MIN_QUANTIZER_ALPHA | AVIF_ENCODER_CHANGE_MAX_QUANTIZER_ALPHA)) {
                cfg->rc_min_quantizer = avmScaleQuantizer(encoder->minQuantizerAlpha);
                cfg->rc_max_quantizer = avmScaleQuantizer(encoder->maxQuantizerAlpha);
                quantizerUpdated = AVIF_TRUE;
            }
        } else {
            if (encoderChanges & (AVIF_ENCODER_CHANGE_MIN_QUANTIZER | AVIF_ENCODER_CHANGE_MAX_QUANTIZER)) {
                cfg->rc_min_quantizer = avmScaleQuantizer(encoder->minQuantizer);
                cfg->rc_max_quantizer = avmScaleQuantizer(encoder->maxQuantizer);
                quantizerUpdated = AVIF_TRUE;
            }
        }
        const int quantizerChangedBit = alpha ? AVIF_ENCODER_CHANGE_QUANTIZER_ALPHA : AVIF_ENCODER_CHANGE_QUANTIZER;
        if (encoderChanges & quantizerChangedBit) {
            if ((cfg->rc_end_usage == AOM_VBR) || (cfg->rc_end_usage == AOM_CBR)) {
                // cq-level is ignored in these two end-usage modes, so adjust minQuantizer and
                // maxQuantizer to the target quantizer.
                if (quantizer == AVIF_QUANTIZER_LOSSLESS) {
                    cfg->rc_min_quantizer = AVIF_QUANTIZER_LOSSLESS;
                    cfg->rc_max_quantizer = AVIF_QUANTIZER_LOSSLESS;
                } else {
                    int minQuantizer;
                    int maxQuantizer;
                    if (alpha) {
                        minQuantizer = encoder->minQuantizerAlpha;
                        maxQuantizer = encoder->maxQuantizerAlpha;
                    } else {
                        minQuantizer = encoder->minQuantizer;
                        maxQuantizer = encoder->maxQuantizer;
                    }
                    minQuantizer = avmScaleQuantizer(minQuantizer);
                    maxQuantizer = avmScaleQuantizer(maxQuantizer);
                    cfg->rc_min_quantizer = AVIF_MAX(quantizer - 4, minQuantizer);
                    cfg->rc_max_quantizer = AVIF_MIN(quantizer + 4, maxQuantizer);
                }
                quantizerUpdated = AVIF_TRUE;
            }
        }
        if (quantizerUpdated || dimensionsChanged) {
            aom_codec_err_t err = aom_codec_enc_config_set(&codec->internal->encoder, cfg);
            if (err != AOM_CODEC_OK) {
                avifDiagnosticsPrintf(codec->diag,
                                      "aom_codec_enc_config_set() failed: %s: %s",
                                      aom_codec_error(&codec->internal->encoder),
                                      aom_codec_error_detail(&codec->internal->encoder));
                return AVIF_RESULT_UNKNOWN_ERROR;
            }
        }
        if (encoderChanges & AVIF_ENCODER_CHANGE_TILE_ROWS_LOG2) {
            aom_codec_control(&codec->internal->encoder, AV1E_SET_TILE_ROWS, tileRowsLog2);
        }
        if (encoderChanges & AVIF_ENCODER_CHANGE_TILE_COLS_LOG2) {
            aom_codec_control(&codec->internal->encoder, AV1E_SET_TILE_COLUMNS, tileColsLog2);
        }
        if (encoderChanges & quantizerChangedBit) {
            if ((cfg->rc_end_usage == AOM_CQ) || (cfg->rc_end_usage == AOM_Q)) {
                aom_codec_control(&codec->internal->encoder, AOME_SET_QP, quantizer);
            }
            avifBool lossless = (quantizer == AVIF_QUANTIZER_LOSSLESS);
            aom_codec_control(&codec->internal->encoder, AV1E_SET_LOSSLESS, lossless);
        }
        if (encoderChanges & AVIF_ENCODER_CHANGE_CODEC_SPECIFIC) {
            if (!avifProcessAOMOptionsPostInit(codec, alpha)) {
                return AVIF_RESULT_INVALID_CODEC_SPECIFIC_OPTION;
            }
        }
    }

    if (codec->internal->currentLayer > encoder->extraLayerCount) {
        avifDiagnosticsPrintf(codec->diag,
                              "Too many layers sent. Expected %u layers, but got %u layers.",
                              encoder->extraLayerCount + 1,
                              codec->internal->currentLayer + 1);
        return AVIF_RESULT_INVALID_ARGUMENT;
    }
    if (encoder->extraLayerCount > 0) {
        aom_codec_control(&codec->internal->encoder, AOME_SET_SPATIAL_LAYER_ID, codec->internal->currentLayer);
    }

    aom_scaling_mode_t aomScalingMode;
    if (!avifFindAOMScalingMode(&encoder->scalingMode.horizontal, &aomScalingMode.h_scaling_mode)) {
        return AVIF_RESULT_NOT_IMPLEMENTED;
    }
    if (!avifFindAOMScalingMode(&encoder->scalingMode.vertical, &aomScalingMode.v_scaling_mode)) {
        return AVIF_RESULT_NOT_IMPLEMENTED;
    }
    if ((aomScalingMode.h_scaling_mode != AOME_NORMAL) || (aomScalingMode.v_scaling_mode != AOME_NORMAL)) {
        // AOME_SET_SCALEMODE only applies to next frame (layer), so we have to set it every time.
        aom_codec_control(&codec->internal->encoder, AOME_SET_SCALEMODE, &aomScalingMode);
    }

    aom_image_t aomImage;
    // We prefer to simply set the aomImage.planes[] pointers to the plane buffers in 'image'. When
    // doing this, we set aomImage.w equal to aomImage.d_w and aomImage.h equal to aomImage.d_h and
    // do not "align" aomImage.w and aomImage.h. Unfortunately this exposes a libaom bug in libavm
    // (https://crbug.com/aomedia/3113) if chroma is subsampled and image->width or image->height is
    // equal to 1. To work around this libavm bug, we allocate the aomImage.planes[] buffers and
    // copy the image YUV data if image->width or image->height is equal to 1. This bug has been
    // fixed in libaom v3.1.3 but not in libavm.
    //
    // Note: The exact condition for the bug is
    //   ((image->width == 1) && (chroma is subsampled horizontally)) ||
    //   ((image->height == 1) && (chroma is subsampled vertically))
    // Since an image width or height of 1 is uncommon in practice, we test an inexact but simpler
    // condition.
    avifBool aomImageAllocated = (image->width == 1) || (image->height == 1);
    if (aomImageAllocated) {
        aom_img_alloc(&aomImage, codec->internal->aomFormat, image->width, image->height, 16);
    } else {
        memset(&aomImage, 0, sizeof(aomImage));
        aomImage.fmt = codec->internal->aomFormat;
        aomImage.bit_depth = (image->depth > 8) ? 16 : 8;
        aomImage.w = image->width;
        aomImage.h = image->height;
        aomImage.d_w = image->width;
        aomImage.d_h = image->height;
        // Get sample size for this format.
        unsigned int bps;
        if (codec->internal->aomFormat == AOM_IMG_FMT_I420) {
            bps = 12;
        } else if (codec->internal->aomFormat == AOM_IMG_FMT_I422) {
            bps = 16;
        } else if (codec->internal->aomFormat == AOM_IMG_FMT_I444) {
            bps = 24;
        } else if (codec->internal->aomFormat == AOM_IMG_FMT_I42016) {
            bps = 24;
        } else if (codec->internal->aomFormat == AOM_IMG_FMT_I42216) {
            bps = 32;
        } else if (codec->internal->aomFormat == AOM_IMG_FMT_I44416) {
            bps = 48;
        } else {
            bps = 16;
        }
        aomImage.bps = bps;
        // See avifImageCalcAOMFmt(). libavm doesn't have AOM_IMG_FMT_I400, so we use AOM_IMG_FMT_I420 as a substitute for monochrome.
        aomImage.x_chroma_shift = (alpha || codec->internal->formatInfo.monochrome) ? 1 : codec->internal->formatInfo.chromaShiftX;
        aomImage.y_chroma_shift = (alpha || codec->internal->formatInfo.monochrome) ? 1 : codec->internal->formatInfo.chromaShiftY;
    }

    avifBool monochromeRequested = AVIF_FALSE;

    if (alpha) {
        aomImage.range = AOM_CR_FULL_RANGE;
        monochromeRequested = AVIF_TRUE;
        if (aomImageAllocated) {
            const uint32_t bytesPerRow = ((image->depth > 8) ? 2 : 1) * image->width;
            for (uint32_t j = 0; j < image->height; ++j) {
                const uint8_t * srcAlphaRow = &image->alphaPlane[j * image->alphaRowBytes];
                uint8_t * dstAlphaRow = &aomImage.planes[0][j * aomImage.stride[0]];
                memcpy(dstAlphaRow, srcAlphaRow, bytesPerRow);
            }
        } else {
            aomImage.planes[0] = image->alphaPlane;
            aomImage.stride[0] = image->alphaRowBytes;
        }

        // Ignore UV planes when monochrome
    } else {
        int yuvPlaneCount = 3;
        if (image->yuvFormat == AVIF_PIXEL_FORMAT_YUV400) {
            yuvPlaneCount = 1; // Ignore UV planes when monochrome
            monochromeRequested = AVIF_TRUE;
        }
        if (aomImageAllocated) {
            uint32_t bytesPerPixel = (image->depth > 8) ? 2 : 1;
            for (int yuvPlane = 0; yuvPlane < yuvPlaneCount; ++yuvPlane) {
                uint32_t planeWidth = avifImagePlaneWidth(image, yuvPlane);
                uint32_t planeHeight = avifImagePlaneHeight(image, yuvPlane);
                uint32_t bytesPerRow = bytesPerPixel * planeWidth;

                for (uint32_t j = 0; j < planeHeight; ++j) {
                    const uint8_t * srcRow = &image->yuvPlanes[yuvPlane][j * image->yuvRowBytes[yuvPlane]];
                    uint8_t * dstRow = &aomImage.planes[yuvPlane][j * aomImage.stride[yuvPlane]];
                    memcpy(dstRow, srcRow, bytesPerRow);
                }
            }
        } else {
            for (int yuvPlane = 0; yuvPlane < yuvPlaneCount; ++yuvPlane) {
                aomImage.planes[yuvPlane] = image->yuvPlanes[yuvPlane];
                aomImage.stride[yuvPlane] = image->yuvRowBytes[yuvPlane];
            }
        }

        aomImage.cp = (aom_color_primaries_t)image->colorPrimaries;
        aomImage.tc = (aom_transfer_characteristics_t)image->transferCharacteristics;
        aomImage.mc = (aom_matrix_coefficients_t)image->matrixCoefficients;
        aomImage.csp = (aom_chroma_sample_position_t)image->yuvChromaSamplePosition;
        aomImage.range = (aom_color_range_t)image->yuvRange;
    }

    unsigned char * monoUVPlane = NULL;
    if (monochromeRequested) {
        if (codec->internal->monochromeEnabled) {
            aomImage.monochrome = 1;
        } else {
            // The user requested monochrome (via alpha or YUV400) but libavm does not support
            // monochrome. Manually set UV planes to 0.5.

            // aomImage is always 420 when we're monochrome
            uint32_t monoUVWidth = (image->width + 1) >> 1;
            uint32_t monoUVHeight = (image->height + 1) >> 1;

            // Allocate the U plane if necessary.
            if (!aomImageAllocated) {
                uint32_t channelSize = avifImageUsesU16(image) ? 2 : 1;
                uint32_t monoUVRowBytes = channelSize * monoUVWidth;
                size_t monoUVSize = (size_t)monoUVHeight * monoUVRowBytes;

                monoUVPlane = avifAlloc(monoUVSize);
                AVIF_CHECKERR(monoUVPlane != NULL, AVIF_RESULT_OUT_OF_MEMORY); // No need for aom_img_free() because !aomImageAllocated
                aomImage.planes[1] = monoUVPlane;
                aomImage.stride[1] = monoUVRowBytes;
            }
            // Set the U plane to 0.5.
            if (image->depth > 8) {
                const uint16_t half = 1 << (image->depth - 1);
                for (uint32_t j = 0; j < monoUVHeight; ++j) {
                    uint16_t * dstRow = (uint16_t *)&aomImage.planes[1][(size_t)j * aomImage.stride[1]];
                    for (uint32_t i = 0; i < monoUVWidth; ++i) {
                        dstRow[i] = half;
                    }
                }
            } else {
                const uint8_t half = 128;
                size_t planeSize = (size_t)monoUVHeight * aomImage.stride[1];
                memset(aomImage.planes[1], half, planeSize);
            }
            // Make the V plane the same as the U plane.
            aomImage.planes[2] = aomImage.planes[1];
            aomImage.stride[2] = aomImage.stride[1];
        }
    }

    aom_enc_frame_flags_t encodeFlags = 0;
    if (addImageFlags & AVIF_ADD_IMAGE_FLAG_FORCE_KEYFRAME) {
        encodeFlags |= AOM_EFLAG_FORCE_KF;
    }
    if (codec->internal->currentLayer > 0) {
        encodeFlags |= AOM_EFLAG_NO_REF_GF | AOM_EFLAG_NO_REF_ARF | AOM_EFLAG_NO_REF_BWD | AOM_EFLAG_NO_REF_ARF2 | AOM_EFLAG_NO_UPD_ALL;
    }
    aom_codec_err_t encodeErr = aom_codec_encode(&codec->internal->encoder, &aomImage, 0, 1, encodeFlags);
    avifFree(monoUVPlane);
    if (aomImageAllocated) {
        aom_img_free(&aomImage);
    }
    if (encodeErr != AOM_CODEC_OK) {
        avifDiagnosticsPrintf(codec->diag,
                              "aom_codec_encode() failed: %s: %s",
                              aom_codec_error(&codec->internal->encoder),
                              aom_codec_error_detail(&codec->internal->encoder));
        return AVIF_RESULT_UNKNOWN_ERROR;
    }

    aom_codec_iter_t iter = NULL;
    for (;;) {
        const aom_codec_cx_pkt_t * pkt = aom_codec_get_cx_data(&codec->internal->encoder, &iter);
        if (pkt == NULL) {
            break;
        }
        if (pkt->kind == AOM_CODEC_CX_FRAME_PKT) {
            AVIF_CHECKRES(
                avifCodecEncodeOutputAddSample(output, pkt->data.frame.buf, pkt->data.frame.sz, (pkt->data.frame.flags & AOM_FRAME_IS_KEY)));
        }
    }

    if ((addImageFlags & AVIF_ADD_IMAGE_FLAG_SINGLE) ||
        ((encoder->extraLayerCount > 0) && (encoder->extraLayerCount == codec->internal->currentLayer))) {
        // Flush and clean up encoder resources early to save on overhead when encoding alpha or grid images,
        // as encoding is finished now. For layered image, encoding finishes when the last layer is encoded.

        if (!avmCodecEncodeFinish(codec, output)) {
            return AVIF_RESULT_UNKNOWN_ERROR;
        }
        aom_codec_destroy(&codec->internal->encoder);
        codec->internal->encoderInitialized = AVIF_FALSE;
    }
    if (encoder->extraLayerCount > 0) {
        ++codec->internal->currentLayer;
    }
    return AVIF_RESULT_OK;
}

static avifBool avmCodecEncodeFinish(avifCodec * codec, avifCodecEncodeOutput * output)
{
    if (!codec->internal->encoderInitialized) {
        return AVIF_TRUE;
    }
    for (;;) {
        // flush encoder
        if (aom_codec_encode(&codec->internal->encoder, NULL, 0, 1, 0) != AOM_CODEC_OK) {
            avifDiagnosticsPrintf(codec->diag,
                                  "aom_codec_encode() with img=NULL failed: %s: %s",
                                  aom_codec_error(&codec->internal->encoder),
                                  aom_codec_error_detail(&codec->internal->encoder));
            return AVIF_FALSE;
        }

        avifBool gotPacket = AVIF_FALSE;
        aom_codec_iter_t iter = NULL;
        for (;;) {
            const aom_codec_cx_pkt_t * pkt = aom_codec_get_cx_data(&codec->internal->encoder, &iter);
            if (pkt == NULL) {
                break;
            }
            if (pkt->kind == AOM_CODEC_CX_FRAME_PKT) {
                gotPacket = AVIF_TRUE;
                const avifResult result = avifCodecEncodeOutputAddSample(output,
                                                                         pkt->data.frame.buf,
                                                                         pkt->data.frame.sz,
                                                                         (pkt->data.frame.flags & AOM_FRAME_IS_KEY));
                if (result != AVIF_RESULT_OK) {
                    avifDiagnosticsPrintf(codec->diag, "avifCodecEncodeOutputAddSample() failed: %s", avifResultToString(result));
                    return AVIF_FALSE;
                }
            }
        }

        if (!gotPacket) {
            break;
        }
    }
    return AVIF_TRUE;
}

const char * avifCodecVersionAVM(void)
{
    return aom_codec_version_str();
}

avifCodec * avifCodecCreateAVM(void)
{
    avifCodec * codec = (avifCodec *)avifAlloc(sizeof(avifCodec));
    if (codec == NULL) {
        return NULL;
    }
    memset(codec, 0, sizeof(struct avifCodec));

    codec->getNextImage = avmCodecGetNextImage;

    codec->encodeImage = avmCodecEncodeImage;
    codec->encodeFinish = avmCodecEncodeFinish;

    codec->destroyInternal = avmCodecDestroyInternal;
    codec->internal = (struct avifCodecInternal *)avifAlloc(sizeof(struct avifCodecInternal));
    if (codec->internal == NULL) {
        avifFree(codec);
        return NULL;
    }
    memset(codec->internal, 0, sizeof(struct avifCodecInternal));
    return codec;
}
