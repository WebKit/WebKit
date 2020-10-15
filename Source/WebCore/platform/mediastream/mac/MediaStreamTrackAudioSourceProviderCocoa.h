/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if ENABLE(WEB_AUDIO) && ENABLE(MEDIA_STREAM)

#include "MediaStreamTrackPrivate.h"
#include "RealtimeMediaSource.h"
#include "WebAudioSourceProviderCocoa.h"

namespace WebCore {

class MediaStreamTrackAudioSourceProviderCocoa final
    : public WebAudioSourceProviderCocoa
    , MediaStreamTrackPrivate::Observer
    , RealtimeMediaSource::AudioSampleObserver {
public:
    static Ref<MediaStreamTrackAudioSourceProviderCocoa> create(MediaStreamTrackPrivate&);
    ~MediaStreamTrackAudioSourceProviderCocoa();

private:
    explicit MediaStreamTrackAudioSourceProviderCocoa(MediaStreamTrackPrivate&);

    // WebAudioSourceProviderCocoa
    void hasNewClient(AudioSourceProviderClient*) final;
#if !RELEASE_LOG_DISABLED
    WTF::LoggerHelper& loggerHelper() final { return m_source.get(); }
#endif

    // MediaStreamTrackPrivate::Observer
    void trackEnded(MediaStreamTrackPrivate&) final { }
    void trackMutedChanged(MediaStreamTrackPrivate&) final { }
    void trackSettingsChanged(MediaStreamTrackPrivate&) final { }
    void trackEnabledChanged(MediaStreamTrackPrivate&) final;

    // RealtimeMediaSource::AudioSampleObserver
    void audioSamplesAvailable(const MediaTime&, const PlatformAudioData&, const AudioStreamDescription&, size_t) final;

    WeakPtr<MediaStreamTrackPrivate> m_captureSource;
    Ref<RealtimeMediaSource> m_source;
    bool m_enabled { true };
    bool m_connected { false };
};

}

#endif
