/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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

#ifndef VideoTrackPrivateMediaStream_h
#define VideoTrackPrivateMediaStream_h

#if ENABLE(VIDEO_TRACK) && ENABLE(MEDIA_STREAM)

#include "MediaStreamTrackPrivate.h"
#include "VideoTrackPrivate.h"

namespace WebCore {

class VideoTrackPrivateMediaStream final : public VideoTrackPrivate {
    WTF_MAKE_NONCOPYABLE(VideoTrackPrivateMediaStream)
public:

    static RefPtr<VideoTrackPrivateMediaStream> create(MediaStreamTrackPrivate& streamTrack)
    {
        return adoptRef(*new VideoTrackPrivateMediaStream(streamTrack));
    }

    virtual ~VideoTrackPrivateMediaStream() { }

    Kind kind() const override { return Kind::Main; }
    AtomicString id() const override { return m_id; }
    AtomicString label() const override { return m_label; }
    AtomicString language() const override { return emptyAtom; }
    int trackIndex() const override { return m_index; }

    void setTrackIndex(int index) { m_index = index; }

    MediaStreamTrackPrivate* streamTrack() const { return m_streamTrack.get(); }

private:
    VideoTrackPrivateMediaStream(MediaStreamTrackPrivate& track)
        : m_streamTrack(&track)
        , m_id(track.id())
        , m_label(track.label())
    {
    }

    RefPtr<MediaStreamTrackPrivate> m_streamTrack;
    AtomicString m_id;
    AtomicString m_label;
    int m_index { 0 };
};

}

#endif // ENABLE(VIDEO_TRACK) && ENABLE(MEDIA_STREAM)

#endif // VideoTrackPrivateMediaStream_h
