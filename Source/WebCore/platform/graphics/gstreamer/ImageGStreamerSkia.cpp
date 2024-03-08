/*
 * Copyright (C) 2024 Igalia S.L
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "ImageGStreamer.h"

#if ENABLE(VIDEO) && USE(GSTREAMER) && USE(SKIA)

#include "GStreamerCommon.h"
#include "NotImplemented.h"
#include <gst/gst.h>
#include <gst/video/gstvideometa.h>
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

    GstVideoInfo videoInfo;
    if (!gst_video_info_from_caps(&videoInfo, gst_sample_get_caps(m_sample.get())))
        return;

    auto videoFrame = makeUnique<GstMappedFrame>(buffer, &videoInfo, GST_MAP_READ);
    if (!*videoFrame)
        return;

    // The frame has to be RGB so we can paint it.
    ASSERT(GST_VIDEO_INFO_IS_RGB(&videoInfo));

    // The video buffer may have these formats in these cases:
    // { BGRx, BGRA } on little endian
    // { xRGB, ARGB } on big endian:
    // { RGBx, RGBA }
    SkColorType colorType = kUnknown_SkColorType;
    SkAlphaType alphaType = kUnknown_SkAlphaType;
    switch (videoFrame->format()) {
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

    auto imageInfo = SkImageInfo::Make(videoFrame->width(), videoFrame->height(), colorType, alphaType);
    SkPixmap pixmap(imageInfo, videoFrame->planeData(0), videoFrame->planeStride(0));
    auto image = SkImages::RasterFromPixmap(pixmap, [](const void*, void* context) {
        std::unique_ptr<GstMappedFrame> videoFrame(static_cast<GstMappedFrame*>(context));
    }, videoFrame.release());

    m_image = BitmapImage::create(WTFMove(image));

    if (auto* cropMeta = gst_buffer_get_video_crop_meta(buffer))
        m_cropRect = FloatRect(cropMeta->x, cropMeta->y, cropMeta->width, cropMeta->height);
}

ImageGStreamer::~ImageGStreamer() = default;

} // namespace WebCore

#endif // ENABLE(VIDEO) && USE(GSTREAMER) && USE(SKIA)
