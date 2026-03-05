/*
 * Copyright (C) 2017 Igalia S.L. All rights reserved.
 * Copyright (C) 2017 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in the documentation and/or other materials provided with the
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#if USE(LIBWEBRTC) && USE(GSTREAMER)
#include "RealtimeIncomingVideoSourceLibWebRTC.h"

#include "GStreamerVideoFrameLibWebRTC.h"
#include "LibWebRTCVideoFrameUtilities.h"
#include "VideoFrameGStreamer.h"

namespace WebCore {

GST_DEBUG_CATEGORY(webkit_libwebrtc_incoming_video_debug);
#define GST_CAT_DEFAULT webkit_libwebrtc_incoming_video_debug

Ref<RealtimeIncomingVideoSource> RealtimeIncomingVideoSource::create(Ref<webrtc::VideoTrackInterface>&& videoTrack, String&& trackId)
{
    auto source = RealtimeIncomingVideoSourceLibWebRTC::create(WTF::move(videoTrack), WTF::move(trackId));
    source->start();
    return source;
}

Ref<RealtimeIncomingVideoSourceLibWebRTC> RealtimeIncomingVideoSourceLibWebRTC::create(Ref<webrtc::VideoTrackInterface>&& videoTrack, String&& trackId)
{
    return adoptRef(*new RealtimeIncomingVideoSourceLibWebRTC(WTF::move(videoTrack), WTF::move(trackId)));
}

RealtimeIncomingVideoSourceLibWebRTC::RealtimeIncomingVideoSourceLibWebRTC(Ref<webrtc::VideoTrackInterface>&& videoTrack, String&& videoTrackId)
    : RealtimeIncomingVideoSource(WTF::move(videoTrack), WTF::move(videoTrackId))
{
    static std::once_flag onceFlag;
    std::call_once(onceFlag, [] {
        GST_DEBUG_CATEGORY_INIT(webkit_libwebrtc_incoming_video_debug, "webkitlibwebrtcvideoincoming", 0, "WebKit LibWebRTC incoming video source");
    });
    GST_DEBUG("Created incoming video source with ID: %s", persistentID().utf8().data());
}

void RealtimeIncomingVideoSourceLibWebRTC::OnFrame(const webrtc::VideoFrame& frame)
{
    if (!isProducingData())
        return;

#if GST_CHECK_VERSION(1, 22, 0)
    GST_TRACE_ID(persistentID().utf8().data(), "Handling incoming video frame");
#else
    GST_TRACE("Handling incoming video frame");
#endif

    auto presentationTime = MediaTime(frame.timestamp_us(), G_USEC_PER_SEC);
    auto sample = convertLibWebRTCVideoFrameToGStreamerSample(frame);
    VideoFrameGStreamer::CreateOptions options;
    options.timeMetadata = std::make_optional(metadataFromVideoFrame(frame));
    options.presentationTime = presentationTime;
    options.rotation = videoRotationFromLibWebRTCVideoFrame(frame);
    videoFrameAvailable(VideoFrameGStreamer::create(WTF::move(sample), options), { });
}

#undef GST_CAT_DEFAULT

} // namespace WebCore

#endif // USE(LIBWEBRTC) && USE(GSTREAMER)
