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

#include "MediaPlayerPrivateGStreamer.h"
#include <gst/pbutils/pbutils.h>

namespace WebCore {

VideoTrackPrivateGStreamer::VideoTrackPrivateGStreamer(WeakPtr<MediaPlayerPrivateGStreamer> player, unsigned index, GRefPtr<GstPad>&& pad, bool shouldHandleStreamStartEvent)
    : TrackPrivateBaseGStreamer(TrackPrivateBaseGStreamer::TrackType::Video, this, index, WTFMove(pad), shouldHandleStreamStartEvent)
    , m_player(player)
{
}

VideoTrackPrivateGStreamer::VideoTrackPrivateGStreamer(WeakPtr<MediaPlayerPrivateGStreamer> player, unsigned index, GRefPtr<GstStream>&& stream)
    : TrackPrivateBaseGStreamer(TrackPrivateBaseGStreamer::TrackType::Video, this, index, WTFMove(stream))
    , m_player(player)
{
    int kind;
    auto tags = adoptGRef(gst_stream_get_tags(m_stream.get()));

    if (tags && gst_tag_list_get_int(tags.get(), "webkit-media-stream-kind", &kind) && kind == static_cast<int>(VideoTrackPrivate::Kind::Main)) {
        auto streamFlags = gst_stream_get_stream_flags(m_stream.get());
        gst_stream_set_stream_flags(m_stream.get(), static_cast<GstStreamFlags>(streamFlags | GST_STREAM_FLAG_SELECT));
    }

    g_signal_connect_swapped(m_stream.get(), "notify::caps", G_CALLBACK(+[](VideoTrackPrivateGStreamer* track) {
        track->updateConfigurationFromCaps();
    }), this);
    g_signal_connect_swapped(m_stream.get(), "notify::tags", G_CALLBACK(+[](VideoTrackPrivateGStreamer* track) {
        track->updateConfigurationFromTags();
    }), this);

    updateConfigurationFromCaps();
    updateConfigurationFromTags();
}

void VideoTrackPrivateGStreamer::updateConfigurationFromTags()
{
    auto tags = adoptGRef(gst_stream_get_tags(m_stream.get()));
    unsigned bitrate;
    if (!tags || !gst_tag_list_get_uint(tags.get(), GST_TAG_BITRATE, &bitrate))
        return;

    auto configuration = this->configuration();
    configuration.bitrate = bitrate;
    callOnMainThreadAndWait([&] {
        setConfiguration(WTFMove(configuration));
    });
}

void VideoTrackPrivateGStreamer::updateConfigurationFromCaps()
{
    auto caps = adoptGRef(gst_stream_get_caps(m_stream.get()));
    if (!caps || !gst_caps_is_fixed(caps.get()))
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

#ifndef GST_DISABLE_GST_DEBUG
        GUniquePtr<char> colorimetry(gst_video_colorimetry_to_string(&GST_VIDEO_INFO_COLORIMETRY(&info)));
#endif
        PlatformVideoColorSpace colorSpace;
        switch (GST_VIDEO_INFO_COLORIMETRY(&info).matrix) {
        case GST_VIDEO_COLOR_MATRIX_RGB:
            colorSpace.matrix = PlatformVideoMatrixCoefficients::Rgb;
            break;
        case GST_VIDEO_COLOR_MATRIX_BT709:
            colorSpace.matrix = PlatformVideoMatrixCoefficients::Bt709;
            break;
        case GST_VIDEO_COLOR_MATRIX_BT601:
            colorSpace.matrix = PlatformVideoMatrixCoefficients::Bt470bg;
            break;
        default:
#ifndef GST_DISABLE_GST_DEBUG
            GST_DEBUG("Unhandled colorspace matrix from %s", colorimetry.get());
#endif
            break;
        }

        switch (GST_VIDEO_INFO_COLORIMETRY(&info).transfer) {
        case GST_VIDEO_TRANSFER_SRGB:
            colorSpace.transfer = PlatformVideoTransferCharacteristics::Iec6196621;
            break;
        case GST_VIDEO_TRANSFER_BT709:
            colorSpace.transfer = PlatformVideoTransferCharacteristics::Bt709;
            break;
#if GST_CHECK_VERSION(1, 18, 0)
        case GST_VIDEO_TRANSFER_BT601:
            colorSpace.transfer = PlatformVideoTransferCharacteristics::Smpte170m;
            break;
#endif
        default:
#ifndef GST_DISABLE_GST_DEBUG
            GST_DEBUG("Unhandled colorspace transfer from %s", colorimetry.get());
#endif
            break;
        }

        switch (GST_VIDEO_INFO_COLORIMETRY(&info).primaries) {
        case GST_VIDEO_COLOR_PRIMARIES_BT709:
            colorSpace.primaries = PlatformVideoColorPrimaries::Bt709;
            break;
        case GST_VIDEO_COLOR_PRIMARIES_BT470BG:
            colorSpace.primaries = PlatformVideoColorPrimaries::Bt470bg;
            break;
        case GST_VIDEO_COLOR_PRIMARIES_SMPTE170M:
            colorSpace.primaries = PlatformVideoColorPrimaries::Smpte170m;
            break;
        default:
#ifndef GST_DISABLE_GST_DEBUG
            GST_DEBUG("Unhandled colorspace primaries from %s", colorimetry.get());
#endif
            break;
        }
        configuration.colorSpace = WTFMove(colorSpace);
    }

#if GST_CHECK_VERSION(1, 20, 0)
    GUniquePtr<char> codec(gst_codec_utils_caps_get_mime_codec(caps.get()));
    configuration.codec = codec.get();
#endif

    callOnMainThreadAndWait([&] {
        setConfiguration(WTFMove(configuration));
    });
}

VideoTrackPrivate::Kind VideoTrackPrivateGStreamer::kind() const
{
    if (m_stream.get() && gst_stream_get_stream_flags(m_stream.get()) & GST_STREAM_FLAG_SELECT)
        return VideoTrackPrivate::Kind::Main;

    return VideoTrackPrivate::kind();
}

void VideoTrackPrivateGStreamer::disconnect()
{
    if (m_stream)
        g_signal_handlers_disconnect_matched(m_stream.get(), G_SIGNAL_MATCH_DATA, 0, 0, nullptr, nullptr, this);

    m_player = nullptr;
    TrackPrivateBaseGStreamer::disconnect();
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
