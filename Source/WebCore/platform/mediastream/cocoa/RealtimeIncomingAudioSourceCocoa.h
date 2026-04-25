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

#include "CAAudioStreamDescription.h"
#include "RealtimeIncomingAudioSource.h"
#include "Timer.h"
#include "WebAudioBufferList.h"
#include <CoreAudio/CoreAudioTypes.h>

typedef const struct opaqueCMFormatDescription *CMFormatDescriptionRef;

namespace WebCore {

class RealtimeIncomingAudioSourceCocoa final : public RealtimeIncomingAudioSource {
public:
    static Ref<RealtimeIncomingAudioSourceCocoa> create(Ref<webrtc::AudioTrackInterface>&&, String&&);

private:
    RealtimeIncomingAudioSourceCocoa(Ref<webrtc::AudioTrackInterface>&&, String&&);

    // RealtimeMediaSource API
    void startProducingData() final;
    void stopProducingData()  final;

    // webrtc::AudioTrackSinkInterface API
    void OnData(const void* audioData, int bitsPerSample, int sampleRate, size_t numberOfChannels, size_t numberOfFrames) final;

#if !RELEASE_LOG_DISABLED
    void logTimerFired();
#endif

    static constexpr Seconds LogTimerInterval = 2_s;
    static constexpr size_t ChunksReceivedCountForLogging = 200; // 200 chunks of 10ms = 2s.

    uint64_t m_numberOfFrames { 0 };

    int m_sampleRate { 0 };
    size_t m_numberOfChannels { 0 };
    CAAudioStreamDescription m_streamDescription;
    std::unique_ptr<WebAudioBufferList> m_audioBufferList;
    size_t m_chunksReceived { 0 };
#if !RELEASE_LOG_DISABLED
    size_t m_lastChunksReceived { 0 };
    bool m_audioFormatChanged { false };
    Timer m_logTimer;
#endif
};

} // namespace WebCore

#endif // USE(LIBWEBRTC)
