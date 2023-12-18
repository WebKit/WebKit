/*
 * Copyright (C) 2015-2017 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if ENABLE(MEDIA_STREAM)

#include "MediaStreamTrackPrivate.h"
#include "VideoTrackPrivate.h"
#include <wtf/text/StringToIntegerConversion.h>

namespace WebCore {

class VideoTrackPrivateMediaStream final : public VideoTrackPrivate {
    WTF_MAKE_NONCOPYABLE(VideoTrackPrivateMediaStream)
public:
    static Ref<VideoTrackPrivateMediaStream> create(MediaStreamTrackPrivate& streamTrack)
    {
        return adoptRef(*new VideoTrackPrivateMediaStream(streamTrack));
    }

    void setTrackIndex(int index) { m_index = index; }

    MediaStreamTrackPrivate& streamTrack() { return m_streamTrack.get(); }

private:
    VideoTrackPrivateMediaStream(MediaStreamTrackPrivate& track)
        : m_streamTrack(track)
    {
    }

    Kind kind() const final { return Kind::Main; }
    std::optional<AtomString> trackUID() const { return AtomString { m_streamTrack->id() }; }
    AtomString label() const final { return AtomString { m_streamTrack->label() }; }
    AtomString language() const final { return emptyAtom(); }
    int trackIndex() const final { return m_index; }

    Ref<MediaStreamTrackPrivate> m_streamTrack;
    int m_index { 0 };
};

}

#endif // ENABLE(MEDIA_STREAM)
