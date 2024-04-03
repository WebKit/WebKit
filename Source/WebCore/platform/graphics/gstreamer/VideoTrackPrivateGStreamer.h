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

#pragma once

#if ENABLE(VIDEO) && USE(GSTREAMER)

#include "GStreamerCommon.h"
#include "TrackPrivateBaseGStreamer.h"
#include "VideoTrackPrivate.h"

#include <wtf/ThreadSafeWeakPtr.h>

namespace WebCore {
class MediaPlayerPrivateGStreamer;

class VideoTrackPrivateGStreamer final : public VideoTrackPrivate, public TrackPrivateBaseGStreamer {
public:
    static Ref<VideoTrackPrivateGStreamer> create(ThreadSafeWeakPtr<MediaPlayerPrivateGStreamer>&& player, unsigned index, GRefPtr<GstPad>&& pad, bool shouldHandleStreamStartEvent = true)
    {
        return adoptRef(*new VideoTrackPrivateGStreamer(WTFMove(player), index, WTFMove(pad), shouldHandleStreamStartEvent));
    }

    static Ref<VideoTrackPrivateGStreamer> create(ThreadSafeWeakPtr<MediaPlayerPrivateGStreamer>&& player, unsigned index, GstStream* stream)
    {
        return adoptRef(*new VideoTrackPrivateGStreamer(WTFMove(player), index, stream));
    }

    Kind kind() const final;

    void disconnect() final;

    void setSelected(bool) final;
    void setActive(bool enabled) final { setSelected(enabled); }

    int trackIndex() const final { return m_index; }

    TrackID id() const final { return m_trackID.value_or(m_index); }
    std::optional<AtomString> trackUID() const final { return m_stringId; }
    AtomString label() const final { return m_label; }
    AtomString language() const final { return m_language; }

    void updateConfigurationFromCaps(GRefPtr<GstCaps>&&) final;

protected:
    void updateConfigurationFromTags(GRefPtr<GstTagList>&&) final;

    void tagsChanged(GRefPtr<GstTagList>&& tags) final { updateConfigurationFromTags(WTFMove(tags)); }
    void capsChanged(const String&, GRefPtr<GstCaps>&&) final;

private:
    VideoTrackPrivateGStreamer(ThreadSafeWeakPtr<MediaPlayerPrivateGStreamer>&&, unsigned index, GRefPtr<GstPad>&&, bool shouldHandleStreamStartEvent);
    VideoTrackPrivateGStreamer(ThreadSafeWeakPtr<MediaPlayerPrivateGStreamer>&&, unsigned index, GstStream*);

    ThreadSafeWeakPtr<MediaPlayerPrivateGStreamer> m_player;
};

} // namespace WebCore

#endif // ENABLE(VIDEO) && USE(GSTREAMER)
