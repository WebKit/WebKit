// Copyright 2019 Joe Drago. All rights reserved.
// SPDX-License-Identifier: BSD-2-Clause

#include "avif/internal.h"

#include "rav1e.h"

#include <string.h>

struct avifCodecInternal
{
    RaContext * rav1eContext;
    RaChromaSampling chromaSampling;
    int yShift;
    uint32_t encodeWidth;
    uint32_t encodeHeight;
};

static void rav1eCodecDestroyInternal(avifCodec * codec)
{
    if (codec->internal->rav1eContext) {
        rav1e_context_unref(codec->internal->rav1eContext);
        codec->internal->rav1eContext = NULL;
    }
    avifFree(codec->internal);
}

// Official support wasn't added until v0.4.0
static avifBool rav1eSupports400(void)
{
    const char * rav1eVersionString = rav1e_version_short();

    // Check major version > 0
    int majorVersion = atoi(rav1eVersionString);
    if (majorVersion > 0) {
        return AVIF_TRUE;
    }

    // Check minor version >= 4
    const char * minorVersionString = strchr(rav1eVersionString, '.');
    if (!minorVersionString) {
        return AVIF_FALSE;
    }
    ++minorVersionString;
    if (!(*minorVersionString)) {
        return AVIF_FALSE;
    }
    int minorVersion = atoi(minorVersionString);
    return minorVersion >= 4;
}

static avifResult rav1eCodecEncodeImage(avifCodec * codec,
                                        avifEncoder * encoder,
                                        const avifImage * image,
                                        avifBool alpha,
                                        int tileRowsLog2,
                                        int tileColsLog2,
                                        avifEncoderChanges encoderChanges,
                                        uint32_t addImageFlags,
                                        avifCodecEncodeOutput * output)
{
    // rav1e does not support changing encoder settings.
    if (encoderChanges) {
        return AVIF_RESULT_NOT_IMPLEMENTED;
    }

    // rav1e does not support changing image dimensions.
    if (!codec->internal->rav1eContext) {
        codec->internal->encodeWidth = image->width;
        codec->internal->encodeHeight = image->height;
    } else if ((codec->internal->encodeWidth != image->width) || (codec->internal->encodeHeight != image->height)) {
        return AVIF_RESULT_NOT_IMPLEMENTED;
    }

    avifResult result = AVIF_RESULT_UNKNOWN_ERROR;

    RaConfig * rav1eConfig = NULL;
    RaFrame * rav1eFrame = NULL;

    if (!codec->internal->rav1eContext) {
        if (codec->csOptions->count > 0) {
            // None are currently supported!
            return AVIF_RESULT_INVALID_CODEC_SPECIFIC_OPTION;
        }

        const avifBool supports400 = rav1eSupports400();
        RaPixelRange rav1eRange;
        if (alpha) {
            rav1eRange = RA_PIXEL_RANGE_FULL;
            codec->internal->chromaSampling = supports400 ? RA_CHROMA_SAMPLING_CS400 : RA_CHROMA_SAMPLING_CS420;
            codec->internal->yShift = 1;
        } else {
            rav1eRange = (image->yuvRange == AVIF_RANGE_FULL) ? RA_PIXEL_RANGE_FULL : RA_PIXEL_RANGE_LIMITED;
            codec->internal->yShift = 0;
            switch (image->yuvFormat) {
                case AVIF_PIXEL_FORMAT_YUV444:
                    codec->internal->chromaSampling = RA_CHROMA_SAMPLING_CS444;
                    break;
                case AVIF_PIXEL_FORMAT_YUV422:
                    codec->internal->chromaSampling = RA_CHROMA_SAMPLING_CS422;
                    break;
                case AVIF_PIXEL_FORMAT_YUV420:
                    codec->internal->chromaSampling = RA_CHROMA_SAMPLING_CS420;
                    codec->internal->yShift = 1;
                    break;
                case AVIF_PIXEL_FORMAT_YUV400:
                    codec->internal->chromaSampling = supports400 ? RA_CHROMA_SAMPLING_CS400 : RA_CHROMA_SAMPLING_CS420;
                    codec->internal->yShift = 1;
                    break;
                case AVIF_PIXEL_FORMAT_NONE:
                case AVIF_PIXEL_FORMAT_COUNT:
                default:
                    return AVIF_RESULT_UNKNOWN_ERROR;
            }
        }

        rav1eConfig = rav1e_config_default();
        if (rav1e_config_set_pixel_format(rav1eConfig,
                                          (uint8_t)image->depth,
                                          codec->internal->chromaSampling,
                                          (RaChromaSamplePosition)image->yuvChromaSamplePosition,
                                          rav1eRange) < 0) {
            goto cleanup;
        }

        if (addImageFlags & AVIF_ADD_IMAGE_FLAG_SINGLE) {
            if (rav1e_config_parse(rav1eConfig, "still_picture", "true") == -1) {
                goto cleanup;
            }
        }
        if (rav1e_config_parse_int(rav1eConfig, "width", image->width) == -1) {
            goto cleanup;
        }
        if (rav1e_config_parse_int(rav1eConfig, "height", image->height) == -1) {
            goto cleanup;
        }
        if (rav1e_config_parse_int(rav1eConfig, "threads", encoder->maxThreads) == -1) {
            goto cleanup;
        }

        int minQuantizer = AVIF_CLAMP(encoder->minQuantizer, 0, 63);
        int maxQuantizer = AVIF_CLAMP(encoder->maxQuantizer, 0, 63);
        if (alpha) {
            minQuantizer = AVIF_CLAMP(encoder->minQuantizerAlpha, 0, 63);
            maxQuantizer = AVIF_CLAMP(encoder->maxQuantizerAlpha, 0, 63);
        }
        minQuantizer = (minQuantizer * 255) / 63; // Rescale quantizer values as rav1e's QP range is [0,255]
        maxQuantizer = (maxQuantizer * 255) / 63;
        if (rav1e_config_parse_int(rav1eConfig, "min_quantizer", minQuantizer) == -1) {
            goto cleanup;
        }
        if (rav1e_config_parse_int(rav1eConfig, "quantizer", maxQuantizer) == -1) {
            goto cleanup;
        }
        if (tileRowsLog2 != 0) {
            if (rav1e_config_parse_int(rav1eConfig, "tile_rows", 1 << tileRowsLog2) == -1) {
                goto cleanup;
            }
        }
        if (tileColsLog2 != 0) {
            if (rav1e_config_parse_int(rav1eConfig, "tile_cols", 1 << tileColsLog2) == -1) {
                goto cleanup;
            }
        }
        if (encoder->speed != AVIF_SPEED_DEFAULT) {
            int speed = AVIF_CLAMP(encoder->speed, 0, 10);
            if (rav1e_config_parse_int(rav1eConfig, "speed", speed) == -1) {
                goto cleanup;
            }
        }

        rav1e_config_set_color_description(rav1eConfig,
                                           (RaMatrixCoefficients)image->matrixCoefficients,
                                           (RaColorPrimaries)image->colorPrimaries,
                                           (RaTransferCharacteristics)image->transferCharacteristics);

        codec->internal->rav1eContext = rav1e_context_new(rav1eConfig);
        if (!codec->internal->rav1eContext) {
            goto cleanup;
        }
    }

    rav1eFrame = rav1e_frame_new(codec->internal->rav1eContext);

    int byteWidth = (image->depth > 8) ? 2 : 1;
    if (alpha) {
        rav1e_frame_fill_plane(rav1eFrame, 0, image->alphaPlane, (size_t)image->alphaRowBytes * image->height, image->alphaRowBytes, byteWidth);
    } else {
        rav1e_frame_fill_plane(rav1eFrame, 0, image->yuvPlanes[0], (size_t)image->yuvRowBytes[0] * image->height, image->yuvRowBytes[0], byteWidth);
        if (image->yuvFormat != AVIF_PIXEL_FORMAT_YUV400) {
            uint32_t uvHeight = (image->height + codec->internal->yShift) >> codec->internal->yShift;
            rav1e_frame_fill_plane(rav1eFrame, 1, image->yuvPlanes[1], (size_t)image->yuvRowBytes[1] * uvHeight, image->yuvRowBytes[1], byteWidth);
            rav1e_frame_fill_plane(rav1eFrame, 2, image->yuvPlanes[2], (size_t)image->yuvRowBytes[2] * uvHeight, image->yuvRowBytes[2], byteWidth);
        }
    }

    RaFrameTypeOverride frameType = RA_FRAME_TYPE_OVERRIDE_NO;
    if (addImageFlags & AVIF_ADD_IMAGE_FLAG_FORCE_KEYFRAME) {
        frameType = RA_FRAME_TYPE_OVERRIDE_KEY;
    }
    rav1e_frame_set_type(rav1eFrame, frameType);

    RaEncoderStatus encoderStatus = rav1e_send_frame(codec->internal->rav1eContext, rav1eFrame);
    if (encoderStatus != RA_ENCODER_STATUS_SUCCESS) {
        goto cleanup;
    }

    RaPacket * pkt = NULL;
    for (;;) {
        encoderStatus = rav1e_receive_packet(codec->internal->rav1eContext, &pkt);
        if (encoderStatus == RA_ENCODER_STATUS_ENCODED) {
            continue;
        }
        if ((encoderStatus != RA_ENCODER_STATUS_SUCCESS) && (encoderStatus != RA_ENCODER_STATUS_NEED_MORE_DATA)) {
            goto cleanup;
        } else if (pkt) {
            if (pkt->data && (pkt->len > 0)) {
                avifCodecEncodeOutputAddSample(output, pkt->data, pkt->len, (pkt->frame_type == RA_FRAME_TYPE_KEY));
            }
            rav1e_packet_unref(pkt);
            pkt = NULL;
        } else {
            break;
        }
    }
    result = AVIF_RESULT_OK;
cleanup:
    if (rav1eFrame) {
        rav1e_frame_unref(rav1eFrame);
        rav1eFrame = NULL;
    }
    if (rav1eConfig) {
        rav1e_config_unref(rav1eConfig);
        rav1eConfig = NULL;
    }
    return result;
}

static avifBool rav1eCodecEncodeFinish(avifCodec * codec, avifCodecEncodeOutput * output)
{
    for (;;) {
        RaEncoderStatus encoderStatus = rav1e_send_frame(codec->internal->rav1eContext, NULL); // flush
        if (encoderStatus != RA_ENCODER_STATUS_SUCCESS) {
            return AVIF_FALSE;
        }

        avifBool gotPacket = AVIF_FALSE;
        RaPacket * pkt = NULL;
        for (;;) {
            encoderStatus = rav1e_receive_packet(codec->internal->rav1eContext, &pkt);
            if (encoderStatus == RA_ENCODER_STATUS_ENCODED) {
                continue;
            }
            if ((encoderStatus != RA_ENCODER_STATUS_SUCCESS) && (encoderStatus != RA_ENCODER_STATUS_LIMIT_REACHED)) {
                return AVIF_FALSE;
            }
            if (pkt) {
                gotPacket = AVIF_TRUE;
                if (pkt->data && (pkt->len > 0)) {
                    avifCodecEncodeOutputAddSample(output, pkt->data, pkt->len, (pkt->frame_type == RA_FRAME_TYPE_KEY));
                }
                rav1e_packet_unref(pkt);
                pkt = NULL;
            } else {
                break;
            }
        }

        if (!gotPacket) {
            break;
        }
    }
    return AVIF_TRUE;
}

const char * avifCodecVersionRav1e(void)
{
    return rav1e_version_full();
}

avifCodec * avifCodecCreateRav1e(void)
{
    avifCodec * codec = (avifCodec *)avifAlloc(sizeof(avifCodec));
    memset(codec, 0, sizeof(struct avifCodec));
    codec->encodeImage = rav1eCodecEncodeImage;
    codec->encodeFinish = rav1eCodecEncodeFinish;
    codec->destroyInternal = rav1eCodecDestroyInternal;

    codec->internal = (struct avifCodecInternal *)avifAlloc(sizeof(struct avifCodecInternal));
    memset(codec->internal, 0, sizeof(struct avifCodecInternal));
    return codec;
}
