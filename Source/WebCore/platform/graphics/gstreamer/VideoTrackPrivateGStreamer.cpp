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
#include <glib-object.h>

namespace WebCore {

VideoTrackPrivateGStreamer::VideoTrackPrivateGStreamer(WeakPtr<MediaPlayerPrivateGStreamer> player, gint index, GRefPtr<GstPad> pad)
    : TrackPrivateBaseGStreamer(this, index, pad)
    , m_player(player)
{
    // FIXME: Get a real ID from the tkhd atom.
    m_id = "V" + String::number(index);
}

VideoTrackPrivateGStreamer::VideoTrackPrivateGStreamer(WeakPtr<MediaPlayerPrivateGStreamer> player, gint index, GRefPtr<GstStream> stream)
    : TrackPrivateBaseGStreamer(this, index, stream)
    , m_player(player)
{
    gint kind;
    auto tags = adoptGRef(gst_stream_get_tags(m_stream.get()));

    if (tags && gst_tag_list_get_int(tags.get(), "webkit-media-stream-kind", &kind) && kind == static_cast<int>(VideoTrackPrivate::Kind::Main)) {
        GstStreamFlags streamFlags = gst_stream_get_stream_flags(stream.get());
        gst_stream_set_stream_flags(stream.get(), static_cast<GstStreamFlags>(streamFlags | GST_STREAM_FLAG_SELECT));
    }

    m_id = gst_stream_get_stream_id(stream.get());
}

VideoTrackPrivate::Kind VideoTrackPrivateGStreamer::kind() const
{
    if (m_stream.get() && gst_stream_get_stream_flags(m_stream.get()) & GST_STREAM_FLAG_SELECT)
        return VideoTrackPrivate::Kind::Main;

    return VideoTrackPrivate::kind();
}

void VideoTrackPrivateGStreamer::disconnect()
{
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
