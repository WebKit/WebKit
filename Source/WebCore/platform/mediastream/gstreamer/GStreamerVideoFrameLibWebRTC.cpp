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

#include <thread>

namespace WebCore {

const GRefPtr<GstSample> GStreamerSampleFromLibWebRTCVideoFrame(const webrtc::VideoFrame& frame)
{
    if (frame.video_frame_buffer()->type() == webrtc::VideoFrameBuffer::Type::kNative) {
        auto framebuffer = static_cast<GStreamerVideoFrameLibWebRTC*>(frame.video_frame_buffer().get());
        auto gstsample = framebuffer->getSample();

        GST_LOG("Reusing native GStreamer sample: %p", gstsample.get());

        return gstsample;
    }

    auto webrtcbuffer = frame.video_frame_buffer().get()->ToI420();
    // FIXME - Check lifetime of those buffers.
    const uint8_t* comps[3] = {
        webrtcbuffer->DataY(),
        webrtcbuffer->DataU(),
        webrtcbuffer->DataV()
    };

    GstVideoInfo info;
    gst_video_info_set_format(&info, GST_VIDEO_FORMAT_I420, frame.width(), frame.height());
    auto buffer = adoptGRef(gst_buffer_new());
    for (gint i = 0; i < 3; i++) {
        gsize compsize = GST_VIDEO_INFO_COMP_STRIDE(&info, i) * GST_VIDEO_INFO_COMP_HEIGHT(&info, i);

        GstMemory* comp = gst_memory_new_wrapped(
            static_cast<GstMemoryFlags>(GST_MEMORY_FLAG_PHYSICALLY_CONTIGUOUS | GST_MEMORY_FLAG_READONLY),
            const_cast<gpointer>(reinterpret_cast<const void*>(comps[i])), compsize, 0, compsize, webrtcbuffer, nullptr);
        gst_buffer_append_memory(buffer.get(), comp);
    }

    auto caps = adoptGRef(gst_video_info_to_caps(&info));
    auto sample = adoptGRef(gst_sample_new(buffer.get(), caps.get(), nullptr, nullptr));
    return sample;
}

rtc::scoped_refptr<webrtc::VideoFrameBuffer> GStreamerVideoFrameLibWebRTC::create(GstSample * sample)
{
    GstVideoInfo info;

    if (!gst_video_info_from_caps(&info, gst_sample_get_caps(sample)))
        ASSERT_NOT_REACHED();

    return rtc::scoped_refptr<webrtc::VideoFrameBuffer>(new GStreamerVideoFrameLibWebRTC(sample, info));
}

std::unique_ptr<webrtc::VideoFrame> LibWebRTCVideoFrameFromGStreamerSample(GstSample* sample, webrtc::VideoRotation rotation,
    int64_t timestamp, int64_t renderTimeMs)
{
    auto frameBuffer(GStreamerVideoFrameLibWebRTC::create(sample));

    return std::unique_ptr<webrtc::VideoFrame>(
        new webrtc::VideoFrame(frameBuffer, timestamp, renderTimeMs, rotation));
}

webrtc::VideoFrameBuffer::Type GStreamerVideoFrameLibWebRTC::type() const
{
    return Type::kNative;
}

GRefPtr<GstSample> GStreamerVideoFrameLibWebRTC::getSample()
{
    return m_sample.get();
}

rtc::scoped_refptr<webrtc::I420BufferInterface> GStreamerVideoFrameLibWebRTC::ToI420()
{
    GstMappedFrame inFrame(m_sample, GST_MAP_READ);

    if (!inFrame) {
        GST_WARNING("Could not map frame");

        return nullptr;
    }

    auto newBuffer = m_bufferPool.CreateBuffer(inFrame.width(), inFrame.height());
    ASSERT(newBuffer);
    if (!newBuffer) {
        GST_WARNING("RealtimeOutgoingVideoSourceGStreamer::videoSampleAvailable unable to allocate buffer for conversion to YUV");
        return nullptr;
    }

    if (inFrame.format() != GST_VIDEO_FORMAT_I420) {
        GstVideoInfo outInfo;

        gst_video_info_set_format(&outInfo, GST_VIDEO_FORMAT_I420, inFrame.width(),
            inFrame.height());
        auto info = inFrame.info();
        outInfo.fps_n = info->fps_n;
        outInfo.fps_d = info->fps_d;

        GRefPtr<GstBuffer> buffer = adoptGRef(gst_buffer_new_wrapped_full(GST_MEMORY_FLAG_NO_SHARE, newBuffer->MutableDataY(),
            outInfo.size, 0, outInfo.size, nullptr, nullptr));

        GstMappedFrame outFrame(buffer.get(), outInfo, GST_MAP_WRITE);

        GUniquePtr<GstVideoConverter> videoConverter(gst_video_converter_new(inFrame.info(),
            &outInfo, gst_structure_new("GstVideoConvertConfig",
            GST_VIDEO_CONVERTER_OPT_THREADS, G_TYPE_UINT, std::thread::hardware_concurrency() || 1 , nullptr)));

        ASSERT(videoConverter);

        gst_video_converter_frame(videoConverter.get(), inFrame.get(), outFrame.get());

        return newBuffer;
    }

    newBuffer->Copy(
        inFrame.width(),
        inFrame.height(),
        inFrame.ComponentData(0),
        inFrame.ComponentStride(0),
        inFrame.ComponentData(1),
        inFrame.ComponentStride(1),
        inFrame.ComponentData(2),
        inFrame.ComponentStride(2));

    return newBuffer;
}
}
#endif // USE(LIBWEBRTC)
