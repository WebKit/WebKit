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

#include "AudioTrackPrivate.h"
#include "TrackBase.h"

namespace WebCore {

class AudioTrack;

class AudioTrackClient {
public:
    virtual ~AudioTrackClient() = default;
    virtual void audioTrackEnabledChanged(AudioTrack&) = 0;
};

class AudioTrack final : public MediaTrackBase, private AudioTrackPrivateClient {
public:
    static Ref<AudioTrack> create(AudioTrackClient& client, AudioTrackPrivate& trackPrivate)
    {
        return adoptRef(*new AudioTrack(client, trackPrivate));
    }
    virtual ~AudioTrack();

    static const AtomString& alternativeKeyword();
    static const AtomString& descriptionKeyword();
    static const AtomString& mainKeyword();
    static const AtomString& mainDescKeyword();
    static const AtomString& translationKeyword();
    static const AtomString& commentaryKeyword();

    bool enabled() const final { return m_enabled; }
    void setEnabled(const bool);

    void clearClient() final { m_client = nullptr; }
    AudioTrackClient* client() const { return m_client; }

    size_t inbandTrackIndex() const;

    void setPrivate(AudioTrackPrivate&);
    void setMediaElement(WeakPtr<HTMLMediaElement>) override;

private:
    AudioTrack(AudioTrackClient&, AudioTrackPrivate&);

    bool isValidKind(const AtomString&) const final;

    // AudioTrackPrivateClient
    void enabledChanged(bool) final;

    // TrackPrivateBaseClient
    void idChanged(const AtomString&) final;
    void labelChanged(const AtomString&) final;
    void languageChanged(const AtomString&) final;
    void willRemove() final;

    void updateKindFromPrivate();

#if !RELEASE_LOG_DISABLED
    const char* logClassName() const final { return "AudioTrack"; }
#endif

    AudioTrackClient* m_client { nullptr };
    Ref<AudioTrackPrivate> m_private;
    bool m_enabled { false };
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::AudioTrack)
    static bool isType(const WebCore::TrackBase& track) { return track.type() == WebCore::TrackBase::AudioTrack; }
SPECIALIZE_TYPE_TRAITS_END()

#endif
