/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 * Copyright (C) 2011-2017 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if ENABLE(VIDEO_TRACK)

#include "TrackBase.h"
#include "VideoTrackPrivate.h"

namespace WebCore {

class MediaDescription;
class VideoTrack;

class VideoTrackClient {
public:
    virtual ~VideoTrackClient() = default;
    virtual void videoTrackSelectedChanged(VideoTrack&) = 0;
};

class VideoTrack final : public MediaTrackBase, private VideoTrackPrivateClient {
public:
    static Ref<VideoTrack> create(VideoTrackClient& client, VideoTrackPrivate& trackPrivate)
    {
        return adoptRef(*new VideoTrack(client, trackPrivate));
    }
    virtual ~VideoTrack();

    static const AtomString& alternativeKeyword();
    static const AtomString& captionsKeyword();
    static const AtomString& mainKeyword();
    static const AtomString& signKeyword();
    static const AtomString& subtitlesKeyword();
    static const AtomString& commentaryKeyword();

    bool selected() const { return m_selected; }
    virtual void setSelected(const bool);

    void clearClient() final { m_client = nullptr; }
    VideoTrackClient* client() const { return m_client; }

    size_t inbandTrackIndex();

#if ENABLE(MEDIA_SOURCE)
    void setKind(const AtomString&) final;
    void setLanguage(const AtomString&) final;
#endif

    const MediaDescription& description() const;

    void setPrivate(VideoTrackPrivate&);
    void setMediaElement(WeakPtr<HTMLMediaElement>) override;

private:
    VideoTrack(VideoTrackClient&, VideoTrackPrivate&);

    bool isValidKind(const AtomString&) const final;

    // VideoTrackPrivateClient
    void selectedChanged(bool) final;

    // TrackPrivateBaseClient
    void idChanged(const AtomString&) final;
    void labelChanged(const AtomString&) final;
    void languageChanged(const AtomString&) final;
    void willRemove() final;

    bool enabled() const final { return selected(); }

    void updateKindFromPrivate();

#if !RELEASE_LOG_DISABLED
    const char* logClassName() const final { return "VideoTrack"; }
#endif

    VideoTrackClient* m_client { nullptr };
    Ref<VideoTrackPrivate> m_private;
    bool m_selected { false };
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::VideoTrack)
    static bool isType(const WebCore::TrackBase& track) { return track.type() == WebCore::TrackBase::VideoTrack; }
SPECIALIZE_TYPE_TRAITS_END()

#endif
