// Copyright 2019 Joe Drago. All rights reserved.
// SPDX-License-Identifier: BSD-2-Clause

#include "avif/internal.h"

#if defined(_MSC_VER)
#pragma warning(disable : 4201) // nonstandard extension used: nameless struct/union
#endif
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wc11-extensions" // C11 extension used: nameless struct/union
#endif
#include "dav1d/dav1d.h"
#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#include <string.h>

// For those building with an older version of dav1d (not recommended).
#ifndef DAV1D_ERR
#define DAV1D_ERR(e) (-(e))
#endif

struct avifCodecInternal
{
    Dav1dContext * dav1dContext;
    Dav1dPicture dav1dPicture;
    avifBool hasPicture;
    avifRange colorRange;
};

static void avifDav1dFreeCallback(const uint8_t * buf, void * cookie)
{
    // This data is owned by the decoder; nothing to free here
    (void)buf;
    (void)cookie;
}

static void dav1dCodecDestroyInternal(avifCodec * codec)
{
    if (codec->internal->hasPicture) {
        dav1d_picture_unref(&codec->internal->dav1dPicture);
    }
    if (codec->internal->dav1dContext) {
        dav1d_close(&codec->internal->dav1dContext);
    }
    avifFree(codec->internal);
}

static avifBool dav1dCodecGetNextImage(struct avifCodec * codec,
                                       struct avifDecoder * decoder,
                                       const avifDecodeSample * sample,
                                       avifBool alpha,
                                       avifBool * isLimitedRangeAlpha,
                                       avifImage * image)
{
    if (codec->internal->dav1dContext == NULL) {
        Dav1dSettings dav1dSettings;
        dav1d_default_settings(&dav1dSettings);
        // Give all available threads to decode a single frame as fast as possible
#if DAV1D_API_VERSION_MAJOR >= 6
        dav1dSettings.max_frame_delay = 1;
        dav1dSettings.n_threads = AVIF_CLAMP(decoder->maxThreads, 1, DAV1D_MAX_THREADS);
#else
        dav1dSettings.n_frame_threads = 1;
        dav1dSettings.n_tile_threads = AVIF_CLAMP(decoder->maxThreads, 1, DAV1D_MAX_TILE_THREADS);
#endif // DAV1D_API_VERSION_MAJOR >= 6
        // Set a maximum frame size limit to avoid OOM'ing fuzzers. In 32-bit builds, if
        // frame_size_limit > 8192 * 8192, dav1d reduces frame_size_limit to 8192 * 8192 and logs
        // a message, so we set frame_size_limit to at most 8192 * 8192 to avoid the dav1d_log
        // message.
        dav1dSettings.frame_size_limit = (sizeof(size_t) < 8) ? AVIF_MIN(decoder->imageSizeLimit, 8192 * 8192) : decoder->imageSizeLimit;
        dav1dSettings.operating_point = codec->operatingPoint;
        dav1dSettings.all_layers = codec->allLayers;

        if (dav1d_open(&codec->internal->dav1dContext, &dav1dSettings) != 0) {
            return AVIF_FALSE;
        }
    }

    avifBool gotPicture = AVIF_FALSE;
    Dav1dPicture nextFrame;
    memset(&nextFrame, 0, sizeof(Dav1dPicture));

    Dav1dData dav1dData;
    if (dav1d_data_wrap(&dav1dData, sample->data.data, sample->data.size, avifDav1dFreeCallback, NULL) != 0) {
        return AVIF_FALSE;
    }

    for (;;) {
        if (dav1dData.data) {
            int res = dav1d_send_data(codec->internal->dav1dContext, &dav1dData);
            if ((res < 0) && (res != DAV1D_ERR(EAGAIN))) {
                dav1d_data_unref(&dav1dData);
                return AVIF_FALSE;
            }
        }

        int res = dav1d_get_picture(codec->internal->dav1dContext, &nextFrame);
        if (res == DAV1D_ERR(EAGAIN)) {
            if (dav1dData.data) {
                // send more data
                continue;
            }
            return AVIF_FALSE;
        } else if (res < 0) {
            // No more frames
            if (dav1dData.data) {
                dav1d_data_unref(&dav1dData);
            }
            return AVIF_FALSE;
        } else {
            // Got a picture!
            if ((sample->spatialID != AVIF_SPATIAL_ID_UNSET) && (sample->spatialID != nextFrame.frame_hdr->spatial_id)) {
                // Layer selection: skip this unwanted layer
                dav1d_picture_unref(&nextFrame);
            } else {
                gotPicture = AVIF_TRUE;
                break;
            }
        }
    }
    if (dav1dData.data) {
        dav1d_data_unref(&dav1dData);
    }

    if (gotPicture) {
        dav1d_picture_unref(&codec->internal->dav1dPicture);
        codec->internal->dav1dPicture = nextFrame;
        codec->internal->colorRange = codec->internal->dav1dPicture.seq_hdr->color_range ? AVIF_RANGE_FULL : AVIF_RANGE_LIMITED;
        codec->internal->hasPicture = AVIF_TRUE;
    } else {
        if (alpha && codec->internal->hasPicture) {
            // Special case: reuse last alpha frame
        } else {
            return AVIF_FALSE;
        }
    }

    Dav1dPicture * dav1dImage = &codec->internal->dav1dPicture;
    avifBool isColor = !alpha;
    if (isColor) {
        // Color (YUV) planes - set image to correct size / format, fill color

        avifPixelFormat yuvFormat = AVIF_PIXEL_FORMAT_NONE;
        switch (dav1dImage->p.layout) {
            case DAV1D_PIXEL_LAYOUT_I400:
                yuvFormat = AVIF_PIXEL_FORMAT_YUV400;
                break;
            case DAV1D_PIXEL_LAYOUT_I420:
                yuvFormat = AVIF_PIXEL_FORMAT_YUV420;
                break;
            case DAV1D_PIXEL_LAYOUT_I422:
                yuvFormat = AVIF_PIXEL_FORMAT_YUV422;
                break;
            case DAV1D_PIXEL_LAYOUT_I444:
                yuvFormat = AVIF_PIXEL_FORMAT_YUV444;
                break;
        }

        if (image->width && image->height) {
            if ((image->width != (uint32_t)dav1dImage->p.w) || (image->height != (uint32_t)dav1dImage->p.h) ||
                (image->depth != (uint32_t)dav1dImage->p.bpc) || (image->yuvFormat != yuvFormat)) {
                // Throw it all out
                avifImageFreePlanes(image, AVIF_PLANES_ALL);
            }
        }
        image->width = dav1dImage->p.w;
        image->height = dav1dImage->p.h;
        image->depth = dav1dImage->p.bpc;

        image->yuvFormat = yuvFormat;
        image->yuvRange = codec->internal->colorRange;
        image->yuvChromaSamplePosition = (avifChromaSamplePosition)dav1dImage->seq_hdr->chr;

        image->colorPrimaries = (avifColorPrimaries)dav1dImage->seq_hdr->pri;
        image->transferCharacteristics = (avifTransferCharacteristics)dav1dImage->seq_hdr->trc;
        image->matrixCoefficients = (avifMatrixCoefficients)dav1dImage->seq_hdr->mtrx;

        avifPixelFormatInfo formatInfo;
        avifGetPixelFormatInfo(yuvFormat, &formatInfo);

        avifImageFreePlanes(image, AVIF_PLANES_YUV);
        int yuvPlaneCount = (yuvFormat == AVIF_PIXEL_FORMAT_YUV400) ? 1 : 3;
        for (int yuvPlane = 0; yuvPlane < yuvPlaneCount; ++yuvPlane) {
            image->yuvPlanes[yuvPlane] = dav1dImage->data[yuvPlane];
            image->yuvRowBytes[yuvPlane] = (uint32_t)dav1dImage->stride[(yuvPlane == AVIF_CHAN_Y) ? 0 : 1];
        }
        image->imageOwnsYUVPlanes = AVIF_FALSE;
    } else {
        // Alpha plane - ensure image is correct size, fill color

        if (image->width && image->height) {
            if ((image->width != (uint32_t)dav1dImage->p.w) || (image->height != (uint32_t)dav1dImage->p.h) ||
                (image->depth != (uint32_t)dav1dImage->p.bpc)) {
                // Alpha plane doesn't match previous alpha plane decode, bail out
                return AVIF_FALSE;
            }
        }
        image->width = dav1dImage->p.w;
        image->height = dav1dImage->p.h;
        image->depth = dav1dImage->p.bpc;

        avifImageFreePlanes(image, AVIF_PLANES_A);
        image->alphaPlane = dav1dImage->data[0];
        image->alphaRowBytes = (uint32_t)dav1dImage->stride[0];
        *isLimitedRangeAlpha = (codec->internal->colorRange == AVIF_RANGE_LIMITED);
        image->imageOwnsAlphaPlane = AVIF_FALSE;
    }
    return AVIF_TRUE;
}

const char * avifCodecVersionDav1d(void)
{
    return dav1d_version();
}

avifCodec * avifCodecCreateDav1d(void)
{
    avifCodec * codec = (avifCodec *)avifAlloc(sizeof(avifCodec));
    memset(codec, 0, sizeof(struct avifCodec));
    codec->getNextImage = dav1dCodecGetNextImage;
    codec->destroyInternal = dav1dCodecDestroyInternal;

    codec->internal = (struct avifCodecInternal *)avifAlloc(sizeof(struct avifCodecInternal));
    memset(codec->internal, 0, sizeof(struct avifCodecInternal));
    return codec;
}
