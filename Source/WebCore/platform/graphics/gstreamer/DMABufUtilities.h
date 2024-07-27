/*
 *  Copyright (C) 2024 Metrological Group B.V.
 *  Copyright (C) 2024 Igalia S.L
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#pragma once

#if USE(GSTREAMER) && USE(TEXTURE_MAPPER_DMABUF)

#include "DMABufColorSpace.h"
#include "DMABufFormat.h"
#include "GBMBufferSwapchain.h"
#include "GStreamerCommon.h"

#include <gbm.h>
#include <gst/video/video.h>
#include <wtf/Lock.h>

namespace WebCore {

// GStreamer's gst_video_format_to_fourcc() doesn't cover RGB-like formats, so we
// provide the appropriate FourCC values for those through this function.
inline uint32_t dmaBufFourccValue(GstVideoFormat format)
{
    switch (format) {
    case GST_VIDEO_FORMAT_RGBx:
        return uint32_t(DMABufFormat::FourCC::XBGR8888);
    case GST_VIDEO_FORMAT_BGRx:
        return uint32_t(DMABufFormat::FourCC::XRGB8888);
    case GST_VIDEO_FORMAT_xRGB:
        return uint32_t(DMABufFormat::FourCC::BGRX8888);
    case GST_VIDEO_FORMAT_xBGR:
        return uint32_t(DMABufFormat::FourCC::RGBX8888);
    case GST_VIDEO_FORMAT_RGBA:
        return uint32_t(DMABufFormat::FourCC::ABGR8888);
    case GST_VIDEO_FORMAT_BGRA:
        return uint32_t(DMABufFormat::FourCC::ARGB8888);
    case GST_VIDEO_FORMAT_ARGB:
        return uint32_t(DMABufFormat::FourCC::BGRA8888);
    case GST_VIDEO_FORMAT_ABGR:
        return uint32_t(DMABufFormat::FourCC::RGBA8888);
    case GST_VIDEO_FORMAT_P010_10LE:
    case GST_VIDEO_FORMAT_P010_10BE:
        return uint32_t(DMABufFormat::FourCC::P010);
    case GST_VIDEO_FORMAT_P016_LE:
    case GST_VIDEO_FORMAT_P016_BE:
        return uint32_t(DMABufFormat::FourCC::P016);
    default:
        break;
    }

    return gst_video_format_to_fourcc(format);
}

inline DMABufColorSpace dmaBufColorSpaceForColorimetry(const GstVideoColorimetry* cinfo)
{
    if (gst_video_colorimetry_matches(cinfo, GST_VIDEO_COLORIMETRY_SRGB))
        return DMABufColorSpace::SRGB;
    if (gst_video_colorimetry_matches(cinfo, GST_VIDEO_COLORIMETRY_BT601))
        return DMABufColorSpace::BT601;
    if (gst_video_colorimetry_matches(cinfo, GST_VIDEO_COLORIMETRY_BT709))
        return DMABufColorSpace::BT709;
    if (gst_video_colorimetry_matches(cinfo, GST_VIDEO_COLORIMETRY_BT2020))
        return DMABufColorSpace::BT2020;
    if (gst_video_colorimetry_matches(cinfo, GST_VIDEO_COLORIMETRY_SMPTE240M))
        return DMABufColorSpace::SMPTE240M;
    return DMABufColorSpace::Invalid;
}

// Destination helper struct, maps the gbm_bo object into CPU-memory space and copies from the accompanying Source.
struct DMABufDestination {
    static Lock& mappingLock()
    {
        static Lock s_mappingLock;
        return s_mappingLock;
    }

    DMABufDestination(struct gbm_bo*, uint32_t width, uint32_t height);
    ~DMABufDestination();

    void copyPlaneData(GstVideoFrame*, unsigned planeIndex);

    bool isValid { false };
    struct gbm_bo* bo { nullptr };
    void* map { nullptr };
    void* mapData { nullptr };
    uint8_t* data { nullptr };
    uint32_t height { 0 };
    uint32_t stride { 0 };
};

bool fillSwapChainBuffer(const RefPtr<GBMBufferSwapchain::Buffer>&, const GRefPtr<GstSample>&);

} // namespace WebCore

#endif // USE(GSTREAMER) && USE(TEXTURE_MAPPER_DMABUF)
