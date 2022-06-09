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

#include "AudioTrackPrivate.h"
#include "TrackPrivateBaseGStreamer.h"

#include <wtf/WeakPtr.h>

namespace WebCore {
class MediaPlayerPrivateGStreamer;

class AudioTrackPrivateGStreamer final : public AudioTrackPrivate, public TrackPrivateBaseGStreamer {
public:
    static Ref<AudioTrackPrivateGStreamer> create(WeakPtr<MediaPlayerPrivateGStreamer> player, unsigned index, GRefPtr<GstPad>&& pad, bool shouldHandleStreamStartEvent = true)
    {
        return adoptRef(*new AudioTrackPrivateGStreamer(player, index, WTFMove(pad), shouldHandleStreamStartEvent));
    }

    static Ref<AudioTrackPrivateGStreamer> create(WeakPtr<MediaPlayerPrivateGStreamer> player, unsigned index, GRefPtr<GstStream>&& stream)
    {
        return adoptRef(*new AudioTrackPrivateGStreamer(player, index, WTFMove(stream)));
    }

    Kind kind() const final;

    void disconnect() final;

    void setEnabled(bool) final;
    void setActive(bool enabled) final { setEnabled(enabled); }

    int trackIndex() const final { return m_index; }

    AtomString id() const final { return m_id; }
    AtomString label() const final { return m_label; }
    AtomString language() const final { return m_language; }

private:
    AudioTrackPrivateGStreamer(WeakPtr<MediaPlayerPrivateGStreamer>, unsigned index, GRefPtr<GstPad>&&, bool shouldHandleStreamStartEvent);
    AudioTrackPrivateGStreamer(WeakPtr<MediaPlayerPrivateGStreamer>, unsigned index, GRefPtr<GstStream>&&);

    WeakPtr<MediaPlayerPrivateGStreamer> m_player;
};

} // namespace WebCore

#endif // ENABLE(VIDEO) && USE(GSTREAMER)
