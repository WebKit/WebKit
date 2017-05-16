/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of Ericsson nor the names of its contributors
 *    may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if USE(LIBWEBRTC)

#include "LibWebRTCMacros.h"
#include "RealtimeMediaSource.h"
#include <CoreAudio/CoreAudioTypes.h>
#include <webrtc/api/mediastreaminterface.h>
#include <wtf/RetainPtr.h>

typedef const struct opaqueCMFormatDescription *CMFormatDescriptionRef;

namespace WebCore {

class WebAudioSourceProviderAVFObjC;

class RealtimeIncomingAudioSource final : public RealtimeMediaSource, private webrtc::AudioTrackSinkInterface {
public:
    static Ref<RealtimeIncomingAudioSource> create(rtc::scoped_refptr<webrtc::AudioTrackInterface>&&, String&&);

    void setSourceTrack(rtc::scoped_refptr<webrtc::AudioTrackInterface>&&);

private:
    RealtimeIncomingAudioSource(rtc::scoped_refptr<webrtc::AudioTrackInterface>&&, String&&);
    ~RealtimeIncomingAudioSource();

    // webrtc::AudioTrackSinkInterface API
    void OnData(const void* audioData, int bitsPerSample, int sampleRate, size_t numberOfChannels, size_t numberOfFrames) final;

    // RealtimeMediaSource API
    void startProducingData() final;
    void stopProducingData()  final;

    const RealtimeMediaSourceCapabilities& capabilities() const final;
    const RealtimeMediaSourceSettings& settings() const final;

    AudioSourceProvider* audioSourceProvider() final;

    RealtimeMediaSourceSettings m_currentSettings;
    rtc::scoped_refptr<webrtc::AudioTrackInterface> m_audioTrack;

    RefPtr<WebAudioSourceProviderAVFObjC> m_audioSourceProvider;
    AudioStreamBasicDescription m_streamFormat { };
    uint64_t m_numberOfFrames { 0 };
};

} // namespace WebCore

#endif // USE(LIBWEBRTC)
