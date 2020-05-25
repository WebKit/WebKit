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

#include "CAAudioStreamDescription.h"
#include "MediaStreamTrackPrivate.h"
#include "WebAudioSourceProvider.h"
#include <CoreAudio/CoreAudioTypes.h>
#include <wtf/Lock.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>

typedef struct AudioBufferList AudioBufferList;
typedef struct OpaqueAudioConverter* AudioConverterRef;
typedef struct AudioStreamBasicDescription AudioStreamBasicDescription;
typedef const struct opaqueCMFormatDescription *CMFormatDescriptionRef;
typedef struct opaqueCMSampleBuffer *CMSampleBufferRef;

namespace WebCore {

class AudioSampleDataSource;
class CAAudioStreamDescription;
class WebAudioBufferList;

class WEBCORE_EXPORT WebAudioSourceProviderAVFObjC final
    : public WebAudioSourceProvider
    , MediaStreamTrackPrivate::Observer
    , RealtimeMediaSource::AudioSampleObserver {
public:
    static Ref<WebAudioSourceProviderAVFObjC> create(MediaStreamTrackPrivate&);
    virtual ~WebAudioSourceProviderAVFObjC();

    void prepare(const AudioStreamBasicDescription&);
    void unprepare();

private:
    explicit WebAudioSourceProviderAVFObjC(MediaStreamTrackPrivate&);

    // AudioSourceProvider
    void provideInput(AudioBus*, size_t) final;
    void setClient(AudioSourceProviderClient*) final;

    // MediaStreamTrackPrivate::Observer
    void trackEnded(MediaStreamTrackPrivate&) final { }
    void trackMutedChanged(MediaStreamTrackPrivate&) final { }
    void trackSettingsChanged(MediaStreamTrackPrivate&) final { }
    void trackEnabledChanged(MediaStreamTrackPrivate&) final;

    // RealtimeMediaSource::AudioSampleObserver
    void audioSamplesAvailable(const MediaTime&, const PlatformAudioData&, const AudioStreamDescription&, size_t) final;

    size_t m_listBufferSize { 0 };
    Optional<CAAudioStreamDescription> m_inputDescription;
    Optional<CAAudioStreamDescription> m_outputDescription;
    std::unique_ptr<WebAudioBufferList> m_audioBufferList;
    RefPtr<AudioSampleDataSource> m_dataSource;

    uint64_t m_writeCount { 0 };
    uint64_t m_readCount { 0 };
    AudioSourceProviderClient* m_client { nullptr };
    MediaStreamTrackPrivate* m_captureSource { nullptr };
    Ref<RealtimeMediaSource> m_source;
    Lock m_mutex;
    bool m_connected { false };
    bool m_enabled { true };
};

}

#endif
