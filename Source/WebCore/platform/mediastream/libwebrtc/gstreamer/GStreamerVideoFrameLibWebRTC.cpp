/*
 *  Copyright (C) 2012, 2015, 2016, 2018 Igalia S.L
 *  Copyright (C) 2015, 2016, 2018 Metrological Group B.V.
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
#include "config.h"

#if USE(GSTREAMER) && USE(LIBWEBRTC)
#include "GStreamerVideoFrameLibWebRTC.h"

#include "GStreamerVideoFrameConverter.h"
#include <gst/video/video-format.h>
#include <gst/video/video-info.h>
#include <wtf/MediaTime.h>
#include <wtf/glib/GUniquePtr.h>

namespace WebCore {

GST_DEBUG_CATEGORY(webkit_libwebrtc_video_frame_debug);
#define GST_CAT_DEFAULT webkit_libwebrtc_video_frame_debug

static void ensureDebugCategoryIsRegistered()
{
    static std::once_flag debugRegisteredFlag;
    std::call_once(debugRegisteredFlag, [] {
        GST_DEBUG_CATEGORY_INIT(webkit_libwebrtc_video_frame_debug, "webkitlibwebrtcvideoframe", 0, "WebKit LibWebRTC Video Frame");
    });
}

GRefPtr<GstSample> convertLibWebRTCVideoFrameToGStreamerSample(const webrtc::VideoFrame& frame)
{
    if (frame.video_frame_buffer()->type() == webrtc::VideoFrameBuffer::Type::kNative)
        return static_cast<GStreamerVideoFrameLibWebRTC&>(*frame.video_frame_buffer().get()).sample();

    auto* i420Buffer = frame.video_frame_buffer()->ToI420().release();
    int height = i420Buffer->height();
    int strides[3] = {
        i420Buffer->StrideY(),
        i420Buffer->StrideU(),
        i420Buffer->StrideV()
    };
    size_t offsets[3] = {
        0,
        static_cast<gsize>(i420Buffer->StrideY() * height),
        static_cast<gsize>(i420Buffer->StrideY() * height + i420Buffer->StrideU() * ((height + 1) / 2))
    };
    GstVideoInfo info;
    gst_video_info_set_format(&info, GST_VIDEO_FORMAT_I420, frame.width(), frame.height());
    auto buffer = adoptGRef(gst_buffer_new_wrapped_full(static_cast<GstMemoryFlags>(GST_MEMORY_FLAG_NO_SHARE | GST_MEMORY_FLAG_READONLY),
        const_cast<gpointer>(reinterpret_cast<const void*>(i420Buffer->DataY())), info.size, 0, info.size, i420Buffer, [](gpointer buffer) {
            reinterpret_cast<webrtc::I420Buffer*>(buffer)->Release();
    }));

    gst_buffer_add_video_meta_full(buffer.get(), GST_VIDEO_FRAME_FLAG_NONE, GST_VIDEO_FORMAT_I420, frame.width(), frame.height(), 3, offsets, strides);
    GST_BUFFER_PTS(buffer.get()) = toGstClockTime(WTF::MediaTime(frame.render_time_ms(), G_USEC_PER_SEC * 1000));
    auto caps = adoptGRef(gst_video_info_to_caps(&info));
    auto sample = adoptGRef(gst_sample_new(buffer.get(), caps.get(), nullptr, nullptr));
    return sample;
}

webrtc::VideoFrame convertGStreamerSampleToLibWebRTCVideoFrame(GRefPtr<GstSample>&& sample, uint32_t rtpTimestamp)
{
    webrtc::VideoFrame::Builder builder;
    auto buffer = gst_sample_get_buffer(sample.get());
    auto pts = GST_BUFFER_PTS(buffer);
    return builder.set_video_frame_buffer(GStreamerVideoFrameLibWebRTC::create(WTFMove(sample)))
        .set_timestamp_rtp(rtpTimestamp)
        .set_timestamp_us(pts * GST_USECOND)
        .build();
}

webrtc::scoped_refptr<webrtc::VideoFrameBuffer> GStreamerVideoFrameLibWebRTC::create(GRefPtr<GstSample>&& sample)
{
    GstVideoInfo info;

    if (!gst_video_info_from_caps(&info, gst_sample_get_caps(sample.get())))
        ASSERT_NOT_REACHED();

    return webrtc::scoped_refptr<webrtc::VideoFrameBuffer>(new GStreamerVideoFrameLibWebRTC(WTFMove(sample), info));
}

webrtc::scoped_refptr<webrtc::I420BufferInterface> GStreamerVideoFrameLibWebRTC::ToI420()
{
    ensureDebugCategoryIsRegistered();
    GstMappedFrame inFrame(m_sample, GST_MAP_READ);
    if (!inFrame) {
        GST_WARNING("Could not map input frame");
        ASSERT_NOT_REACHED_WITH_MESSAGE("Could not map input frame");
        return nullptr;
    }

    if (inFrame.format() != GST_VIDEO_FORMAT_I420) {
        GstVideoInfo outInfo;
        gst_video_info_set_format(&outInfo, GST_VIDEO_FORMAT_I420, inFrame.width(), inFrame.height());

        auto info = inFrame.info();
        outInfo.fps_n = info->fps_n;
        outInfo.fps_d = info->fps_d;
        auto caps = adoptGRef(gst_video_info_to_caps(&outInfo));

        auto& converter = GStreamerVideoFrameConverter::singleton();
        auto sample = converter.convert(m_sample, caps);
        if (!sample) [[unlikely]]
            return nullptr;

        GstMappedFrame frame(sample, GST_MAP_READ);
        return webrtc::I420Buffer::Copy(frame.width(), frame.height(), frame.componentData(0).data(), frame.componentStride(0),
            frame.componentData(1).data(), frame.componentStride(1), frame.componentData(2).data(), frame.componentStride(2));
    }

    return webrtc::I420Buffer::Copy(inFrame.width(), inFrame.height(), inFrame.componentData(0).data(), inFrame.componentStride(0),
        inFrame.componentData(1).data(), inFrame.componentStride(1), inFrame.componentData(2).data(), inFrame.componentStride(2));
}

}

#undef GST_CAT_DEFAULT

#endif // USE(LIBWEBRTC) && USE(GSTREAMER)
