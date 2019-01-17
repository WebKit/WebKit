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

#include "RealtimeIncomingAudioSource.h"

#include <CoreAudio/CoreAudioTypes.h>

typedef const struct opaqueCMFormatDescription *CMFormatDescriptionRef;

namespace WebCore {

class WebAudioSourceProviderAVFObjC;

class RealtimeIncomingAudioSourceCocoa final : public RealtimeIncomingAudioSource {
public:
    static Ref<RealtimeIncomingAudioSourceCocoa> create(rtc::scoped_refptr<webrtc::AudioTrackInterface>&&, String&&);

private:
    RealtimeIncomingAudioSourceCocoa(rtc::scoped_refptr<webrtc::AudioTrackInterface>&&, String&&);

    // webrtc::AudioTrackSinkInterface API
    void OnData(const void* audioData, int bitsPerSample, int sampleRate, size_t numberOfChannels, size_t numberOfFrames) final;

    uint64_t m_numberOfFrames { 0 };

#if !RELEASE_LOG_DISABLED
    size_t m_chunksReceived { 0 };
#endif
};

} // namespace WebCore

#endif // USE(LIBWEBRTC)
