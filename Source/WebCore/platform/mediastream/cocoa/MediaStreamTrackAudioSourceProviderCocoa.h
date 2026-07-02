/*
 * Copyright (C) 2020-2025 Apple Inc. All rights reserved.
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

#include "AudioSampleDataSource.h"
#include "MediaStreamTrackPrivate.h"
#include "RealtimeMediaSource.h"
#include "WebAudioSourceProvider.h"
#include <CoreAudio/CoreAudioTypes.h>
#include <wtf/CheckedRef.h>
#include <wtf/MediaTime.h>
#include <wtf/TZoneMalloc.h>

typedef struct AudioBufferList AudioBufferList;
typedef struct OpaqueAudioConverter* AudioConverterRef;
typedef struct AudioStreamBasicDescription AudioStreamBasicDescription;
typedef const struct opaqueCMFormatDescription *CMFormatDescriptionRef;
typedef struct opaqueCMSampleBuffer *CMSampleBufferRef;

namespace WebCore {

class CAAudioStreamDescription;
class PlatformAudioData;
class WebAudioBufferList;

class MediaStreamTrackAudioSourceProviderCocoa final
    : public WebAudioSourceProvider
    , MediaStreamTrackPrivateObserver
    , RealtimeMediaSource::AudioSampleObserver
    , public CanMakeCheckedPtr<MediaStreamTrackAudioSourceProviderCocoa> {
    WTF_MAKE_TZONE_ALLOCATED(MediaStreamTrackAudioSourceProviderCocoa);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(MediaStreamTrackAudioSourceProviderCocoa);
public:
    static Ref<MediaStreamTrackAudioSourceProviderCocoa> create(MediaStreamTrackPrivate&);
    ~MediaStreamTrackAudioSourceProviderCocoa();

    void ref() const final { WebAudioSourceProvider::ref(); }
    void deref() const final { WebAudioSourceProvider::deref(); }

private:
    explicit MediaStreamTrackAudioSourceProviderCocoa(MediaStreamTrackPrivate&);

    // CheckedPtr interface
    uint32_t checkedPtrCount() const final { return CanMakeCheckedPtr::checkedPtrCount(); }
    uint32_t checkedPtrCountWithoutThreadCheck() const final { return CanMakeCheckedPtr::checkedPtrCountWithoutThreadCheck(); }
    void incrementCheckedPtrCount() const final { CanMakeCheckedPtr::incrementCheckedPtrCount(); }
    void decrementCheckedPtrCount() const final { CanMakeCheckedPtr::decrementCheckedPtrCount(); }
    void setDidBeginCheckedPtrDeletion() final { CanMakeCheckedPtr::setDidBeginCheckedPtrDeletion(); }

    void setPollSamplesCount(size_t);

    void hasNewClient(AudioSourceProviderClient*);

    // AudioSourceProvider
    void provideInput(AudioBus&, size_t) final;
    void setClient(WeakPtr<AudioSourceProviderClient>&&) final;
    void prepare(const AudioStreamBasicDescription&);
#if !RELEASE_LOG_DISABLED
    WTF::LoggerHelper& loggerHelper() { return m_source.get(); }
#endif

    // MediaStreamTrackPrivateObserver
    void trackEnded(MediaStreamTrackPrivate&) final { }
    void trackMutedChanged(MediaStreamTrackPrivate&) final { }
    void trackSettingsChanged(MediaStreamTrackPrivate&) final { }
    void trackEnabledChanged(MediaStreamTrackPrivate&) final;

    // RealtimeMediaSource::AudioSampleObserver
    void audioSamplesAvailable(const WTF::MediaTime&, const PlatformAudioData&, const AudioStreamDescription&, size_t) final;

    Lock m_lock;
    WeakPtr<AudioSourceProviderClient> m_client;

    std::optional<CAAudioStreamDescription> m_inputDescription;
    std::optional<CAAudioStreamDescription> m_outputDescription;
    std::unique_ptr<WebAudioBufferList> m_audioBufferList;
    RefPtr<AudioSampleDataSource> m_dataSource;

    size_t m_pollSamplesCount { 3 };
    uint64_t m_writeCount { 0 };
    uint64_t m_readCount { 0 };
    WeakPtr<MediaStreamTrackPrivate> m_captureSource;
    const Ref<RealtimeMediaSource> m_source;
    bool m_enabled { true };
    bool m_connected { false };
};

inline void MediaStreamTrackAudioSourceProviderCocoa::setPollSamplesCount(size_t count)
{
    m_pollSamplesCount = count;
}

}

#endif
