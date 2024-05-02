/*
 * Copyright (C) 2020-2023 Apple Inc. All rights reserved.
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
#include <wtf/CheckedRef.h>
#include <wtf/MediaTime.h>

namespace WebCore {

class MediaStreamTrackAudioSourceProviderCocoa final
    : public WebAudioSourceProviderCocoa
    , MediaStreamTrackPrivateObserver
    , RealtimeMediaSource::AudioSampleObserver
    , public CanMakeCheckedPtr<MediaStreamTrackAudioSourceProviderCocoa> {
    WTF_MAKE_FAST_ALLOCATED;
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(MediaStreamTrackAudioSourceProviderCocoa);
public:
    static Ref<MediaStreamTrackAudioSourceProviderCocoa> create(MediaStreamTrackPrivate&);
    ~MediaStreamTrackAudioSourceProviderCocoa();

private:
    explicit MediaStreamTrackAudioSourceProviderCocoa(MediaStreamTrackPrivate&);

    // CheckedPtr interface
    uint32_t ptrCount() const final { return CanMakeCheckedPtr::ptrCount(); }
    uint32_t ptrCountWithoutThreadCheck() const final { return CanMakeCheckedPtr::ptrCountWithoutThreadCheck(); }
    void incrementPtrCount() const final { CanMakeCheckedPtr::incrementPtrCount(); }
    void decrementPtrCount() const final { CanMakeCheckedPtr::decrementPtrCount(); }

    // WebAudioSourceProviderCocoa
    void hasNewClient(AudioSourceProviderClient*) final;
#if !RELEASE_LOG_DISABLED
    WTF::LoggerHelper& loggerHelper() final { return m_source.get(); }
#endif

    // MediaStreamTrackPrivateObserver
    void trackEnded(MediaStreamTrackPrivate&) final { }
    void trackMutedChanged(MediaStreamTrackPrivate&) final { }
    void trackSettingsChanged(MediaStreamTrackPrivate&) final { }
    void trackEnabledChanged(MediaStreamTrackPrivate&) final;

    // RealtimeMediaSource::AudioSampleObserver
    void audioSamplesAvailable(const WTF::MediaTime&, const PlatformAudioData&, const AudioStreamDescription&, size_t) final;

    WeakPtr<MediaStreamTrackPrivate> m_captureSource;
    Ref<RealtimeMediaSource> m_source;
    bool m_enabled { true };
    bool m_connected { false };
};

}

#endif
