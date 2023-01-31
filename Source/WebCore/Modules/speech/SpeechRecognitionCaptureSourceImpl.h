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

#include "RealtimeMediaSource.h"
#include "SpeechRecognitionConnectionClientIdentifier.h"
#include <wtf/Lock.h>

#if PLATFORM(COCOA)
#include "AudioSampleDataSource.h"
#endif

namespace WTF {
class MediaTime;
}

namespace WebCore {

class AudioStreamDescription;
class PlatformAudioData;
class SpeechRecognitionUpdate;
enum class SpeechRecognitionUpdateType : uint8_t;

class SpeechRecognitionCaptureSourceImpl
    : public RealtimeMediaSource::Observer
    , public RealtimeMediaSource::AudioSampleObserver {
    WTF_MAKE_FAST_ALLOCATED;
public:
    using DataCallback = Function<void(const MediaTime&, const PlatformAudioData&, const AudioStreamDescription&, size_t)>;
    using StateUpdateCallback = Function<void(const SpeechRecognitionUpdate&)>;
    SpeechRecognitionCaptureSourceImpl(SpeechRecognitionConnectionClientIdentifier, DataCallback&&, StateUpdateCallback&&, Ref<RealtimeMediaSource>&&);
    ~SpeechRecognitionCaptureSourceImpl();
    void mute();

private:
    // RealtimeMediaSource::AudioSampleObserver
    void audioSamplesAvailable(const MediaTime&, const PlatformAudioData&, const AudioStreamDescription&, size_t) final;

#if PLATFORM(COCOA)
    void pullSamplesAndCallDataCallback(AudioSampleDataSource*, const MediaTime&, const CAAudioStreamDescription&, size_t sampleCount);
#endif

    // RealtimeMediaSource::Observer
    void sourceStarted() final;
    void sourceStopped() final;
    void sourceMutedChanged() final;

    SpeechRecognitionConnectionClientIdentifier m_clientIdentifier;
    DataCallback m_dataCallback;
    StateUpdateCallback m_stateUpdateCallback;
    Ref<RealtimeMediaSource> m_source;

#if PLATFORM(COCOA)
    RefPtr<AudioSampleDataSource> m_dataSource WTF_GUARDED_BY_LOCK(m_dataSourceLock);
    Lock m_dataSourceLock;
#endif
};

} // namespace WebCore

#endif

