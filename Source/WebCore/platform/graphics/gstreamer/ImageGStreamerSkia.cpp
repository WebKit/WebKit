/*
 * Copyright (C) 2024 Igalia S.L
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "ImageGStreamer.h"

#if ENABLE(VIDEO) && USE(GSTREAMER) && USE(SKIA)

#include "NotImplemented.h"
#include <skia/ColorSpaceSkia.h>
#include <skia/core/SkImage.h>

IGNORE_CLANG_WARNINGS_BEGIN("cast-align")
#include <skia/core/SkPixmap.h>
IGNORE_CLANG_WARNINGS_END

namespace WebCore {

ImageGStreamer::ImageGStreamer(GRefPtr<GstSample>&& sample)
    : m_sample(WTFMove(sample))
{
    GstBuffer* buffer = gst_sample_get_buffer(m_sample.get());
    if (UNLIKELY(!GST_IS_BUFFER(buffer)))
        return;

    GstMappedFrame videoFrame(m_sample, GST_MAP_READ);
    if (!videoFrame)
        return;

    auto* videoInfo = videoFrame.info();

    // The frame has to be RGB so we can paint it.
    ASSERT(GST_VIDEO_INFO_IS_RGB(videoInfo));

    // The video buffer may have these formats in these cases:
    // { BGRx, BGRA } on little endian
    // { xRGB, ARGB } on big endian:
    // { RGBx, RGBA }
    SkColorType colorType = kUnknown_SkColorType;
    SkAlphaType alphaType = kUnknown_SkAlphaType;
    switch (videoFrame.format()) {
    case GST_VIDEO_FORMAT_BGRx:
        colorType = kBGRA_8888_SkColorType;
        alphaType = kOpaque_SkAlphaType;
        break;
    case GST_VIDEO_FORMAT_BGRA:
        colorType = kBGRA_8888_SkColorType;
        alphaType = kUnpremul_SkAlphaType;
        break;
    case GST_VIDEO_FORMAT_xRGB:
    case GST_VIDEO_FORMAT_ARGB:
        // FIXME: we need a conversion here.
        notImplemented();
        return;
    case GST_VIDEO_FORMAT_RGBx:
        colorType = kRGB_888x_SkColorType;
        alphaType = kOpaque_SkAlphaType;
        break;
    case GST_VIDEO_FORMAT_RGBA:
        colorType = kRGBA_8888_SkColorType;
        alphaType = kUnpremul_SkAlphaType;
        break;
    default:
        ASSERT_NOT_REACHED();
        break;
    }

    m_size = { static_cast<float>(videoFrame.width()), static_cast<float>(videoFrame.height()) };

    auto toSkiaColorSpace = [](const PlatformVideoColorSpace&) {
        notImplemented();
        return SkColorSpace::MakeSRGB();
    };
    auto imageInfo = SkImageInfo::Make(videoFrame.width(), videoFrame.height(), colorType, alphaType, toSkiaColorSpace(videoColorSpaceFromInfo(*videoInfo)));

    // Copy the buffer data. Keeping the whole mapped GstVideoFrame alive would increase memory
    // pressure and the file descriptor(s) associated with the buffer pool open. We only need the
    // data here.
    SkPixmap pixmap(imageInfo, videoFrame.planeData(0), videoFrame.planeStride(0));
    m_image = SkImages::RasterFromPixmapCopy(pixmap);

    if (auto* cropMeta = gst_buffer_get_video_crop_meta(buffer))
        m_cropRect = FloatRect(cropMeta->x, cropMeta->y, cropMeta->width, cropMeta->height);
}

ImageGStreamer::~ImageGStreamer() = default;

} // namespace WebCore

#endif // ENABLE(VIDEO) && USE(GSTREAMER) && USE(SKIA)
