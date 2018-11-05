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
    return WTFMove(sample);
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
    GstVideoInfo info;
    GstVideoFrame frame;

    if (!gst_video_info_from_caps(&info, gst_sample_get_caps(m_sample.get())))
        ASSERT_NOT_REACHED();

    if (GST_VIDEO_INFO_FORMAT(&info) != GST_VIDEO_FORMAT_I420)
        return nullptr;

    gst_video_frame_map(&frame, &info, gst_sample_get_buffer(m_sample.get()), GST_MAP_READ);

    auto newBuffer = m_bufferPool.CreateBuffer(GST_VIDEO_FRAME_WIDTH(&frame),
        GST_VIDEO_FRAME_HEIGHT(&frame));

    ASSERT(newBuffer);
    if (!newBuffer) {
        gst_video_frame_unmap(&frame);
        GST_WARNING("RealtimeOutgoingVideoSourceGStreamer::videoSampleAvailable unable to allocate buffer for conversion to YUV");
        return nullptr;
    }

    newBuffer->Copy(
        GST_VIDEO_FRAME_WIDTH(&frame),
        GST_VIDEO_FRAME_HEIGHT(&frame),
        GST_VIDEO_FRAME_COMP_DATA(&frame, 0),
        GST_VIDEO_FRAME_COMP_STRIDE(&frame, 0),
        GST_VIDEO_FRAME_COMP_DATA(&frame, 1),
        GST_VIDEO_FRAME_COMP_STRIDE(&frame, 1),
        GST_VIDEO_FRAME_COMP_DATA(&frame, 2),
        GST_VIDEO_FRAME_COMP_STRIDE(&frame, 2));
    gst_video_frame_unmap(&frame);

    return newBuffer;
}
}
#endif // USE(LIBWEBRTC)
