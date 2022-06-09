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

#include "AudioTrackPrivateGStreamer.h"

#include "MediaPlayerPrivateGStreamer.h"

namespace WebCore {

AudioTrackPrivateGStreamer::AudioTrackPrivateGStreamer(WeakPtr<MediaPlayerPrivateGStreamer> player, unsigned index, GRefPtr<GstPad>&& pad, bool shouldHandleStreamStartEvent)
    : TrackPrivateBaseGStreamer(TrackPrivateBaseGStreamer::TrackType::Audio, this, index, WTFMove(pad), shouldHandleStreamStartEvent)
    , m_player(player)
{
}

AudioTrackPrivateGStreamer::AudioTrackPrivateGStreamer(WeakPtr<MediaPlayerPrivateGStreamer> player, unsigned index, GRefPtr<GstStream>&& stream)
    : TrackPrivateBaseGStreamer(TrackPrivateBaseGStreamer::TrackType::Audio, this, index, WTFMove(stream))
    , m_player(player)
{
    int kind;
    auto tags = adoptGRef(gst_stream_get_tags(m_stream.get()));

    if (tags && gst_tag_list_get_int(tags.get(), "webkit-media-stream-kind", &kind) && kind == static_cast<int>(AudioTrackPrivate::Kind::Main)) {
        auto streamFlags = gst_stream_get_stream_flags(m_stream.get());
        gst_stream_set_stream_flags(m_stream.get(), static_cast<GstStreamFlags>(streamFlags | GST_STREAM_FLAG_SELECT));
    }
}

AudioTrackPrivate::Kind AudioTrackPrivateGStreamer::kind() const
{
    if (m_stream.get() && gst_stream_get_stream_flags(m_stream.get()) & GST_STREAM_FLAG_SELECT)
        return AudioTrackPrivate::Kind::Main;

    return AudioTrackPrivate::kind();
}

void AudioTrackPrivateGStreamer::disconnect()
{
    m_player = nullptr;
    TrackPrivateBaseGStreamer::disconnect();
}

void AudioTrackPrivateGStreamer::setEnabled(bool enabled)
{
    if (enabled == this->enabled())
        return;
    AudioTrackPrivate::setEnabled(enabled);

    if (m_player)
        m_player->updateEnabledAudioTrack();
}

} // namespace WebCore

#endif // ENABLE(VIDEO) && USE(GSTREAMER)
