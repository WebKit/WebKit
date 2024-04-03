/*
 * Copyright (C) 2018 Metrological Group B.V.
 * Copyright (C) 2018 Igalia S.L. All rights reserved.
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
 * aint with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#pragma once

#if USE(GSTREAMER) && USE(LIBWEBRTC)

#include "GStreamerCommon.h"
#include "LibWebRTCMacros.h"
#include "webrtc/api/video/i420_buffer.h"
#include "webrtc/api/video/video_frame.h"
#include "webrtc/common_video/include/video_frame_buffer.h"
#include "webrtc/rtc_base/ref_counted_object.h"

namespace WebCore {

WARN_UNUSED_RETURN GRefPtr<GstSample> convertLibWebRTCVideoFrameToGStreamerSample(const webrtc::VideoFrame&);

std::unique_ptr<webrtc::VideoFrame> convertGStreamerSampleToLibWebRTCVideoFrame(GRefPtr<GstSample>&&, webrtc::VideoRotation, int64_t timestamp, int64_t renderTimeMs);

class GStreamerVideoFrameLibWebRTC : public rtc::RefCountedObject<webrtc::VideoFrameBuffer> {
public:
    GStreamerVideoFrameLibWebRTC(GRefPtr<GstSample>&& sample, GstVideoInfo info)
        : m_sample(WTFMove(sample))
        , m_info(info)
    {
    }

    static rtc::scoped_refptr<webrtc::VideoFrameBuffer> create(GRefPtr<GstSample>&&);

    GstSample* getSample() const { return m_sample.get(); }
    rtc::scoped_refptr<webrtc::I420BufferInterface> ToI420() final;

    int width() const final { return GST_VIDEO_INFO_WIDTH(&m_info); }
    int height() const final { return GST_VIDEO_INFO_HEIGHT(&m_info); }

private:
    webrtc::VideoFrameBuffer::Type type() const final { return Type::kNative; }

    GRefPtr<GstSample> m_sample;
    GstVideoInfo m_info;
};

}

#endif // USE(GSTREAMER) && USE(LIBWEBRTC)
