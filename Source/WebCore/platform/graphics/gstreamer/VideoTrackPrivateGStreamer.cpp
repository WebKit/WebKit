/*
 * Copyright (C) 2013 Cable Television Laboratories, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#if ENABLE(VIDEO) && USE(GSTREAMER)

#include "VideoTrackPrivateGStreamer.h"

#include "GStreamerCommon.h"
#include "MediaPlayerPrivateGStreamer.h"
#include <gst/pbutils/pbutils.h>

namespace WebCore {

VideoTrackPrivateGStreamer::VideoTrackPrivateGStreamer(WeakPtr<MediaPlayerPrivateGStreamer> player, unsigned index, GRefPtr<GstPad>&& pad, bool shouldHandleStreamStartEvent)
    : TrackPrivateBaseGStreamer(TrackPrivateBaseGStreamer::TrackType::Video, this, index, WTFMove(pad), shouldHandleStreamStartEvent)
    , m_player(player)
{
}

VideoTrackPrivateGStreamer::VideoTrackPrivateGStreamer(WeakPtr<MediaPlayerPrivateGStreamer> player, unsigned index, GstStream* stream)
    : TrackPrivateBaseGStreamer(TrackPrivateBaseGStreamer::TrackType::Video, this, index, stream)
    , m_player(player)
{
    int kind;
    auto tags = adoptGRef(gst_stream_get_tags(m_stream.get()));

    if (tags && gst_tag_list_get_int(tags.get(), "webkit-media-stream-kind", &kind) && kind == static_cast<int>(VideoTrackPrivate::Kind::Main)) {
        auto streamFlags = gst_stream_get_stream_flags(m_stream.get());
        gst_stream_set_stream_flags(m_stream.get(), static_cast<GstStreamFlags>(streamFlags | GST_STREAM_FLAG_SELECT));
    }

    g_signal_connect_swapped(m_stream.get(), "notify::caps", G_CALLBACK(+[](VideoTrackPrivateGStreamer* track) {
        track->m_taskQueue.enqueueTask([track]() {
            track->updateConfigurationFromCaps();
        });
    }), this);
    g_signal_connect_swapped(m_stream.get(), "notify::tags", G_CALLBACK(+[](VideoTrackPrivateGStreamer* track) {
        if (isMainThread())
            track->updateConfigurationFromTags();
        else
            track->m_taskQueue.enqueueTask([track]() {
                track->updateConfigurationFromTags();
            });
    }), this);

    updateConfigurationFromCaps();
    updateConfigurationFromTags();
}

void VideoTrackPrivateGStreamer::updateConfigurationFromTags()
{
    ASSERT(isMainThread());
    if (!m_stream)
        return;

    auto tags = adoptGRef(gst_stream_get_tags(m_stream.get()));
    unsigned bitrate;
    if (!tags || !gst_tag_list_get_uint(tags.get(), GST_TAG_BITRATE, &bitrate))
        return;

    auto configuration = this->configuration();
    configuration.bitrate = bitrate;
    setConfiguration(WTFMove(configuration));
}

void VideoTrackPrivateGStreamer::updateConfigurationFromCaps()
{
    ASSERT(isMainThread());
    if (!m_stream)
        return;

    auto caps = adoptGRef(gst_stream_get_caps(m_stream.get()));
    if (!caps || !gst_caps_is_fixed(caps.get()))
        return;

    // We might be notified of RTP caps here, when an incoming video track is re-enabled. Since
    // those caps most likely do not contain the information we need (width, height, colorimetry,
    // ...), keep previous configuration and return early.
    if (!doCapsHaveType(caps.get(), GST_VIDEO_CAPS_TYPE_PREFIX))
        return;

    auto configuration = this->configuration();
    GstVideoInfo info;
    if (gst_video_info_from_caps(&info, caps.get())) {
        if (GST_VIDEO_INFO_FPS_N(&info)) {
            double framerate;
            gst_util_fraction_to_double(GST_VIDEO_INFO_FPS_N(&info), GST_VIDEO_INFO_FPS_D(&info), &framerate);
            configuration.framerate = framerate;
        }
        configuration.width = GST_VIDEO_INFO_WIDTH(&info);
        configuration.height = GST_VIDEO_INFO_HEIGHT(&info);
        configuration.colorSpace = videoColorSpaceFromInfo(info);
    }

#if GST_CHECK_VERSION(1, 20, 0)
    GUniquePtr<char> codec(gst_codec_utils_caps_get_mime_codec(caps.get()));
    configuration.codec = String::fromLatin1(codec.get());
#endif

    setConfiguration(WTFMove(configuration));
}

VideoTrackPrivate::Kind VideoTrackPrivateGStreamer::kind() const
{
    if (m_stream && gst_stream_get_stream_flags(m_stream.get()) & GST_STREAM_FLAG_SELECT)
        return VideoTrackPrivate::Kind::Main;

    return VideoTrackPrivate::kind();
}

void VideoTrackPrivateGStreamer::disconnect()
{
    m_taskQueue.startAborting();

    if (m_stream)
        g_signal_handlers_disconnect_matched(m_stream.get(), G_SIGNAL_MATCH_DATA, 0, 0, nullptr, nullptr, this);

    m_player = nullptr;
    TrackPrivateBaseGStreamer::disconnect();

    m_taskQueue.finishAborting();
}

void VideoTrackPrivateGStreamer::setSelected(bool selected)
{
    if (selected == this->selected())
        return;
    VideoTrackPrivate::setSelected(selected);

    if (m_player)
        m_player->updateEnabledVideoTrack();
}

} // namespace WebCore

#endif // ENABLE(VIDEO) && USE(GSTREAMER)
