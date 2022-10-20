// Copyright 2020 Google LLC. All rights reserved.
// SPDX-License-Identifier: BSD-2-Clause

#include "avif/internal.h"

#include "gav1/decoder.h"

#include <string.h>

struct avifCodecInternal
{
    Libgav1DecoderSettings gav1Settings;
    Libgav1Decoder * gav1Decoder;
    const Libgav1DecoderBuffer * gav1Image;
    avifRange colorRange;
};

static void gav1CodecDestroyInternal(avifCodec * codec)
{
    if (codec->internal->gav1Decoder != NULL) {
        Libgav1DecoderDestroy(codec->internal->gav1Decoder);
    }
    avifFree(codec->internal);
}

static avifBool gav1CodecGetNextImage(struct avifCodec * codec,
                                      struct avifDecoder * decoder,
                                      const avifDecodeSample * sample,
                                      avifBool alpha,
                                      avifBool * isLimitedRangeAlpha,
                                      avifImage * image)
{
    if (codec->internal->gav1Decoder == NULL) {
        codec->internal->gav1Settings.threads = decoder->maxThreads;
        codec->internal->gav1Settings.operating_point = codec->operatingPoint;
        codec->internal->gav1Settings.output_all_layers = codec->allLayers;

        if (Libgav1DecoderCreate(&codec->internal->gav1Settings, &codec->internal->gav1Decoder) != kLibgav1StatusOk) {
            return AVIF_FALSE;
        }
    }

    if (Libgav1DecoderEnqueueFrame(codec->internal->gav1Decoder,
                                   sample->data.data,
                                   sample->data.size,
                                   /*user_private_data=*/0,
                                   /*buffer_private_data=*/NULL) != kLibgav1StatusOk) {
        return AVIF_FALSE;
    }
    // Each Libgav1DecoderDequeueFrame() call invalidates the output frame
    // returned by the previous Libgav1DecoderDequeueFrame() call. Clear
    // our pointer to the previous output frame.
    codec->internal->gav1Image = NULL;

    const Libgav1DecoderBuffer * nextFrame = NULL;
    for (;;) {
        if (Libgav1DecoderDequeueFrame(codec->internal->gav1Decoder, &nextFrame) != kLibgav1StatusOk) {
            return AVIF_FALSE;
        }
        if (nextFrame && (sample->spatialID != AVIF_SPATIAL_ID_UNSET) && (nextFrame->spatial_id != sample->spatialID)) {
            nextFrame = NULL;
        } else {
            break;
        }
    }
    // Got an image!

    if (nextFrame) {
        codec->internal->gav1Image = nextFrame;
        codec->internal->colorRange = (nextFrame->color_range == kLibgav1ColorRangeStudio) ? AVIF_RANGE_LIMITED : AVIF_RANGE_FULL;
    } else {
        if (alpha && codec->internal->gav1Image) {
            // Special case: reuse last alpha frame
        } else {
            return AVIF_FALSE;
        }
    }

    const Libgav1DecoderBuffer * gav1Image = codec->internal->gav1Image;
    avifBool isColor = !alpha;
    if (isColor) {
        // Color (YUV) planes - set image to correct size / format, fill color

        avifPixelFormat yuvFormat = AVIF_PIXEL_FORMAT_NONE;
        switch (gav1Image->image_format) {
            case kLibgav1ImageFormatMonochrome400:
                yuvFormat = AVIF_PIXEL_FORMAT_YUV400;
                break;
            case kLibgav1ImageFormatYuv420:
                yuvFormat = AVIF_PIXEL_FORMAT_YUV420;
                break;
            case kLibgav1ImageFormatYuv422:
                yuvFormat = AVIF_PIXEL_FORMAT_YUV422;
                break;
            case kLibgav1ImageFormatYuv444:
                yuvFormat = AVIF_PIXEL_FORMAT_YUV444;
                break;
        }

        if (image->width && image->height) {
            if ((image->width != (uint32_t)gav1Image->displayed_width[0]) ||
                (image->height != (uint32_t)gav1Image->displayed_height[0]) || (image->depth != (uint32_t)gav1Image->bitdepth) ||
                (image->yuvFormat != yuvFormat)) {
                // Throw it all out
                avifImageFreePlanes(image, AVIF_PLANES_ALL);
            }
        }
        image->width = gav1Image->displayed_width[0];
        image->height = gav1Image->displayed_height[0];
        image->depth = gav1Image->bitdepth;

        image->yuvFormat = yuvFormat;
        image->yuvRange = codec->internal->colorRange;
        image->yuvChromaSamplePosition = (avifChromaSamplePosition)gav1Image->chroma_sample_position;

        image->colorPrimaries = (avifColorPrimaries)gav1Image->color_primary;
        image->transferCharacteristics = (avifTransferCharacteristics)gav1Image->transfer_characteristics;
        image->matrixCoefficients = (avifMatrixCoefficients)gav1Image->matrix_coefficients;

        avifPixelFormatInfo formatInfo;
        avifGetPixelFormatInfo(yuvFormat, &formatInfo);

        // Steal the pointers from the decoder's image directly
        avifImageFreePlanes(image, AVIF_PLANES_YUV);
        int yuvPlaneCount = (yuvFormat == AVIF_PIXEL_FORMAT_YUV400) ? 1 : 3;
        for (int yuvPlane = 0; yuvPlane < yuvPlaneCount; ++yuvPlane) {
            image->yuvPlanes[yuvPlane] = gav1Image->plane[yuvPlane];
            image->yuvRowBytes[yuvPlane] = gav1Image->stride[yuvPlane];
        }
        image->imageOwnsYUVPlanes = AVIF_FALSE;
    } else {
        // Alpha plane - ensure image is correct size, fill color

        if (image->width && image->height) {
            if ((image->width != (uint32_t)gav1Image->displayed_width[0]) ||
                (image->height != (uint32_t)gav1Image->displayed_height[0]) || (image->depth != (uint32_t)gav1Image->bitdepth)) {
                // Alpha plane doesn't match previous alpha plane decode, bail out
                return AVIF_FALSE;
            }
        }
        image->width = gav1Image->displayed_width[0];
        image->height = gav1Image->displayed_height[0];
        image->depth = gav1Image->bitdepth;

        avifImageFreePlanes(image, AVIF_PLANES_A);
        image->alphaPlane = gav1Image->plane[0];
        image->alphaRowBytes = gav1Image->stride[0];
        *isLimitedRangeAlpha = (codec->internal->colorRange == AVIF_RANGE_LIMITED);
        image->imageOwnsAlphaPlane = AVIF_FALSE;
    }

    return AVIF_TRUE;
}

const char * avifCodecVersionGav1(void)
{
    return Libgav1GetVersionString();
}

avifCodec * avifCodecCreateGav1(void)
{
    avifCodec * codec = (avifCodec *)avifAlloc(sizeof(avifCodec));
    memset(codec, 0, sizeof(struct avifCodec));
    codec->getNextImage = gav1CodecGetNextImage;
    codec->destroyInternal = gav1CodecDestroyInternal;

    codec->internal = (struct avifCodecInternal *)avifAlloc(sizeof(struct avifCodecInternal));
    memset(codec->internal, 0, sizeof(struct avifCodecInternal));
    Libgav1DecoderSettingsInitDefault(&codec->internal->gav1Settings);
    return codec;
}
